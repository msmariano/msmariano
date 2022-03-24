#include <Arduino.h>
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "ArduinoJson.h"
#include <FS.h>
#include <SPIFFS.h>
#include <NTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ArduinoHttpClient.h>


AsyncWebServer server(80);
StaticJsonDocument<1000> config;
StaticJsonDocument<1000> conector;
StaticJsonDocument<1000> buttonIOTs;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600, 60000);
String nomeDevice = "NeuverseESP32";

void trataInterrupcao(){
  for(int i=0;i<buttonIOTs.size();i++) {
    switch(digitalRead(buttonIOTs[i]["gpioNumControle"])){
      case HIGH:        
        if(buttonIOTs[i]["funcao"] == "HOLD") {
          buttonIOTs[i]["status"] = "ON";
          digitalWrite(buttonIOTs[i]["gpioNum"],HIGH);
        }
        else if (buttonIOTs[i]["funcao"] == "PUSH") {
          if(buttonIOTs[i]["tecla"] != "HIGH") {
            buttonIOTs[i]["status"] == "ON" ?  digitalWrite(buttonIOTs[i]["gpioNum"],LOW) :  digitalWrite(buttonIOTs[i]["gpioNum"],HIGH);
            buttonIOTs[i]["status"] == "ON" ?  buttonIOTs[i]["status"] = "OFF" :  buttonIOTs[i]["status"] = "ON";
          }
        }
        buttonIOTs[i]["tecla"] = "HIGH";
        break;
      case LOW:                
        if(buttonIOTs[i]["funcao"] == "HOLD") {
          buttonIOTs[i]["status"] = "OFF";
          digitalWrite(buttonIOTs[i]["gpioNum"],LOW);
        }
        else if (buttonIOTs[i]["funcao"] == "PUSH") {
          if(buttonIOTs[i]["tecla"] != "LOW") {
            buttonIOTs[i]["status"] == "ON" ?  digitalWrite(buttonIOTs[i]["gpioNum"],LOW) :  digitalWrite(buttonIOTs[i]["gpioNum"],HIGH);
            buttonIOTs[i]["status"] == "ON" ?  buttonIOTs[i]["status"] = "OFF" :  buttonIOTs[i]["status"] = "ON";
          }
        }
        buttonIOTs[i]["tecla"] = "LOW";
        break;
    }
  }
}

void log(String mens) {
  Serial.println(mens);
}

void taskPlugOnAlive(void *arg) {
  while(1) {
    if(WiFi.localIP().toString() != "0.0.0.0") {
      String jSon = "";
      conector["id"] = 0;
      conector["nome"] = nomeDevice+"Conector";
      conector["usuario"] = config["conectorSessao"]["usuario"];
      conector["senha"]   = config["conectorSessao"]["senha"];
      conector["ip"] = WiFi.localIP().toString();
      conector["iot"]["id"] = 0;
      conector["iot"]["name"] = nomeDevice+"IOT";
      conector["iot"]["ip"] = WiFi.localIP().toString();
      conector["iot"]["tipoIOT"] = "CONTROLEREMOTO";    
      String jSonBts;
      serializeJson(buttonIOTs,jSonBts);
      log("Bts:"+jSonBts);
      conector["iot"]["jSon"] = jSonBts;
      conector["status"] = "LOGIN";
      serializeJson(conector, jSon);
      WiFiClient wifi;
      String ip = config["servidorRestSessao"]["ip"];
      int porta = config["servidorRestSessao"]["porta"];
      HttpClient client = HttpClient(wifi, ip.c_str(), porta);
      log("iniciando requisição plugon...");
      client.beginRequest();
      if(client.post("/ServidorIOT/plugon")==0){
        log("Enviando requisição...");
        client.sendHeader(HTTP_HEADER_CONTENT_TYPE, "application/x-www-form-urlencoded");
        client.sendHeader(HTTP_HEADER_CONTENT_LENGTH, jSon.length());        
        client.endRequest();
        client.write((const byte*)jSon.c_str(), jSon.length());
        if(client.getWriteError()==0){
          int statusCode = client.responseStatusCode();
          log(statusCode);
          String response = client.responseBody();
          deserializeJson(conector,response);
          log(response);
        }
        else
          log(client.getWriteError());
      }
      else
         log("erro requisição plugon...");
      vTaskDelay(30000);
    }
  }
}

void taskRunningOnAppCore(void *arg) {
  while(1) {
    timeClient.update();
    String data = timeClient.getFormattedTime();
    log(data);
    vTaskDelay(5000);
  }
}

