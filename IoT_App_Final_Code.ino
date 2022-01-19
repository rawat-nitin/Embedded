//Import required Libraries
#include<ESP8266WiFi.h>   //For ESP Board
#include<DHT.h>           //For DHT Sensor
#include<FirebaseFS.h>    //For Firebase's Cloud Storage, Messagging, and Functions
#include<Firebase.h>      //For Firebase Configuration
#include<Utils.h>         //For Common Embedded Functionalities
#include<common.h>        //For Common objects
#include<Servo.h>         //For Servo Motor

//Define network credentials
#define WIFI_SSID  "FC8743C03D7F-2G"
#define WIFI_PASSWORD  "55401105054674"

//Provide the token generation process info.
#include "addons/TokenHelper.h"

//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

//Insert Firebase project API Key
#define API_KEY "AIzaSyAtghgFxY81PUiaIeqnfpTbwLhNmfh1JeM"

//Insert Authorized Email and Corresponding Password
#define USER_EMAIL "st042091@m2.kcg.edu"
#define USER_PASSWORD "AbxFdp6C"

//Define Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

//Defining Servo Objects
Servo myservo;

//Defingin PIN for the Sensors
#define DHTPIN 5            //Digital PIN connected to the DHT (D1 Pin)Sensor
#define LDRPIN 4            //Digital PIN connected to the LDR (D2 Pin)Sensor
#define SOILMOISTUREPIN 14  //Digital PIN connected to the (D5 Pin) SOILSensor

//Defining the Sensor Type
#define DHTTYPE DHT11       //Type 11 
//Instanitate a DHT object with type
DHT dht(DHTPIN, DHTTYPE);

//Defining Variables
String uid;                 //Variable to save USER ID
String databasePath;         //Variable to save Database Path
String tempPathC;           //Variable to save temperature Path
String humdPath;            //Variable to save Humidity Path
String ldrPath;             //Variable to save Light Dependent Resistor Path
String soilSensorPath;       //Varibale to save Soil Sensor Path
String motorStatusPath;     //Variable to save Sataus of Motor Path

//Defining the WiFi Client
WiFiClient client;

//Variable to store sensor values
float t = 0.0;              //For Temperature
float h = 0.0;              //For Humidity
float ldrValue = 0.0;       //For Light
int soilMoistureValue = 0;  //For Soil
int motorStatus = 0;        //For Motor
int fireStatus = 0;         //Status for Motor ON or OFF

//Time variable {It will store last time DHT was updated}
unsigned long previousMillis = 0;
const long interval =10000;   //Updates DHT reading every 10 Seconds

// Timer variables (send new readings every three minutes)
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 180000;

//Function for the LDR Sensor
double Light (float RawADC0){
  float Vo=RawADC0* (5.0/1023.0);
  float lux = (((1650/Vo)-500)/9.89);
  return lux;
}

//Function for Reading the Values of Sensor using Multiplexing technique
float analogRead1(){
  digitalWrite(LDRPIN, HIGH);   //Turn LDR ON
  digitalWrite(SOILMOISTUREPIN, LOW);     //Turn SOIL MOISTURE OFF
  return (float(Light(analogRead(A0))));
}

int analogRead2(){
  digitalWrite(LDRPIN, LOW);   //Turn LDR OFF
  digitalWrite(SOILMOISTUREPIN, HIGH);     //Turn SOIL MOISTURE ON
  return analogRead(A0);
}

