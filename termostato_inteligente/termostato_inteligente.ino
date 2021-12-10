#include <ESP8266WiFi.h>
#include <DHT.h>
#include <PubSubClient.h>



//#define DHTTYPE DHT22
#define DHTTYPE DHT11
#define pinBotao1 16 // D0=GPIO-16 /// <<<<<<<<<<<<<<<<<<<<<
// porta disponivel 15 // D8 = GPIO15

const char* SSID = "VIVO-8965";
const char* PASSWORD = "C9D3C88965";
WiFiClient wifiClient;

//MQTT Server
const char* BROKER_MQTT = "mqtt.eclipseprojects.io"; //URL do broker MQTT que se deseja utilizar novo link
int BROKER_PORT = 1883;                      // Porta do Broker MQTT <<<<<<<<<<<<<
//int BROKER_PORT = 123; //obs: definir a porta do briker MQTT

//#define ID_MQTT "TI-IOT01"
//#define TOPIC_PUBLISH "TI-TempUmid"
#define ID_MQTT  "BCI01"            //Informe um ID unico e seu. Caso sejam usados IDs repetidos a ultima conexão irá sobrepor a anterior. 
#define TOPIC_PUBLISH "BCIBotao1"    //Informe um Tópico único. Caso sejam usados tópicos em duplicidade, o último irá eliminar o anterior.
//PubSubClient MQTT(WiFiClient);
PubSubClient MQTT(wifiClient);        // Instancia o Cliente MQTT passando o objeto espClient

WiFiServer server(80); //Shield irá receber as requisições das páginas (o padrão WEB é a porta 80)

String HTTP_req; 
String URLValue;

void processaPorta(byte porta, byte posicao, WiFiClient cl);
void lePortaDigital(byte porta, byte posicao, WiFiClient cl);        
void lePortaAnalogica(byte porta, byte posicao, WiFiClient cl);   
String getURLRequest(String *requisicao);
bool mainPageRequest(String *requisicao);

void mantemConexoes();  //Garante que as conexoes com WiFi e MQTT Broker se mantenham ativas //<<<<<<<<<<<<<<<<<
void conectaWiFi();     //Faz conexão com WiFi
void conectaMQTT();     //Faz conexão com Broker <<<<<<<<<<<<<<<<<<<
void enviaPacote();

int statusCooler = 4; // (D2 = 4)
const byte dhtPin = 5; // (D1 = 5)

DHT dht(dhtPin, DHTTYPE);
float temp, tempf, humi;

const byte qtdePinosDigitais = 5;
byte pinosDigitais[qtdePinosDigitais] = {2               ,12     , 13    , 14      }; // ((2=d4),  (12=D6), (13=D7), (14=D5 ), (15=D8))
byte modoPinos[qtdePinosDigitais]     = {INPUT_PULLUP,  OUTPUT, OUTPUT, OUTPUT,};

const byte qtdePinosAnalogicos = 1;
byte pinosAnalogicos[qtdePinosAnalogicos] = {A0};

