#include <SoftwareSerial.h>
#include <MQ2.h>

#define MQ_PIN (0)     
#define RL_VALUE  (5)     
#define RO_CLEAN_AIR_FACTOR (3)  
#define CALIBRATION_SAMPLE_TIMES (50)   
#define CALIBRATION_SAMPLE_INTERVAL (500)   
#define READ_SAMPLE_INTERVAL (50)    
#define READ_SAMPLE_TIMES (5)     
#define GAS_LPG (0)
#define GAS_CO (1)
#define GAS_SMOKE (2)

// Function prototypes
float MQCalibration(int mq_pin);
float MQResistanceCalculation(int raw_adc);
float MQRead(int mq_pin);
int MQGetGasPercentage(float rs_ro_ratio, int gas_id);
int MQGetPercentage(float rs_ro_ratio, float *pcurve);
void SendSMS(const char* phoneNumbers[], int numNumbers);
void updateSerial();

#define SIM800_RX_PIN 2
#define SIM800_TX_PIN 3

SoftwareSerial sim800l(SIM800_TX_PIN, SIM800_RX_PIN); // RX,TX for Arduino and for the module it's TXD RXD, they should be inverted


float lpg, co, smoke;



float LPGCurve[3] = {2.3, 0.21, -0.47};   
float COCurve[3] = {2.3, 0.72, -0.34};    
float SmokeCurve[3] = {2.3, 0.53, -0.44};    
float Ro = 10; 

const char* phoneNumbers[] = {
   "+123456789012" //change the number
   "+219876543221"
};

const int buzzer = 9; //buzzer to arduino pin 9



void setup() {
  sim800l.begin(9600);   //Module baud rate, this is on max, it depends on the version
  Serial.begin(9600);    //UART  setup, baudrate = 9600bps
  Serial.println("Calibrating...");
  Ro = MQCalibration(MQ_PIN); // Calibrating the sensor. Please make sure the sensor is in clean air
  Serial.print("Calibration is done. Ro = ");
  Serial.print(Ro);
  Serial.println(" kohm");
  pinMode(buzzer, OUTPUT); // Set buzzer - pin 9 as an output

}

void loop() {
  float lpgConcentration = MQGetGasPercentage(MQRead(MQ_PIN)/Ro, GAS_LPG);
  float coConcentration = MQGetGasPercentage(MQRead(MQ_PIN)/Ro, GAS_CO);
  float smokeConcentration = MQGetGasPercentage(MQRead(MQ_PIN)/Ro, GAS_SMOKE);
  
  Serial.print("LPG:"); 
  Serial.print(lpgConcentration);
  Serial.print("ppm");
  Serial.print("    ");   
  Serial.print("CO:");  
  Serial.print(coConcentration);
  Serial.print("ppm");
  Serial.print("    ");   
  Serial.print("SMOKE:"); 
  Serial.print(smokeConcentration);
  Serial.println("ppm");

  
  if (sim800l.available()) { // Check if there's communication from the module
    String message = sim800l.readString();
    Serial.println(message); // Display the received message on the serial monitor

    if (message.indexOf("STATUS") != -1) {
    SendSMS(phoneNumbers, sizeof(phoneNumbers) / sizeof(phoneNumbers[0]));

    }
  }
    if (lpgConcentration > 10 || coConcentration > 50 || smokeConcentration > 50) {
    // If any gas concentration exceeds 200 ppm, send an SMS
    SendSMS(phoneNumbers, sizeof(phoneNumbers) / sizeof(phoneNumbers[0])); // Send SMS to multiple numbers
    digitalWrite(buzzer, HIGH);
  }
  
  updateSerial();
  
  delay(1000); // Delay for stability
}
  



float MQResistanceCalculation(int raw_adc) {
  return (float)RL_VALUE * (1023 - raw_adc) / raw_adc;
}

float MQCalibration(int mq_pin) {
  float val = 0;
  for (int i = 0; i < CALIBRATION_SAMPLE_TIMES; i++) {
    val += MQResistanceCalculation(analogRead(mq_pin));
    delay(CALIBRATION_SAMPLE_INTERVAL);
  }
  val /= CALIBRATION_SAMPLE_TIMES;
  val /= RO_CLEAN_AIR_FACTOR;
  return val;
}

float MQRead(int mq_pin) {
  float rs = 0;
  for (int i = 0; i < READ_SAMPLE_TIMES; i++) {
    rs += MQResistanceCalculation(analogRead(mq_pin));
    delay(READ_SAMPLE_INTERVAL);
  }
  return rs / READ_SAMPLE_TIMES;
}

int MQGetGasPercentage(float rs_ro_ratio, int gas_id) {
  if (gas_id == GAS_LPG) {
    return MQGetPercentage(rs_ro_ratio, LPGCurve);
  } else if (gas_id == GAS_CO) {
    return MQGetPercentage(rs_ro_ratio, COCurve);
  } else if (gas_id == GAS_SMOKE) {
    return MQGetPercentage(rs_ro_ratio, SmokeCurve);
  }    
  return 0;
}

int MQGetPercentage(float rs_ro_ratio, float *pcurve) {
  return pow(10, ((log(rs_ro_ratio) - pcurve[1]) / pcurve[2]) + pcurve[0]);
}
 
void SendSMS(const char* phoneNumbers[], int numNumbers) {
  Serial.println("Sending SMS...");
  for (int i = 0; i < numNumbers; i++) {
    sim800l.print("AT+CMGF=1\r"); // Set the module to SMS mode
    delay(100);
    sim800l.print("AT+CMGS=\"");
    sim800l.print(phoneNumbers[i]); // Send SMS to the i-th number
    sim800l.print("\"\r");
    delay(500);
    sim800l.print("WARNING POTENTIAL SMOKE / FIRE !!!!!!");
    sim800l.print(" ppm\n");
    sim800l.print("LPG: ");
    sim800l.print(MQGetGasPercentage(MQRead(MQ_PIN) / Ro, GAS_LPG));
    sim800l.print(" ppm\n");
    sim800l.print("CO: ");
    sim800l.print(MQGetGasPercentage(MQRead(MQ_PIN) / Ro, GAS_CO));
    sim800l.print(" ppm\n");
    sim800l.print("SMOKE: ");
    sim800l.print(MQGetGasPercentage(MQRead(MQ_PIN) / Ro, GAS_SMOKE));
    sim800l.print(" ppm\n");
    delay(500);
    sim800l.print((char)26); // End the message (required according to the datasheet)
    delay(500);
    sim800l.println();
    Serial.println("Text Sent.");
    delay(500);
  }
}
void updateSerial() {
  delay(500);
  while (Serial.available()) {
    sim800l.write(Serial.read()); // Forward what Serial received to Software Serial Port
  }

  while (sim800l.available()) {
    Serial.write(sim800l.read()); // Forward what Software Serial received to Serial Port
  }
}
