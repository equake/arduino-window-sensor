#include <ESP8266WiFi.h>   //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <WiFiClient.h>
#include <DHT.h>
#include <LinkedList.h>
#include <GaussianAverage.h>

GaussianAverage leftWindowAverage = GaussianAverage(10);
GaussianAverage rightWindowAverage = GaussianAverage(10);

bool debug = true; // :(

// Sonar Sensor Right 
#define TRIGGERR D5 // Trigger Right Sensor
#define ECHO D6 // //echo Right Sensor

// Sonar Sensor Left
#define TRIGGERL D1 // Trigger Left Sensor
#define ECHO2 D2  // echo Left Sensor

// variables of Sonar Sensors
long duration, distance, RightSensor, LeftSensor;
int RightOpen;
int LeftOpen;

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
}


// the loop function runs over and over again forever
void loop() {
  
  Serial.print("Luminosity: ");
  Serial.println(analogRead(AnalogInput));

  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" *C ");
  Serial.print(f);
  Serial.print(" *F\t");
  Serial.print("Heat index: ");
  Serial.print(hic);
  Serial.print(" *C ");
  Serial.print(hif);
  Serial.println(" *F");
  //Serial.println("Fuck!!!");
  

  // Call Sonar Sensor function
  // Right Sensor
  SonarSensor(TRIGGERR, ECHO);
  RightSensor = distance;
  // Left Sensor
  SonarSensor(TRIGGERL, ECHO2);
  LeftSensor = distance;

 
  
  Serial.print("Centimeter Left:"); 
  Serial.println(LeftSensor);
  Serial.print("Centimeter Right:");
  Serial.println(RightSensor);
  
  WindowSonarSensor(RightSensor, LeftSensor);
  Serial.print("Status Lamina Esquerda:"); 
  Serial.println(LeftOpen);
  Serial.print("Status Lamina Direita:");
  Serial.println(RightOpen);

  LightSensor(analogRead(AnalogInput));
  Serial.print("Status Lâmpada:"); 
  Serial.println(Lamp);
}


void SonarSensor(int trigPin,int echoPin){
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = (duration/2) / 29.1;
  delay(100);
}

void WindowSonarSensor(long RightSensor, long LeftSensor){
  int FullOpenedRight = 5; // distance between sensor and glass window when is open ;o)
  int FullClosedRight = 110; // distance between sensor and glass window when is close. 
  int FullOpenedLeft = 5; // distance between wall and glass window when is open ;o)
  int FullClosedLeft = 90; // distance between sensor and glass window when is close.

  int tmpRightOpen;
  if (RightSensor >= FullClosedRight)
    tmpRightOpen = 0;
  else if (RightSensor <= FullOpenedRight)
    tmpRightOpen = 100;
  else
    tmpRightOpen = map(RightSensor, FullClosedRight, FullOpenedLeft, 0, 100);
  rightWindowAverage.add(tmpRightOpen)
  RightSensor = rightWindowAverage.process();

  int tmpLeftOpen;
  if (LeftSensor >= FullClosedLeft)
    tmpLeftOpen = 0;
  else if (LeftSensor <= FullOpenedLeft)
    tmpLeftOpen = 100;
  else
    tmpLeftOpen = map(LeftSensor, FullClosedLeft, FullOpenedLeft, 0, 100);
  leftWindowAverage.add(tmpLeftOpen)
  LeftSensor = leftWindowAverage.process();   
}


void LightSensor(long AnalogInput) {
  if(AnalogInput > 0) {
      if(AnalogInput <= MaxLight) {
        Lamp = "On";
      }else{
        Lamp = "Off";
      }
    }else {
        Lamp = "Sensor not working";
     }
}
 



