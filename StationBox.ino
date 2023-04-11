#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <TridentTD_LineNotify.h>

#define SECRET_WRITE_APIKEY "xxxx" //thingspeak api key
#define LINE_TOKEN "xxxx" //line notify token
#define WIFI_NAME "xxxx" //wifi name for connecting
#define WIFI_PASSWORD "xxxx" //wifi password for connecting
#define REPORTING_PERIOD_MS     1000
 
PulseOximeter pox;
uint32_t tsLastReport = 0;
 
const int oneWireBus = 13; //data of temperature sensor     

//Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

//Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

LiquidCrystal_I2C lcd(0x27,16,2);  //set the LCD address to 0x27 for a 16 chars and 2 line display

float o2_data;
float bpm_data;
float temperatureC0;
int i = 0;
String server = "api.thingspeak.com";
const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {19, 18, 5, 17}; //keypad rows pin
byte colPins[COLS] = {16, 4, 0, 2}; //keypad cols pin

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

void setup(){
  Serial.begin(115200);

  lcd.init(); //initialize the lcd 
  Serial.print("Initializing pulse oximeter..");
  
  lcd.backlight();

  pinMode(13,INPUT_PULLUP);
  sensors.begin();

  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  Serial.printf("WiFi connecting ", WIFI_NAME);
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    lcd.clear();
    delay(500);
  }
  Serial.printf("\nWiFi connected\nIP : ");
  Serial.println(WiFi.localIP());
  showMenuKeypad(); //print a menu to the LCD
  LINE.setToken(LINE_TOKEN);
  
  //initialize the PulseOximeter instance
  //failures are generally due to an improper I2C wiring, missing power supply or wrong target chip
  if (!pox.begin()) {
      Serial.println("FAILED");
  } else {
      Serial.println("SUCCESS");
  }
  pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
 
  //register a callback for the beat detection
  pox.setOnBeatDetectedCallback(onBeatDetected); 
}

void loop(){
  char key = keypad.getKey();

  pox.update();

  //for detected heart rate and oxigen
  if (millis() - tsLastReport > REPORTING_PERIOD_MS) { 
    Serial.print("Heart rate:");
    Serial.print(pox.getHeartRate());
    Serial.print("bpm / SpO2:");
    Serial.print(pox.getSpO2());
    Serial.println("%");

    tsLastReport = millis();

  }
  //for show temperature data to the LCD       
  else if (key == '1'){
    Serial.println(key);
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("bpm:");
      lcd.print(pox.getHeartRate());
      bpm_data = pox.getHeartRate();
      lcd.print(" O2:");     
      lcd.print(pox.getSpO2());
      o2_data = pox.getSpO2();
      lcd.print("%");
      lcd.setCursor(0,1);
      // lcd.print("*)menu A)notify"); 
      lcd.print("A.notify D.menu"); 
  }
  //for detected tempurature       
  else if (key == '2'){
    Serial.println(key);
    showTempLCD();
  }
  //for send heart rate and oxigen to line notify and thingspeak  
  else if (key == 'A'){
    if (i == 0){
      Serial.println(key);
      i = i + 1;           
      sendOxigenBloodToLine();}
    else if (i == 1){
      Serial.println(key);
      LINE.notify("หากต้องการวัดการเต้นของหัวใจและออกซิเจนอีกครั้ง กรุณากด # ");      
    }            
  }
  //for send tempurature to line notify and thingspeak  
  else if (key == 'B'){
    Serial.println(key);     
    sendTempToLine();        
  }
  //for back to menu      
  // else if (key == '*'){
  else if (key == 'D'){
    Serial.println(key);
    showMenuKeypad();
  } 
  //for restart esp
  else if (key == '#'){
    Serial.println(key);
    ESP.restart();
  }
}

//send temperature data to line notify
void sendTempToLine(){
  //send to line when normal body temperature
  if ( (temperatureC0 < 37.5) && (temperatureC0 > 35.5)){
    LINE.notify("อุณหภูมิปกติ : "+String(temperatureC0,2)+" C");
  }
  //send to line when high body temperature
  else if ( temperatureC0 >= 37.5){
    LINE.notify("อุณหภูมิสูงกว่าปกติ : "+String(temperatureC0,2)+" C");
  }
  //send to line when low body temperature
  else if ( temperatureC0 <= 35.5){
    LINE.notify("อุณหภูมิต่ำกว่าปกติ : "+String(temperatureC0,2)+" C");
  }  
  SendWriteTempRequestToThingSpeak();
  delay(1000);
}

