// Node definitions
#define MY_DEBUG
#define MY_RADIO_NRF24
#define MY_NODE_ID    0xEE
//#define MY_PARENT_NODE_ID 0
//#define MY_REPEATER_FEATURE

#include <SPI.h>
#include <MySensor.h>  


#define NODE_TEXT     "PowerWaterGas"
#define NODE_VERSION  "0.7"
// Sensor definitions
#define SENSOR_COUNT    3
#define SENSOR_ID_POWER 1
#define SENSOR_ID_WATER 2
#define SENSOR_ID_GAS   3


// Construct MySensors library
MyMessage currentMsg(SENSOR_ID_POWER, V_WATT);
MyMessage totalMsg(SENSOR_ID_POWER, V_KWH);
MyMessage pulseCountMsg(SENSOR_ID_POWER, V_VAR1);

MyMessage waterCurrentMsg(SENSOR_ID_WATER, V_FLOW);
MyMessage waterTotalMsg(SENSOR_ID_WATER, V_VOLUME);
MyMessage waterPulseCountMsg(SENSOR_ID_WATER, V_VAR1);

MyMessage gasCurrentMsg(SENSOR_ID_GAS, V_FLOW);
MyMessage gasTotalMsg(SENSOR_ID_GAS, V_VOLUME);
MyMessage gasPulseCountMsg(SENSOR_ID_GAS, V_VAR1);

unsigned long lastSendCurrent[SENSOR_COUNT];
unsigned long lastSendTotal[SENSOR_COUNT];

void setup() 
{
  Serial.println(F(NODE_TEXT  " "  NODE_VERSION));
  for (byte i = 0; i < SENSOR_COUNT; i++)
  {
    pinMode(4 + i, OUTPUT);
  }
  for (byte blink = 0; blink < 15; blink++)
  {
    for (byte i = 0; i < SENSOR_COUNT; i++) digitalWrite(4+i, HIGH);   // sets the LED on
    delay(125 - 5 * blink);
    for (byte i = 0; i < SENSOR_COUNT; i++) digitalWrite(4+i, LOW);    // sets the LED off
    delay(125 - 5 * blink);
  }
}

void presentation()
{
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo(NODE_TEXT, NODE_VERSION);

  // Register this device as power sensor
  present(SENSOR_ID_POWER, S_POWER);
  // Register this device as Waterflow sensor
  present(SENSOR_ID_WATER, S_WATER); 
  // Register this device as Waterflow sensor
  present(SENSOR_ID_GAS, S_GAS); 

  for (byte idx = 0; idx < SENSOR_COUNT; idx++) 
  {
    lastSendCurrent[idx] = millis();
    lastSendTotal[idx] = millis();
    // Fetch last known pulse count value from the controller
    request(idx + 1, V_VAR1);
  }
}

bool          prevPulse[SENSOR_COUNT]          = {true, true, true};
int           min[SENSOR_COUNT]                = {1023, 1023, 1023};
int           max[SENSOR_COUNT]                = {0, 0, 0};
unsigned long pulseCount[SENSOR_COUNT]         = {0, 0, 0};
boolean       pulseCountReceived[SENSOR_COUNT] = {false, false, false};
double        total[SENSOR_COUNT]              = {0, 0, 0};
double        current[SENSOR_COUNT]            = {0, 0, 0};
const char *  CURRENT_UNIT[SENSOR_COUNT]       = {
  "W",
  "l/m",
  "l/m"
};
const char * TOTAL_UNIT[SENSOR_COUNT] = {
  "Wh",
  "m3",
  "m3"
};
bool          sendTotalNeeded[SENSOR_COUNT]    = {false, false, false};
bool          sendCurrentNeeded[SENSOR_COUNT]  = {false, false, false};
unsigned long prevMillis[SENSOR_COUNT]         = {0, 0, 0};
#define SEND_FREQUENCY 20000
unsigned long MAX_PULSE_MILLIS[SENSOR_COUNT]   = {
  120000,
  120000,
  120000
};

