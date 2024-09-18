/**
 * CloudBrick
 * MODEL : ( 2out-pwm or 1out-ipwm ) and 2 digital output
 * 
 * This sketch enable you to pilot a dual H-Bridge to have two analog
 * output to drive motors, leds, and so on.

 * 
 * This sketch add some configuration parameter, now you can customize:
 *
 * THING NAME    : the hostname of the device, and the name of this brick in the MQTT broker
 * AP PASSWORD   : the WIFI password for initialization or in case the configured WiFi is no more reachable
 *                 this is also the password to access configuration page (username : admin)
 * WiFi SSID     : the SSID to connect to
 * WiFi Password : the Password of the Wifi network you want the brick to connect
 * MQTT server   : FQDN of the MQTT server
 * MQTT Port     : Port of the MQTT server (usualli 1883 or 8883)
 * MQTT User     : username to connect to MQTT Server
 * MQTT Password : password to connect to MQTT Server  
 * PWM-inverted  : true or false
 *                 == FALSE, pinout == 
 *                 this settings is compatible with L239D H-bridge. Every output has 1 PWM and 2 pin to set the polarity/rotation
 *                 GPIO4  - used to set OUTPUT1 motor direction/polarity (setup as digital output)
 *                 GPIO5  - used to set OUTPUT1 motor direction/polarity (setup as digital output)
 *                 GPIO14 - used to set OUTPUT1 power (setup as analog / pwm output) 
 *                 GPIO1  - used to set OUTPUT2 motor direction/polarity (setup as digital output)
 *                 GPIO3  - used to set OUTPUT2 motor direction/polarity (setup as digital output)
 *                 GPIO12 - used to set OUTPUT2 power (setup as analog / pwm output) 
 * 
 *                 == TRUE, pinout == 
 *                 this settings is compatible with MAX1508 H-bridge. Every output has 1 digital output (to set polarity/direction) and 1 PWM to set polarity/rotation
 *                 GPIO12  - used to set OUTPUT1 motor direction/polarity (setup as digital output)
 *                 GPIO14  - used to set OUTPUT1 power (setup as analog / pwm output) 
 *                 GPIO15  - used to set OUTPUT2 motor direction/polarity (setup as digital output)
 *                 GPIO4  - used to set OUTPUT2 power (setup as analog / pwm output) 
 * 
 * 
 *
 * Once connected to MQTT broker, this device will 
 * SUBSCRIBE TO : /CloudBrick/BRC_ESP_<thingname>/Command
 * PUBLISH TO : /CloudBrick/BRC_ESP_<thingname>/Status
 *
 * DEFAULTS
 * thing name = myBrickThing
 * wifi password when in AP mode (eg:first start) = mybrickthing
 * username to access config page : admin
 * password to access config page : mybrickthing
 */

#include <MQTT.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <ESP8266HTTPUpdateServer.h>


//d e f i ne _DEEP_SLEEP

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "myBrickThing";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "mybrickthing";

#define STRING_LEN 128
#define NUMBER_LEN 10

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "2pwm-1ipwm-2dig-v3"

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN

// -- Callback method declarations.
void wifiConnected();
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper);
void mqttMessageReceived(String &topic, String &payload);

DNSServer dnsServer;
WebServer server(80);
//HTTPUpdateServer httpUpdater;
ESP8266HTTPUpdateServer httpUpdater;

WiFiClientSecure net;
MQTTClient mqttClient(512);

char mqttServerValue[STRING_LEN];
char mqttPortValue[NUMBER_LEN];
char mqttUserNameValue[STRING_LEN];
char mqttUserPasswordValue[STRING_LEN];
//char invertedPwmParamValue[STRING_LEN];
char invertedPwmParamValue[2];

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

