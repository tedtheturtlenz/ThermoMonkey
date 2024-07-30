#include "LowPower.h"
#define resistor 99500 //resistance of the specific resistor used
#define THERMISTORNOMINAL 100000 //Nominal resistance at 25 C for the thermistor, from datasheet
#define TEMPERATURENOMINAL 25 //Temperature of that nominal resistance

// The beta coefficient of the thermistor (usually 3000-4000)
#define BCOEFFICIENT 3950
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans12pt7b.h> //A nice font, but it takes up a lot of room on the device!
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
int sleepCounter = 0;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  
  //Make sure USB is attached on start
  USBDevice.attach();
  Serial.begin(9600);
  delay(7000);
  

  //Checking if IIC connection is valid
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  //Initializing the Display
  display.setFont(&FreeSans12pt7b);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.ssd1306_command(SSD1306_SETCONTRAST);                   // 0x81
  display.ssd1306_command(0);
  display.ssd1306_command(0xD9);
  display.ssd1306_command(34);
  
}

void loop() {

  //Sleep Mode
  //This is a low-power mode for the Pro Micro. The maximum time the device can sleep for is 8s, so the sleepCounter iterates every 8s
  //when the device wakes, then it checks if it has been asleep for long enough, and if not goes back to sleep.
  if (sleepCounter < 40) {  //Change back to 8 for a 3 minute delay
    sleepCounter = sleepCounter + 1;
    //Disabling a bunch of USB things to reduce power consumption
    // Disable USB clock
    USBCON |= _BV(FRZCLK);
    // Disable USB PLL
    PLLCSR &= ~_BV(PLLE);
    // Disable USB
    USBCON &= ~_BV(USBE);
    //Actual Low Power Mode
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);

    //Else wake and measure/update display
  } else {
    //reset sleep counter and perform temperature measurement
    sleepCounter = 0;
    // In order for PC to have enough time to show the connected COM port
    USBDevice.attach();

  

    
//Getting the VCC voltage to check that the Arduino is in proper operating voltage.
// Read 1.1V reference against AVcc
// set the reference to Vcc and the measurement to the internal 1.1V reference
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
#else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#endif

    delay(2);             // Wait for Vref to settle
    ADCSRA |= _BV(ADSC);  // Start conversion
    while (bit_is_set(ADCSRA, ADSC))
      ;  // measuring

    uint8_t low = ADCL;   // must read ADCL first - it then locks ADCH
    uint8_t high = ADCH;  // unlocks both

    long result = (high << 8) | low;

    result = 1054312L / result;  // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
    float VccVal = result;       // Vcc in volts



    //If Vcc is over 4 Volts, then do reading and display
    if (VccVal > 4000) {
      //Take 3 voltage readings 0.5s apart, then average them
      float sensorValue1 = analogRead(A0);
      delay(500);
      float sensorValue2 = analogRead(A0);
      delay(500);
      float sensorValue3 = analogRead(A0);
      //averaging readings
      float sensorValue = (sensorValue1 + sensorValue2 +sensorValue3) / 3;
      //Calculating the resistance of the thermister based on this voltage reading
      float thermistorRes = resistor / ((1023 / sensorValue) - 1);
      //thermistorRes = 136000; for testing, is equal to 18 degrees

      //Performing Steinhart-Hart calculation
      float steinhart;
      steinhart = log(thermistorRes / THERMISTORNOMINAL);  // (R/Ro)
      steinhart /= BCOEFFICIENT;                           // 1/B * ln(R/Ro)
      steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15);    // + (1/To)                       // Invert
      steinhart = 1.0 / steinhart;
      float val = (steinhart - 273.15) * 0.95;  // convert absolute temp to C, adjust for calibration
      //now convert that temperature to a nicely formatted string
      char buff[10];
      int i;
      String valueString = "";
      for (i = 0; i < 10; i++) {
        dtostrf(val, 3, 1, buff);  //4 is mininum width, 6 is precision
        valueString = buff;
      }
      //Display the temperature
      display.clearDisplay();

      display.setCursor(2, 48);
      // Printing the temperature value
      display.println(valueString);
      //Now printing the "C" for degrees Calcius
      display.setCursor(96, 48);
      display.println("C");
      //Drawing the "degree" symbol nicely
      display.drawCircle(97, 12, 4, WHITE);
      display.display();

      //Serial.println(val); //For testing



      //Else if Vcc is lower than 4V, then display a low power warning
    } else {
      display.clearDisplay();
      display.setFont(&FreeSans12pt7b);
      display.setTextSize(2);
      display.setTextColor(WHITE);
      display.setCursor(2, 48);
      
      // Display Vcc voltage
      display.println(VccVal);
      display.display();
      //Now wait 0.5s and then display Low power warning "LOW"
      delay(500);
      display.clearDisplay();
      display.setFont(&FreeSans12pt7b);
      display.setTextSize(2);
      display.setTextColor(WHITE);
      display.setCursor(2, 48);
    
      display.println("LOW");
      display.display();
      Serial.println("LOW");
    }


    
  }
}
