#include "InputDebounce.h"

#define BUTTON_DEBOUNCE_DELAY 20    // [ms]

#define PRUEF_BUTTON_HOLD_TIME 5000 // [ms]
#define BLINK_INTERVAL 500          // [ms]
#define TRAIN_LENGTH 26
#define TERM_LINES 24
#define TERM_COLUMNS 80
#define CONSOLE_DRAW_INTERVAL 5000

// DISPLAY PINS
#define PIN_B0 2
#define PIN_B1 3
#define PIN_B2 4
#define PIN_B3 5

#define PIN_CS1 7
#define PIN_CS2 6

#define PIN_DS1 8
#define PIN_DS2 11 // vorher 9, eventuell wird Timer 0 schon verwendet

// BUTTON PINS

#define PIN_T_FREI A4  // vorher 10
#define PIN_T_RUECK A5 // vorher 11
#define PIN_T_PRUEF 12

// LAMP PINS

#define PIN_L_BETR 13
#define PIN_L_FREI A0
#define PIN_L_RUECK A1
#define PIN_L_PRUEF A2

#define PIN_BUZZER A3

// Tachometer PINS
#define PIN_TACHO_IST 9
#define PIN_TACHO_SOLL 10 

bool on = 1;
bool off = 0;

int currentSpeedLimit = 25;
int currentSpeed = 0;

int H0count = 0;
int resetCount = 0;
int currentTestDisplay = 0;

/* Test Displays:
 *  1 - VMAX
 *  2 - Test (8888, Buzzer)
 *  3 - count H0 H000
 *  4 - E010 - Freischaltungen an H0
 *  5 - L000 ?
 *  6 - Zuglänge 35H
 *  7 - 2221
 *  8 - blank
 */

unsigned long previousBlinkMillisFrei = millis();
unsigned long previousBlinkMillisBetr = millis();

bool lastBlinkStateBetr = false;
bool lastBlinkStateFrei = false;

// Button debouncing

bool freiLastState = HIGH;
bool pruefLastState = HIGH;
bool rueckLastState = HIGH;

unsigned long lastDebounceTimeFrei = 0;
unsigned long lastDebounceTimePruef = 0;
unsigned long lastDebounceTimeRueck = 0;

// States

bool lightFreiOn  = false;
bool lightPruefOn = false;
bool lightBetrOn  = false;
bool lightRueckOn = false;
bool buzzerOn     = false;


static InputDebounce freiButton;
static InputDebounce pruefButton;
static InputDebounce rueckButton;
unsigned long lastDrawOfConsole = 0;

bool lcdNum[16][4] =
    {
        {off, off, off, off}, // 0
        {off, off, off, on},  // 1
        {off, off, on, off},  // 2
        {off, off, on, on},   // 3
        {off, on, off, off},  // 4
        {off, on, off, on},   // 5
        {off, on, on, off},   // 6
        {off, on, on, on},    // 7
        {on, off, off, off},  // 8
        {on, off, off, on},   // 9
        {on, off, on, off},   // -      //array pos 10
        {on, off, on, on},    // E      //array pos 11
        {on, on, off, off},   // H      //array pos 12
        {on, on, off, on},    // L      //array pos 13
        {on, on, on, off},    // P      //array pos 14
        {on, on, on, on}      // Blank  //array pos 15
};


char charsInArray[] = "0123456789-EHLP ";

int getIndexOfChar(char c) {
  const char *ptr = strchr(charsInArray, c);
  if (ptr) {
    int index = ptr-charsInArray;
    return index;
  }
  return -1;
}

void displayString(char* str) {
  if (strlen(str) == 4) {
     int charIndex1 = getIndexOfChar(str[0]);
     int charIndex2 = getIndexOfChar(str[1]);
     int charIndex3 = getIndexOfChar(str[2]);
     int charIndex4 = getIndexOfChar(str[3]);
     if (charIndex1 != -1 && charIndex2 != -1 && charIndex3 != -1 && charIndex4 != -1) {
      disD1(lcdNum[charIndex1][0], lcdNum[charIndex1][1], lcdNum[charIndex1][2], lcdNum[charIndex1][3]); 
      disD2(lcdNum[charIndex2][0], lcdNum[charIndex2][1], lcdNum[charIndex2][2], lcdNum[charIndex2][3]); 
      disD3(lcdNum[charIndex3][0], lcdNum[charIndex3][1], lcdNum[charIndex3][2], lcdNum[charIndex3][3]); 
      disD4(lcdNum[charIndex4][0], lcdNum[charIndex4][1], lcdNum[charIndex4][2], lcdNum[charIndex4][3]); 
     }
  }
}