// custom parameters in setup
// --- IotWebConfParameter mqttServerParam = IotWebConfParameter("MQTT server", "mqttServer", mqttServerValue, STRING_LEN);
// --- IotWebConfParameter mqttUserNameParam = IotWebConfParameter("MQTT user", "mqttUser", mqttUserNameValue, STRING_LEN);
// --- IotWebConfParameter mqttUserPasswordParam = IotWebConfParameter("MQTT password", "mqttPass", mqttUserPasswordValue, STRING_LEN, "password");

IotWebConfTextParameter mqttServerParam = IotWebConfTextParameter("MQTT server", "mqttServer", mqttServerValue, STRING_LEN);
IotWebConfTextParameter mqttUserNameParam = IotWebConfTextParameter("MQTT user", "mqttUser", mqttUserNameValue, STRING_LEN);
IotWebConfTextParameter mqttUserPasswordParam = IotWebConfTextParameter("MQTT password", "mqttPass", mqttUserPasswordValue, STRING_LEN);
IotWebConfTextParameter mqttPortParam = IotWebConfTextParameter("MQTT port", "mqttPort", mqttPortValue, STRING_LEN);
IotWebConfCheckboxParameter useInvertedPwm = IotWebConfCheckboxParameter("Use inverted PWM", "useInvertedPwm", invertedPwmParamValue, 2);



boolean needMqttConnect = false;
boolean needReset = false;
//int pinState = HIGH;
//unsigned long lastReport = 0;
unsigned long lastMqttConnectionAttempt = 0;

// ------ DEEPSLEEP contol ------
//ultimo millis a cui ho ricevuto un command
unsigned long last_cmd_running_time = 0;
//tempo per cui devo rimanere sveglio dopo l'ultimo comando
#define msec_alive_postcommand=600000;         
//tempo di deepsleep - microsecondi prima di svegliarsi nuovamente (MICROSECONDI)
#define usec_to_deepsleep=180e6; 


// ----- variabili di stato delle uscite pwm/ipwm-----
int out1_status=0;
int out2_status=0;
int out1_polarity=0;
int out1_old_polarity=0;
int out2_old_polarity=0;
int out2_polarity=0;
int out1_pwr=0;
int out2_pwr=0;

// ----- variabili di stato delle uscite digitali
int out3_status=0;
int out4_status=0;

// This will change output pin setup and use. Please verify cabling!
// default = false, populated by "Use Inverted PWM" checkbox configuration parameter 
bool use_inverted_pwm=false;

// ----- pinout for Use inverted PWM = FALSE configuration
// ----- ideal for 1x PWM  2x POLARITY h-bridge (eg: L239D)
#define  EN_34_PIN  12
#define  EN_12_PIN  14

#define out1_polarity_PIN1 5
#define out1_polarity_PIN2 4
#define out2_polarity_PIN1 1
#define out2_polarity_PIN2 3

// ----- pinout digital output
#define  out3_PIN  16
#define  out4_PIN  13


// ----- pinout for Use inverted PWM = TRUE configuration
// ----- ideal for 1x PWM  1x POLARITY h-bridge (eg: MAX1508)
#define out1_ipwm_PIN1 12
#define out1_ipwm_PIN2 14
//int out2_ipwm_PIN1 = 4;
//int out2_ipwm_PIN2 = 15;

// --- output timeout : at the end of these msecs the power will be 0
long  out1_timeout_val = 0;
long  out2_timeout_val = 0;
long  out3_timeout_val = 0;
long  out4_timeout_val = 0;

String status_topic="";
String command_topic="";

// json static buffer allocation
StaticJsonDocument<1000> jsonDoc;      //forse anche meno caratteri, ma lo tengo per implementazioni future.