void WiFiEvent(WiFiEvent_t event)
{
  switch(event){
    case SYSTEM_EVENT_STA_CONNECTED:
      WiFi.enableIpV6();
    break;
    case SYSTEM_EVENT_STA_GOT_IP:
      log("ipv4:"+WiFi.localIP().toString());
      log("gateway:"+WiFi.gatewayIP().toString());
    break;
    case SYSTEM_EVENT_GOT_IP6:
      log(WiFi.localIPv6().toString());
    break;
    case SYSTEM_EVENT_STA_WPS_ER_PBC_OVERLAP:
    case SYSTEM_EVENT_WIFI_READY:
    case SYSTEM_EVENT_SCAN_DONE:
    case SYSTEM_EVENT_STA_START:
    case SYSTEM_EVENT_STA_STOP:
    case SYSTEM_EVENT_STA_DISCONNECTED:
    case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
    case SYSTEM_EVENT_STA_LOST_IP:
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
    case SYSTEM_EVENT_STA_WPS_ER_FAILED:
    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
    case SYSTEM_EVENT_STA_WPS_ER_PIN:
    case SYSTEM_EVENT_AP_START:
    case SYSTEM_EVENT_AP_STOP:
    case SYSTEM_EVENT_AP_STACONNECTED:
    case SYSTEM_EVENT_AP_STADISCONNECTED:
    case SYSTEM_EVENT_AP_STAIPASSIGNED:
    case SYSTEM_EVENT_AP_PROBEREQRECVED:
    case SYSTEM_EVENT_ETH_START:
    case SYSTEM_EVENT_ETH_STOP:
    case SYSTEM_EVENT_ETH_CONNECTED:
    case SYSTEM_EVENT_ETH_DISCONNECTED:
    case SYSTEM_EVENT_ETH_GOT_IP:
    case SYSTEM_EVENT_MAX:
      break;
  }
}

String leArquivoConfig() {
  if(SPIFFS.begin(true)) {
    File fConfig = SPIFFS.open(F("/config.json"), "r");
    if (fConfig) {
      String sConfig = fConfig.readString();
      if (sConfig != "") {
        deserializeJson(config, sConfig);
        return sConfig;
      }
    }
  }
  return "";
}

void gravaArquivoConfig() {
  if (SPIFFS.begin(true))
  {
    SPIFFS.remove(F("/config.json"));
    File fConfig = SPIFFS.open(F("/config.json"), "w");
    if (fConfig) {
      String sConfig;
      serializeJson(config, sConfig);
      fConfig.print(sConfig);
      fConfig.close();
    }
    SPIFFS.end();
  }
}

void setConfig(AsyncWebServerRequest *request,StaticJsonDocument<1000> ret,StaticJsonDocument<1000> doc) {
  String jSon;
  String sConfig = doc["conteudo"];
  deserializeJson(doc, sConfig);
  config["servidorSessao"]["endereco"] = doc["servidorSessao"]["endereco"];
  config["servidorSessao"]["porta"] = doc["servidorSessao"]["porta"];
  config["ssidSessao"]["ssid"] = doc["ssidSessao"]["ssid"];
  config["ssidSessao"]["password"] = doc["ssidSessao"]["password"];
  config["conectorSessao"]["usuario"] = doc["conectorSessao"]["usuario"];
  config["conectorSessao"]["senha"] = doc["conectorSessao"]["senha"];
  config["servidorRestSessao"]["ip"] = doc["servidorRestSessao"]["ip"];
  config["servidorRestSessao"]["porta"] = doc["servidorRestSessao"]["porta"];
  gravaArquivoConfig();
  ret["resultado"] = "restart/config";
  serializeJson(ret, jSon);
  request->send(200, "application/json", jSon);
  delay(1000);
  ESP.restart();

}

void processaReq(AsyncWebServerRequest *request,StaticJsonDocument<1000> doc) {
  StaticJsonDocument<1000> ret;
  String jSon = "";
  ret["resultado"] = "Ok";
  if(doc["acao"] == "info") {
    ret["resultado"] = "v_1.0.0";
  }
  else if(doc["acao"] == "ipLocal") {
    ret["resultado"] = "ipv4:"+WiFi.localIP().toString()+" ipv6:"+WiFi.localIPv6().toString();
  }
  else if (doc["acao"] == "setConfig") {
    setConfig(request,ret,doc);
  }
  else if (doc["acao"] == "clearConfig") {
    if (SPIFFS.begin(true)) {
      SPIFFS.remove(F("/config.json"));
      ret["resultado"] = "restart/clear";
      serializeJson(ret, jSon);
      request->send(200, "application/json", jSon);
      delay(1000);
      ESP.restart();
    }
  }
  else if (doc["acao"] == "retConfig") {
    ret["resultado"] = leArquivoConfig();
  }   
  else if (doc["acao"] == "desligar") {

  }
  else if (doc["acao"] == "atualizaBotao") {
    
  }
  else if (doc["acao"] == "push") {

  }
  else if (doc["acao"] == "estadoAtual") {

  }
  else if( doc["acao"] == "getHora") {

  }
  else if (doc["acao"] == "ligar"){

  }
  else if (doc["acao"] == "restart"){

  }
  else if(doc["acao"] == "listarBotoes") {
    String jSonBts;
    serializeJson(buttonIOTs,jSonBts);
    ret["resultado"] = jSonBts;
  }
  else {
    request->send(404, "application/json", ""); 
  }

  serializeJson(ret, jSon);
  if(jSon == ""){
    request->send(204, "application/json", jSon);
  }
  else
    request->send(200, "application/json", jSon);
}

