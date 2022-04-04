#include <Arduino.h>
#include <WiFi.h>
#include "WiFiClient.h"
#include "ESPAsyncWebServer.h"
#include "ArduinoJson.h"
#include <FS.h>
#include <SPIFFS.h>
#include <NTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ArduinoHttpClient.h>
#include <Wire.h>
#include "SSD1306Wire.h"
#include "BluetoothSerial.h"

#define _30S 30*1000

//BluetoothSerial SerialBT;
typedef struct par {
  String par1;
}par;

int timeoutAlive = 0;
const int tamJsonDoc =  1000;
WiFiClient cli;
AsyncWebServer server(80);
DynamicJsonDocument config(2048);
DynamicJsonDocument buttonIOTs(2048);
SSD1306Wire  display(0x3c, SDA, SCL);
String dataGlobal = "";
String ipGlobal = "";
String globalUltimoLog = "";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600, 60000);
String nomeDevice = "NeuverseESP32";

void log(String mens,String tipo = "DEBUG") {
  if(tipo == "TESTE"){
    Serial.println(dataGlobal+" "+tipo+" "+mens);
    globalUltimoLog = mens;
    globalUltimoLog = globalUltimoLog.substring(0,20);    
  }
  /*if(SerialBT.available()){
      SerialBT.println(dataGlobal+" "+tipo+" "+mens);
  }*/
}

void desenhaLinhasInt() {
  display.drawVerticalLine(109,0, 15);
  display.drawVerticalLine(108,0, 15);
  display.drawVerticalLine(106,4, 11);
  display.drawVerticalLine(105,4, 11);
  display.drawVerticalLine(103,8, 7);
  display.drawVerticalLine(102,8, 7);
  display.drawVerticalLine(100,12, 3);
  display.drawVerticalLine(99,12, 3);

}

void mostraHora(String hora) {
  log("Hora:"+hora,"INFO");
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  //Seleciona a fonte
  display.setFont(ArialMT_Plain_16);
  display.drawString(63, 10, hora);
    if(ipGlobal != "") {
    display.drawString(63, 26, ipGlobal);
    display.drawString(63, 42,"Neuverse Inc.");
    desenhaLinhasInt();
  }
  else
    display.drawString(63, 42,"Neuverse Inc.");
  display.display();
}

void telainicial()
{
  //Apaga o display
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  //Seleciona a fonte
  display.setFont(ArialMT_Plain_16);
  display.drawString(63, 10, "NodeMCU");
  display.drawString(63, 26, "ESP8266");
  display.drawString(63, 45, "Display Oled");
  display.display();
}

void graficos()
{
  display.clear();
  //Desenha um quadrado
  display.drawRect(12, 12, 30, 30);
  //Desenha um quadrado cheio
  display.fillRect(20, 20, 35, 35);
  //Desenha circulos
  for (int i = 1; i < 8; i++)
  {
    display.setColor(WHITE);
    display.drawCircle(92, 32, i * 3);
  }
  display.display();
}
void ProgressBar()
{
  for (int counter = 0; counter <= 100; counter++)
  {
    display.clear();
    //Desenha a barra de progresso
    display.drawProgressBar(0, 32, 120, 10, counter);
    //Atualiza a porcentagem completa
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 15, String(counter) + "%");
    display.display();
    delay(10);
  }
}

String preencheCelula(int id,String value,String attributo,String disabled){
  String txtHtmlCfg;
  txtHtmlCfg = "<td><input "+disabled+" name = '"+attributo+String(id)+"'  id = '"+attributo+String(id)+"' type='text'value='"+value+"'></input></td>";
  return txtHtmlCfg;
}