void setup() 
{
  # ifdef _DEBUG_SERIAL  
    Serial.begin(115200);
    Serial.println();
    Serial.println("Starting up...");
  # endif
  iotWebConf.setStatusPin(STATUS_PIN);
  //iotWebConf.setConfigPin(CONFIG_PIN);
  iotWebConf.addSystemParameter(&mqttServerParam);
  iotWebConf.addSystemParameter(&mqttPortParam);
  iotWebConf.addSystemParameter(&mqttUserNameParam);
  iotWebConf.addSystemParameter(&mqttUserPasswordParam);
  iotWebConf.addSystemParameter(&useInvertedPwm);
  
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  
  
  //iotWebConf.setupUpdateServer(&httpUpdater);
  // -- Define how to handle updateServer calls.
  iotWebConf.setupUpdateServer(
    [](const char* updatePath) { httpUpdater.setup(&server, updatePath); },
    [](const char* userName, char* password) { httpUpdater.updateCredentials(userName, password); });

    
  // -- Initializing the configuration.
  boolean validConfig = iotWebConf.init();
  int mqtt_port_value=1883;
  if (!validConfig)
  {
    mqttServerValue[0] = '\0';
    mqttUserNameValue[0] = '\0';
    mqttUserPasswordValue[0] = '\0';
  }else{
    mqtt_port_value=atoi(mqttPortValue);
  }

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });

  //allow insecure wifi client
  net.setInsecure();


  //timeout=10, clean session=1, timeout=100
  //!!! il watchdog scatta a 6 secondi, devo tenermi sotto con il timeout altrimenti va in blocco e viene resettata
  //       con il messaggio del WDT
  mqttClient.setOptions(3, 0, 100);
  mqttClient.begin(mqttServerValue, mqtt_port_value, net);
  mqttClient.onMessage(mqttMessageReceived);


  // init internal boolean variable use pwm or use inverted pwm
  //use_inverted_pwm = useInvertedPwm.isChecked();
  if (invertedPwmParamValue[0] == 's') {
    use_inverted_pwm = true;
    publishToStatus("use_inverted_pwm = true;");
  }else{
    use_inverted_pwm = false;
    publishToStatus("use_inverted_pwm = false;");
  }
  
  
  #ifdef _DEEP_SLEEP
    last_cmd_running_time = millis();
  #endif

  // IF debugging via serial monitor we cannot use GPIO1 and GPIO3
  // IF debugging via serial monitor, we use also the integrated LED.
  #ifdef _DEBUG_SERIAL
    pinMode(integrated_led,OUTPUT);
    Serial.println("Serial debugging enabled - cannot use OUT2 : GPIO01+GPIO03 used by serial connection.");
    
    if (use_inverted_pwm){
      setupOutput1_ipwm();
    }else{
     setupOutput1_pwm();
    }
  
    Serial.println("Ready.");
  #else
    Serial.println("Ready - this is the last serial print."); 
    Serial.end();

    // Serial closed - GPIO1 and GPIO3 are now free to be used
    // we could use also OUTPUT2
    if (use_inverted_pwm){
      setupOutput1_ipwm();
      //setupOutput2_ipwm();      
    }else{
      setupOutput1_pwm();
      setupOutput2_pwm();
    }

    // setup digital output 3 and 4
    setupOutput34();

  #endif  

  // set strings after getting ThingName loaded from configuration.
  status_topic="/CloudBrick/BRC_ESP_";
  status_topic += iotWebConf.getThingName();
  status_topic += "/Status";


  //command_topic= "/CloudBrick/BRC_ESP_" + iotWebConf.getThingName() + "/Command";
  command_topic="/CloudBrick/BRC_ESP_";
  command_topic += iotWebConf.getThingName();
  command_topic += "/Command";
}


// setup pin for OUTPUT3 and OUTPUT4
void setupOutput34(){
    pinMode(out3_PIN, OUTPUT); 
    digitalWrite(out3_PIN,LOW);
    pinMode(out4_PIN, OUTPUT); 
    digitalWrite(out4_PIN,LOW);
}

// setup pin for OUTPUT1 
void setupOutput1_pwm(){
    
    pinMode(out1_polarity_PIN2, OUTPUT); 
    digitalWrite(out1_polarity_PIN2,LOW);
    pinMode(out1_polarity_PIN1, OUTPUT); 
    digitalWrite(out1_polarity_PIN1,LOW);

    pinMode(EN_12_PIN, OUTPUT); 
    analogWrite(EN_12_PIN,0);
  
}