void setup()
{   
    dht.begin();
    Serial.begin(115200);
    pinMode(statusCooler, OUTPUT);
     pinMode(pinBotao1, INPUT_PULLUP); 
    //  MQTT.setServer(BROKER_MQTT, BROKER_PORT);   

    //Conexão na rede WiFi  
    Serial.println();
    Serial.print("Conectando a ");
    Serial.println(SSID);

    WiFi.begin(SSID, PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi conectado!");

    // Inicia o servidor WEB
    server.begin();
    Serial.println("Server iniciado");

    // Mostra o endereco IP
    Serial.println(WiFi.localIP());

    //Configura o modo dos pinos
    for (int nP=0; nP < qtdePinosDigitais; nP++) {
        pinMode(pinosDigitais[nP], modoPinos[nP]);
    }
   
}

void loop()
{
   mantemConexoes(); // <<<<<<<
   enviaValores();   // <<<<<<<
   MQTT.loop(); //<<<<<<<<<
    WiFiClient  client = server.available();

    if (client) { 
        boolean currentLineIsBlank = true;
        while (client.connected()) {
            if (client.available()) { 
                char c = client.read(); 
                HTTP_req += c;  
                
                if (c == '\n' && currentLineIsBlank ) { 

                    if ( mainPageRequest(&HTTP_req) ) {
                        URLValue = getURLRequest(&HTTP_req);
                        Serial.println(HTTP_req);
                                                 
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println("Connection: keep-alive");              //<------ ATENCAO
                        client.println();
                        
                        //Conteudo da Página HTML
                        client.println("<!DOCTYPE html>");
                        client.println("<html>");
                                                
                        client.println("<head>");
                        client.println("<title>Arduino via WEB</title>");

                        client.println("<script>");
                        client.println("function LeDadosDoArduino() {");
                        client.println("nocache = \"&nocache=\" + Math.random() * 1000000;");
                        client.println("var request = new XMLHttpRequest();");
                        client.println("var posIni;");
                        client.println("var valPosIni;");
                        client.println("var valPosFim;");
                        client.println("request.onreadystatechange = function() {");
                        client.println("if (this.readyState == 4) {");
                        client.println("if (this.status == 200) {");
                        client.println("if (this.responseText != null) {");

                        for (int nL=0; nL < qtdePinosDigitais; nL++) {                                                    //<-------NOVO
                            client.print("posIni = this.responseText.indexOf(\"PD");
                            client.print(pinosDigitais[nL]);
                            client.println("\");");
                            client.println("if ( posIni > -1) {");
                            client.println("valPosIni = this.responseText.indexOf(\"#\", posIni) + 1;");
                            client.println("valPosFim = this.responseText.indexOf(\"|\", posIni);");
                            client.print("document.getElementById(\"pino");
                            client.print(pinosDigitais[nL]);
                            client.println("\").checked = Number(this.responseText.substring(valPosIni, valPosFim));");
                            client.println("}");
                        }

                        for (int nL=0; nL < qtdePinosAnalogicos; nL++) {                                                    //<-------NOVO
                            
                            client.print("posIni = this.responseText.indexOf(\"PA");
                            client.print(pinosAnalogicos[nL]);
                            client.println("\");"); 
                            client.println("if ( posIni > -1) {");
                            client.println("valPosIni = this.responseText.indexOf(\"#\", posIni) + 1;");
                            client.println("valPosFim = this.responseText.indexOf(\"|\", posIni);");
                            client.print("document.getElementById(\"pino");
                            client.print(pinosAnalogicos[nL]);
                            client.print("\").innerHTML = \"Porta ");
                            client.print(pinosAnalogicos[nL]);
                            client.print(" - Valor: \" + this.responseText.substring(valPosIni, valPosFim);");
                            client.println("}");
                        }
                          
                        client.println("}}}}");
                        client.println("request.open(\"GET\", \"solicitacao_via_ajax\" + nocache, true);");
                        client.println("request.send(null);");
                        client.println("setTimeout('LeDadosDoArduino()', 1000);");
                        client.println("}");
                        client.println("</script>");
                        
                        client.println("</head>");

                        //-------------------------------------------------------------------------------------------------------------

                        client.print("<style>");
                        client.print("h1   {color: blue;font-size: 23px;}");
                        client.print("h1.sbt  {font-size: 20px;}");
                        client.print("h1.title  {margin:0 0 30px;}");
                        client.print("body {background-color: #A9A9A9;}");
                        client.print(" .styleMobile {background-color: white;margin: 25px auto; border: solid 12px; border-top: solid 60px; border-bottom: solid 61px; border-radius: 43px; width: 481px; background-color: white; height: 801px; padding: 10px;font-family: tahoma;}");
                        client.print("p    {color: white;}");  
                        client.println(".statusCooler {color: black}");                                           
                        client.print(" .dadosDTH11 {background-color: slateblue; margin: 40px auto;border: solid 5px; border-top: solid 5px; border-bottom: solid 5px; max-width: 372px; height: 127px; padding: 17px;}");
                        client.print("</style>");
                        
                        client.println("<body onload=\"LeDadosDoArduino()\">");                      //<------ALTERADO 
                        client.println("<div class='styleMobile'>");
                        client.println("<h1 class='title'>------- TERMOSTATO INTELIGENTE -------</h1>");                   
                        client.println("<h1 class='sbt'>PORTAS EM FUN&Ccedil;&Atilde;O ANAL&Oacute;GICA</h1>");

                        for (int nL=0; nL < qtdePinosAnalogicos; nL++) {

                            client.print("<div id=\"pino");                         //<----- NOVO
                            client.print(pinosAnalogicos[nL]);
                            client.print("\">"); 
                                                         
                            client.print("Porta ");
                            client.print(pinosAnalogicos[nL]);
                            client.println(" - Valor: ");
                               
                            client.print( analogRead(pinosAnalogicos[nL]) );
                            client.println("</div>");                               //<----- NOVO
                               
                            client.println("<br/>");                             
                        }
                        
                        client.println("<br/>");                        
                        client.println("<h1 class='sbt'>PORTAS EM FUN&Ccedil;&Atilde;O DIGITAL</h1>");
                        client.println("<form method=\"get\">");

                        for (int nL=0; nL < qtdePinosDigitais; nL++) {
                            processaPorta(pinosDigitais[nL], nL, client);
                            client.println("<br/>");
                        }
                        
                        client.println("<br/>");
                        client.println("<button type=\"submit\">Envia para o ESP8266</button>");
                        client.println("</form>"); 
                       
                        client.println("<br/>");                      

                        client.println("<div class='dadosDTH11'>"); 
                    
                          client.println("<p>");
                            client.println("Temperatura");
                            client.println(tempf);
                            client.println("*F");
                          client.println("</p>"); 

                          client.println("<p>");
                            client.println("Temperatura");
                            client.println(temp);
                            client.println("*C");
                          client.println("</p>");   
                         
                          client.println("<p>");   
                            client.println("Umidade");
                            client.println(humi);
                            client.println("%");
                         client.println("</p>");
                         
                        client.println("</div>");                       
                       if(tempf >= 90){
                        
                            digitalWrite(statusCooler,HIGH);
                            client.println("<p class='statusCooler'>VENTILA&Ccedil;&Atilde;O - <button style= 'background-color: green'>LIGADA</button></p>");
                        }else{
                                client.println("<p class='statusCooler'>VENTILA&Ccedil;&Atilde;O - <button style= 'background-color: red'>DESLIGADA</button></p>");
                                digitalWrite(statusCooler,LOW);
                          }          
                     
                          client.println("</div>");
                        client.println("</body>");
                        //------------------------------------------------------------------------------------
                        client.println("</html>");

                   
                    } else if (HTTP_req.indexOf("solicitacao_via_ajax") > -1) {     //<----- NOVO

                        Serial.println(HTTP_req);

                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println("Connection: keep-alive");      
                        client.println();                      

                        for (int nL=0; nL < qtdePinosAnalogicos; nL++) {
                            lePortaAnalogica(pinosAnalogicos[nL], nL, client);                            
                        }
                        for (int nL=0; nL < qtdePinosDigitais; nL++) {
                            lePortaDigital(pinosDigitais[nL], nL, client);
                        }
                            
                    } else {

                        Serial.println(HTTP_req);
                        client.println("HTTP/1.1 200 OK");
                    }
                    HTTP_req = "";    
                    break;
                }
                
                if (c == '\n') {
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') {
                    currentLineIsBlank = false;
                }
            }
        } 
        delay(1);     
        client.stop(); 
    } 
    if(getTH() == true)
  {
    Serial.print("Temp  = ");
    Serial.print(tempf);
    Serial.print(" Deg F  ");
    Serial.print("  Humidity  = ");
    Serial.print(humi);
    Serial.println(" %RH");    
  }
} // ### Fim do loop
void processaPorta(byte porta, byte posicao, WiFiClient cl)
{
static boolean LED_status = 0;
String cHTML;

    cHTML = "P";
    cHTML += porta;
    cHTML += "=";
    cHTML += porta;

    if (modoPinos[posicao] == OUTPUT) { 
        
        if (URLValue.indexOf(cHTML) > -1) { 
           LED_status = HIGH;
        } else {
           LED_status = LOW;
        }
        digitalWrite(porta, LED_status);
    } else {

        LED_status = digitalRead(porta);
    }
        
    cl.print("<input type=\"checkbox\" name=\"P");
    cl.print(porta);
    cl.print("\" value=\"");
    cl.print(porta);
    
    cl.print("\"");

    cl.print(" id=\"pino");           //<------NOVO
    cl.print(porta);
    cl.print("\"");
    
    if (LED_status) { 
        cl.print(" checked ");
    }

    if (modoPinos[posicao] != OUTPUT) { 
        cl.print(" disabled ");
    }
    
    cl.print(">Porta ");
    cl.print(porta);

    cl.println();
}

void lePortaDigital(byte porta, byte posicao, WiFiClient cl)
{
    if (modoPinos[posicao] != OUTPUT) { 
       cl.print("PD");
       cl.print(porta);
       cl.print("#");
       
       if (digitalRead(porta)) {
          cl.print("1");
       } else {
          cl.print("0");
       }
       cl.println("|");
    }
}

void lePortaAnalogica(byte porta, byte posicao, WiFiClient cl)
{
   cl.print("PA");
   cl.print(porta);
   cl.print("#");
   
   cl.print(analogRead(porta));

   //especifico para formatar o valor da porta analogica A0
   if (porta == A0) { 
      cl.print(" (");
      cl.print(map(analogRead(A0),0,1023,0,179)); 
      cl.print("&deg;)");
   }
   
   cl.println("|");   
}


String getURLRequest(String *requisicao) {
int inicio, fim;
String retorno;

  inicio = requisicao->indexOf("GET") + 3;
  fim = requisicao->indexOf("HTTP/") - 1;
  retorno = requisicao->substring(inicio, fim);
  retorno.trim();

  return retorno;
}

bool mainPageRequest(String *requisicao) {
String valor;
bool retorno = false;

  valor = getURLRequest(requisicao);
  valor.toLowerCase();

  if (valor == "/") {
     retorno = true;
  }

  if (valor.substring(0,2) == "/?") {
     retorno = true;
  }  

  if (valor.substring(0,10) == "/index.htm") {
     retorno = true;
  }  

  return retorno;
}
boolean getTH()
{
  static unsigned long timer = 0;
  unsigned long interval = 5000;

  if (millis() - timer > interval)
  {
    timer = millis();   
    humi = dht.readHumidity();
    temp = dht.readTemperature();
    tempf =  dht.readTemperature(true);
    if (isnan(humi) || isnan(temp) || isnan(tempf))
    {
      Serial.println("Failed to read from DHT sensor!");      
      return false;
    }
    return true;
  }
  else
  {
    return false;
  }
} 
void conectaMQTT() { 
    while (!MQTT.connected()) {
        Serial.print("Conectando ao Broker MQTT: ");
        Serial.println(BROKER_MQTT);
        if (MQTT.connect(ID_MQTT)) {
            Serial.println("Conectado ao Broker com sucesso!");
        } 
        else {
            Serial.println("Noo foi possivel se conectar ao broker.");
            Serial.println("Nova tentatica de conexao em 10s");
            delay(10000);
        }
    }
} // FIM CONECTA MQTT
void mantemConexoes() {
    if (!MQTT.connected()) {
       conectaMQTT(); 
    }
    
   conectaWiFi(); //se não há conexão com o WiFI, a conexão é refeita
} // FIM MATEM CONEXÔES

void conectaWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
     return;
  }
        
  Serial.print("Conectando-se na rede: ");
  Serial.print(SSID);
  Serial.println("  Aguarde!");

  WiFi.begin(SSID, PASSWORD); // Conecta na rede WI-FI  
  while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Conectado com sucesso, na rede: ");
  Serial.print(SSID);  
  Serial.print("  IP obtido: ");
  Serial.println(WiFi.localIP()); 
} //FIM CONECTA WIFI

void enviaValores() {
static bool estadoBotao1 = HIGH;
static bool estadoBotao1Ant = HIGH;
static unsigned long debounceBotao1;

  estadoBotao1 = digitalRead(pinBotao1);
  if (  (millis() - debounceBotao1) > 30 ) {  //Elimina efeito Bouncing
     if (!estadoBotao1 && estadoBotao1Ant) {

        //Botao Apertado     
        MQTT.publish(TOPIC_PUBLISH, "1");
        Serial.println("Botao1 APERTADO. Payload enviado.");
        
        debounceBotao1 = millis();
     } else if (estadoBotao1 && !estadoBotao1Ant) {

        //Botao Solto
        MQTT.publish(TOPIC_PUBLISH, "0");
        Serial.println("Botao1 SOLTO. Payload enviado.");
        
        debounceBotao1 = millis();
     }
     
  }
  estadoBotao1Ant = estadoBotao1;
} // Fim enviaValores
