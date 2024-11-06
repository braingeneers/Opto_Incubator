// InCuber Project 
// Noah Williams, Oct 9 2024
// Sharf Lab, UCSC

/* 

  MicroController Pinout:
    pin5: button
    pin7: fan
    pin8: pnumatic valve
    pin9: heater
    grnd: common with ps
    qwiic conn sensor

--------------------
  
  This project uses the Sparkfun STC31 board (SPX-18385) which
    includes a temperature, humidity, and gas sensor. Temperature,
    Humidity, and Pressure (HTP) need to be set prior to reqesting
    a CO2 reading. Note that delay for the multi processes should 
    not cross below 1 second minimum for accurate readings 
    (according to manufacturer).

--------------------

  Multi Process Algorithm
    The arduino can only run one main loop. In order to not 
    overlap processes that require time (i.e. two processes 
    that need to execute simultaniously but at different times)
    the program uses millis(). This is shown below:

        unsigned long prevTime = millis();
        void loop() {
          unsigned long currTime = millis();
          if (currTime - prevTime > 1000) {
            // do something
            prevTime = currTime;
          }
        }

--------------------

Note on PinMode() instead of analogWrite()
  Uses internal pin mode set to input or output instead of 
    analogWrite(). This allows us to get 5v analog to the 
    gate using a pullup resistor. 

--------------------
*/


#include <Wire.h>   // I2C 
#include "SparkFun_STC3x_Arduino_Library.h" // For STC31 on breakout board (CO2)
#include "SparkFun_SHTC3.h" // For the SHTC3 sensor on breakout board (temp)

#define BUTTON_PIN 6
#define PNUMATIC_VALVE_PIN 8
#define HEATER_PIN 9

STC3x STC3x;
SHTC3 SHTC3;

// Set initial time to startup
unsigned long prevTime = millis();

void setup() {
  
  // Start I2C and Serial comm
  Wire.begin();
  Serial.begin(115200);

  // Sensor Setup (STC31)
  STC3x.begin();
  STC3x.setBinaryGas(STC3X_BINARY_GAS_CO2_AIR_25);

  // Sensor Setup (SHTC3)
  SHTC3.begin();

  // Button
  pinMode(BUTTON_PIN, INPUT_PULLUP);   

  // Check everything is working
  if (SHTC3.update() != SHTC3_Status_Nominal) // Request a measurement
  {
    while (1) {
      Serial.println("ERROR: STC3X -- Press RESET after connection is fixed");
      delay(2000);
    }
  }

  // Set initial Vout to 0
  pinMode(9, OUTPUT);
  pinMode(8, OUTPUT);
}

void loop() {

  unsigned long currTime = millis();

  // Manual CO2 Calibration (stored after power off)
  int button = digitalRead(6);
  checkCalibrationButton(button);

  // Longer times give more accurate readings
  //  because the sensor heats up less. neat.
  if (currTime - prevTime > 2000) {
    prevTime = currTime;
    
    // Returns true if new data is available
    if (STC3x.measureGasConcentration())
    {
    
      //// Sensors ////
      // Temp
      SHTC3.update();
      float tempReading = SHTC3.toDegC();
      STC3x.setTemperature(tempReading);

      // Humidity
      float humidityReading = SHTC3.toPercent();
      STC3x.setRelativeHumidity(humidityReading);

      // CO2
      float co2Reading = STC3x.getCO2();

      // PID
      float pidTemp = pid_tune(35, tempReading, 500, 10, 0);
      float pidCO2 = pid_tune(5, co2Reading, 30, 0, 0);

      // Clamp PID values
      float clampedTemp = clampAndNormalize(pidTemp, 0, 100);
      float clampedCO2 = clampAndNormalize(pidCO2, 0, 100);

      // Scale (seconds)
      float scaledTemp = clampedTemp * 3000; 
      float scaledCO2 = clampedCO2 * 500;   // .5 second maximum

      // Write to pins
      tempPinMode(scaledTemp);
      co2PinMode(scaledCO2);

      serLogV2_formatted(tempReading, co2Reading);
    
      //////// Debugging / Printing //////////////

      // Serial output for debugging
      // serialPrintFourValues(co2Reading, pidCO2, clampedCO2, scaledCO2);
      // serialPrintFourValues(tempReading, pidTemp, clampedTemp, scaledTemp);
    }
  }
}


// PID Function 
//   If a term is not desired, set the constant to zero. 
unsigned long currTime, deltaTime;
float pidError, pidP, pidI, pidD, pidOutput;
float prevError = 0;
float deltaError;
float integral = 0;
int pid_tune(float target, float value, float kP, float kI, float kD) {

  // Error
  pidError = target - value;
  // Proportion
  (kP <= 0) ? pidP = 0 : pidP = kP * pidError;
  // Integral 
  (kI <= 0) ? pidI = 0 : pidI = pidI  + (pidError * kI);
  // Derivative 
  if (kD <= 0) {
    pidD = 0;
  } else {
    currTime = prevTime;
    prevTime = millis();
    deltaTime = (prevTime - currTime) / 1000;
    deltaError = pidError - prevError;
    pidD = kD * (deltaError / deltaTime);
  }
  // Sum
  pidOutput = pidP + pidI + pidD;

  return pidOutput;
}


/* Function to check if the button on internal pullup has
    been pressed. Force calibrates the STC3x sensor to .06. */
void checkCalibrationButton(int buttonValue) {
  if (buttonValue == LOW) {
    STC3x.forcedRecalibration(0.06);
    Serial.println("RESET");
    delay(1000);
  }
}


// Temporary pin mode to input to use 5v Pullup 
void tempPinMode(float seconds) {
  pinMode(9, INPUT);  // ~5V Pullup
  digitalWrite(9, 255);
  delay(seconds);
  pinMode(9, OUTPUT);  // ~0V
  digitalWrite(9, 0);
}

// Temporary pin mode to input to use 5v Pullup 
void co2PinMode(float seconds) {
  pinMode(8, INPUT);  // ~5V Pullup
  digitalWrite(8, 255);
  delay(seconds);
  pinMode(8, OUTPUT);  // ~0V
  digitalWrite(8,0);
}

// Clamp between values
float clampAndNormalize (float val, float min, float max) {
  
  // Clamp if outside range
  if (val < min) val = min;
  else if (val > max) val = max;

  // Normalize 
  return ((val - min) / (max - min));
}


// Print temp and co2 for SerLogV2 program
void serLogV2_formatted(float temp, float co2) {
  Serial.print(temp);
  Serial.print(",");
  Serial.println(co2);
}

//// Printing Functions ////
// Print two values to serial port, formatted for arunio plotter
void serialPrintTempCo2(float temp, float co2) {
  Serial.print("Temp:");
  Serial.print(temp);
  Serial.print(",");
  Serial.print("CO2:");
  Serial.print(co2);
  Serial.println(",");
}

// Prints four values to Serial port 
// Implicit conversion to float
void serialPrintFourValues(float a, float b, float c, float d) {
    Serial.print(a);
    Serial.print(", ");
    Serial.print(b);
    Serial.print(", ");
    Serial.print(c);
    Serial.print(", ");
    Serial.print(d);
    Serial.println("\n");
}


