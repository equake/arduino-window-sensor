#include <FS.h>  //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>   //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <WiFiClient.h>
#include <ArduinoJson.h>         //https://github.com/bblanchon/ArduinoJson
#include <DHT.h>
#include <MovingAverage.h> // TO DO: fazer movingAverage funcionar com ESP8266 2.4
#include <MQTTClient.h>

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40] = "192.168.1.105";
char mqtt_port[6] = "1883";
//default custom static IP of IP for window_sensor
//char static_ip[16] = "192.168.1.104";
//char static_gw[16] = "191.168.1.1";
//char static_sn[16] = "255.255.255.0";

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

bool debug = true;

// Sonar Sensor Right 
#define TRIGGERR D1 // Trigger Right Sensor
#define ECHO  D2 //echo Right Sensor

// Sonar Sensor Left
#define TRIGGERL D5 // Tr

#define ECHO2 D6 // echo 


// Constant and variables
int AnalogInput = A0; // 
char max_light[4] = "30";
DHT dht(D3, DHT11);

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
          strcpy(max_light, json["max_light"]);
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
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
  WiFiManagerParameter custom_max_light("max_light", "Max Light", max_light, 4);

  ESP8266WebServer server(80);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager; //default custom static IP

  //reset saved settings
  //wifiManager.resetSettings();

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

 //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_max_light);

  //for some reason that I can't understood why at my home, I only have sucess to connect to wifi AP using the code below :(
  if(!wifiManager.autoConnect("window_sensor")) {
    Serial.println("failed to connect, we should reset as see if it connects");
    delay(3000);
    ESP.reset();
    delay(5000);
  }
  
  Serial.println("local ip:");
  Serial.println(WiFi.localIP());

  if (debug) {
    Serial.println("");
    Serial.print("Connected to:"); 
    Serial.println(WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");    
  }

  //read updated parameters (if applicable)
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(max_light, custom_max_light.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["max_light"] = max_light;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    } else {
      json.printTo(Serial);
      json.printTo(configFile);
    }
    configFile.close();
  }

    /* Initialize PIN Modes Sonar Sensors*/
    pinMode(TRIGGERR, OUTPUT);  // Trigger Right Sensor
    pinMode(ECHO, INPUT); // Echo Right Sensor
    pinMode(TRIGGERL, OUTPUT); // Trigger Left Sensor
    pinMode(ECHO2, INPUT); // Echo Left Sensor

    dht.begin();

    leftWindowAverage.reset(50);
    rightWindowAverage.reset(50);

    Serial.print("MQTT Server:"); 
    Serial.println(mqtt_server);
    Serial.print("MQTT Port:");
    Serial.println(mqtt_port);
    Serial.println("Local ip: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway:");
    Serial.println(WiFi.gatewayIP());
    Serial.print("subnetMask:");
    Serial.println(WiFi.subnetMask());
    Serial.print("MAX Light:");
    Serial.println(max_light);

    mqttClient.begin(mqtt_server, netClient);
    Serial.print("\nOn the Setup()! connecting to MQTT...");
    while (!mqttClient.connect("window_sensor")) {
      Serial.print(".");
      delay(1000);
    }
}

// the loop function runs over and over again forever
void loop() {
  mqttClient.loop();

  if (!mqttClient.connected()) {
    mqttClient.connect("window_sensor");
    Serial.print("\n On the loop()! connecting to MQTT...");
    while (!mqttClient.connect("window_sensor")) {
      Serial.print(".");
      delay(1000);
    }
  }

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
  int FullOpenedRight = 2; // distance between sensor and glass window when is open ;o)
  int FullClosedRight = 51; // distance between sensor and glass window when is close. 
  int FullOpenedLeft = 2; // distance between wall and glass window when is open ;o)
  int FullClosedLeft = 78; // distance between sensor and glass window when is close.

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
  //int MaxLight = 200;
  int MaxLight = atoi(max_light);
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