String gerarComboRedesWiFi() {
  String mac = WiFi.macAddress();
  int n = WiFi.scanNetworks();
  String combo = "<select name='ssid' >\r\n";
  for (int i = 0; i < n; i++)
  {
    String sel = "";
    String ssidAtual = config["ssidSessao"]["ssid"];
    if(WiFi.SSID(i) == ssidAtual)
      sel = "selected"; 
    combo = combo+"<option "+sel+" id= '"+WiFi.SSID(i)+"' value='"+WiFi.SSID(i)+"'>"+WiFi.SSID(i)+"</option>\r\n";
  }
  combo = combo+" </select>\r\n";
  log(combo);
  log(mac);
  return combo;
}


String carregaHtmlBtns() {   
  String ssid = config["ssidSessao"]["ssid"];
  String password = config["ssidSessao"]["password"];
  String ip = config["servidorRestSessao"]["ip"] ;
  String porta = config["servidorRestSessao"]["porta"];
  String redes = gerarComboRedesWiFi();
  String paginaConf = 
    "<html>"
    " <h1>Configuração DeviceIOT</h1>\r\n"
    " <h2>Geral</h2>\r\n"
    " <form>\r\n"
    "   <table>\r\n"
    "     <tr>\r\n"
    "       <td align = 'right' >Ssid:</td>\r\n"
    "       <td align = 'left' >"+redes+"\r\n"
    "       </td>"
    "     </tr>"
    "     <tr>"
    "       <td align = 'right' >Chave:</td>"
    "       <td align = 'left' ><input type='password' name = 'chave' value = '"+password+"'/></td>"
    "     </tr>"
    "     <tr>"
    "       <td align = 'right' >ServidorRest:</td>"
    "       <td align = 'left' ><input type='text' name = 'ip' value = '"+ip+"'/><input size = '5' type='text' name = 'porta' value = '"+porta+"'/></td>"
    "      </tr>"
    "   </table>"
    " <h2>Botões</h2>\r\n"
      "<table border='1'><tr><td><font color='red'>ID</font></td><td>nomeGpio</td><td>ButtonID</td><td>gpioNumControle</td><td>gpioNum</td>gpioNum</tr>";
      for(int i=0;i<buttonIOTs.size();i++) {
        String nomeGpio = buttonIOTs[i]["nomeGpio"];
        String ButtonID = buttonIOTs[i]["buttonID"];
        String gpioNumControle = buttonIOTs[i]["gpioNumControle"];
        String gpioNum = buttonIOTs[i]["gpioNum"];
        paginaConf = paginaConf+      
          "<tr>"
            +preencheCelula(i,String(i),"ID","")
            +preencheCelula(i,nomeGpio,"nomeGpio","")
            +preencheCelula(i,ButtonID,"ButtonID","")
            +preencheCelula(i,gpioNumControle,"gpioNumControle","")+
            preencheCelula(i,gpioNum,"gpioNum","")+
          "</tr>";
      }
      paginaConf = paginaConf+"</table><input type = 'HIDDEN' name= 'acao' value='gravarBts'/><input type = 'submit' value='gravar'/></form></html>";
      return paginaConf;
}

void atualizaStatus(String nivel,int pos){  
  buttonIOTs[pos]["status"] == "ON" ?  digitalWrite(buttonIOTs[pos]["gpioNum"],LOW) :  digitalWrite(buttonIOTs[pos]["gpioNum"],HIGH);
  buttonIOTs[pos]["status"] == "ON" ?  buttonIOTs[pos]["status"] = "OFF" :  buttonIOTs[pos]["status"] = "ON";
  buttonIOTs[pos]["tecla"] = nivel;
}

void key(String nivel,int pos){
  nivel == "HIGH" ?  digitalWrite(buttonIOTs[pos]["gpioNum"],HIGH) :  digitalWrite(buttonIOTs[pos]["gpioNum"],LOW);
  nivel == "HIGH" ?  buttonIOTs[pos]["status"] = "ON" :  buttonIOTs[pos]["status"] = "OFF";
  buttonIOTs[pos]["tecla"] = nivel;
}