//send heart rate and oxigen data to line notify
void sendOxigenBloodToLine(){
  //send to line when normal heart rate and normal oxigen
  if ( (o2_data >= 90.0) && (bpm_data >= 60.0) && (bpm_data < 100.0)){
    LINE.notify("การเต้นของหัวใจและออกซิเจนอยู่ในเกณฑ์ปกติ " + String("Heart rate : ") + String(bpm_data) + String(" bpm " )+ String(", O2 : ") + String(o2_data) + String("%"));
    SendWriteOxigenBloodRequestToThingSpeak();
  }
  //send to line when high heart rate and normal oxigen
  else if ( (o2_data >= 90.0) && (bpm_data >= 100.0) && (bpm_data <= 150.0)){
    LINE.notify("การเต้นของหัวใจเร็วกว่าปกติและออกซิเจนอยู่ในเกณฑ์ปกติ " + String("Heart rate : ") + String(bpm_data) + String(" bpm " )+ String(", O2 : ") + String(o2_data) + String("%"));
    SendWriteOxigenBloodRequestToThingSpeak();
  }
  //send to line when risk heart rate and normal oxigen
  else if ( (o2_data >= 90.0) && (bpm_data >159.0)){
    LINE.notify("การเต้นของหัวใจอยู่ในภาวะเร็วกว่าปกติเข้าขั้นอันตรายและออกซิเจนอยู่ในเกณฑ์ปกติ " + String("Heart rate : ") + String(bpm_data) + String(" bpm " )+ String(", O2 : ") + String(o2_data) + String("%"));
    SendWriteOxigenBloodRequestToThingSpeak();
  }
  //send to line when low heart rate and normal oxigen
  else if ( (o2_data >= 90.0) && (bpm_data < 60.0) && (bpm_data > 0.00) ){
    LINE.notify("การเต้นของหัวใจช้ากว่าปกติและออกซิเจนอยู่ในเกณฑ์ปกติ " + String("Heart rate : ") + String(bpm_data) + String(" bpm " )+ String(", O2 : ") + String(o2_data) + String("%"));
    SendWriteOxigenBloodRequestToThingSpeak();
  }
  //send to line when normal heart rate and low oxigen
  else if ( (o2_data < 90.0) && (bpm_data >= 60.0) && (bpm_data < 100.0)){
    LINE.notify("การเต้นของหัวใจอยู่ในเกณฑ์ปกติและออกซิเจนอยู่ในเกณฑ์ต่ำกว่าปกติ " + String("Heart rate : ") + String(bpm_data) + String(" bpm " )+ String(", O2 : ") + String(o2_data) + String("%"));
    SendWriteOxigenBloodRequestToThingSpeak();
  }
  //send to line when high heart rate and low oxigen
  else if ( (o2_data < 90.0) && (bpm_data >= 100.0) && (bpm_data <= 150.0)){
    LINE.notify("การเต้นของหัวใจเร็วกว่าปกติและออกซิเจนอยู่ในเกณฑ์ต่ำกว่าปกติ " + String("Heart rate : ") + String(bpm_data) + String(" bpm " )+ String(", O2 : ") + String(o2_data) + String("%"));
    SendWriteOxigenBloodRequestToThingSpeak();
  }
  //send to line when risk heart rate and low oxigen
  else if ( (o2_data < 90.0) && (bpm_data >159.0)){
    LINE.notify("การเต้นของหัวใจอยู่ในภาวะเร็วกว่าปกติเข้าขั้นอันตรายและออกซิเจนอยู่ในเกณฑ์ต่ำกว่าปกติ " + String("Heart rate : ") + String(bpm_data) + String(" bpm " )+ String(", O2 : ") + String(o2_data) + String("%"));
    SendWriteOxigenBloodRequestToThingSpeak();
  }
  //send to line when low heart rate and low oxigen
  else if ( (o2_data < 90.0) && (bpm_data <60.0) && (bpm_data > 0.00) && (o2_data > 0)){
    LINE.notify("การเต้นของหัวใจช้ากว่าปกติและออกซิเจนอยู่ในเกณฑ์ต่ำกว่าปกติ " + String("Heart rate : ") + String(bpm_data) + String(" bpm " )+ String(", O2 : ") + String(o2_data) + String("%"));
    SendWriteOxigenBloodRequestToThingSpeak();
  }
  //send to line when abnornal detected
  else if ((bpm_data == 0.00)){
    LINE.notify("การตรวจสอบผิดปกติ " + String("Heart rate : ") + String(bpm_data) + String(" bpm " )+ String(", O2 : ") + String(o2_data) + String("%"));
    LINE.notify("กรุณากด # เพื่อลองอีกครั้ง");
  }
  else if ((o2_data == 0)){
    LINE.notify("การตรวจสอบผิดปกติ " + String("Heart rate : ") + String(bpm_data) + String(" bpm " )+ String(", O2 : ") + String(o2_data) + String("%"));
    LINE.notify("กรุณากด # เพื่อลองอีกครั้ง");
  }
  else if ( (o2_data == 0) && (bpm_data == 0.00)){
    LINE.notify("การตรวจสอบผิดปกติ " + String("Heart rate : ") + String(bpm_data) + String(" bpm " )+ String(", O2 : ") + String(o2_data) + String("%"));
    LINE.notify("กรุณากด # เพื่อลองอีกครั้ง");
  }
  
  delay(1000);
}

