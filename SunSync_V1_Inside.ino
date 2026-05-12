#include <esp_now.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// -------- LCD --------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// -------- SERVO --------
Servo myservo;
const int servoPin = 26;

// -------- INPUTS --------
const int modeSwitch = 25;
const int potPin = 34;
const int tempButton = 27;

// -------- TEMP DISPLAY --------
bool showFahrenheit = true;
bool lastButtonState = HIGH;

// -------- SERVO POSITION --------
float currentServoPos = 90;

// -------- SLOW CONTROL TIMER --------
unsigned long lastServoUpdate = 0;
const int servoDelayMs = 80;   // 🔥 BIG SLOWDOWN (increase = slower)

// -------- DATA STRUCT --------
typedef struct struct_message {
  float tempC;
  float tempF;
  float voltage;
  int lightAnalog;
  int lightDigital;
} struct_message;

struct_message incomingData;

// -------- LIGHT RANGE --------
const int LIGHT_MIN = 500;
const int LIGHT_MAX = 3500;

// -------- CALLBACK --------
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incoming, int len)
{
  memcpy(&incomingData, incoming, sizeof(incomingData));

  bool manualMode = digitalRead(modeSwitch);

  int targetServoPos = 0;

  // -------- MODE CONTROL --------
  if (manualMode == HIGH)
  {
    int potValue = analogRead(potPin);
    targetServoPos = map(potValue, 0, 4095, 0, 180);
  }
  else
  {
    float normalized = (incomingData.lightAnalog - LIGHT_MIN) /
                       (float)(LIGHT_MAX - LIGHT_MIN);

    normalized = constrain(normalized, 0.0, 1.0);

    targetServoPos = normalized * 180;
  }

  targetServoPos = constrain(targetServoPos, 0, 180);

  // -------- EXTREME SLOW MOVEMENT --------
  unsigned long now = millis();

  if (now - lastServoUpdate >= servoDelayMs)
  {
    lastServoUpdate = now;

    if (currentServoPos < targetServoPos)
    {
      currentServoPos += 0.1;   // 🔥 very small step
    }
    else if (currentServoPos > targetServoPos)
    {
      currentServoPos -= 0.1;
    }

    myservo.write((int)currentServoPos);
  }

  // -------- LCD TOP LINE --------
  lcd.setCursor(0, 0);
  lcd.print("Ambient Temp:");

  if (manualMode == HIGH)
  {
    lcd.setCursor(15,0);
    lcd.print("M");
  }
  else
  {
    lcd.setCursor(15,0);
    lcd.print("A");
  }

  // -------- LCD BOTTOM LINE --------
  lcd.setCursor(0,1);
  lcd.print("                ");

  lcd.setCursor(0,1);

  if(showFahrenheit)
  {
    lcd.print(incomingData.tempF,1);
    lcd.print(" F");
  }
  else
  {
    lcd.print(incomingData.tempC,1);
    lcd.print(" C");
  }

  // -------- DEBUG --------
  Serial.print("Servo: ");
  Serial.println((int)currentServoPos);
}

void setup()
{
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();

  myservo.attach(servoPin, 500, 2400);

  pinMode(modeSwitch, INPUT_PULLUP);
  pinMode(potPin, INPUT);
  pinMode(tempButton, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);

  esp_now_init();

  esp_now_register_recv_cb(onDataRecv);

  myservo.write(90);
}

void loop()
{
  bool buttonState = digitalRead(tempButton);

  if(buttonState == LOW && lastButtonState == HIGH)
  {
    showFahrenheit = !showFahrenheit;
    delay(200);
  }

  lastButtonState = buttonState;

  delay(50);
}