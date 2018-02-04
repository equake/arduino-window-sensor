#include <ESP8266WiFi.h>   //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <WiFiClient.h>
#include <DHT.h>
#include <MovingAverage.h>
#include <MQTTClient.h>


MovingAverage leftWindowAverage(10);
MovingAverage rightWindowAverage(10);
MovingAverage lightAverage(10);
MQTTClient mqttClient;
WiFiClient netClient;

bool debug = true; // :(

// Sonar Sensor Right 
#define TRIGGERR D5 // Trigger Right Sensor
#define ECHO D6 // //echo Right Sensor

// Sonar Sensor Left
#define TRIGGERL D1 // Trigger Left Sensor
#define ECHO2 D2  // echo Left Sensor

// Constant and variables of Light Sensor
int AnalogInput = A0; // Input Light Sensor
long MinLight = 900;
long MaxLight = 100;
char *Lamp = "On";  
DHT dht(D3, DHT11);

volatile boolean closed = false;


void setup() {
  
   // put your setup code here, to run once:
   Serial.begin(115200);
   Serial.println();

   ESP8266WebServer server(80);

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    //reset saved settings
    //wifiManager.resetSettings();
    
    //set custom ip for portal
    //wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
    
       //for some reason that I can't understood why at my home, I only have sucess to connect to wifi AP using the code below :(
    if(!wifiManager.autoConnect("Window_Sensor")) {
      Serial.println("failed to connect, we should reset as see if it connects");
      delay(3000);
      ESP.reset();
      delay(5000);
    }
  
    Serial.println("local ip");
    Serial.println(WiFi.localIP());

    //  if (debug) {
        Serial.println("");
        Serial.print("Connected to ");
        Serial.println(WiFi.SSID());
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    //  }

       /* Initialize PIN Modes Sonar Sensors*/
    pinMode(TRIGGERR, OUTPUT);  // Trigger Right Sensor
    pinMode(ECHO, INPUT); // Echo Right Sensor
    pinMode(TRIGGERL, OUTPUT); // Trigger Left Sensor
    pinMode(ECHO2, INPUT); // Echo Left Sensor

   // pinMode(D4, OUTPUT);
  //  pinMode(D7, INPUT);
  //  pinMode(D8, INPUT);
    dht.begin();

    leftWindowAverage.reset(50);
    rightWindowAverage.reset(50);

    mqttClient.begin("192.168.16.181", netClient);
    Serial.print("\nconnecting to MQTT...");
    while (!mqttClient.connect("window_sensor")) {
      Serial.print(".");
      delay(1000);
    }

}


// the loop function runs over and over again forever
void loop() {
  LightSensor(lightAverage.update(analogRead(AnalogInput)));
  Thermometer();
  WindowSonarSensor();
  delay(1000);
}


void Thermometer() {
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);
  
  send_data("home/balcony/bedroom/humidity", h);
  send_data("home/balcony/bedroom/temperature", t);
  send_data("home/balcony/bedroom/heat_index", hic);
}

void SonarSensor(int trigPin,int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  return (duration/2) / 29.1;
}

void WindowSonarSensor(){
  int FullOpenedRight = 5; // distance between sensor and glass window when is open ;o)
  int FullClosedRight = 110; // distance between sensor and glass window when is close. 
  int FullOpenedLeft = 5; // distance between wall and glass window when is open ;o)
  int FullClosedLeft = 90; // distance between sensor and glass window when is close.

  // Right Sensor
  long RightSensor = SonarSensor(TRIGGERR, ECHO);

  // Left Sensor
  long LeftSensor = SonarSensor(TRIGGERL, ECHO2);

  int RightOpen = 0;
  if (RightSensor >= FullClosedRight)
    RightOpen = rightWindowAverage.update(0);
  else if (RightSensor <= FullOpenedRight)
    RightOpen = rightWindowAverage.update(100);
  else
    RightOpen = rightWindowAverage.update(map(RightSensor, FullClosedRight, FullOpenedLeft, 0, 100));
  send_data("home/bedroom/window/right/centimeters", RightSensor);
  send_data("home/bedroom/window/right/percentage", RightOpen);

  int LeftOpen = 0;
  if (LeftSensor >= FullClosedLeft)
    LeftOpen = leftWindowAverage.update(0);
  else if (LeftSensor <= FullOpenedLeft)
    LeftOpen = leftWindowAverage.update(100);
  else
    LeftOpen = leftWindowAverage.update(map(LeftSensor, FullClosedLeft, FullOpenedLeft, 0, 100));
  send_data("home/bedroom/window/left/centimeters", LeftSensor);
  send_data("home/bedroom/window/left/percentage", LeftOpen);
}


void LightSensor(long analogInputValue) {
  send_data("home/balcony/bedroom/light/raw", analogInputValue);
  if(analogInputValue > 0) {
      if(analogInputValue <= MaxLight) {
        send_data("home/balcony/bedroom/light", "true");
      }else{
        send_data("home/balcony/bedroom/light", "false");
      }
    } else {
      send_data("home/balcony/bedroom/light", "???");
    }
}
 

void send_data(String topic, long data) {
  send_data(topic, String(data));
}

void send_data(String topic, String data) {
  Serial.print(topic);
  Serial.print(":");
  Serial.println(data);
  mqttClient.publish(topic, data);
}