// setup pin for OUTPUT2
void setupOutput2_pwm(){
    pinMode(out2_polarity_PIN1, OUTPUT); 
    digitalWrite(out2_polarity_PIN1,LOW);
    pinMode(out2_polarity_PIN2, OUTPUT); 
    digitalWrite(out2_polarity_PIN2,LOW);

    pinMode(EN_34_PIN, OUTPUT); 
    analogWrite(EN_34_PIN,0);  
}

void setupOutput1_ipwm(){
    pinMode( out1_ipwm_PIN1, OUTPUT); 
    pinMode(out1_ipwm_PIN2, OUTPUT); 

//    analogWrite(out1_ipwm_PIN1,0); 
//    analogWrite(out1_ipwm_PIN2,0); 
    digitalWrite(out1_ipwm_PIN1,LOW);
    digitalWrite(out1_ipwm_PIN2,LOW);
}

/*
void stopout1_pwm(){
  analogWrite(EN_12_PIN,0);
    digitalWrite(out1_polarity_PIN2,LOW);
    digitalWrite(out1_polarity_PIN1,LOW);
    
}

void stopout2_pwm(){
    analogWrite(EN_34_PIN,0);
    digitalWrite(out2_polarity_PIN2,LOW);
    digitalWrite(out2_polarity_PIN1,LOW);
    
}

void stopout1_ipwm(){
    digitalWrite(out1_ipwm_PIN1,LOW); 
    digitalWrite(out1_ipwm_PIN2,LOW); 
}
*/
/*
// setup OUTPUT2 for inverted PWM
void setupOutput2_ipwm(){
    pinMode(out2_ipwm_PIN1, OUTPUT); 
    pinMode(out2_ipwm_PIN2, OUTPUT); 

    analogWrite(out2_ipwm_PIN1,0);
    analogWrite(out2_ipwm_PIN2,0); 
}
*/

void loop() 
{
  // -- doLoop should be called as frequently as possible.
  iotWebConf.doLoop();

  
  mqttClient.loop();

  if (needMqttConnect)
  {
    if (connectMqtt())
    {
      needMqttConnect = false;
    }
  }
  else if ((iotWebConf.getState() == iotwebconf::OnLine) && (!mqttClient.connected()))
  {
    # ifdef _DEBUG_SERIAL
      Serial.println("MQTT reconnect");
    # endif
    connectMqtt();
  }

  if (needReset)
  {
    # ifdef _DEBUG_SERIAL
      Serial.println("Rebooting after 1 second.");
    # endif
    iotWebConf.delay(1000);
    ESP.restart();
  }


  unsigned long now = millis();

/* disabilito la lettura del config pin : io devo ricevere, non inviare.
 *  
  if ((500 < now - lastReport) && (pinState != digitalRead(CONFIG_PIN)))
  {
    pinState = 1 - pinState; // invert pin state as it is changed
    lastReport = now;
    Serial.print("Sending on MQTT channel '/test/status' :");
    Serial.println(pinState == LOW ? "ON" : "OFF");
    mqttClient.publish("/test/status", pinState == LOW ? "ON" : "OFF");
  }
*/
  //publishToStatus(String(use_inverted_pwm));
  
  if (use_inverted_pwm){
    setout1_ipwm();  
    //setout2_ipwm();
  }else{
    setout1_pwm();  
    setout2_pwm();    
  }

  setout34();
  
  // deepsleep : se il tempo dell'ultimo comando è > del timeout allora lo mando in deepsleep
  #ifdef _DEEP_SLEEP
    if (millis() > ( last_cmd_running_time + msec_alive_postcommand )) {
      ESP.deepSleep(usec_to_deepsleep);     
    }
  #endif

  if (out1_timeout_val>0 && out1_timeout_val<millis())
  {
    out1_timeout_val=0;
    out1_pwr=0;
    

    //publishToStatus("TIMEOUT O1");    
  }

  if (out2_timeout_val>0 && out2_timeout_val<millis())
  {
    out2_timeout_val=0;
    out2_pwr=0;
 
   // publishToStatus("TIMEOUT O2");  
  }


  if (out3_timeout_val>0 && out3_timeout_val<millis())
  {
    out3_timeout_val=0;
    out3_status=0;
    

    //publishToStatus("TIMEOUT O1");    
  }

  if (out4_timeout_val>0 && out4_timeout_val<millis())
  {
    out4_timeout_val=0;
    out4_status=0;
 
   // publishToStatus("TIMEOUT O2");  
  }  


}

