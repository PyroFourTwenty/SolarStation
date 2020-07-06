#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WifiCredentials.h>
#include <EEPROM.h>
#include <Mux.h>
#include <ESP8266HTTPClient.h>

ESP8266WebServer server(80);
WifiCredentials credentials;
IPAddress ap_local_IP(192,168,1,1);
IPAddress ap_gateway(192,168,1,254);
IPAddress ap_subnet(255,255,255,0);
HTTPClient http;

Mux mux(D0,D1,D2,D3, A0);

double solarCurrent = 0;
double batteryCurrent = 0;
double loadCurrent = 0;

double solarVoltage = 0;
double batteryVoltage = 0;
double loadVoltage = 0;

int credentialsTimeoutInSeconds = 30;
bool accessPointMode = false;
bool credentialsHaveBeenSubmitted = false;
const char INDEX_HTML[] =
"<!DOCTYPE HTML>"
"<html>"
"<head>"
"<meta content=\"text/html; charset=ISO-8859-1\""
" http-equiv=\"content-type\">"
"<meta name = \"viewport\" content = \"width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0\">"
"<title>ESP8266 Air Quality</title>"
"<style>"
"\"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000;}\""
"</style>"
"</head>"
"<body>"
"<center>"
"<h1>ESP8266 Air Quality</h1>"
"<FORM action=\"/\" method=\"post\">"
"<P>"
"<label>SSID:&nbsp;</label>"
"<input maxlength=\"32\" name=\"ssid\"><br>"
"<label>Password:&nbsp;</label><input maxlength=\"63\" name=\"password\"><br>"
"<label>Target IP:&nbsp;</label><input maxlength=\"15\" name=\"targetIp\"><br>"
"<INPUT type=\"submit\" value=\"Send\"> <INPUT type=\"reset\">"
"</P>"
"</FORM>"
"</center>"
"</body>"
"</html>";

void readWifiCredentialsFromEeprom(){
  struct{
    char str[32] = "";
  } ssid;

  struct{
    char str[63] = "";
  } password;

  struct{
    char str[15] = "";
  } target;

  //int byteSize = sizeof(ssid.str) + sizeof(password.str) + sizeof(target.str);
  
  int addressPointer = 0;
  //Read ssid
  EEPROM.get(addressPointer, ssid);
  //Move pointer to the beginning of password
  addressPointer+=sizeof(ssid.str);
  //Read password
  EEPROM.get(addressPointer, password);
  //Move pointer to the beginning of address
  addressPointer+=sizeof(password.str);
  //Get target ip
  EEPROM.get(addressPointer, target);
  
  Serial.println("Credentials read from EEPROM: "+String(ssid.str) +", "+String(password.str)+", "+String(target.str));

  IPAddress addr;
  addr.fromString(target.str);
  //Setup WifiCredentials for easier use
  credentials.setup(ssid.str, password.str, addr);
  if(!credentials.ipIsSet()){
    String notset = "not set";
    credentials.setup(notset,notset,addr);
    Serial.println("Credentials are not valid");
  }else{
    Serial.println("Credentials setup");
    credentials.printWifiCredentials(); 
  }
}

bool timeout(long startTimeInMillis){
  unsigned long millisToSecondFactor = 1000;
  return ((millis()-startTimeInMillis)>credentialsTimeoutInSeconds*millisToSecondFactor)?true:false;
}

void invalidateCredentials(){
  credentials.setup("invalid", "invalid", NULL);
}

void writeWifiCredentialsToEeprom(){
  
  struct{
    char str[32];
  } ssid;

  struct{
    char str[63];
  } password;

  struct{
    char str[15];
  } target;

  credentials.getSsid().toCharArray(ssid.str, sizeof(ssid.str));
  credentials.getPassword().toCharArray(password.str, sizeof(password.str));
  credentials.getTargetIpString().toCharArray(target.str, sizeof(target.str));
  
  int addressPointer = 0;
  EEPROM.put(addressPointer, ssid);
  addressPointer+=sizeof(ssid.str);
  EEPROM.put(addressPointer, password.str);
  addressPointer+=sizeof(password.str);
  EEPROM.put(addressPointer, target.str);
  
  EEPROM.commit();
  Serial.println("New credentials saved to EEPROM, "+String(ssid.str) +", "+String(password.str)+", "+String(target.str));


}
void startSoftAp(){
  Serial.print("Configuring access point...");
	Serial.print("Setting soft-AP configuration ... ");
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ap_local_IP, ap_gateway, ap_subnet);
  Serial.print("Setting soft-AP ... ");
  Serial.print("Soft-AP name = ");
  Serial.println(WiFi.macAddress());
  WiFi.softAP(WiFi.macAddress());
  accessPointMode = true;
}
void saveSettingsAndReboot(){
  String response;
  if(credentialsHaveBeenSubmitted){
    writeWifiCredentialsToEeprom();
    response="<p>Rebooting";
    server.send(200, "text/html", response);
    server.close();
    WiFi.softAPdisconnect(true);
    Serial.println("Rebooting");
    ESP.restart();
  }else{
    response="<p>Rejecting because no arguments were provided.";
    server.send(200, "text/html", response);
    Serial.println("Rejecting attempt to save without entering arguments");
  }
}