void trataInterrupcao(){
  log("Interrupcao gpio gerada.");
  for(int i=0;i<buttonIOTs.size();i++) {
    switch(digitalRead(buttonIOTs[i]["gpioNumControle"])){
      case HIGH:        
        if(buttonIOTs[i]["funcao"] == "HOLD") {
          if(buttonIOTs[i]["tecla"] != "HIGH")
            atualizaStatus("HIGH",i);
        }
        else if (buttonIOTs[i]["funcao"] == "KEY") {
          key("HIGH",i);
        }
        else if (buttonIOTs[i]["funcao"] == "PUSH") {
          if(buttonIOTs[i]["tecla"] != "HIGH") {
             atualizaStatus("HIGH",i);
          }
        }
        break;
      case LOW:                        
        if(buttonIOTs[i]["funcao"] == "HOLD") {
          if(buttonIOTs[i]["tecla"] != "LOW")
            atualizaStatus("LOW",i);
        }
        else if (buttonIOTs[i]["funcao"] == "KEY") {
          key("LOW",i);  
        }
        else if (buttonIOTs[i]["funcao"] == "PUSH") {
          if(buttonIOTs[i]["tecla"] != "LOW") {
            atualizaStatus("HIGH",i);
          }
        }
        break;
    }
  }
}

void taskPlugOnAlive(void *arg) {
  DynamicJsonDocument conector(2048);
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
      log("Bts:"+jSonBts,"INFO");
      conector["iot"]["jSon"] = jSonBts;
      conector["status"] = "LOGIN";
      serializeJson(conector, jSon);
      WiFiClient wifi;
      String ip = config["servidorRestSessao"]["ip"];
      int porta = config["servidorRestSessao"]["porta"];
      HttpClient client = HttpClient(wifi, ip.c_str(), porta);
      log("iniciando requisição plugon...","INFO");
      client.beginRequest();
      try {
        if(client.post("/ServidorIOT/plugon")==0){
          log("Enviando requisição...","INFO");
          client.sendHeader(HTTP_HEADER_CONTENT_TYPE, "application/x-www-form-urlencoded");
          client.sendHeader(HTTP_HEADER_CONTENT_LENGTH, jSon.length());        
          client.endRequest();
          client.write((const byte*)jSon.c_str(), jSon.length());
          if(client.getWriteError()==0){
            int statusCode = client.responseStatusCode();
            log(""+statusCode,"INFO");
            String response = client.responseBody();
            deserializeJson(conector,response);
            log(response,"INFO");
          }
          else
            log(""+client.getWriteError());
        }
        else
          log("erro requisição plugon...");
      }
      catch (...){

      }
      vTaskDelay(30000);
    }
  }
}