int lcdState[4] = {0, 0, 0, 0};
/* #region LAMP */

void turnOnBetr()
{
  digitalWrite(PIN_L_BETR, HIGH);
  lightBetrOn = true;
}

void turnOffBetr()
{
  digitalWrite(PIN_L_BETR, LOW);
  lightBetrOn = false;
}
void turnOnFrei()
{
  digitalWrite(PIN_L_FREI, HIGH);
  lightFreiOn = true;
}
void turnOffFrei()
{
  digitalWrite(PIN_L_FREI, LOW);
  lightFreiOn = false;
}
void turnOnPruef()
{
  digitalWrite(PIN_L_PRUEF, HIGH);
  lightPruefOn = true;
}
void turnOffPruef()
{
  digitalWrite(PIN_L_PRUEF, LOW);
  lightPruefOn = false;
}
void turnOnRueck()
{
  digitalWrite(PIN_L_RUECK, HIGH);
  lightRueckOn = true;
}
void turnOffRueck()
{
  digitalWrite(PIN_L_RUECK, LOW);
  lightRueckOn = false;
}
void turnOnBuzzer() {
  Serial.println("Turning on buzzer");
  digitalWrite(PIN_BUZZER, HIGH);
  buzzerOn = true;
}
void turnOffBuzzer() {
  digitalWrite(PIN_BUZZER, LOW);
  buzzerOn = false;
}
// #endregion

// #region Tacho

/* Mapping Speed to PWM */
// at 390 ohm
/* 12 = 5 km /h
 * 25 = 10km/h
 * 39 = 15km/h
 * 51 = 20 kh/h
 * 64 = 25km/h
 * 77 = 30 km/h
 * 90 = 35 km/h
 * 103 = 40 km/h
 * 117 = 45 km/h
 * 130 = 50 km/h
 * 143 = 55 km/h
 * 156 = 60 km/h
 * 168 = 65 km/h
 * 180 = 70 km/h
 * 194 = 75 km/h
 * 207 = 80 km/h
 * 218 = 85 km/h
 * 231 = 90 km/h
 */

int convertToPWM(int desiredSpeed) {
    if (desiredSpeed == 0) {
      return 0;
    }
    int speeds[] = {0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90};
    int pwms[] = {0, 12, 25, 39, 51, 64, 77, 90, 103, 117, 130, 143, 156, 168, 180, 194, 207, 218, 231};

    int i = 0;
    while (speeds[i] < desiredSpeed) {
        i++;
    }

    int speed1 = speeds[i - 1];
    int speed2 = speeds[i];
    int pwm1 = pwms[i - 1];
    int pwm2 = pwms[i];

    int estimatedPWM = pwm1 + (pwm2 - pwm1) * (desiredSpeed - speed1) / (speed2 - speed1);
    
    // Ensure PWM value is within the valid range of 0 to 255
    if (estimatedPWM < 0) {
        estimatedPWM = 0;
    } else if (estimatedPWM > 255) {
        estimatedPWM = 255;
    }

    return estimatedPWM;
}


void setCurrentSpeed(int kmh) {
    if (kmh > -1 && kmh < 91) {
      currentSpeed = kmh;
      analogWrite(PIN_TACHO_IST, convertToPWM(kmh));
    }
}
void setCurrentSpeedLimit(int kmh) {
      if (kmh > -1 && kmh < 91) {
        currentSpeedLimit = kmh;
        analogWrite(PIN_TACHO_SOLL, convertToPWM(kmh));
      }

}

// #endregion


void drawStateOnSerial()
{
  if ((millis() - lastDrawOfConsole) >= CONSOLE_DRAW_INTERVAL)
  {
    lastDrawOfConsole = millis();
    // Clear screen
    Serial.print("\033[2J");
    // Draw border
    
    // print display

    // assume 80 columns, 25 rows
    for (int y = 0; y < 6; y++)
    {
      Serial.print("\n");
    }
    Serial.print(lcdState[2]);
    Serial.print(lcdState[3]);
    Serial.print("\n");

    for (int i = 0; i < TERM_COLUMNS; i++)
    {
      // If we are in a display subroutine
      if (i == 38)
      {
        Serial.print(lcdState[0]);
      }
      else if (i == 39)
      {
        Serial.print(lcdState[1]);
      }
      else if (i == 40)
      {
        Serial.print(lcdState[2]);
      }
      else if (i == 41)
      {
        Serial.print(lcdState[3]);
      }
      else
      {
        Serial.print(" ");
      }
    }
    Serial.print("\n");
    // print light status
    Serial.print(" Frei: ");
    Serial.print(lightFreiOn);
    Serial.print(" Pruef: ");
    Serial.print(lightPruefOn);
    Serial.print(" Betr: ");
    Serial.print(lightBetrOn);
    Serial.print(" Rueck: ");
    Serial.print(lightRueckOn);

    // print if pressed
    Serial.print("\n");
    Serial.print("Current test mode: ");
    Serial.println(currentTestDisplay);
  }
}