/**
 * Handle web requests to "/" path.
 */
void handleRoot()
{
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>Cantylab CloudBrick</title></head><body><h2>CloudBrick</h2><br/>Configuration:";
  s += "<ul>";
  s += "<li>MQTT server: ";
  s += mqttServerValue;
  s += ":";
  s += atoi(mqttPortValue);
  s += "</li><li>";
  s += "MQTT command topic:";
  s += command_topic;
  s += "</li><li>";
  s += "MQTT status topic:";
  s += status_topic;
  s += "</li>";

  if (iotWebConf.getState() == iotwebconf::OnLine)
  {
    s += "<li>Wifi connected: TRUE</li>";
    String myIP = WiFi.localIP().toString();
    s += "<li>IP: "+ myIP+ "</li>";
  }else
  {
    s += "<li>Wifi connected: FALSE</li>";
  }

  if (mqttClient.connected())
  {
    s += "<li>MQTT connected: TRUE</li>";
  }else
  {
    s += "<li>MQTT connected: FALSE</li>";
  }
    
  s += "<li>";
  s += "Brick name: ";
  s += iotWebConf.getThingName();

  s += "<li>CheckBox selected: ";
  s += invertedPwmParamValue[0];
  
  s += "</li></ul>";
  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}

void wifiConnected()
{
  needMqttConnect = true;
}

void configSaved()
{
  # ifdef _DEBUG_SERIAL
    Serial.println("Configuration was updated.");
  # endif
  needReset = true;
}


bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper)
{
  # ifdef _DEBUG_SERIAL
  Serial.println("Validating form.");
  # endif
  bool valid = true;


  int l = webRequestWrapper->arg(mqttServerParam.getId()).length();
  if (l < 3)
  {
    mqttServerParam.errorMessage = "Please provide at least 3 characters for MQTT server!";
    valid = false;
  }

  return valid;
}


boolean connectMqtt() {
  unsigned long now = millis();
  if (1000 > now - lastMqttConnectionAttempt)
  {
    // Do not repeat within 1 sec.
    return false;
  }
  # ifdef _DEBUG_SERIAL
    Serial.println("Connecting to MQTT server...");
  # endif
  if (!connectMqttOptions()) {
    lastMqttConnectionAttempt = now;
    return false;
  }
  # ifdef _DEBUG_SERIAL
    Serial.println("Connected!");
  # endif


  # ifdef _DEBUG_SERIAL
    Serial.println("Subscribing to topic:");
    Serial.println(command_topic);
  # endif
  
  mqttClient.subscribe( command_topic );
  return true;
}

/*
// -- This is an alternative MQTT connection method.
boolean connectMqtt() {
  Serial.println("Connecting to MQTT server...");
  while (!connectMqttOptions()) {
    iotWebConf.delay(1000);
  }
  Serial.println("Connected!");

  mqttClient.subscribe("/test/action");
  return true;
}
*/

boolean connectMqttOptions()
{
  boolean result;
  if (mqttUserPasswordValue[0] != '\0')
  {
    result = mqttClient.connect(iotWebConf.getThingName(), mqttUserNameValue, mqttUserPasswordValue);
  }
  else if (mqttUserNameValue[0] != '\0')
  {
    result = mqttClient.connect(iotWebConf.getThingName(), mqttUserNameValue);
  }
  else
  {
    result = mqttClient.connect(iotWebConf.getThingName());
  }
  return result;
}