void taskRunningOnAppCore(void *arg) {
  while(1) {
    timeClient.update();
    dataGlobal = timeClient.getFormattedTime();
    mostraHora(dataGlobal);
    vTaskDelay(1000);
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
      ipGlobal = "IP:"+WiFi.localIP().toString();
    break;
    case SYSTEM_EVENT_GOT_IP6:
      log(WiFi.localIPv6().toString());
    break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      ESP.restart();
    break;
    case SYSTEM_EVENT_STA_LOST_IP:
      ipGlobal = "";
    break;
    case SYSTEM_EVENT_STA_WPS_ER_PBC_OVERLAP:
    case SYSTEM_EVENT_WIFI_READY:
    case SYSTEM_EVENT_SCAN_DONE:
    case SYSTEM_EVENT_STA_START:
    case SYSTEM_EVENT_STA_STOP:
    case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
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
  String sConfig = "";
  if(SPIFFS.begin(true)) {
    File fConfig = SPIFFS.open(F("/config.json"), "r");
    if (fConfig) {
      sConfig = fConfig.readString();
      if (sConfig != "" && sConfig != NULL) {
        deserializeJson(config, sConfig);
      }
      else
        sConfig ="";
      fConfig.close();
      log("Arquivo conf:"+sConfig,"INFO");
      
    }
    else
      log("Arquivo de configuração não encontrado!","INFO");
  }
  return sConfig;
}

void removeConfig() {
  if (SPIFFS.begin(true))
  {
    SPIFFS.remove(F("/config.json"));
    SPIFFS.end();
  }
}

void gravaArquivoConfigRest(String content) {
  if (SPIFFS.begin(true))
  {
    SPIFFS.remove(F("/config.json"));
    File fConfig = SPIFFS.open(F("/config.json"), "w");
    if (fConfig) {
      fConfig.print(content);
      fConfig.close();
    }
    SPIFFS.end();
  }
}

void gravaArquivoConfig() {
  if (SPIFFS.begin(true))
  {
    SPIFFS.remove(F("/config.json"));
    File fConfig = SPIFFS.open(F("/config.json"), "w");
    if (fConfig) {
      String sConfig;
      serializeJson(config, sConfig);
      log("Arquivo gravado:"+sConfig,"INFO");
      fConfig.print(sConfig);
      fConfig.close();
    }
    SPIFFS.end();
  }
}

void setConfig(AsyncWebServerRequest *request,DynamicJsonDocument ret,DynamicJsonDocument doc) {
  String jSon;
  String sConfig = doc["conteudo"];
  log(sConfig,"INFO");
  deserializeJson(doc, sConfig);
  config["servidorSessao"]["endereco"] = doc["servidorSessao"]["endereco"];
  config["servidorSessao"]["porta"] = doc["servidorSessao"]["porta"];
  config["ssidSessao"]["ssid"] = doc["ssidSessao"]["ssid"];
  config["ssidSessao"]["password"] = doc["ssidSessao"]["password"];
  config["conectorSessao"]["usuario"] = doc["conectorSessao"]["usuario"];
  config["conectorSessao"]["senha"] = doc["conectorSessao"]["senha"];
  config["servidorRestSessao"]["ip"] = doc["servidorRestSessao"]["ip"];
  config["servidorRestSessao"]["porta"] = doc["servidorRestSessao"]["porta"];
  config["dataAtualizacao"] = doc["dataAtualizacao"];
  config["buttonIOTSessao"] = doc["buttonIOTSessao"];
  gravaArquivoConfigRest(sConfig);
  ret["resultado"] = "Ok/restart";
  serializeJson(ret, jSon);
  request->send(200, "application/json", jSon);
  delay(5000);
  ESP.restart();

}

void processaReq(AsyncWebServerRequest *request,DynamicJsonDocument doc) {
  DynamicJsonDocument ret(2048);
  String jSon = "";
  ret["resultado"] = "Ok";
  String acao = doc["acao"];
  log(acao,"INFO");
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
    ret["resultado"] =leArquivoConfig();
    log(ret["resultado"],"INFO");

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
     String data = timeClient.getFormattedTime(); 
     ret["resultado"] = data;      
  }
  else if (doc["acao"] == "processaBtn") {
    String jSon;
    String sConfig = doc["conteudo"];
    log(sConfig);
    deserializeJson(doc, sConfig);
    String btnsJson = doc["buttonIOTSessao"];
    deserializeJson(buttonIOTs,btnsJson);
    for(int i=0;i<buttonIOTs.size();i++) {
      if( buttonIOTs[i]["status"] == "OFF") {
        digitalWrite( buttonIOTs[i]["gpioNum"],LOW);
      }
      else{
          digitalWrite( buttonIOTs[i]["gpioNum"],HIGH);
      }
    }
  }
  else if (doc["acao"] == "ligar"){

  }
  else if (doc["acao"] == "restart"){
    ret["resultado"] = "restartando...";
    serializeJson(ret, jSon);
    request->send(200, "application/json", jSon);
    delay(1000);
    ESP.restart();
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
    request->send(204, "application/json", "");
  }
  else{
    request->send(200, "application/json", jSon);
  }
}

