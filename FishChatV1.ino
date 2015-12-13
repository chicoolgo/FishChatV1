
//  Version: 0.1  
//  (1)這個程式會在程式剛執行時，讀取記憶體(EPROM)內是否有帳號密碼，如果沒有帳號密碼，將裝置轉變為AP Mode，此時可透過FishChat_AP連上裝置，並在網址列輸入192.168.4.1， 
//     找到你想連線的無線網路，輸入在SSID和PASSWORD按儲存，畫面顯示儲存成功，即可重新開機，重新開機後會試著連線你剛輸入的SSID和PASSWORD, 若連上會顯示WiFi Connected.
//  (2)這個程式負責將溫度資料每隔 60 秒使用 HTTP GET 傳送到 ThingSpeak 做紀錄，並下載控制狀態命令，當狀態為2時，控制馬達轉動
//  (3)校正裝置時間，並透過APP傳入裝置餵食時間，並記錄在裝置記憶體內，
// Materials:
//  (1) ESP8266 Arduino IDE 開發板 ( 3V3 系統 )
//  (2)DS18B20溫度感應
//  (3)SG90 步進馬達
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
//#include <SPI.h>
//#include <SD.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <ArduinoJson.h>
#include <Servo.h>
#define _baudrate  115200
// Arduino數位腳位2接到1-Wire裝置
#define ONE_WIRE_BUS 2

const char* host = "esp8266";
const char* ssid = "FishChat_AP";
const char* passphrase = "12345678";
String content;
String st;
int statusCode;
//File uploadFile;

//#define offset 0x00    // SDD1306                      // offset=0 for SSD1306 controller

//馬達
Servo myservo;
const int MOTOR=0;
int cnt_MOTOR=0;

// 運用程式庫建立物件
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

#define HOST   "api.thingspeak.com" // ThingSpeak IP Address: 184.106.153.149
#define PORT  80
#define READAPIKEY  "VZ9VU6P7QQEYT0U5"  // READ APIKEY for the CHANNEL_ID

// 使用 GET 傳送資料的格式
// GET /update?key=[THINGSPEAK_KEY]&field1=[data 1]&filed2=[data 2]...;
String GET = "GET /update?key=KVGUZ141Q7D3NCXQ";
String Channel_ID= "58605";

// Create an instance of the server
// specify the port to listen on as an argument
ESP8266WebServer server(80);

void setup() {

  Serial.begin( _baudrate );
 // Serial.begin(115200);
  
  WiFi.mode(WIFI_AP);
  EEPROM.begin(512);
  Serial.println();
  Serial.println("The program startup..."); 

  // read eeprom for ssid and password
  Serial.println("Reading EEPROM SSID (MAX=32 BYTE)");
  String essid;
  for (int i = 0; i < 32; ++i) 
    {     
     essid += char(EEPROM.read(i));
    }

  Serial.print("SSID NAME : ");
  Serial.println(essid);
  char charBuf[32];
  essid.toCharArray(charBuf, 32);
  Serial.println("Reading EEPROM PASSWORD (MAX=64 BYTE)");
  String epassword = "";
  for (int i = 32; i < 96; ++i) 
    {     
      epassword += char(EEPROM.read(i));
    }
  Serial.print("PASSWORD NAME : ");
  Serial.println(epassword); 

  Serial.print("essid.length = ");
  Serial.println(essid.length());


  WiFi.begin(essid.c_str(), epassword.c_str());

  if (testWifi()) {

    Serial.print("WIFI connected by Hank");
    launchWeb(0);
    return;
    }
  SetupToAPMode(); //沒連上時執行這個

  //myservo.attach(3);
  myservo.attach(MOTOR, 500, 2400); // 修正脈衝寬度範圍
   myservo.write(90); // 一開始先置中90度
  // myservo.attach(3);  // attaches the servo on GIO2 to the servo object 
  
  // 溫度計初始化
  sensors.begin();
  delay(2000);
 
}


void loop() {

  server.handleClient();
  if (testWifi()) {
  Serial.print("WIFI connected by Hank");

  //取回狀態
   delay( 1000 );
   retrieveField( 58605, 2 );  // filed_id=2 是狀態

    delay( 1000 );
   // 要求匯流排上的溫度感測器
    sensors.requestTemperatures();
    // 取得溫度
    Serial.print( " Temperature:\n " );
    Serial.print(sensors.getTempCByIndex(0));
    Serial.println( "" );
    float temperature =sensors.getTempCByIndex(0);
   
   // 上傳溫度
    updateDHT11(temperature );
   
  // 每隔多久傳送一次資料
  delay( 60000 ); // 60 second
    }
    
  }