//#region LCD drawing

void disD1(bool a, bool b, bool c, bool d)
{
  digitalWrite(PIN_B3, a);
  digitalWrite(PIN_B2, b);
  digitalWrite(PIN_B1, c);
  digitalWrite(PIN_B0, d);
  digitalWrite(PIN_DS1, LOW);
  digitalWrite(PIN_DS2, LOW);
  delay(2);
  digitalWrite(PIN_CS2, LOW);
  delay(2);
  digitalWrite(PIN_CS2, HIGH);
}
void disD2(bool a, bool b, bool c, bool d)
{
  digitalWrite(PIN_B3, a);
  digitalWrite(PIN_B2, b);
  digitalWrite(PIN_B1, c);
  digitalWrite(PIN_B0, d);
  digitalWrite(PIN_DS1, HIGH);
  digitalWrite(PIN_DS2, LOW);
  delay(2);
  digitalWrite(PIN_CS2, LOW);
  delay(2);
  digitalWrite(PIN_CS2, HIGH);
}
void disD3(bool a, bool b, bool c, bool d)
{
  digitalWrite(PIN_B3, a);
  digitalWrite(PIN_B2, b);
  digitalWrite(PIN_B1, c);
  digitalWrite(PIN_B0, d);
  digitalWrite(PIN_DS1, LOW);
  digitalWrite(PIN_DS2, HIGH);
  delay(2);
  digitalWrite(PIN_CS2, LOW);
  delay(2);
  digitalWrite(PIN_CS2, HIGH);
}
void disD4(bool a, bool b, bool c, bool d)
{
  digitalWrite(PIN_B3, a);
  digitalWrite(PIN_B2, b);
  digitalWrite(PIN_B1, c);
  digitalWrite(PIN_B0, d);
  digitalWrite(PIN_DS1, HIGH);
  digitalWrite(PIN_DS2, HIGH);
  delay(2);
  digitalWrite(PIN_CS2, LOW);
  delay(2);
  digitalWrite(PIN_CS2, HIGH);
}
void blankDisplay() // "  "
{
  disD1(lcdNum[15][0], lcdNum[15][1], lcdNum[15][2], lcdNum[15][3]);
  disD2(lcdNum[15][0], lcdNum[15][1], lcdNum[15][2], lcdNum[15][3]);
  disD3(lcdNum[15][0], lcdNum[15][1], lcdNum[15][2], lcdNum[15][3]);
  disD4(lcdNum[15][0], lcdNum[15][1], lcdNum[15][2], lcdNum[15][3]);
}