void iniciaWifi() {
  WiFi.mode(WIFI_AP_STA);
  //WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE,INADDR_NONE);
  WiFi.setAutoConnect(true);
  WiFi.setHostname("Neuverse");
  WiFi.setAutoReconnect(true);
  WiFi.softAP("neuverse", "neuverse");
  WiFi.onEvent(WiFiEvent);
  log("Verificando existencia confInicial","INFO");
  if(config["ssidSessao"]["ssid"] == "") {
    log("Criando confInicial","INFO");
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
      DynamicJsonDocument buttonIOT(2048);
      if(i==0){
        buttonIOT["nomeGpio"] = "ConfInicialTeste";
	      buttonIOT["gpioNum"] = GPIO_NUM_13;
        buttonIOT["gpioNumControle"] = GPIO_NUM_14;
        buttonIOT["status"] = "OFF";
        buttonIOT["funcao"] = "HOLD";
        pinMode(GPIO_NUM_13,OUTPUT);
        pinMode(GPIO_NUM_14,INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(GPIO_NUM_14), trataInterrupcao, CHANGE);
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
    log("gravando arquivo de configuração inicial","INFO");
    gravaArquivoConfig();
  }
  else {
    String ssid = config["ssidSessao"]["ssid"];
    String pass = config["ssidSessao"]["password"];
    String btnsJson  = config["buttonIOTSessao"];
    log("Btns cfg json:"+btnsJson,"INFO");
  
    deserializeJson(buttonIOTs,btnsJson);
    int totalBtn = 0;
    for(int i=0;i<buttonIOTs.size();i++) {
      totalBtn++;
      String status = buttonIOTs[i]["status"];
      int gpio = buttonIOTs[i]["gpioNum"];
      log("Conf gpio "+ String(gpio)+" para saída");
      pinMode(gpio,OUTPUT);
      int gpioNumControle = buttonIOTs[i]["gpioNumControle"];
      buttonIOTs[i]["status"] = "OFF";
      pinMode(gpioNumControle,INPUT_PULLUP);
      log("Colocando Gpio "+String(gpioNumControle)+" na interrupcao");
      if(buttonIOTs[i]["funcao"] == "HOLD" || buttonIOTs[i]["funcao"] == "KEY")
        attachInterrupt(gpioNumControle, trataInterrupcao, CHANGE);
      else if(buttonIOTs[i]["funcao"] == "PUSH")
        attachInterrupt(gpioNumControle, trataInterrupcao, FALLING);
      
      buttonIOTs[i]["status"] = "OFF";
      digitalWrite(gpio,LOW);
      
      switch(digitalRead(gpioNumControle)){
        case HIGH:
          buttonIOTs[i]["tecla"] = "HIGH";
          break;
        case LOW:
          buttonIOTs[i]["tecla"] = "LOW";         
          break;
      }
    }
    log("Total btns:"+String(totalBtn),"INFO");
    config["buttonIOTSessao"] = buttonIOTs;
    WiFi.begin(ssid.c_str(), pass.c_str());
  }
  
  server.begin();
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
    String param = request->getParam("acao")->value();
    String paginaConf;
    if(param == "gravarBts") {
      for(int j=0;j< buttonIOTs.size();j++) {
        String indice = String(j);
        String id = request->getParam("ID"+indice)->value();
        log("Pegando id do botao "+id);
        String nomeGpio = request->getParam("nomeGpio"+indice)->value();
        String ButtonID = request->getParam("ButtonID"+indice)->value();
        String gpioNumControle = request->getParam("gpioNumControle"+indice)->value();
         String gpioNum = request->getParam("gpioNum"+indice)->value();
        buttonIOTs[j]["nomeGpio"] = nomeGpio;
        buttonIOTs[j]["buttonID"] = ButtonID;
        buttonIOTs[j]["gpioNum"] = gpioNum;
      }

      String ssid = request->getParam("ssid")->value(); 
      config["ssidSessao"]["ssid"] = ssid;
      String password = request->getParam("chave")->value();
      config["ssidSessao"]["password"] = password;
      config["servidorRestSessao"]["ip"] = request->getParam("ip")->value();
      config["servidorRestSessao"]["porta"] = request->getParam("porta")->value();


      paginaConf = carregaHtmlBtns();
      config["dataAtualizacao"] = timeClient.getFormattedTime();
      config["buttonIOTSessao"] = buttonIOTs;
      log("gravando arquivo de configuração através html");
      gravaArquivoConfig();
    } 
    else if(param == "inicial"){
      paginaConf = carregaHtmlBtns();
    } 
    else
      paginaConf = "nada a fazer"; 
    
    request->send(200, "text/html; charset=utf-8", paginaConf);
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
  {
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, (const char *)data);
    if (error == NULL) {
      processaReq(request,doc);
    }
    else {
      Serial.println("ERRO GRAVE em deserializeJson server.onRequestBody: "+ String(error.c_str()));
    }
  });
}

