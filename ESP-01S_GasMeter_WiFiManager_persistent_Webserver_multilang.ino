/****************************************************************
 * 
 * Code for ESP8266 ESP-01S at GasMeter
 * created: Patrick Kanzler using ChatGPT
 * V2.19:  2026-02-05
 * Original: https://www.kabza.de/MyHome/GasMeter/GasMeter.php
 * Changes since Version 2.12: 
 * GPIO Pins swapped because ESP-01S LED is on GPIO2
 * Subscribe Topic changed to ESP8266_GasMeter/value/set
 * Home Assistant autodiscovery feature added
 * ArduinoOTA Password: admin
 * Webserver added
 * Value update through Home Assistant and Webserver
 * WiFiManager added to better connect to WiFi Routers
 * Value is now also saved on device in case of power loss
 * Nonblocking LED handling
 * Switch automatically between German and English Language
 * Time, Ver, MAC, SSID, IP, Started, and OpHrs is not send anymore
 * Removed serial functionality
 * 
****************************************************************/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

/* ================== Hardware ================== */
#define LED_PIN 2
#define IN_PIN 0
#define LED_ON LOW
#define LED_OFF HIGH

/* ================== Device ================== */
String DEVICENAME = "ESP8266_GasMeter";
String CODEVERSION = "V2.19 2026-02-02";

/* ================== Globals ================== */
ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long value = 0;   // value *100
int actual = 1, prev = 1;

/* ================== LED ================== */
bool ledOn = false;
unsigned long ledOffAt = 0, ledHeartbeatAt = 0;

void ledPulse(unsigned long ms){
  digitalWrite(LED_PIN,LED_ON);
  ledOn=true;
  ledOffAt=millis()+ms;
}

void updateLED(){
  unsigned long now=millis();
  if(ledOn && now>=ledOffAt){
    digitalWrite(LED_PIN,LED_OFF);
    ledOn=false;
  }
  if(!ledOn && now-ledHeartbeatAt>=2000){
    ledPulse(25);
    ledHeartbeatAt=now;
  }
}

/* ================== Config ================== */
struct {
  String mqtt_host;
  String mqtt_user;
  String mqtt_pass;
  uint16_t mqtt_port = 1883;
} cfg;

/* ================== Flash handling ================== */
bool valueDirty = false;
unsigned long lastValueSave = 0;

void markValueDirty(){
  valueDirty = true;
}

void saveValue(){
  File f = LittleFS.open("/value.txt","w");
  if(!f) return;
  f.println(value);
  f.close();
}

void loadValue(){
  if(!LittleFS.exists("/value.txt")) return;
  File f = LittleFS.open("/value.txt","r");
  if(!f) return;
  value = f.readStringUntil('\n').toInt();
  f.close();
}

void saveConfig(){
  File f = LittleFS.open("/config.txt","w");
  if(!f) return;
  f.println(cfg.mqtt_host);
  f.println(cfg.mqtt_user);
  f.println(cfg.mqtt_pass);
  f.println(cfg.mqtt_port);
  f.close();
}

void loadConfig(){
  if(!LittleFS.exists("/config.txt")) return;
  File f = LittleFS.open("/config.txt","r");
  if(!f) return;
  cfg.mqtt_host = f.readStringUntil('\n'); cfg.mqtt_host.trim();
  cfg.mqtt_user = f.readStringUntil('\n'); cfg.mqtt_user.trim();
  cfg.mqtt_pass = f.readStringUntil('\n'); cfg.mqtt_pass.trim();
  cfg.mqtt_port = f.readStringUntil('\n').toInt();
  f.close();
  if(cfg.mqtt_port==0) cfg.mqtt_port=1883;
}

/* ================== MQTT Topics ================== */
String topicValue    = DEVICENAME + "/value";
String topicValueSet = DEVICENAME + "/value/set";
String topicStatus   = DEVICENAME + "/status";
String topicHAValue  = "homeassistant/sensor/"+DEVICENAME+"/config";
String topicHANumber = "homeassistant/number/"+DEVICENAME+"/config";