//send heart rate and oxigen data to thingspeak
void SendWriteOxigenBloodRequestToThingSpeak(){
  HTTPClient https;
  //Prepare Http
  String STR_HTTP = "https://" + server + "/update?api_key=";
  STR_HTTP += SECRET_WRITE_APIKEY;
  STR_HTTP += "&field2=";
  STR_HTTP += String(o2_data);
  STR_HTTP += "&field4=";
  STR_HTTP += String(bpm_data);
  //send http request
  if (https.begin(STR_HTTP)){
    // HTTPS
    Serial.print("[HTTPS] GET...\n");
    // start connection and send HTTP header
    int httpCode = https.GET();
    // httpCode will be negative on error
    if (httpCode > 0){
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
      if (httpCode == 200){
        Serial.println("Channel update successful.");
        }
      else{
        Serial.println("Problem updating channel. HTTP error code " + String(httpCode));
      }
    }
    else{
      Serial.printf("[HTTPS] GET... failed, error:%s\n", https.errorToString(httpCode).c_str());
  }
    https.end();
  }
  else{
    Serial.printf("[HTTPS] Unable to connect\n");
  }  
}

//send temperature data to thingspeak
void SendWriteTempRequestToThingSpeak(){
  HTTPClient https;
  //Prepare Http
  String STR_HTTP = "https://" + server + "/update?api_key=";
  STR_HTTP += SECRET_WRITE_APIKEY;
  STR_HTTP += "&field1=";
  STR_HTTP += String(temperatureC0);
  //send http request
  if (https.begin(STR_HTTP)){
    // HTTPS
    Serial.print("[HTTPS] GET...\n");
    // start connection and send HTTP header
    int httpCode = https.GET();
    // httpCode will be negative on error
    if (httpCode > 0){
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
      if (httpCode == 200){
        Serial.println("Channel update successful.");
        }
      else{
        Serial.println("Problem updating channel. HTTP error code " + String(httpCode));
      }
    }
    else{
      Serial.printf("[HTTPS] GET... failed, error:%s\n", https.errorToString(httpCode).c_str());
  }
    https.end();
  }
  else{
    Serial.printf("[HTTPS] Unable to connect\n");
  }  
}

//show temperature data to the LCD
void showTempLCD(){
  lcd.clear();
  sensors.requestTemperatures(); 
  temperatureC0 = sensors.getTempCByIndex(0);

  lcd.setCursor(0,0);
  lcd.print(temperatureC0);
  lcd.print("C");
  lcd.setCursor(0,1);
  // lcd.print("*.menu B.notify");
  lcd.print("B.notify D.menu");  
}

//show menu to the LCD
void showMenuKeypad(){
  lcd.clear();
  lcd.setCursor(0,0);
  // lcd.print("(1) Heart rate & O2");
  lcd.print("1. BPM & SPO2");
  lcd.setCursor(0,1);
  lcd.print("2. Temperature");  
}

//when detected finger but don't beat
void onBeatDetected()
{
    Serial.println("Beat!");
}