void displayDigitAtPosition(unsigned short digit, unsigned short position)
{
  if (digit < 10 && position < 4)
  {
    switch (position)
    {
    case 0:
      lcdState[0] = digit;
      disD1(lcdNum[digit, 0], lcdNum[digit, 1], lcdNum[digit, 2], lcdNum[digit, 3]);
    case 1:
      lcdState[1] = digit;
      disD2(lcdNum[digit, 0], lcdNum[digit, 1], lcdNum[digit, 2], lcdNum[digit, 3]);
    case 3:
      lcdState[2] = digit;

      disD3(lcdNum[digit, 0], lcdNum[digit, 1], lcdNum[digit, 2], lcdNum[digit, 3]);
    case 4:
      lcdState[3] = digit;
      disD4(lcdNum[digit, 0], lcdNum[digit, 1], lcdNum[digit, 2], lcdNum[digit, 3]);
    }
  }
}
void displayNumber(int number)
{
  // Check if number is four digits
  if (number > 0 && number < 10000)
  {
    if (number < 10)
    {
      disD1(lcdNum[15][0], lcdNum[15][1], lcdNum[15][2], lcdNum[15][3]);
      disD2(lcdNum[15][0], lcdNum[15][1], lcdNum[15][2], lcdNum[15][3]);
      disD3(lcdNum[15][0], lcdNum[15][1], lcdNum[15][2], lcdNum[15][3]);
      displayDigitAtPosition(number, 3);
    }
    else if (number < 100)
    {
      disD1(lcdNum[15][0], lcdNum[15][1], lcdNum[15][2], lcdNum[15][3]);
      disD2(lcdNum[15][0], lcdNum[15][1], lcdNum[15][2], lcdNum[15][3]);
      displayDigitAtPosition((number / 10), 2);
      displayDigitAtPosition((number % 10), 3);
    }
    else if (number < 1000)
    {
      disD1(lcdNum[15][0], lcdNum[15][1], lcdNum[15][2], lcdNum[15][3]);
      displayDigitAtPosition((number / 10), 1);
      displayDigitAtPosition((number / 10 % 10), 2);
      displayDigitAtPosition((number % 10), 3);
    }
    else
    {
      displayDigitAtPosition((number / 1000), 0);
      displayDigitAtPosition((number / 100 % 10), 1);
      displayDigitAtPosition((number / 10 % 10), 2);
      displayDigitAtPosition((number % 10), 3);
    }
  }
}


//#pragma endregion


// #pragma region Test functions

void displayH0()
{
  turnOffBuzzer();
  setCurrentSpeed(0);
  setCurrentSpeedLimit(25);
  char h0CountString[5];
  sprintf(h0CountString, "H%3d", H0count);
  displayString(h0CountString);
}

void displayLength()
{
  char trainLengthString[5];
  sprintf(trainLengthString, "%3dH", TRAIN_LENGTH);
  displayString(trainLengthString);
}
void displayFunc7()
{
  displayString("2221");
}
void displayResetCount()
{
   char resetCountString[5];
  sprintf(resetCountString, "E%3d", resetCount);
  displayString(resetCountString);
}

void displayL000()
{
  displayString("L000");
}

void testFunction()
{
  turnOnBuzzer();
  displayString("8888");
  setCurrentSpeed(80);
  setCurrentSpeedLimit(80);
}




//#pragma endregion


void blinkBetr()
{
  if (millis() - previousBlinkMillisBetr >= BLINK_INTERVAL)
  {
    previousBlinkMillisBetr = millis();
    if (lastBlinkStateBetr == true)
    {
      turnOffBetr();
    }
    else
    {
      turnOnBetr();
    }
    lastBlinkStateBetr = !lastBlinkStateBetr;
  }
}
void blinkFrei()
{
  if (millis() - previousBlinkMillisFrei >= BLINK_INTERVAL)
  {
    previousBlinkMillisFrei = millis();
    if (lastBlinkStateFrei == true)
    {
      turnOffFrei();
    }
    else
    {
      turnOnFrei();
    }
    lastBlinkStateFrei = !lastBlinkStateFrei;
  }
}

//#region Button callbacks
void pruefButton_pressedCallback()
{

    Serial.println("Pruef pressed");
    // Pruef pressed
    if (currentTestDisplay == 0)
    {
      currentTestDisplay = 1;
    }
    else
    {
      
      currentTestDisplay = 0;
      setCurrentSpeed(0);
      setCurrentSpeedLimit(25);
    }

}
void freiButton_pressedCallback()
{

}

void rueckButton_pressedCallback()
{
  Serial.println("Rueck pressed");
  if (currentTestDisplay != 0) {
    if (currentTestDisplay < 8)
    {
      currentTestDisplay++;
    }
  else
    {
      currentTestDisplay = 1;
    }
  }
}

void rueckButton_releasedCallback() {
  
}
void pruefButton_releasedCallback() {
  
}
void freiButton_releasedCallback() {
  
}
//#endregion

void initDisplay() {
  pinMode(PIN_B0, OUTPUT);
  pinMode(PIN_B1, OUTPUT);
  pinMode(PIN_B2, OUTPUT);
  pinMode(PIN_B3, OUTPUT);
  pinMode(PIN_DS1, OUTPUT);
  pinMode(PIN_DS2, OUTPUT);
  pinMode(PIN_CS1, OUTPUT);
  pinMode(PIN_CS2, OUTPUT);
  digitalWrite(PIN_B0, LOW);
  digitalWrite(PIN_B1, LOW);
  digitalWrite(PIN_B2, LOW);
  digitalWrite(PIN_B3, LOW);
  digitalWrite(PIN_DS1, HIGH);
  digitalWrite(PIN_DS2, HIGH);
  digitalWrite(PIN_CS1, LOW);
  digitalWrite(PIN_CS2, HIGH);
}
void setupPins() {
  // Leuchten
  pinMode(PIN_L_BETR, OUTPUT);
  pinMode(PIN_L_FREI, OUTPUT);
  pinMode(PIN_L_PRUEF, OUTPUT);
  pinMode(PIN_L_RUECK, OUTPUT);

  // Tacho
  pinMode(PIN_TACHO_IST, OUTPUT);
  pinMode(PIN_TACHO_SOLL, OUTPUT);
}