void handleSubmit(){
  IPAddress addr;
  addr.fromString(server.arg("targetIp"));
  credentials.setup(server.arg("ssid"),server.arg("password"), addr);
  String response="<p>The ssid is: ";
  response += server.arg("ssid");
  response +="<br>";
  response +="The password is: ";
  response +=server.arg("password");
  response +="<br>";
  response +="The target IP address is: ";
  response +=server.arg("targetIp");
  response +="</P><BR>";
  response +="<H2><a href=\"/\">Reenter settings</a></H2><br>";
  if(credentials.ipIsSet()){
    response +="<H2><a href=\"/saveSettingsAndReboot\">Save settings and reboot</a></H2><br>";
  }
  server.send(200, "text/html", response);
}

void handleRoot() {
   if (server.hasArg("ssid")&& server.hasArg("password")&& server.hasArg("targetIp") ) {//If all form fields contain data, call handleSubmit()
    credentialsHaveBeenSubmitted = true;
    handleSubmit();
  }
  else {//Redisplay the form
    server.send(200, "text/html", INDEX_HTML);
  }
}
//Shows when the web server gets a misformed or wrong request 
void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  message +="<H2><a href=\"/\">Enter settings</a></H2><br>";
  server.send(404, "text/html", message);
}

void configureWebserver(){
  //Configuring the web server
  Serial.println("Configuring webserver");
	server.on("/", handleRoot);
  server.on("/saveSettingsAndReboot", saveSettingsAndReboot);
  server.onNotFound(handleNotFound);
	server.begin();
	Serial.println("HTTP server started");
}

void setup() {
  Serial.begin(9600);
  EEPROM.begin(110);
  delay(300);
  Serial.println();
  Serial.println();
  readWifiCredentialsFromEeprom();
  long startTime = millis();
  if(credentials.ipIsSet()){ 
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED && !timeout(startTime)) {
    delay(500);
    Serial.print(".");
    }
    if(WiFi.isConnected()){
      Serial.println("connected");
    }
    if(!WiFi.isConnected() && timeout(startTime)){ //when ESP couldn't connect in time, reset credentials to enable accessPointMode and reboot
      Serial.println("Could not connect to Wifi "+credentials.getSsid()+", invalidating credentials and rebooting to AP mode");
      invalidateCredentials();
      writeWifiCredentialsToEeprom();
      ESP.restart();
    }
  }
  else{
    //Start soft AP
    startSoftAp();
    configureWebserver();

  }
}

double measureCurrent (int channel, int repeat){
  double sensorVoltage=3.285;
  double voltageDifferencePerAmp = sensorVoltage/60;
  double voltageCorrectionFactor = 1.050922779;
  double currentCorrectionFactor = 1.165918383; 
  double offset = 30.0;
  double current = 0;

  for(int i = 0; i < repeat; i++){
    int pinReading = mux.read(channel); 
    double measuredVoltage = ((pinReading/1024.0)*3.3)/voltageCorrectionFactor;
    current += ((measuredVoltage/voltageDifferencePerAmp)-offset)*currentCorrectionFactor;
  }

  return current/repeat; 
  
}

double measureVoltage (int channel, int repeat){
  float vOUT = 0.0;
  int value = 0;
  float R1 = 30000.0;
  float R2 = 7455.0;
  double correctionfactor = 1.050922779;
  double voltage = 0;
  for (int i= 0; i<repeat; i++){
    value = mux.read(channel);
    vOUT = ((value * 3.33 ) / 1024.0)/correctionfactor;
    voltage += vOUT / (R2/(R1+R2));
    delay(10);
  }
  voltage = voltage/repeat;
  return voltage;
}

void measureAndPost(){
  int repeat = 100 ;
  solarCurrent = measureCurrent(1, repeat);
  batteryCurrent = measureCurrent(3, repeat);
  loadCurrent = measureCurrent(5, repeat);

  solarVoltage = measureVoltage(0, repeat);
  batteryVoltage = measureVoltage(2, repeat);
  loadVoltage = measureVoltage(4, repeat);
  String url = "http://";
  url += credentials.getTargetIpString();
  url+=":";
  url+="5000/";
  url+="measurement/";
  url+= String(solarVoltage,2);
  url+=",";
  url+= String(solarCurrent,2);
  url+=",";
  url+= String(batteryVoltage,2);
  url+=",";
  url+= String(batteryCurrent,2);
  url+=",";
  url+= String(loadVoltage,2);
  url+=",";
  url+= String(loadCurrent,2);
  //Serial.println(url);
  http.begin(url);
  Serial.println(http.GET());
  http.end();
  
}

void loop() {
  if(accessPointMode){
    server.handleClient();
  }else{    
    measureAndPost();
    delay(5000);
  }
}