void mqttMessageReceived(String &topic, String &payload)
{
  # ifdef _DEBUG_SERIAL
    Serial.println("Incoming: " + topic + " - " + payload);
  # endif

  publishToStatus("JSON : received >>" + payload +"<<");

//  StaticJsonDocument<300> jsonDoc;      //forse anche meno caratteri, ma lo tengo per implementazioni future.
  auto error = deserializeJson(jsonDoc,payload); // jsonBuffer.parseObject(strPayload);
  String outString="";
  
  
  // se sono in debug con la seriale, flash del led integrato
  # ifdef _DEBUG_SERIAL
    digitalWrite(integrated_led,LOW);
  # endif
  
  // Test if parsing succeeds.
  if (jsonDoc.isNull()) {
    #ifdef _DEBUG_SERIAL 
      Serial.println("JSON : parseObject() failed");
    #endif
    publishToStatus("JSON : parseObject() failed");   
    return;
  }
  
  if (!jsonDoc["O1_dir"].isNull() ){
    out1_polarity = jsonDoc["O1_dir"];
    outString = "{\"O1_dir\":" + String(out1_polarity) +"}";
    # ifdef _DEBUG_SERIAL
      Serial.println("Received O1_dir TARGET STATUS : " + String(out1_polarity));
    # endif
  }

  if (!jsonDoc["O2_dir"].isNull() ){
    out2_polarity = jsonDoc["O2_dir"];
    outString = "{\"O2_dir\":" + String(out2_polarity) +"}";
    # ifdef _DEBUG_SERIAL
      Serial.println("Received O2_dir TARGET STATUS : " + String(out2_polarity));
    # endif
  }

  if (!jsonDoc["O1_pwr"].isNull() ){
    int out1_val = jsonDoc["O1_pwr"];
    out1_pwr = abs(out1_val);
    outString = "{\"O1_pwr\":" + String(out1_pwr) +"}";

    //se è negativo, inverto la marcia (considero dir=0 per avanti, dir=1 inverte)
    if (out1_val<0){
      out1_polarity=1;
    }
    
    # ifdef _DEBUG_SERIAL
      Serial.println("Received O1_pwr TARGET STATUS : " + String(out1_pwr));
    # endif
  }


  if (!jsonDoc["O2_pwr"].isNull() ){
    int out2_val = jsonDoc["O2_pwr"];
    out2_pwr = abs(out2_val);
    outString = "{\"O2_pwr\":" + String(out2_pwr) +"}";

    //se è negativo, inverto la marcia (considero dir=0 per avanti, dir=1 inverte)
    if (out2_val<0){
      out2_polarity=1;
    }
    
    # ifdef _DEBUG_SERIAL
      Serial.println("Received O2_pwr TARGET STATUS : " + String(out2_pwr));
    # endif
  }

  if (!jsonDoc["O3_status"].isNull() ){
    out3_status = jsonDoc["O3_status"];
    outString = "{\"O3_status\":" + String(out3_status) +"}";
    # ifdef _DEBUG_SERIAL
      Serial.println("Received O3_status TARGET STATUS : " + String(out3_status));
    # endif
  }  

  if (!jsonDoc["O4_status"].isNull() ){
    out4_status = jsonDoc["O4_status"];
    outString = "{\"O4_status\":" + String(out4_status) +"}";
    # ifdef _DEBUG_SERIAL
      Serial.println("Received O4_status TARGET STATUS : " + String(out4_status));
    # endif
  }  

  if (!jsonDoc["O1_timeout"].isNull() ){
    out1_timeout_val = jsonDoc["O1_timeout"];
    out1_timeout_val += millis();
    outString = "{\"O1_timeout\":" + String(out1_timeout_val) +"}";
    # ifdef _DEBUG_SERIAL
      Serial.println("Received O1_timeout TARGET STATUS : " + String(out1_timeout_val));
    # endif
    

  }  


  if (!jsonDoc["O2_timeout"].isNull() ){
    out2_timeout_val = jsonDoc["O2_timeout"];
    out2_timeout_val += millis();
    outString = "{\"O2_timeout\":" + String(out2_timeout_val) +"}";
    
    # ifdef _DEBUG_SERIAL
      Serial.println("Received O2_timeout TARGET STATUS : " + String(out2_timeout_val));
    # endif
    
  }

  if (!jsonDoc["O3_timeout"].isNull() ){
    out3_timeout_val = jsonDoc["O3_timeout"];
    out3_timeout_val += millis();
    outString = "{\"O3_timeout\":" + String(out3_timeout_val) +"}";
    
    # ifdef _DEBUG_SERIAL
      Serial.println("Received O3_timeout TARGET STATUS : " + String(out3_timeout_val));
    # endif
    
  }      

  if (!jsonDoc["O4_timeout"].isNull() ){
    out4_timeout_val = jsonDoc["O4_timeout"];
    out4_timeout_val += millis();
    outString = "{\"O4_timeout\":" + String(out4_timeout_val) +"}";
    
    # ifdef _DEBUG_SERIAL
      Serial.println("Received O4_timeout TARGET STATUS : " + String(out4_timeout_val));
    # endif
    
  }      
  
  if (outString.length()>0){

    # ifdef _DEBUG_SERIAL
      Serial.println(outString);
      Serial.println("publishing to topic:");
      Serial.println(topic);
    # endif

    publishToStatus(outString);
  }

  // resetto il tempo dell'ultimo comando ricevuto
  last_cmd_running_time = millis();

}

