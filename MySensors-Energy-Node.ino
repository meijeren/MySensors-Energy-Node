#include <SPI.h>
#include <MyHwATMega328.h>
#include <MySensor.h>  

// Node definitions
#define NODE_ID       0xEE
#define NODE_TEXT     "ElectricityGasWater"
#define NODE_VERSION  "0.2"
// Sensor definitions
#define SENSOR_ID       1
#define SENSOR_ID_GAS   2
#define SENSOR_ID_WATER 3

#define SEND_FREQUENCY 20000

// NRFRF24L01 radio driver (set low transmit power by default)
MyTransportNRF24 radio(RF24_CE_PIN, RF24_CS_PIN, RF24_PA_LEVEL_GW);
// Select AtMega328 hardware profile
MyHwATMega328 hw;
// Construct MySensors library
MySensor  gw(radio, hw);
MyMessage wattMsg(SENSOR_ID, V_WATT);
MyMessage kwhMsg(SENSOR_ID, V_KWH);
MyMessage pcMsg(SENSOR_ID, V_VAR1);

MyMessage gasFlowMsg(SENSOR_ID_GAS, V_FLOW);
MyMessage gasVolumeMsg(SENSOR_ID_GAS, V_VOLUME);
MyMessage gasLastCounterMsg(SENSOR_ID_GAS, V_VAR1);

MyMessage waterFlowMsg(SENSOR_ID_WATER, V_FLOW);
MyMessage waterVolumeMsg(SENSOR_ID_WATER, V_VOLUME);
MyMessage waterLastCounterMsg(SENSOR_ID_WATER, V_VAR1);

unsigned long lastSend;

void setup() {
  gw.begin(NULL, NODE_ID, false);
  Serial.println(F(NODE_TEXT  " "  NODE_VERSION));

  // Send the sketch version information to the gateway and Controller
  gw.sendSketchInfo(NODE_TEXT, NODE_VERSION);

  // Register this device as power sensor
  gw.present(SENSOR_ID, S_POWER);
  // Register this device as Waterflow sensor
  gw.present(SENSOR_ID_GAS, S_WATER); 
  // Register this device as Waterflow sensor
  gw.present(SENSOR_ID_WATER, S_WATER); 

  lastSend=millis();
}

bool prevPulse = true;
#define DREMPEL 570
unsigned long pulseCount = 0;
float totalkWh = 0;
unsigned long curWatts = 0;
bool sendNeeded = false;
unsigned long prevMillis = 0;
#define C 375
int min = 1023;
int max = 0;

void loop() 
{
  gw.process();
  unsigned long now = millis();
  // read the input on analog pin 0:
  unsigned short sum[3];
  for (byte i = 0; i < 3; i++)
  {
    sum[i] = 0;
  }
  for (byte i = 0; i < 40; i++) 
  {
    for (byte j = 0; j < 3; j++)
    {
      sum[j] += analogRead(A0 + j);
    }
  }
  int sensorValue = sum[0] / 40;
  if (sensorValue < min) min = sensorValue;
  if (sensorValue > max) max = sensorValue;
  if (pulseCount < 1)
  {
    Serial.print(sensorValue);
    Serial.print(" [");
    Serial.print(min);
    Serial.print("-");
    Serial.print(max);
    Serial.println("]");
  }
  bool pulse = (sensorValue < DREMPEL);

  if ((prevPulse != pulse) && pulse) 
  {
    pulseCount++;
    
    unsigned long pulseTime = (now - prevMillis);
    prevMillis = now;
    totalkWh = float(pulseCount) / C;
    curWatts = (3600000000.0 / (C * pulseTime));
    Serial.print("Pulse ");
    Serial.print(pulseCount);
    Serial.print(" T=");
    Serial.print(pulseTime);
    Serial.print(" Curr=");
    Serial.print(curWatts);
    
    Serial.print(" Total=");
    Serial.println(totalkWh);
    
    sendNeeded = true;
  }
  prevPulse = pulse;
  if (sendNeeded)
  {
    bool send = (now - lastSend) > SEND_FREQUENCY;
    if (send)
    {
      gw.send(pcMsg.set(pulseCount));  // Send pulse count value to gw 
      gw.send(wattMsg.set(curWatts));  // Send watt value to gw 
      gw.send(kwhMsg.set(totalkWh, 4));  // Send kwh value to gw 
      lastSend = now;
      sendNeeded = false;
    }
  }
}