void bootSequence() {
  displayString("8007");
  setCurrentSpeed(70);
  setCurrentSpeedLimit(70);
  turnOnBuzzer();
  turnOnFrei();
  delay(100);
  turnOffBuzzer();
  delay(300);
  turnOffFrei();
  turnOnPruef();
  delay(500);
  turnOffPruef();
  turnOnBetr();
  delay(500);
  turnOffBetr();
  turnOnRueck();
  delay(500);
  turnOffRueck();
  setCurrentSpeed(0);
  setCurrentSpeedLimit(25);
  delay(2000);
}

void setup()
{
  setupPins();
  initDisplay();
  turnOffBuzzer();
  turnOffPruef();
  turnOffFrei();
  turnOffRueck();
  turnOffBetr();
  bootSequence();
  // Betr light on in normal mode
  turnOnBetr();
  Serial.begin(9600);
  freiButton.registerCallbacks(freiButton_pressedCallback, freiButton_releasedCallback);
  pruefButton.registerCallbacks(pruefButton_pressedCallback, pruefButton_releasedCallback);
  rueckButton.registerCallbacks(rueckButton_pressedCallback,rueckButton_releasedCallback);

  // setup input buttons (debounced)
  freiButton.setup(PIN_T_FREI, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES);
  pruefButton.setup(PIN_T_PRUEF, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES, 300); // single-shot pressed-on time duration callback
  rueckButton.setup(PIN_T_RUECK, BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES, 300); // single-shot pressed-on time duration callback

  
}


void normalMode() {
    turnOffBuzzer();
    turnOffPruef();
    turnOffFrei();
    turnOffRueck();
    turnOnBetr();
    displayVmax();
}

void displayVmax()
{
  char vmaxString[5];
  sprintf(vmaxString,"%4d",currentSpeedLimit);
  displayString(vmaxString);
}

// Main loop

void loop()
{

  unsigned long now = millis();
  freiButton.process(now);
  pruefButton.process(now);
  rueckButton.process(now);

  // Check if we need to enter or exit test mode
 
  if (currentTestDisplay > 0)
  {
    // constant pruef,rueck, blink frei, betr
    turnOnPruef();
    turnOnRueck();
    blinkFrei();
    blinkBetr();
    Serial.print("Prueffunktion ");
    Serial.println(currentTestDisplay);
    
    // Switch between display modes

    switch (currentTestDisplay)
    {
    case 1:
      displayVmax();
      break;
    case 2:
      testFunction();
      break;
    case 3:
      displayH0();
      break;
    case 4:
      displayResetCount();
      break;
    case 5:
      displayL000();
      break;
    case 6:
      displayLength();
      break;
    case 7:
      displayFunc7();
      break;
    case 8:
      blankDisplay();
      break;
    }
  }
  else
  {
      normalMode();
  }
  if (Serial.available() > 0) {
    String input = Serial.readString();
    input.trim();
    if (input[0] == 'I') {
      String stringValue = input.substring(1);
      int intValue = stringValue.toInt();
      analogWrite(PIN_TACHO_IST, intValue);
    }
    if (input[0] == 'S') {
      String stringValue = input.substring(1);
      int intValue = stringValue.toInt();
      analogWrite(PIN_TACHO_SOLL, intValue);
    }
   
    
  }
   Serial.print("Outputting PWM for TACHO SOLL at");
    Serial.print(PIN_TACHO_SOLL);
    Serial.print(" with value: ");
    Serial.println(convertToPWM(currentSpeedLimit));
    Serial.print("Outputting PWM for TACHO IST at ");
    Serial.print(PIN_TACHO_IST);
    Serial.print(" with value: ");
    Serial.println(convertToPWM(currentSpeed));
    Serial.print("currentSpeed is");
    Serial.println(currentSpeed);
    delay(5000);
  
}
