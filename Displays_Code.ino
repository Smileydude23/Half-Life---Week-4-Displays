#include <Wire.h>
#include <DHT.h>
#include <RTClib.h>
#include <TM1637Display.h>
#include <Adafruit_NeoPixel.h>

#define PIN_DISP_CLK  1    
#define PIN_DISP_DIO  2    
#define PIN_DHTDATA   3    
#define PIN_SDA       4    
#define PIN_SCL       5    
#define PIN_NEODATA   6    

#define PIN_ENC_A     7   
#define PIN_ENC_B     8    
#define PIN_S1        9    // (Encoder Button)
#define PIN_LED_1     10   // (AM LED)
#define PIN_LED_2     11   // (PM LED)

#define DHTTYPE DHT11
#define NUM_PIXELS 20      // Set this to actual NeoPixel strip count when I get the strip

DHT dht(PIN_DHTDATA, DHTTYPE);
RTC_DS3231 rtc;
TM1637Display display(PIN_DISP_CLK, PIN_DISP_DIO);
Adafruit_NeoPixel strip(NUM_PIXELS, PIN_NEODATA, NEO_GRB + NEO_KHZ800);

enum DisplayMode { TIME_MODE, TEMP_MODE, HUMID_MODE };
DisplayMode currentMode = TIME_MODE;

unsigned long modeSwitchTime = 0;
const unsigned long RETURN_TO_TIME_TIMEOUT = 15000;

int lastEncAState;
int brightnessLevel = 4; 

int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

unsigned long lastDisplayUpdate = 0;
unsigned long lastRainbowUpdate = 0;
uint16_t rainbowHue = 0;
bool colonBlink = true;

void setup() {
  Serial.begin(115200);

  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_S1, INPUT_PULLUP);
  
  pinMode(PIN_LED_1, OUTPUT);
  pinMode(PIN_LED_2, OUTPUT);

  Wire.begin(PIN_SDA, PIN_SCL);
  dht.begin();
  display.setBrightness(brightnessLevel);
  strip.begin();
  strip.show();

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC module!");
  }
  
  if (rtc.lostPower()) {
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

  lastEncAState = digitalRead(PIN_ENC_A);
}

void loop() {
  handleEncoder();
  handleButton();
  updateDisplayAndLEDs();
  updateRainbowBorder();
}


void handleEncoder() {
  int currentEncAState = digitalRead(PIN_ENC_A);
  
  if (currentEncAState != lastEncAState && currentEncAState == LOW) {
    if (digitalRead(PIN_ENC_B) != currentEncAState) {
      if (brightnessLevel < 7) brightnessLevel++;
    } else {
      if (brightnessLevel > 1) brightnessLevel--;
    }
    
    display.setBrightness(brightnessLevel);
    Serial.print("Brightness Level Updated: ");
    Serial.println(brightnessLevel);
  }
  lastEncAState = currentEncAState;
}

void handleButton() {
  int reading = digitalRead(PIN_S1);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    static int stableButtonState = HIGH;
    if (reading != stableButtonState) {
      stableButtonState = reading;

      if (stableButtonState == LOW) {
        modeSwitchTime = millis(); 
        
        if (currentMode == TIME_MODE) {
          currentMode = TEMP_MODE;
        } else if (currentMode == TEMP_MODE) {
          currentMode = HUMID_MODE;
        } else {
          currentMode = TIME_MODE;
        }
      }
    }
  }
  lastButtonState = reading;

  if (currentMode != TIME_MODE && (millis() - modeSwitchTime >= RETURN_TO_TIME_TIMEOUT)) {
    currentMode = TIME_MODE;
  }
}

void updateDisplayAndLEDs() {
  if (millis() - lastDisplayUpdate < 250) return; 
  lastDisplayUpdate = millis();

  DateTime now = rtc.now();

  int hour12 = now.hour();
  bool isPM = false;
  
  if (hour12 >= 12) {
    isPM = true;
    if (hour12 > 12) hour12 -= 12;
  }
  if (hour12 == 0) hour12 = 12;

  if (currentMode == TIME_MODE) {
    digitalWrite(PIN_LED_1, !isPM ? HIGH : LOW);
    digitalWrite(PIN_LED_2, isPM ? HIGH : LOW);

    colonBlink = !colonBlink; 
    int displayValue = (hour12 * 100) + now.minute();
    display.showNumberDecEx(displayValue, colonBlink ? 0b01000000 : 0, true);

  } else if (currentMode == TEMP_MODE) {
    digitalWrite(PIN_LED_1, LOW);
    digitalWrite(PIN_LED_2, LOW);

    float tempC = dht.readTemperature();
    if (isnan(tempC)) tempC = 0;
    int temp = (int)tempC;


    uint8_t data[] = {
      display.encodeDigit(tempC / 10),
      display.encodeDigit(tempC % 10),
      0x00,       // Blank spacer panel
      0b00111001  // Letter 'C'
    };
    display.setSegments(data);

  } else if (currentMode == HUMID_MODE) {
    digitalWrite(PIN_LED_1, LOW);
    digitalWrite(PIN_LED_2, LOW);

    float humidity = dht.readHumidity();
    if (isnan(humidity)) humidity = 0;
    int hum = (int)humidity;


    uint8_t data[] = {
      display.encodeDigit(humidity / 10),
      display.encodeDigit(humidity % 10),
      0x00,       // Blank spacer panel
      0b01110110  // Letter 'H'
    };
    display.setSegments(data);
  }
}

void updateRainbowBorder() {
  if (millis() - lastRainbowUpdate < 20) return;
  lastRainbowUpdate = millis();

  strip.setBrightness(brightnessLevel * 25);

  for (int i = 0; i < strip.numPixels(); i++) {
    int pixelHue = rainbowHue + (i * 65536L / strip.numPixels());
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
  }
  strip.show();
  
  rainbowHue += 256;
}