void iniciaWifi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname("Neuverse");
  WiFi.setAutoReconnect(true);
  WiFi.softAP("neuverse", "neuverse");
  WiFi.onEvent(WiFiEvent);
  if(config["ssidSessao"]["ssid"] == "") {
    WiFi.begin("Escritorio","80818283");
    config["ssidSessao"]["ssid"] = "Escritorio";
    config["ssidSessao"]["password"] = "80818283";
    config["servidorSessao"]["endereco"] = "192.168.10.254";
    config["servidorSessao"]["porta"] = "27015";
    config["conectorSessao"]["usuario"] = "neuverse";
    config["conectorSessao"]["senha"] = "neuverse";
    config["servidorRestSessao"]["ip"] = "192.168.10.7";
    config["servidorRestSessao"]["porta"] = 8080;
    for(int i = 0 ;i<1;i++) {
      StaticJsonDocument<1000> buttonIOT;
      if(i==0){
        buttonIOT["nomeGpio"] = "GPIO_NUM_13";
	      buttonIOT["gpioNum"] = GPIO_NUM_13;
        buttonIOT["gpioNumControle"] = GPIO_NUM_14;
        buttonIOT["status"] = "OFF";
        buttonIOT["funcao"] = "HOLD";
        pinMode(GPIO_NUM_13,OUTPUT);
        pinMode(GPIO_NUM_14,INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(GPIO_NUM_14), trataInterrupcao, FALLING);
        switch(digitalRead(GPIO_NUM_14)){
          case HIGH:
            buttonIOT["tecla"] = "HIGH";
            buttonIOT["status"] = "ON";
            digitalWrite(GPIO_NUM_13,HIGH);
            break;
          case LOW:
            buttonIOT["tecla"] = "LOW";
            buttonIOT["status"] = "OFF";
            digitalWrite(GPIO_NUM_13,LOW);
            break;
        }
      }
      buttonIOT["buttonID"] = i;
      buttonIOTs.add(buttonIOT);
    }
    config["buttonIOTSessao"] = buttonIOTs;
    log("gravando arquivo de configuração inicial");
    gravaArquivoConfig();
  }
  else {
    String ssid = config["ssidSessao"]["ssid"];
    String pass = config["ssidSessao"]["password"];
    buttonIOTs = config["buttonIOTSessao"];
    for(int i=0;i<buttonIOTs.size();i++) {
      String status = buttonIOTs[i]["status"];
      int gpio = buttonIOTs[i]["gpioNum"];
      log("Conf gpio "+ String(gpio)+" para saída");
      pinMode(gpio,OUTPUT);
      int gpioNumControle = buttonIOTs[i]["gpioNumControle"];
      buttonIOTs[i]["status"] = "OFF";
      pinMode(gpioNumControle,INPUT_PULLUP);
      attachInterrupt(digitalPinToInterrupt(gpioNumControle), trataInterrupcao, FALLING);
      switch(digitalRead(gpioNumControle)){
        case HIGH:
          buttonIOTs[i]["tecla"] = "HIGH";
          buttonIOTs[i]["status"] = "ON";
          digitalWrite(gpio,HIGH);
          break;
        case LOW:
          buttonIOTs[i]["tecla"] = "LOW";
          buttonIOTs[i]["status"] = "OFF";
          digitalWrite(gpio,LOW);
          break;
      }
    }
    config["buttonIOTSessao"] = buttonIOTs;
    WiFi.begin(ssid.c_str(), pass.c_str());
  }
  server.begin();
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
  {
    StaticJsonDocument<1000> doc;
    DeserializationError error = deserializeJson(doc, (const char *)data);
    if (error == NULL) {
        processaReq(request,doc);
    }
  });
}

void setup(){
  Serial.begin(115200);
  if(leArquivoConfig()==""){
    config["ssidSessao"]["ssid"] = "";
    config["ssidSessao"]["password"] = ""; 
  }
  iniciaWifi();
  timeClient.begin();
  xTaskCreatePinnedToCore(taskRunningOnAppCore, "TaskOnApp",2048,NULL,4,NULL,APP_CPU_NUM);
  xTaskCreatePinnedToCore(taskPlugOnAlive, "TaskPlugOnAlive",2048*4,NULL,4,NULL,APP_CPU_NUM);
}

void loop() {
}