String login()
{
  DynamicJsonDocument doc(2048);
  int id = config["conectorSessao"]["id"];
  String nome = config["conectorSessao"]["nome"];
  String usuario = config["conectorSessao"]["usuario"];
  String senha = config["conectorSessao"]["senha"];
  String retorno = "";
  doc["id"] = id;
  doc["nome"] = nome;
  doc["usuario"] = usuario;
  doc["senha"]   = senha;
  doc["iot"]["id"] = 0;
  doc["iot"]["name"] = "nameIot";
  doc["status"] = "LOGIN";
  serializeJson(doc, retorno);
  return retorno + "\r\n";
}

String alive(String id)
{
  DynamicJsonDocument doc(2048); 
  String retorno = "";
  doc["id"] = id;
  doc["usuario"] = config["conectorSessao"]["usuario"];;
  doc["nome"] = config["conectorSessao"]["nome"];
  doc["tipo"] = "CONTROLEREMOTO";
  doc["senha"]   =  config["conectorSessao"]["senha"];
  doc["iot"]["id"] = 0;
  doc["iot"]["name"] = "";
  doc["status"] = "ALIVE";
  serializeJson(doc, retorno);
  return retorno + "\r\n";
}

void taskAlive(void *arg) {
  par * p  = (par*)arg;
  String id = p->par1;
  DynamicJsonDocument conector(2048);
  log("Id do Alive:"+id,"TESTE");
  while(cli.connected()) {
    log("Alive "+id,"TESTE");
    timeoutAlive = 1;
    cli.print(alive(id));
    vTaskDelay(_30S);
    if(timeoutAlive == 1){
      cli.stop();
    }
  }
}

void processar(String dados) {
  DynamicJsonDocument conector(2048);
  deserializeJson(conector,dados);
  String idConector = conector["id"];
  String stConector = conector["status"];
  if(stConector == "LOGIN_OK"){
    log("Cliente conectado com id: "+idConector,"TESTE");
    par * p = new par;
    p->par1 = idConector;
    xTaskCreatePinnedToCore(taskAlive, "taskAlive",2048*4,(void*)p,4,NULL,APP_CPU_NUM);
  }
  else if(stConector == "CONECTADO"){
    log("retorno Alive!","TESTE");
    timeoutAlive = 0;
  }
}

void taskCliente(void *arg) {
  
  String server = config["servidorSessao"]["endereco"];
  uint16_t porta = config["servidorSessao"]["porta"];
  int32_t timeout = 10000;
  while(1) {
    log("Cliente tcp conectando.","TESTE");
    if (cli.connect(server.c_str(), porta,timeout)) {
      log("Cliente tcp conectado.","TESTE");
      cli.print(login());
      while (cli.connected())
      {
        if (cli.available())
        {
          String dados = cli.readStringUntil('\n');
          if (dados == "")
            continue;
          processar(dados);
        }
      }
      cli.stop();   
    }
    vTaskDelay(1000);
  }
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
  xTaskCreatePinnedToCore(taskCliente, "taskCliente",2048*4,NULL,4,NULL,APP_CPU_NUM);
  display.init();
  display.flipScreenVertically();
  //SerialBT.begin("neuverseBTIot");
 }

void loop() {
}