//Initalize WiFi
void initWiFi(){
  WiFi.begin (WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

//Write float values to the database
void sendFloat(String path, float value){
  if (Firebase.RTDB.setFloat(&fbdo, path.c_str(), value)){
    Serial.print("Writing value: ");
    Serial.print (value);
    Serial.print(" on the following path: ");
    Serial.println(path);
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  }
  else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }
}
//Reading Values from the values from the database
int readValue(String path){
   if (Firebase.RTDB.getInt(&fbdo, path.c_str())){
    motorStatus = fbdo.intData();
    Serial.println("\n Motor Status Value:");
    Serial.print(motorStatus);
    delay(5000);
    if (motorStatus == 1){
      fireStatus = 1;
    }
    else {
      fireStatus = 0;
    }
    
  }
  else {
    Serial.println("error");
    }

    
    return fireStatus;
}

void setup(){

  Serial.begin(9600); //Serial port for debugging process and baud rate set to 9600

  //Initialize WiFi
  initWiFi();

  dht.begin();        //calling dht object

  pinMode(LDRPIN, OUTPUT);            //Setting LDRPIN as Output
  pinMode(SOILMOISTUREPIN, OUTPUT);   //Setting SoilMoistureSensor as Output
  
  myservo.attach(15);                 //Setting GPIO15 Pin for Servo (D8)

  //Assign the api key (required)
  config.api_key = API_KEY;

  //Assign the user sign in Credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  //Assign the RTDB URL (required)
  config.database_url = "https://node-firebase-demo-80b37-default-rtdb.asia-southeast1.firebasedatabase.app/";


  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  //Assign the callback function for the long running token generation task
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  //Assign the maximum retry of token generation
  config.max_token_generation_retry = 5;

  //Initialize the library with the Firebase authentication and configuration
  Firebase.begin(&config, &auth);

  //Getting the user UID might take a few seconds
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }
  //Print user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.print(uid);

   //Update database path
  databasePath = "/ParamsSensor/" + uid;

  // Update database path for sensor readings
  tempPathC = databasePath + "/temperature"; // --> UsersData/<user_uid>/temperature
  humdPath = databasePath + "/humidity"; // --> UsersData/<user_uid>/humidity
  ldrPath = databasePath + "/light Intensity"; // --> UsersData/<user_uid>/Light Intensity
  soilSensorPath = databasePath + "/Soil Moisture Value"; // --> UsersData/<user_uid>/Soil Moisture Value
  motorStatusPath = databasePath + "/Motor Status"; // --> UsersData/<user_uid>/Motor Status
  
}


void loop () {

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval){
  // save the last time you updated the DHT values
  previousMillis = currentMillis;
    
  // Send new readings to database
  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)){
  // Read temperature as Celsius (the default)
  float newT = dht.readTemperature();
  //if temperature read failes, don't change t value
  if (isnan(newT)){
    Serial.println("Failed to read from DHT sensor!");
  }
  else {
    t = newT;
    Serial.println("Temperature in C:");
    Serial.println(t);
  }
  //Read Humidity
  float newH = dht.readHumidity();
  //if humidity read failed, don't change h value
  if (isnan(newH)){
    Serial.println("Failed to read from DHT Sensor!");
  }
  else {
    h = newH;
    Serial.println("Humidity in %:");
    Serial.println(h);
  }
  
  //Reading LDR Sensor value
  float newldrValue = analogRead1();
  //if ldr read failed, don't have change ldrValue value
  if (isnan(newldrValue)){
    Serial.println("Failed to read from LDR Sensor!");
  }
  else{
    ldrValue = newldrValue;
    delay(1500);
    Serial.println("Light Intensity in Lux");
    Serial.println(ldrValue);
  }
 
  //Reading Soil Moisture Sensor value
  int newSM = analogRead2();
  //if Soil Moisture Sensor read failed, don't have change ldrValue value
  if (isnan(newSM)){
    Serial.println("Failed to read from Soil Sensor!");
  }
  else{
    soilMoistureValue = newSM;
    delay(1500);
    Serial.println("Soil Moisture Value");
    Serial.println(soilMoistureValue);
  }
 }
  // Send readings to database:
    sendFloat(tempPathC, t);
    sendFloat(humdPath, h);
    sendFloat(ldrPath, ldrValue);
    sendFloat(soilSensorPath,soilMoistureValue);
  }

  //Reading Values from Database
  int Stats =  readValue(motorStatusPath);
  //Checking for Status Condition
  if(Stats == 1){
    Serial.println("Pump is ON");
    myservo.write(180);
  }
  else if (Stats == 0){
    Serial.println("Pump is OFF");
    myservo.write(0);
  }
  else {
    Serial.println("Command Error! Please send 0/1");
  }
}
