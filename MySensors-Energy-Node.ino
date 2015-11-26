// Node definitions
#define MY_DEBUG
#define MY_RADIO_NRF24
#define MY_NODE_ID    0xEE
//#define MY_PARENT_NODE_ID 0
//#define MY_REPEATER_FEATURE

#include <SPI.h>
#include <MySensor.h>  


#define NODE_TEXT     "PowerGasWater"
#define NODE_VERSION  "0.4"
// Sensor definitions
#define SENSOR_COUNT    3
#define SENSOR_ID_POWER 1
#define SENSOR_ID_GAS   2
#define SENSOR_ID_WATER 3
#define CHILD_ID 1

#define SEND_FREQUENCY 20000

// Construct MySensors library
MyMessage currentMsg(SENSOR_ID_POWER, V_WATT);
MyMessage totalMsg(SENSOR_ID_POWER, V_KWH);
MyMessage pulseCountMsg(SENSOR_ID_POWER, V_VAR1);

MyMessage gasCurrentMsg(SENSOR_ID_GAS, V_FLOW);
MyMessage gasTotalMsg(SENSOR_ID_GAS, V_VOLUME);
MyMessage gasPulseCountMsg(SENSOR_ID_GAS, V_VAR1);

MyMessage waterCurrentMsg(SENSOR_ID_WATER, V_FLOW);
MyMessage waterTotalMsg(SENSOR_ID_WATER, V_VOLUME);
MyMessage waterPulseCountMsg(SENSOR_ID_WATER, V_VAR1);

unsigned long lastSend[SENSOR_COUNT];

void setup() 
{
  Serial.println(F(NODE_TEXT  " "  NODE_VERSION));
}

void presentation()
{
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo(NODE_TEXT, NODE_VERSION);

  // Register this device as power sensor
  present(SENSOR_ID_POWER, S_POWER);
  // Register this device as Waterflow sensor
  present(SENSOR_ID_GAS, S_GAS); 
  // Register this device as Waterflow sensor
  present(SENSOR_ID_WATER, S_WATER); 

  for (byte idx = 0; idx < SENSOR_COUNT; idx++) 
  {
    lastSend[idx] = millis();
    // Fetch last known pulse count value from the controller
    request(idx + 1, V_VAR1);
  }
}


bool          prevPulse[SENSOR_COUNT]          = {true, true, true};
const int     DREMPEL[SENSOR_COUNT]            = {570, 512, 512};
unsigned long pulseCount[SENSOR_COUNT]         = {0, 0, 0};
boolean       pulseCountReceived[SENSOR_COUNT] = {false, false, false};
float         totalkWh[SENSOR_COUNT]           = {0, 0, 0};
unsigned long curWatts[SENSOR_COUNT]           = {0, 0, 0};
bool          sendNeeded[SENSOR_COUNT]         = {false, false, false};
unsigned long prevMillis[SENSOR_COUNT]         = {0, 0, 0};
const int     PULSE_FACTOR[SENSOR_COUNT]       = {375, 1000, 1000};
const double  PULSE_PER_UNIT[SENSOR_COUNT]     = {
  ((double)PULSE_FACTOR[0]) / 1000, // Pulses per watt hour
  ((double)PULSE_FACTOR[1]) / 1000, // Pulses per liter
  ((double)PULSE_FACTOR[2]) / 1000, // Pulses per liter
};
int           min[SENSOR_COUNT]                = {1023, 1023, 1023};
int           max[SENSOR_COUNT]                = {0, 0, 0};
const char *  NAME[SENSOR_COUNT]               = {"E: ", "W: ", "G: "};

void loop() 
{
  unsigned long now = millis();
  unsigned short sum[3] = {0, 0, 0};
  // read the input on analog pin A0, A1 and A2:
  for (byte i = 0; i < 40; i++) 
  {
    for (byte idx = 0; idx < 3; idx++)
    {
      sum[idx] += analogRead(A0 + idx);
    }
  }
  for (byte idx = 0; idx < 3; idx++)
  {
    // Calculate average sensorvalue
    int sensorValue = sum[idx] / 40;
    // Update min/max values
    if (sensorValue < min[idx]) min[idx] = sensorValue;
    if (sensorValue > max[idx]) max[idx] = sensorValue;
    if (pulseCount[idx] < 1)
    {
      // print min/max values until the first pulse has passed
      Serial.print(NAME[idx]);
      Serial.print(sensorValue);
      Serial.print(" [");
      Serial.print(min[idx]);
      Serial.print("-");
      Serial.print(max[idx]);
      Serial.println("]");
    }
    // are we now in a pulse?
    bool pulse = (sensorValue < DREMPEL[idx]);
    if ((prevPulse[idx] != pulse) && pulse) 
    {
      // increase the total pulse count
      pulseCount[idx]++;
      // determine the time between the pulses
      unsigned long pulseTime = (now - prevMillis[idx]);
      prevMillis[idx] = now;
      totalkWh[idx] = float(pulseCount[idx]) / PULSE_FACTOR[idx];
      curWatts[idx] = (3600000000.0 / (PULSE_FACTOR[idx] * pulseTime));
      if (idx < 1)
      {
        Serial.print(NAME[idx]);
        Serial.print("Pulse ");
        Serial.print(pulseCount[idx]);
        Serial.print(" T=");
        Serial.print(pulseTime);
        Serial.print(" Curr=");
        Serial.print(curWatts[idx]);
        Serial.print(" Total=");
        Serial.println(totalkWh[idx]);
      }

      // Start sending messages only when the initial pulsecount has been received.
      sendNeeded[idx] = pulseCountReceived[idx];
    }
    prevPulse[idx] = pulse;
    if (sendNeeded[idx])
    {
      bool sendNow = (now - lastSend[idx]) > SEND_FREQUENCY;
      if (sendNow)
      {
        switch (idx)
        {
          case 0:
            send(pulseCountMsg.set(pulseCount[idx]));  // Send pulse count value to gw 
            send(currentMsg.set(curWatts[idx]));  // Send watt value to gw 
            send(totalMsg.set(totalkWh[idx], 4));  // Send kwh value to gw 
            break;
          case 1:
            /*
            gw.send(gasPulseCountMsg.set(pulseCount[idx]));  // Send pulse count value to gw 
            gw.send(gasCurrentMsg.set(curWatts[idx]));  // Send watt value to gw 
            gw.send(gasTotalMsg.set(totalkWh[idx], 4));  // Send kwh value to gw 
            */
            break;
          case 2:
            /*
            gw.send(waterPulseCountMsg.set(pulseCount[idx]));  // Send pulse count value to gw 
            gw.send(waterCurrentMsg.set(curWatts[idx]));  // Send watt value to gw 
            gw.send(waterTotalMsg.set(totalkWh[idx], 4));  // Send kwh value to gw 
            */
            break;
        }
        lastSend[idx] = now;
        sendNeeded[idx] = false;
      }
    }
  }
}

void receive(const MyMessage & message) 
{
  if (V_VAR1 == message.type) 
  {
    byte idx = message.sensor - 1;
    if (idx >= SENSOR_COUNT) return;
    pulseCount[idx] = message.getLong();
    Serial.print(NAME[idx]);
    Serial.print("Received last pulse count from gw ");
    Serial.println(pulseCount[idx]);
    pulseCountReceived[idx] = true;
  }
}