/* ================== HA Autodiscovery ================== */
void publishHAConfig(){
  String ip = WiFi.localIP().toString();

  String payloadS = "{";
  payloadS += "\"name\":\"" + DEVICENAME + "\",";
  payloadS += "\"uniq_id\":\"" + DEVICENAME + "_Value\",";
  payloadS += "\"stat_t\":\"" + topicValue + "\",";
  payloadS += "\"unit_of_meas\":\"m³\",";
  payloadS += "\"dev_cla\":\"gas\",";
  payloadS += "\"stat_cla\":\"total_increasing\",";
  payloadS += "\"dev\":{";
  payloadS += "\"ids\":[\"" + DEVICENAME + "\"],";
  payloadS += "\"mf\":\"Custom\",";
  payloadS += "\"mdl\":\"ESP-01S GasMeter\",";
  payloadS += "\"name\":\"" + DEVICENAME + "\",";
  payloadS += "\"sw\":\"" + CODEVERSION + "\",";
  payloadS += "\"cu\":\"http://" + ip + "/\"";
  payloadS += "}}";
  client.publish(topicHAValue.c_str(), payloadS.c_str(), true);

  String payloadN = "{";
  payloadN += "\"name\":\"" + DEVICENAME + " Corrector\",";
  payloadN += "\"uniq_id\":\"" + DEVICENAME + "_Correct\",";
  payloadN += "\"cmd_t\":\"" + topicValueSet + "\",";
  payloadN += "\"min\":0,\"max\":100000,\"step\":0.01,";
  payloadN += "\"dev\":{";
  payloadN += "\"ids\":[\"" + DEVICENAME + "\"],";
  payloadN += "\"mf\":\"Custom\",";
  payloadN += "\"mdl\":\"ESP-01S GasMeter\",";
  payloadN += "\"name\":\"" + DEVICENAME + "\",";
  payloadN += "\"sw\":\"" + CODEVERSION + "\",";
  payloadN += "\"cu\":\"http://" + ip + "/\"";
  payloadN += "}}";
  client.publish(topicHANumber.c_str(), payloadN.c_str(), true);
}

/* ================== MQTT ================== */
unsigned long lastMQTTtry = 0;

void mqttCallback(char* topic, byte* payload, unsigned int length){
  String t(topic), p;
  for(unsigned int i=0;i<length;i++) p+=(char)payload[i];
  float newVal = p.toFloat();
  float cur = value/100.0;
  if(t==topicValueSet){ //&& newVal>cur){
    value = (unsigned long)(newVal*100);
    markValueDirty();
    client.publish(topicValue.c_str(),String(newVal,2).c_str(),true);
  }
}

void mqttHandle(){
  if(client.connected()){
    client.loop();
    return;
  }
  if(millis()-lastMQTTtry < 5000) return;
  lastMQTTtry = millis();
  if(client.connect(
      DEVICENAME.c_str(),
      cfg.mqtt_user.c_str(),
      cfg.mqtt_pass.c_str(),
      topicStatus.c_str(),1,true,"offline")){
    client.publish(topicStatus.c_str(),"online",true);
    publishHAConfig();
    client.subscribe(topicValueSet.c_str());
  }
}

/* ================== Language ================== */
String getLang(){
  if(!server.hasHeader("Accept-Language")) return "en";
  String l = server.header("Accept-Language");
  l.toLowerCase();
  return l.startsWith("de") ? "de" : "en";
}

/* ================== Web ================== */
void pageRoot(){
  String lang = getLang();
  String html;
  if(lang=="de"){
    html="<h2>GasMeter ESP-01S</h2><p>IP: "+WiFi.localIP().toString()+"</p><ul>";
    html+="<li><a href='/value'>Z&auml;hlerstand anzeigen/korrigieren</a></li>";
    html+="<li><a href='/mqtt'>MQTT konfigurieren</a></li>";
    html+="<li><a href='/wifi'>WLAN neu konfigurieren</a></li>";
    html+="<li><a href='/reset'>Reset</a></li></ul>";
  } else {
    html="<h2>GasMeter ESP-01S</h2><p>IP: "+WiFi.localIP().toString()+"</p><ul>";
    html+="<li><a href='/value'>Show/Correct Meter Value</a></li>";
    html+="<li><a href='/mqtt'>Configure MQTT</a></li>";
    html+="<li><a href='/wifi'>Reconfigure WiFi</a></li>";
    html+="<li><a href='/reset'>Reset</a></li></ul>";
  }
  server.send(200,"text/html",html);
}

