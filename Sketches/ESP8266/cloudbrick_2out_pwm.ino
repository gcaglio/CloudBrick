/**
 * IotWebConf06MqttApp.ino -- IotWebConf is an ESP8266/ESP32
 * Example: MQTT Demo Application
 * 
 * Description:
 *   All IotWebConf specific aspects of this example are described in
 *   previous examples, so please get familiar with IotWebConf before
 *   starting this example. So nothing new will be explained here, 
 *   but a complete demo application will be build.
 *   It is also expected from the reader to have a basic knowledge over
 *   MQTT to understand this code.
 *   
 *   This example starts an MQTT client with the configured
 *   connection settings.
 *   Will post the status changes of the D2 pin in channel "/test/status".
 *   Receives messages appears in channel "/test/action", and writes them to serial.
 *   This example also provides the firmware update option.
 *   (See previous examples for more details!)
 * 
 * Software setup for this example:
 *   This example utilizes Joel Gaehwiler's MQTT library.
 *   https://github.com/256dpi/arduino-mqtt
 * 
 * Hardware setup for this example:
 *   - An LED is attached to LED_BUILTIN pin with setup On=LOW.
 *   - [Optional] A push button is attached to pin D2, the other leg of the
 *     button should be attached to GND.
 */

#include <MQTT.h>
#include <IotWebConf.h>
#include <ArduinoJson.h>

//d e f i ne _DEEP_SLEEP

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "myBrickThing";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "mybrickthing01";

#define STRING_LEN 128

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "out2-pwm_v2"

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN

// -- Callback method declarations.
void wifiConnected();
void configSaved();
boolean formValidator();
void mqttMessageReceived(String &topic, String &payload);

DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;
WiFiClient net;
MQTTClient mqttClient;

char mqttServerValue[STRING_LEN];
char mqttUserNameValue[STRING_LEN];
char mqttUserPasswordValue[STRING_LEN];

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

// custom parameters in setup
IotWebConfParameter mqttServerParam = IotWebConfParameter("MQTT server", "mqttServer", mqttServerValue, STRING_LEN);
IotWebConfParameter mqttUserNameParam = IotWebConfParameter("MQTT user", "mqttUser", mqttUserNameValue, STRING_LEN);
IotWebConfParameter mqttUserPasswordParam = IotWebConfParameter("MQTT password", "mqttPass", mqttUserPasswordValue, STRING_LEN, "password");

boolean needMqttConnect = false;
boolean needReset = false;
//int pinState = HIGH;
//unsigned long lastReport = 0;
unsigned long lastMqttConnectionAttempt = 0;

// ------ DEEPSLEEP contol ------
//ultimo millis a cui ho ricevuto un command
unsigned long last_cmd_running_time = 0;
//tempo per cui devo rimanere sveglio dopo l'ultimo comando
int msec_alive_postcommand=600000;         
//tempo di deepsleep - microsecondi prima di svegliarsi nuovamente (MICROSECONDI)
const int usec_to_deepsleep=180e6; 


// ----- variabili di stato delle uscite -----
int out1_status=0;
int out2_status=0;
int out1_polarity=0;
int out1_old_polarity=0;
int out2_old_polarity=0;
int out2_polarity=0;
int out1_pwr=0;
int out2_pwr=0;

// ----- pin verso L239D -----
int EN_34_PIN = 12;
int EN_12_PIN = 14;

int out1_polarity_PIN1 =5;
int out1_polarity_PIN2 =4;
int out2_polarity_PIN1 =1;
int out2_polarity_PIN2 =3;