bool testWifi(void) {
  int c = 0;
  Serial.println("Waiting for Wifi to connect...");
  //delay 10秒讓wifi連線
  while ( c < 15 ) {
    if (WiFi.status() == WL_CONNECTED) { return true; } //return一執行就結束testwifi這個函數, 有連上就return true, 沒連上就return false
    delay(1000);
    Serial.print(WiFi.status());   
    c++;
  }
  Serial.println("");
  Serial.println("Connect timed out, ESP8266 change to AP mode");
  return false;
}

void launchWeb(int webtype) {
  Serial.println("");
  if(webtype) {
    Serial.println("Operation in AP Mode.");
  }
  else {
    Serial.println("WiFi connected.");
  }

  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
  createWebServer(webtype);
  // Start the server
  server.begin();
  Serial.println("Server started");

  if(WiFi.softAPIP()[0]==0 && WiFi.softAPIP()[1]==0 && WiFi.softAPIP()[2]==0 && WiFi.softAPIP()[3]==0) {   
    char LocalIP[16];
    sprintf(LocalIP, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
    }
  else {   

    char SoftAP[16];
    sprintf(SoftAP, "%d.%d.%d.%d", WiFi.softAPIP()[0], WiFi.softAPIP()[1], WiFi.softAPIP()[2], WiFi.softAPIP()[3]);
  }
  if (MDNS.begin(host)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("MDNS responder started");
    Serial.print("You can now connect to http://");
    Serial.print(host);
    Serial.println(".local");  
  }
}

void SetupToAPMode(void) {
  ScanNetwork();
  WiFi.softAP(ssid, passphrase, 6,0);
  Serial.println("Soft AP Starting...");
  launchWeb(1);
  Serial.println("The End.");
}

//設定無線網路帳號密碼的webservser
void createWebServer(int webtype)
{
  if ( webtype == 1 ) {  //ap mode
    server.on("/", []() {
        IPAddress ip = WiFi.softAPIP();
        content = "<!DOCTYPE HTML><html><title>FishChat無線網路設定</title><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><p>     你好, 請輸入您家裡的無線網路設定</p>";
        content += st;
        //&nbsp;代表塞一個空白, 重覆5個就是塞5個空白     
        content += "<br>";
        content += "<form method='get' action='setting'>";
        content += "<table border=\"0\"><tr><td><label>SSID</label></td><td><input type=\"text\" placeholder=\"請輸入要連接的SSID\" name='ssid' maxlength=32 size=64></td></tr>";
        content += "<tr><td><label>PASSWORD</label></td><td><input type=\"text\" placeholder=\"請輸入要連接的密碼\" name='pass' maxlength=64 size=64></td></tr></table>";
        content += "<input type=\"button\" value=\"重新掃瞄無線網路\" onclick=\"self.location.href='/rescannetwork'\">";
        content += "&nbsp;&nbsp;&nbsp;<input type='reset' value=\"重設\">&nbsp;&nbsp;&nbsp;<input type='submit' value=\"儲存\"></form></html>";
        server.send(200, "text/html", content);  //200代表伺服器回應OK, text/html代表用html網頁類型, 不加這個會找不到網頁
      });   

    server.on("/rescannetwork", []() {       
      content = "<!DOCTYPE HTML><html><title>FishChat無線網路設定</title><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><p>重新掃瞄無線網路中...</p>";
      content += "<Meta http-equiv=\"ReFresh\" Content=\"5; URL='/'\">"; //讓網頁5秒自動跳回root page
      server.send(200, "text/html", content);
      ScanNetwork();   
      });

  server.on("/setting", []() {
    String qsid = server.arg("ssid");
    String qpass = server.arg("pass");
    if (qsid.length() > 0 && qpass.length() > 0) {
      //EEPROM
      Serial.println("clearing eeprom");
      for (int i = 0; i < 96; ++i) { EEPROM.write(i, 0); }
      Serial.println(qsid);
      Serial.println("");
      Serial.println(qpass);
      Serial.println("");
        
      Serial.println("writing eeprom ssid:");
      for (int i = 0; i < qsid.length(); ++i)
        {
        EEPROM.write(i, qsid[i]);
        Serial.print("Wrote: ");
        Serial.println(qsid[i]);
        }
      Serial.println("writing eeprom pass:");
      for (int i = 0; i < qpass.length(); ++i)
        {
        EEPROM.write(32+i, qpass[i]);
        Serial.print("Wrote: ");
        Serial.println(qpass[i]);
        }   
      EEPROM.commit(); //EEPROM.write並不會馬上寫入EEPROM, 而是要執行EEPROM.commit()才會實際的寫入EEPROM
      content = "儲存成功, 請按RESET鍵重新開機!";
      statusCode = 200;
      }
    else {         
      content = "<p>輸入錯誤!!!SSID或PASSWORD任何其中一欄不能是空白, 按上一頁重新輸入</p>";
      content += "<input type=\"button\" value=\"上一頁\" onclick=\"self.location.href='/'\"></html>";
      //content = "{\"Error\":\"404 not found\"}";
      statusCode = 404;
      Serial.println("Sending 404");   
      }   
    server.send(statusCode, "text/html", content);     
    });
  }
    server.on("/cleareeprom", []() {                             
      for (int i = 0; i < 96; ++i) { EEPROM.write(i, 0); }
      EEPROM.commit();
      content = "<!DOCTYPE HTML>\r\n<html>";
      content += "<p>已清除無線網路設定</p></html>";
      server.send(200, "text/html", content);
      //Serial.println("clearing eeprom");
      });
    }
    
//掃描環境中的無線網路
void ScanNetwork() {
  WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    delay(100);
    int n = WiFi.scanNetworks();
    Serial.print("Scan Network Done...and ");
    if (n == 0)
      Serial.println("No Any Networks Found!");
    else
      {
      Serial.print(n);
      Serial.println(" Networks Found!");
      for (int i = 0; i < n; ++i)
        {
        // Print SSID and RSSI for each network found
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(WiFi.SSID(i));
        Serial.print(" (");
        Serial.print(WiFi.RSSI(i));
        Serial.print(")");
        byte encryption = WiFi.encryptionType(i);
        Serial.print(" Encryption Type:");
        switch (encryption) {
          case 2: Serial.println("TKIP(WPA)");break;
          case 5: Serial.println("WEP");break;
          case 4: Serial.println("CCMP(WPA)");break;
          case 7: Serial.println("NONE");break;
          case 8: Serial.println("AUTO(WPA or WPA2)");break;
          }     
        //Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
        delay(100);
      }
    }
  Serial.println("");
  String k;
  st = "<ol type=\"1\" start=\"1\">";
  for (int i = 0; i < n; ++i)
    {
    // Print SSID and RSSI for each network found
    st += "<table border=\"0\"><tr><td width=\"300px\">";
    k=String(i+1);
    st += k + ". ";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);
    st += ")";
    st += "</td><td width=\"200px\">";
    //st += (WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*";
    byte encryption = WiFi.encryptionType(i);
    switch (encryption) {
      case 2: st += "TKIP(WPA)";break;
      case 5: st += "WEP";break;
      case 4: st +="CCMP(WPA)";break;
      case 7: st +="NONE";break;
      case 8: st +="AUTO(WPA or WPA2)";break;
      } 
    //st += "</li></td></tr>"; 
    st += "</td></tr>";   
    }
  st += "</ol></table><br>";
  }

