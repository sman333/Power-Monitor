#include <ESP8266WiFi.h>
#include <HTTPSRedirect.h>
#include <string.h>
#include "RTClib.h"

IPAddress local_IP (192, 168, 1, 234);
IPAddress gateway (192, 168, 1, 1);
IPAddress subnet (255, 255, 255, 0);
IPAddress primaryDNS (8, 8, 8, 8);
IPAddress secondaryDNS (8, 8, 4, 4);
const char* ssid1 = "BSNL_FTTH";
const char* pass = "4712260337";

HTTPSRedirect* client = nullptr;
const char* GScriptId1 = "AKfycbwbrGKlZlxO0pqPcyIoBAYj-VQuHaTXBCBmVPp42-E783JDrpN--qYgmuuO8FNeWEtr";
const char* host1 = "script.google.com";
const int httpsPort = 443;
String url = String ("/macros/s/") + GScriptId1 + "/exec";
String reqArgs, resp;

const int ipPow = A0;
const int hibernateVolt = 510;
const int wakeUpVolt = 515;
bool savOn = false, savOf = true;
RTC_DS1307 rtc;
DateTime now;
const int BUF_LEN = 50;
String CIRC_BUFF[50], tmStr;
int rd = 0, wrt = 0;

bool hibernate (int volt) {
  if (analogRead (ipPow) < volt) {
    ESP.deepSleep (10e6);      /* 10 seconds - fn takes micro seconds argument, 10 x (10 ^ 6) , (e means 10 ^) */
    return true;
  }
  return false;
}

void setup() {
  if (hibernate (wakeUpVolt)) return;
  Serial.begin (115200);
  Wire.begin();
  Serial.println ('\n');
  if (!rtc.begin()) {
    Serial.println ("Couldn't find RTC");
    Serial.flush();
    abort();
  }
  if (!WiFi.config (local_IP, gateway, subnet, primaryDNS, secondaryDNS)) Serial.println ("STA Failed to configure");
  WiFi.begin (ssid1, pass);             
  Serial.print ("Connecting to " + (String) ssid1);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print (".");
    delay (500);
  }
  Serial.println('\n');
  Serial.print ("Connected @ IP: ");
  Serial.println (WiFi.localIP());
  appendOnTime();
}

void appendOffTime() {
  if (savOf || 700 < analogRead (ipPow)) return;
  timeStr();
  wrtBuf (tmStr + ",OFF");
  savOf = true;
  savOn = false;
}
void appendOnTime() {
  if (savOn || analogRead (ipPow) < 700) return;
  timeStr();
  wrtBuf (tmStr + ",ON");
  savOn = true;
  savOf = false;
}
void delBuf() {
  CIRC_BUFF[rd] = "";
  rd++;
  if (rd == wrt) {
    rd = 0;
    wrt = 0;
  }
  if (rd == BUF_LEN) rd = 0;
}
void fmt (int x) {
  if (x < 10) tmStr += "0";
  tmStr += x;
}
void sendData() {
  if (!(rd || wrt)) return;
  if (WiFi.status() != WL_CONNECTED) WiFi.begin (ssid1, pass);
  Serial.println ("Connecting to... " + (String) host1);
  client = new HTTPSRedirect (httpsPort);
  client->setInsecure();
  client->setPrintResponseBody (true);
  client->setContentTypeHeader ("application/json");
  if (!client->connected()) if (!client->connect (host1, httpsPort)) return;
  Serial.println ("Connected");
  reqArgs = "{\"values\":\"" + CIRC_BUFF[rd] + "\"}";
  Serial.println ("Sending request..." + (String) reqArgs);
  if (!client->POST (url, host1, reqArgs)) return;
  resp = client->getResponseBody();
  if (resp.substring (0, 7) == "Success") delBuf();
  rtc.adjust (DateTime (resp.substring (resp.length() - 12).toInt()));
  delete client;
  client = nullptr;
}
void timeStr() { now = rtc.now();
  tmStr = String (now.year());
  fmt (now.month());
  fmt (now.day());
  tmStr += ",";
  fmt (now.hour());
  fmt (now.minute());
  fmt (now.second());
}
void wrtBuf (String x) {
  CIRC_BUFF[wrt] = x;
  wrt++;
  if (wrt == BUF_LEN) wrt = 0;
}

void loop() {
  hibernate (hibernateVolt);
  appendOffTime();
  appendOnTime();
  yield();
  sendData();
  yield();
}