void pageValue(){
  String lang = getLang();
  float cur = value/100.0;
  String html;
  if(lang=="de"){
    html="<h2>GasMeter Z&auml;hlerstand</h2>";
    html+="<p>Aktueller Z&auml;hlerstand: "+String(cur,2)+" m&sup3;</p>";
    html+="<form method='POST'>Neuer Wert (nur erh&ouml;hen): ";
    html+="<input name='newval' type='number' step='0.01'>";
    html+="<input type='submit' value='Korrigieren'></form>";
  } else {
    html="<h2>GasMeter Meter Value</h2>";
    html+="<p>Current value: "+String(cur,2)+" m&sup3;</p>";
    html+="<form method='POST'>New value (increase only): ";
    html+="<input name='newval' type='number' step='0.01'>";
    html+="<input type='submit' value='Apply'></form>";
  }

  if(server.method()==HTTP_POST){
    float nv = server.arg("newval").toFloat();
    //if(nv>cur){
      value = (unsigned long)(nv*100);
      markValueDirty();
      client.publish(topicValue.c_str(),String(nv,2).c_str(),true);
      html += (lang=="de") ?
        "<p style='color:green'>Z&auml;hlerstand erh&ouml;ht</p>" :
        "<p style='color:green'>Value increased</p>";
    //}
  }
  server.send(200,"text/html",html);
}

void pageMQTT(){
  String lang = getLang();
  if(server.method()==HTTP_POST){
    cfg.mqtt_host=server.arg("host");
    cfg.mqtt_user=server.arg("user");
    cfg.mqtt_pass=server.arg("pass");
    cfg.mqtt_port=server.arg("port").toInt();
    saveConfig();
    server.send(200,"text/html",(lang=="de")?
      "<p>MQTT gespeichert. Neustart</p>":
      "<p>MQTT saved. Restart</p>");
    delay(1000);
    ESP.restart();
  }
  String html;
  if(lang=="de"){
    html="<h2>MQTT Setup</h2><form method='POST'>";
    html+="Broker:<br><input name='host' value='"+cfg.mqtt_host+"'><br>";
    html+="Port:<br><input name='port' value='"+String(cfg.mqtt_port)+"'><br>";
    html+="User:<br><input name='user' value='"+cfg.mqtt_user+"'><br>";
    html+="Pass:<br><input name='pass' type='password'><br><br>";
    html+="<input type='submit' value='Speichern'></form>";
  } else {
    html="<h2>MQTT Setup</h2><form method='POST'>";
    html+="Broker:<br><input name='host' value='"+cfg.mqtt_host+"'><br>";
    html+="Port:<br><input name='port' value='"+String(cfg.mqtt_port)+"'><br>";
    html+="User:<br><input name='user' value='"+cfg.mqtt_user+"'><br>";
    html+="Pass:<br><input name='pass' type='password'><br><br>";
    html+="<input type='submit' value='Save'></form>";
  }
  server.send(200,"text/html",html);
}

void pageWiFi(){
  server.send(200,"text/html","<p>WLAN Setup startet</p>");
  delay(500);
  WiFiManager wm;
  wm.startConfigPortal("GasMeter-Setup");
}

void pageReset(){
  saveValue();
  LittleFS.remove("/config.txt");
  server.send(200,"text/html","<p>Reset</p>");
  delay(500);
  ESP.restart();
}

/* ================== Setup ================== */
void setup(){
  pinMode(LED_PIN,OUTPUT);
  digitalWrite(LED_PIN,LED_OFF);
  pinMode(IN_PIN,INPUT_PULLUP);

  Serial.begin(115200);
  LittleFS.begin();

  loadValue();
  loadConfig();

  WiFiManager wm;
  wm.setDebugOutput(false);
  if(!wm.autoConnect("GasMeter-Setup")) ESP.restart();

  server.on("/",pageRoot);
  server.on("/value",pageValue);
  server.on("/mqtt",pageMQTT);
  server.on("/wifi",pageWiFi);
  server.on("/reset",pageReset);
  server.collectHeaders("Accept-Language");
  server.begin();

  client.setServer(cfg.mqtt_host.c_str(),cfg.mqtt_port);
  client.setCallback(mqttCallback);
  client.setBufferSize(1024);

  ArduinoOTA.setHostname(DEVICENAME.c_str());
  ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3"); // admin

  ArduinoOTA.begin();
}

/* ================== Loop ================== */
void loop(){
  ArduinoOTA.handle();
  server.handleClient();
  updateLED();
  mqttHandle();

  actual = digitalRead(IN_PIN);
  if(actual==LOW && prev==HIGH){
    value++;
    markValueDirty();
    ledPulse(250);
  }
  prev = actual;

  if(valueDirty && millis()-lastValueSave > 60000){
    saveValue();
    valueDirty = false;
    lastValueSave = millis();
  }

  static unsigned long prevMQTT=0;
  if(millis()-prevMQTT>=10000){
    if(client.connected()){
      client.publish(topicValue.c_str(),
        String(value/100.0,2).c_str(),true);
    }
    prevMQTT = millis();
  }
}