const boolean HIGH_PULSE[SENSOR_COUNT]                       = {true, false, true};
const char *  NAME[SENSOR_COUNT]               = {
  "POWER",  // power
  "WATER",  // water
  " GAS "   // gas
};
const double  PULSE_FACTOR[SENSOR_COUNT]       = {
  375.0,  // 375 pulses per kWh
  1000.0, // 1000 pulses per m3 water
  10.0    // 100 pulses per m3 gas
};
const double  PULSE_ACTUAL[SENSOR_COUNT]     = {
  3600000.0 /* h */ * 1000.0 / PULSE_FACTOR[0], // Pulses per watt hour
    60000.0 /* m */ * 1000.0 / PULSE_FACTOR[1], // Pulses per liter water
    60000.0 /* m */ * 1000.0 / PULSE_FACTOR[2]  // Pulses per liter gas
};
const int     DREMPEL[SENSOR_COUNT]            = {
  570, 
  300, 
  512
};

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
    bool change = false;
    if (sensorValue < min[idx])
    {
      min[idx] = sensorValue;
      change = true;
    }
    if (sensorValue > max[idx])
    {
      max[idx] = sensorValue;
      change = true;
    }
    //if (pulseCount[idx] < 100)
    if (change)
    {
      if (idx == 1)
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
    }
    // are we now in a pulse?
    bool pulse;
    if (HIGH_PULSE[idx])
    {
      pulse = (sensorValue > DREMPEL[idx]);
    }
    else
    {
      pulse = (sensorValue < DREMPEL[idx]);
    }
    if (prevPulse[idx] != pulse)
    {
      if (pulse) 
      {
        // increase the total pulse count
        pulseCount[idx]++;
        // determine the time between the pulses
        unsigned long pulseTime = (now - prevMillis[idx]);
        prevMillis[idx] = now;
        total[idx]   = double(pulseCount[idx]) / PULSE_FACTOR[idx];
        //current[idx] = (3600000000.0 / (PULSE_FACTOR[idx] * pulseTime));
        current[idx] = PULSE_ACTUAL[idx] / pulseTime;
        if (idx != 2)
        {
          Serial.print(NAME[idx]);
          Serial.print(" pulse ");
          Serial.print(pulseCount[idx]);
          Serial.print(" T=");
          Serial.print(pulseTime);
          Serial.print("ms Curr=");
          Serial.print(current[idx]);
          Serial.print(CURRENT_UNIT[idx]);
          Serial.print(" Total=");
          Serial.print(total[idx]);
          Serial.println(TOTAL_UNIT[idx]);
        }
  
        // The current usage/flow can be sent without reception of the original pulsecount.
        sendCurrentNeeded[idx] = true;
        // Start sending messages only when the initial pulsecount has been received.
        sendTotalNeeded[idx] = pulseCountReceived[idx];
        digitalWrite(4 + idx, LOW);
      }
      else
      {
        digitalWrite(4 + idx, HIGH);
      }
    }
    else
    {
      unsigned long pulseTime = (now - prevMillis[idx]);
      if (pulseTime > MAX_PULSE_MILLIS[idx])
      {
        // No pulse received, set the current usage to zero.
        Serial.print(NAME[idx]);
        Serial.println(" nopulse ");
        prevMillis[idx] = now;
        current[idx] = 0;
        sendCurrentNeeded[idx] = true;
      }
    }
    prevPulse[idx] = pulse;
    if (sendTotalNeeded[idx])
    {
      bool sendNow = (now - lastSendTotal[idx]) > SEND_FREQUENCY;
      if (sendNow)
      {
        switch (idx)
        {
          case 0: // power
            send(pulseCountMsg.set(pulseCount[idx]));  // Send pulse count value to gw 
            send(totalMsg.set(total[idx], 4));  // Send kwh value to gw 
            break;
          case 1: // water
            send(waterPulseCountMsg.set(pulseCount[idx]));  // Send pulse count value to gw 
            send(waterTotalMsg.set(total[idx], 4));  // Send kwh value to gw 
            break;
          case 2: // gas
            //send(gasPulseCountMsg.set(pulseCount[idx]));  // Send pulse count value to gw 
            //send(gasTotalMsg.set(total[idx], 4));  // Send kwh value to gw 
            break;
        }
        lastSendTotal[idx] = now;
        sendTotalNeeded[idx] = false;
      }
    }
    if (sendCurrentNeeded[idx])
    {
      bool sendNow = (now - lastSendCurrent[idx]) > SEND_FREQUENCY;
      if (sendNow)
      {
        switch (idx)
        {
          case 0:
            send(currentMsg.set(current[idx], 0));  // Send watt value to gw 
            break;
          case 1:
            send(waterCurrentMsg.set(current[idx], 1));  // Send watt value to gw 
            break;
          case 2:
            send(gasCurrentMsg.set(current[idx], 1));  // Send watt value to gw 
            break;
        }
        lastSendCurrent[idx] = now;
        sendCurrentNeeded[idx] = false;
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
    Serial.print(" Received last pulse count from gw ");
    Serial.println(pulseCount[idx]);
    pulseCountReceived[idx] = true;
  }
}