void updateDHT11(float t )
{
  // 設定 ESP8266 作為 Client 端
  WiFiClient client;
  if( !client.connect( HOST, PORT ) )
  {
    Serial.println( "connection failed" );
    return;
  }
  else
  {
  Serial.println( "update success" );
    // 準備上傳到 ThingSpeak IoT Server 的資料
    // 已經預先設定好 ThingSpeak IoT Channel 的欄位
    // field1：溫度
      String getStr = GET + "&field1=" + String((float)t) + 
       "&field2=" + String((int)0) +
                " HTTP/1.1\r\n";;


               
      client.print( getStr );
      client.print( "Host: api.thingspeak.com\n" );
      client.print( "Connection: close\r\n\r\n" );
      
        delay(10);
        //
        // 處理遠端伺服器回傳的訊息，程式碼可以寫在這裡！
        //

  }
}

void retrieveField( uint32_t channel_id, uint8_t field_id )
{
  // 設定 ESP8266 作為 Client 端
  WiFiClient client;
  if( !client.connect( HOST, PORT ) )
  {
    Serial.println( "connection failed" );
    return;
  }
  else
  {
    ////// 使用 GET 取回最後一筆 FIELD_ID 資料 //////

    // 有兩種方式可以取回 Field_id 中的最後一筆資料
    //
    //  NOTE: 
    //    If your channel is public, you don't need a key. 
    //    If your channel is private, you need to generate a read key.
    //
    
    /*** Method 2: ***/
    //
    //-- Get Last Entry in a Field Feed --//
    // To get the last entry in a Channel's field feed, send an HTTP GET to 
    //
    //  https://api.thingspeak.com/channels/CHANNEL_ID/fields/FIELD_ID/last.json
    //
    //  replacing CHANNEL_ID with the ID of your Channel and FIELD_ID with the ID of your field. 
    //
    // Example:
    //
    // https://api.thingspeak.com/channels/18795/fields/1/last.json?key=READAPIKEY012345
    //
    // DHT11, field 1: temperature, field 2: humidity
    //----
    String GET = "GET /channels/" + String(channel_id) + "/fields/" + String(field_id) + 
          "/last.json?key=" + READAPIKEY;
    //
    //----

    String getStr = GET + " HTTP/1.1\r\n";;
      client.print( getStr );
      client.print( "Host: api.thingspeak.com\n" );
      client.print( "Connection: close\r\n\r\n" );
      
        delay(10);
    
    // 讀取所有從 ThingSpeak IoT Server 的回應並輸出到串列埠
    String section="HEAD";
    while(client.available())
    {
      String line = client.readStringUntil('\r');

      //** parsing the JSON response here ! **//
      // parse the HTML body
      if(section == "HEAD" )  // HEAD
      {
        Serial.print( "." );
        if( line == "\n" )  // 空白行
        {
          section = "LENGTH";
        }
      }
      else if( section == "LENGTH" )
      {
        // 這裡可以取回 JSON　字串的長度
        // String content_length = line.substring(1);

        /* 有需要處理的寫程式在這裡 */

        section = "JSON";
      }
      else if( section == "JSON" )  // print the good stuff
      {
        Serial.println( "" );

        section = "END";
        /**************************
        {
        "created_at":"2015-08-28T14:08:14Z",
        "entry_id":25,
        "field1":"29.5"
        }
        ***************************/
        String jsonStr = line.substring(1); // 給定一個從索引到尾的字串

        // 開始解析 JSON
        int size = jsonStr.length() + 1;
        char json[size];
        jsonStr.toCharArray(json, size);
        Serial.println( json );
        StaticJsonBuffer<200> jsonBuffer;
        JsonObject& jsonParsed = jsonBuffer.parseObject(json);
        if (!jsonParsed.success())
        {
          Serial.println("parseObject() failed");
          return;
        }
        
        const char *createdat = jsonParsed["created_at"];
        int entryid = jsonParsed["entry_id"];
  //      float field1 = jsonParsed["field1"];
        float field2 = jsonParsed["field2"];
        Serial.println("-- Decoding / Parsing --");
        Serial.print( "Created at: " ); Serial.println( createdat );
        Serial.print( "Entry id: " ); Serial.println( entryid );
  //      Serial.print( "Field 1: " ); Serial.println( field1, 1 );
        Serial.print( "Field 2: " ); Serial.println( field2, 2 );
        
      //當抓取當狀態是2的時候,轉動馬達
      if(field2==2){
          Serial.println("MOTOR Turning");      
     myservo.write(90); // 一開始先置中90度
     //旋轉馬達180度後反轉180度
         MOTOR_C(1);
         MOTOR_C(0);  
  
      }
      /////
      }   
    } 
    client.stop();  
  }
}
void MOTOR_C(int a)
{
  if (a == 0)
  {
    for(int i = 0; i <= 180; i+=1)
    {
      myservo.write(i); // 使用write，傳入角度，從0度轉到180度
      delay(10);
    }
  }
  else if (a==1)
  {
    for(int i = 180; i >= 0; i-=1)
    {
      myservo.write(i);// 使用write，傳入角度，從180度轉到0度
      delay(10);
    }
  }
  else
  {
    myservo.write(90); // 一開始先置中90度
  }  
}