void publishToStatus(String message){
  //String outTopic="/CloudBrick/BRC_ESP_";
  //outTopic += iotWebConf.getThingName();
  //outTopic += "/Status";

  mqttClient.publish(status_topic, message);
}


void setout34(){
  String outString="";

  if (out3_status==0){
    //stop digital out 3
    digitalWrite(out3_PIN,LOW);
  }else{
    digitalWrite(out3_PIN,HIGH);
  }

  if (out4_status==0){
    //stop digital out 4
    digitalWrite(out4_PIN,LOW);
  }else{
    digitalWrite(out4_PIN,HIGH);
  }
}


// Set output parameter for OUTPUT1 for PWM mode
void setout1_pwm(){
  String outString="";



  // poi attivo il motore, nel caso cambi stato, per partire con la velocità desiderata
  if (out1_status==0){
    //stop motor 1
    digitalWrite(out1_polarity_PIN1,LOW);
    digitalWrite(out1_polarity_PIN2,LOW);    
  }else{
    // se devo invertire la polarità, prima fermo il motore
    if ( out1_polarity != out1_old_polarity ) {
      analogWrite(EN_12_PIN, 0);      
    }

    if (out1_polarity==0){
      //rotate CCW
      digitalWrite(out1_polarity_PIN1,LOW);
      digitalWrite(out1_polarity_PIN2,HIGH);          
      if ( out1_polarity != out1_old_polarity ) {
        outString = "{\"O1_dir\": \"ccw\" }";
        publishToStatus(outString);      
      }
    }else{
      //rotate CW
      digitalWrite(out1_polarity_PIN1,HIGH);
      digitalWrite(out1_polarity_PIN2,LOW);          
      if ( out1_polarity != out1_old_polarity ) {
        outString = "{\"O1_dir\": \"cw\" }";
        publishToStatus(outString);
      }
    }    

    out1_old_polarity = out1_polarity ;

    // per ultima setto la velocità
    analogWrite(EN_12_PIN, out1_pwr);
       
    //String outString = "{\"O1\": "+out1_status+" }";
    //outString = "{\"O1_pwr\": "+ String(out1_pwr)+" }";
    //publishToStatus(outString);
  
  }

  if (out1_pwr>0){
    out1_status=1;   // aggiorno la variabile temporanea indicando che il motore sta andando.
  }else {
    out1_status=0;
  }
}


