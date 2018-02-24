#include <FS.h>  //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>   //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <WiFiClient.h>
#include <ArduinoJson.h> 
#include <DHT.h>
#include <MovingAverage.h> // TO DO: fazer movingAverage funcionar com ESP8266 2.4
#include <MQTTClient.h>


//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40];
char mqtt_port[6] = "1883";

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

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

char FullOpenedRight[4]; // distance between sensor and glass window when is open ;o)
char FullClosedRight[4]; // distance between sensor and glass window when is close. 
char FullOpenedLeft[4]; // distance between wall and glass window when is open ;o)
char FullClosedLeft[4]; // distance between sensor and glass window when is close.

// Constant and variables of Light Sensor
int AnalogInput = A0; // Input Light Sensor
char MinLight[4]; //= 900;
char MaxLight[4]; //= 100;
char *Lamp = "On";  
DHT dht(D3, DHT11);

volatile boolean closed = false;


void setup() {
  
   // put your setup code here, to run once:
   Serial.begin(115200);
   Serial.println();

   //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(MinLight, json["MinLight"]);
          strcpy(MaxLight, json["MaxLight"]);
          strcpy(FullOpenedRight, json["FullOpenedRight"]);
          strcpy(FullClosedRight, json["FullCloseddRight"]);
          strcpy(FullOpenedLeft, json["FullOpenedLeft"]);
          strcpy(FullClosedLeft, json["FullClosedLeft"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_MinLight("MinLight", "Min Light", MinLight, 4);
  WiFiManagerParameter custom_MaxLight("MaxLight", "Max Light", MaxLight, 4);
  WiFiManagerParameter custom_FullOpenedRight("FullOpenedRight", "Full Opened Right", FullOpenedRight, 4);
  WiFiManagerParameter custom_FullClosedRight("FullClosedRight", "Full Closed Right", FullClosedRight, 4);
  WiFiManagerParameter custom_FullOpenedLeft("FullOpenedLeft", "Full Opened Left", FullOpenedLeft, 4);
  WiFiManagerParameter custom_FullClosedLeft("FullClosedLeft", "Full Closed Left", FullClosedLeft, 4);

  ESP8266WebServer server(80);

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    //reset saved settings
    //wifiManager.resetSettings();

      //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
    
    
 //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_MinLight);
  wifiManager.addParameter(&custom_MaxLight);
  wifiManager.addParameter(&custom_FullOpenedRight);
  wifiManager.addParameter(&custom_FullClosedRight);
  wifiManager.addParameter(&custom_FullOpenedLeft);
  wifiManager.addParameter(&custom_FullClosedLeft);

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
        //if you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");    
    //  }

    //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(MinLight, custom_MinLight.getValue());
  strcpy(MaxLight, custom_MaxLight.getValue());
  strcpy(FullOpenedRight, custom_FullOpenedRight.getValue());
  strcpy(FullClosedRight, custom_FullClosedRight.getValue());
  strcpy(FullOpenedLeft, custom_FullOpenedLeft.getValue());
  strcpy(FullClosedLeft, custom_FullClosedLeft.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["MinLight"] = MinLight;
    json["MaxLight"] = MaxLight;
    json["FullOpenedRight"] = FullOpenedRight;
    json["FullClosedRight"] = FullClosedRight;
    json["FullOpenedLeft"] = FullOpenedLeft;
    json["FullClosedLeft"] = FullClosedLeft;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

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

    mqttClient.begin(mqtt_server, netClient);
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
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);
  
  send_data("home/balcony/bedroom/humidity", h);
  send_data("home/balcony/bedroom/temperature", t);
  send_data("home/balcony/bedroom/heat_index", hic);
}

float SonarSensor(int trigPin,int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  return (duration/2) / 29.1;
}

void WindowSonarSensor(){
  int full_opened_right = atoi(FullOpenedRight);
  int full_closed_right = atoi(FullClosedRight);
  int full_opened_left = atoi(FullOpenedLeft);
  int full_closed_left = atoi(FullClosedLeft);
  //int FullOpenedRight = 5; // distance between sensor and glass window when is open ;o)
  //int FullClosedRight = 110; // distance between sensor and glass window when is close. 
  //int FullOpenedLeft = 5; // distance between wall and glass window when is open ;o)
  //int FullClosedLeft = 90; // distance between sensor and glass window when is close.

  // Right Sensor
  long RightSensor = SonarSensor(TRIGGERR, ECHO);

  // Left Sensor
  long LeftSensor = SonarSensor(TRIGGERL, ECHO2);

  int RightOpen = 0;
  if (RightSensor >= full_closed_right)
    RightOpen = rightWindowAverage.update(0);
  else if (RightSensor <= full_opened_right)
    RightOpen = rightWindowAverage.update(100);
  else
    RightOpen = rightWindowAverage.update(map(RightSensor, full_closed_right, full_opened_left, 0, 100));
  send_data("home/bedroom/window/right/centimeters", RightSensor);
  send_data("home/bedroom/window/right/percentage", RightOpen);

  int LeftOpen = 0;
  if (LeftSensor >= full_closed_left)
    LeftOpen = leftWindowAverage.update(0);
  else if (LeftSensor <= full_opened_left)
    LeftOpen = leftWindowAverage.update(100);
  else
    LeftOpen = leftWindowAverage.update(map(LeftSensor, full_closed_left, full_opened_left, 0, 100));
  send_data("home/bedroom/window/left/centimeters", LeftSensor);
  send_data("home/bedroom/window/left/percentage", LeftOpen);
}


void LightSensor(long analogInputValue) {
  int max_light = atoi(MaxLight);
  send_data("home/balcony/bedroom/light/raw", analogInputValue);
  if(analogInputValue > 0) {
      if(analogInputValue <= max_light) {
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
