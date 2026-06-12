#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <RF24.h>

Adafruit_BMP280 bmp;
#define CE_PIN 4
#define CSN_PIN 5
RF24 radio(CE_PIN, CSN_PIN);
#define SEALEVELPRESSURE_HPA (1013.25)

const int MPU_ADDR = 0x68;
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Booting Direct-Read Telemetry...");

  Wire.begin(21, 22); 

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);  
  Wire.write(0);     
  byte mpuError = Wire.endTransmission(true);
  
  if (mpuError == 0) {
    Serial.println("MPU6050: AWAKE and LISTENING!");
  } else {
    Serial.println("MPU6050: Not responding to wake command.");
  }

  if (!bmp.begin(0x76)) { 
    Serial.println("Failed to find BMP280 chip at 0x76!");
    while (1) { delay(10); }
  }
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     
                  Adafruit_BMP280::SAMPLING_X2,     
                  Adafruit_BMP280::SAMPLING_X16,    
                  Adafruit_BMP280::FILTER_X16,      
                  Adafruit_BMP280::STANDBY_MS_500); 
  Serial.println("BMP280: Initialized Successfully!");

  if (radio.begin()) {
    radio.startListening();
    Serial.println("NRF24L01: Initialized Successfully!");
  } else {
    Serial.println("NRF24L01: Initialization Failed!");
  }

  Serial.println("\n--- Starting Data Stream in 3 seconds ---");
  delay(3000);
}

void loop() {
  Serial.println("\n==================================");
  
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);  
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);

  AcX = Wire.read()<<8 | Wire.read();    
  AcY = Wire.read()<<8 | Wire.read();  
  AcZ = Wire.read()<<8 | Wire.read();  
  Tmp = Wire.read()<<8 | Wire.read();  
  GyX = Wire.read()<<8 | Wire.read();  
  GyY = Wire.read()<<8 | Wire.read();  
  GyZ = Wire.read()<<8 | Wire.read();  
  
  Serial.println("--- MPU6050 (RAW TILT DATA) ---");
  Serial.print("Accel X: "); Serial.print(AcX);
  Serial.print(" | Y: "); Serial.print(AcY);
  Serial.print(" | Z: "); Serial.println(AcZ);

  Serial.println("--- BMP280 (ALTIMETER) ---");
  Serial.print("Temperature: "); Serial.print(bmp.readTemperature()); Serial.println(" *C");
  Serial.print("Altitude:    "); Serial.print(bmp.readAltitude(SEALEVELPRESSURE_HPA)); Serial.println(" m");

  Serial.println("--- NRF24L01 (RADIO) ---");
  if (radio.isChipConnected()) {
    Serial.println("Status: ONLINE & LISTENING");
  } else {
    Serial.println("Status: CONNECTION LOST");
  }

  Serial.println("==================================");
  
  delay(500);
}
