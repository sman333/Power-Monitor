#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
//Send data as UNIXtimestamp to javascript as it is easier...
#include <string.h>
#include "RTClib.h"

RTC_DS1307 rtc;
DateTime now;
File f;
bool savOn = false, savOf = false;

const int ipPow = A0;
const int hibernateVolt = 510;
const int wakeUpVolt = 515;

/* wifi network ssid (name) & password */
const char* ssid = "Power_Monitor";
const char* password = "password";

#define DATAFILE "/power_data.js"

const char* dat_end_str = "'; if(typeof dataReady === 'function')dataReady();";
int dat_end_str_len = 50;

String powMonData, tmStr;

DNSServer dnsServer;      /* DNS server -- for captive portal */
AsyncWebServer server (80);   /* Web server */


void hibernate (int volt) {
	if (analogRead (ipPow) < volt) ESP.deepSleep (10e6);			/* 10 seconds - fn takes micro seconds argument, 10 x (10 ^ 6) , (e means 10 ^) */
}

void setup() {

	hibernate (wakeUpVolt);

  Serial.begin(115200);

  // Wire.begin(2,0);  //sda, scl
  Wire.begin();

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  if (!LittleFS.begin()) {
    Serial.println ("An error has occurred while mounting LittleFS");
    return;
  }else
    Serial.println ("LittleFS started");
  Serial.print ("Configuring soft-AP ... ");
  Serial.println (WiFi.softAP (ssid, password) ? "Ready" : "Failed!");          /* wifi soft access point */
  delay (500);
  Serial.print ("AP IP : ");
  Serial.println (WiFi.softAPIP());

  /* Setup the DNS server redirecting all the domains to the captive Portal IP */
  dnsServer.setErrorReplyCode (DNSReplyCode::NoError);
  dnsServer.start (53, "*", WiFi.softAPIP());

  /* setup route for web pages */
  server.on ("/", HTTP_GET, handleRoot);
  server.on ("/generate_204", HTTP_GET, handleRoot);    /* Android captive portal. */
  server.on ("/connecttest.txt", HTTP_GET, handleRoot);    /* Win 10 captive portal. */
  server.on ("/hotspot-detect.html", HTTP_GET, handleRoot);    /* IOS (apple) captive portal. */
  server.on ("/kindle-wifi/wifistub.html", HTTP_GET, handleRoot);    /* Kindle captive portal. */
  server.onNotFound (handleRoot);

  /* serve bootstrap resources */
  server.on ("/bootstrap.min.css", HTTP_GET, serveBootstrapCss);
  server.on ("/bootstrap.bundle.min.js", HTTP_GET, serveBootstrapJs);
  server.on ("/jquery-3.5.1.min.js", HTTP_GET, serveJquery);
  server.on ("/html2canvas.min.js", HTTP_GET, serveHtml2canvas);
  server.on ("/power_data.js", HTTP_GET, servePower_data);

  /* handle post request */
  server.on ("/documentReady", HTTP_POST, handle_documentReady);

  server.begin();    /* async web server start */
  Serial.println ("HTTP server started");

  dat_end_str_len = strlen (dat_end_str);
  appendOnTime();
}

void fmt (int x) {
  if (x < 10) tmStr += "0";
  tmStr += x;
}

void timeStr() { now = rtc.now();
  tmStr = String (now.year() % 2000);
  fmt (now.month());
  fmt (now.day());
  tmStr += " ";
  fmt (now.hour());
  fmt (now.minute());
  fmt (now.second());
}

void displayContents() {
  f = LittleFS.open (DATAFILE, "r");
  if (!f) Serial.println ("Unable to open file");
  else {
    String s;
    powMonData = "";
    Serial.println ("Contents of file");
    while (f.position() < f.size())
    {
      s = f.readStringUntil ('\n');
      s.trim();
      Serial.println (s);
      powMonData += (String) s;
    }
    f.close();
  }
}

void setTime (short Y, byte M, byte D, byte h, byte m, byte s) {
  rtc.adjust (DateTime (Y, M, D, h, m, s));
  Serial.println ("set RTC time");
  Serial.println (Y);
  Serial.println (M);
  Serial.println (D);
  Serial.println (h);
  Serial.println (m);
  Serial.println (s);
}

void serveBootstrapCss (AsyncWebServerRequest *request) {
  request->send (LittleFS, "/bootstrap.min.css", "text/css");
  Serial.println ("served bootstrap css");
}
void serveBootstrapJs (AsyncWebServerRequest *request) {
  request->send (LittleFS, "/bootstrap.bundle.min.js", "application/javascript");
  Serial.println ("served bootstrap bundle js");
}
void serveJquery (AsyncWebServerRequest *request) {
  request->send (LittleFS, "/jquery-3.5.1.min.js", "application/javascript");
  Serial.println ("served jquery-3.5.1");
}
void serveHtml2canvas (AsyncWebServerRequest *request) {
  request->send (LittleFS, "/html2canvas.min.js", "application/javascript");
  Serial.println ("served html2canvas js");
}
void servePower_data (AsyncWebServerRequest *request) {
  request->send (LittleFS, "/power_data.js", "application/javascript");
  Serial.println ("served power monitor log data js");
}

void handleRoot (AsyncWebServerRequest *request) {
  request->send (LittleFS, "/index.html", "text/html");
  Serial.println ("served index.html");
}

void handle_documentReady (AsyncWebServerRequest *request) {
  setTime ((short) request->arg ("Y").toInt(),
      (byte) request->arg ("M").toInt(),
      (byte) request->arg ("D").toInt(),
      (byte) request->arg ("h").toInt(),
      (byte) request->arg ("m").toInt(),
      (byte) request->arg ("s").toInt());
  request->send (200, "text/html", (String) analogRead (ipPow) + "," + tmStr);
  Serial.println ("document ready");
}

void appendOnTime() {
  if (!savOn && (700 < analogRead (ipPow))) {
    f = LittleFS.open (DATAFILE, "r+");
    if (!f) Serial.println ("Unable to open file");
    else {
      f.seek (dat_end_str_len + 2, SeekEnd);
	  if ('F' == (char)f.read()) {
		timeStr();
		f.seek (dat_end_str_len, SeekEnd);
		f.print (tmStr + "ON," + dat_end_str);
		f.close();
		savOn = true;
		savOf = false;
		Serial.println ("saved on time " + tmStr);
		displayContents();
	  }
    }
  }
}

void appendOffTime() {
  if (!savOf && (analogRead (ipPow) < 700)) {
    f = LittleFS.open (DATAFILE, "r+");
    if (!f) Serial.println ("Unable to open file");
    else {
      f.seek (dat_end_str_len + 2, SeekEnd);
	  if ('N' == (char)f.read()) {
		timeStr();
		f.seek (dat_end_str_len, SeekEnd);
		f.print (tmStr + "OF," + dat_end_str);
		f.close();
		savOf = true;
		savOn = false;
		Serial.println ("saved off time " + tmStr);
		displayContents();
	  }
    }
  }
}

void loop() {
  hibernate (hibernateVolt);
  timeStr();
  dnsServer.processNextRequest();     /* DNS Captive Portal */
  appendOffTime();
  delay (100);
  appendOnTime();
}