// Set output parameter for OUTPUT1 for INVERTED PWM mode
void setout1_ipwm(){
  String outString="";

  String outStringDebug="setout1_ipwm();";
  
  // poi attivo il motore, nel caso cambi stato, per partire con la velocità desiderata
  if (out1_status==0){
    //stop motor 1
    digitalWrite(out1_ipwm_PIN1,LOW);
    digitalWrite(out1_ipwm_PIN2,LOW);    
    outStringDebug+="out1_status == 0; ";
  }else{
     outStringDebug+="out1_status != 0; ";
    // se devo invertire la polarità, prima fermo il motore
    if ( out1_polarity != out1_old_polarity ) {
      digitalWrite(out1_ipwm_PIN1,LOW);
      digitalWrite(out1_ipwm_PIN2,LOW);
      outStringDebug+="out1_polarity != out1_old_polarity ; ";
    }


    // prima setto la direzione
    if (out1_polarity==0){
      
      outStringDebug+="out1_polarity==0; ";

      //rotate CCW
      digitalWrite(out1_ipwm_PIN1, LOW);
      // set PWM output
      analogWrite(out1_ipwm_PIN2,out1_pwr);   
      
      outStringDebug+=out1_pwr;
      if ( out1_polarity != out1_old_polarity ) {          
        outString = "{\"O1_dir\": \"ipwm_ccw\" }";
        publishToStatus(outString);

        outStringDebug+="out1_polarity != out1_old_polarity; ";

      }
      out1_old_polarity = out1_polarity ;

    }else{
      outStringDebug+="out1_polarity!=0; ";

      //rotate CW
      digitalWrite(out1_ipwm_PIN2, LOW);
      // set PWM output
      analogWrite(out1_ipwm_PIN1, out1_pwr); 

      outStringDebug+=out1_pwr;
      if ( out1_polarity != out1_old_polarity ) {                
        outString = "{\"O1_dir\": \"ipwm_cw\" }";
        publishToStatus(outString);

        outStringDebug+="out1_polarity != out1_old_polarity; ";
      }
      out1_old_polarity = out1_polarity ;
    }  

  }

  if (out1_pwr>0){
    out1_status=1;   // aggiorno la variabile temporanea indicando che il motore sta andando.
  }else {
    out1_status=0;
  }

 // publishToStatus(outStringDebug);
}


// imposta i parametri per l'uscita del motore2
void setout2_pwm(){
  String outString="";


  
  // poi attivo il motore, nel caso cambi stato, per partire con la velocità desiderata
  if (out2_status==0){
    //stop motor 2
    digitalWrite(out2_polarity_PIN1,LOW);
    digitalWrite(out2_polarity_PIN2,LOW);    
  }else{
    // se devo invertire la polarità, prima fermo il motore
    if ( out2_polarity != out2_old_polarity ) {
      analogWrite(EN_34_PIN, 0);      
      
    }


    // prima setto la direzione
    if (out2_polarity==0){
      //rotate CW
      digitalWrite(out2_polarity_PIN1,LOW);
      digitalWrite(out2_polarity_PIN2,HIGH);
      if ( out2_polarity != out2_old_polarity ) {          
        outString = "{\"O2_dir\": \"ccw\" }";
        publishToStatus(outString);
      }
      
    }else{
      //rotate CCW
      digitalWrite(out2_polarity_PIN1,HIGH);
      digitalWrite(out2_polarity_PIN2,LOW);      
      if ( out2_polarity != out2_old_polarity ) {                
        outString = "{\"O2_pwr\": \"cw\" }";
        publishToStatus(outString);
      }
      
    }    

    out2_old_polarity = out2_polarity ;
    
    // set PWM output
    analogWrite(EN_34_PIN, out2_pwr);
      
  }

  if (out2_pwr>0){
    out2_status=1;   // aggiorno la variabile temporanea indicando che il motore sta andando.
  }else {
    out2_status=0;
  }
}