void setup() 
{
  # ifdef _DEBUG_SERIAL  
    Serial.begin(115200);
    Serial.println();
    Serial.println("Starting up...");
  # endif
  iotWebConf.setStatusPin(STATUS_PIN);
  //iotWebConf.setConfigPin(CONFIG_PIN);
  iotWebConf.addParameter(&mqttServerParam);
  iotWebConf.addParameter(&mqttUserNameParam);
  iotWebConf.addParameter(&mqttUserPasswordParam);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  iotWebConf.setupUpdateServer(&httpUpdater);

  // -- Initializing the configuration.
  boolean validConfig = iotWebConf.init();
  if (!validConfig)
  {
    mqttServerValue[0] = '\0';
    mqttUserNameValue[0] = '\0';
    mqttUserPasswordValue[0] = '\0';
  }

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });

  //timeout=10, clean session=1, timeout=100
  //!!! il watchdog scatta a 6 secondi, devo tenermi sotto con il timeout altrimenti va in blocco e viene resettata
  //       con il messaggio del WDT
  mqttClient.setOptions(3, 0, 100);
  mqttClient.begin(mqttServerValue, net);
  mqttClient.onMessage(mqttMessageReceived);

  #ifdef _DEEP_SLEEP
    last_cmd_running_time = millis();
  #endif

  // se sono in debug con la seriale, abilito il led in output
  // altrimenti abilito i pin
  #ifdef _DEBUG_SERIAL
    pinMode(integrated_led,OUTPUT);
    Serial.println("Impossibile utilizzare le uscite GPIO01 e GPIO03, usate dalla seriale.");
    
    // sarà attivo solo il motore 1
    pinMode(out1_polarity_PIN2, OUTPUT); 
    digitalWrite(out1_polarity_PIN2,LOW);
    pinMode(out1_polarity_PIN1, OUTPUT); 
    digitalWrite(out1_polarity_PIN1,LOW);

    pinMode(EN_12_PIN, OUTPUT); 
    analogWrite(EN_12_PIN,0);
    Serial.println("Ready.");
  #else
    Serial.println("Ready - this is the last serial print."); 
    Serial.end();

    //mi si sono liberate 2 GPIO, attivo 2 uscite.
    
    pinMode(out1_polarity_PIN2, OUTPUT); 
    digitalWrite(out1_polarity_PIN2,LOW);
    pinMode(out1_polarity_PIN1, OUTPUT); 
    digitalWrite(out1_polarity_PIN1,LOW);

    pinMode(out2_polarity_PIN1, OUTPUT); 
    digitalWrite(out2_polarity_PIN1,LOW);
    pinMode(out2_polarity_PIN2, OUTPUT); 
    digitalWrite(out2_polarity_PIN2,LOW);

    pinMode(EN_34_PIN, OUTPUT); 
    analogWrite(EN_34_PIN,0);

    pinMode(EN_12_PIN, OUTPUT); 
    analogWrite(EN_12_PIN,0);

  #endif  



}

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
  else if ((iotWebConf.getState() == IOTWEBCONF_STATE_ONLINE) && (!mqttClient.connected()))
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

  setout1();  
  setout2();
  
  // deepsleep : se il tempo dell'ultimo comando è > del timeout allora lo mando in deepsleep
  #ifdef _DEEP_SLEEP
    if (millis() > ( last_cmd_running_time + msec_alive_postcommand )) {
      ESP.deepSleep(usec_to_deepsleep);     
    }
  #endif


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
  s += "<title>Cantylab CloudBrick</title></head><body>Informations";
  s += "<ul>";
  s += "<li>MQTT server: ";
  s += mqttServerValue;
  s += "</li><li>";
  s += "brick name: ";
  s += iotWebConf.getThingName();
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

boolean formValidator()
{
  # ifdef _DEBUG_SERIAL
    Serial.println("Validating form.");
  # endif
  boolean valid = true;

  int l = server.arg(mqttServerParam.getId()).length();
  if (l < 3)
  {
    mqttServerParam.errorMessage = "Please provide at least 3 characters!";
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
  String topic="/CloudBrick/BRC_ESP_";
  topic += iotWebConf.getThingName();
  topic += "/Command";

  # ifdef _DEBUG_SERIAL
    Serial.println("Subscribing to topic:");
    Serial.println(topic);
  # endif
  
  mqttClient.subscribe( topic );
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

  StaticJsonDocument<100> jsonDoc;      //forse anche meno caratteri, ma lo tengo per implementazioni future.
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
  String outTopic="/CloudBrick/BRC_ESP_";
  outTopic += iotWebConf.getThingName();
  outTopic += "/Status";

  mqttClient.publish(outTopic, message);
}

// imposta i parametri per l'uscita del motore1
void setout1(){
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
      //rotate CW
      digitalWrite(out1_polarity_PIN1,LOW);
      digitalWrite(out1_polarity_PIN2,HIGH);          
      if ( out1_polarity != out1_old_polarity ) {
        outString = "{\"O1_dir\": \"ccw\" }";
        publishToStatus(outString);      
      }
    }else{
      //rotate CCW
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




// imposta i parametri per l'uscita del motore1
void setout2(){
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
    
    // poi setto la velocità
    analogWrite(EN_34_PIN, out2_pwr);
      
    //start motor 2
 //   outString = "{\"O2_pwr\": "+ String(out2_pwr)+" }";
 //   publishToStatus( outString);
  
  }

  if (out2_pwr>0){
    out2_status=1;   // aggiorno la variabile temporanea indicando che il motore sta andando.
  }else {
    out2_status=0;
  }
}
