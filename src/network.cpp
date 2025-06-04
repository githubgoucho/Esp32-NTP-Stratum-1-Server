#include "datastore.h"
#include "webfunctions.h"
#include <ArduinoJson.h>

#include <WebServer.h>
#include <ESPmDNS.h> // Include the mDNS library
#include <FS.h>
#include <SPIFFS.h>
#include "network.h"

String APSSID = "Mini NTP Server";
String m_wifiHostname = "mini-ntp-server"; // STA Mode

String ssid = "";
String pass = "";
IPAddress ip;
bool IsAP = false;

WebServer *server = NULL;

int WifiGetRssiAsQuality(int rssi)
{
  int quality = 0;

  if (rssi <= -100)
  {
    quality = 0;
  }
  else if (rssi >= -50)
  {
    quality = 100;
  }
  else
  {
    quality = 2 * (rssi + 100);
  }
  return quality;
}

/**************************************************************************************************
 *    Function      : SSIDList
 *    Description   : Returns the SSID List
 *    Input         : String separator
 *    Output        : String
 *    Remarks       : none
 **************************************************************************************************/
void SSIDList(JsonArray data)
{
  Serial.println("Scanning networks");
  // String ssidList;
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++)
  {
    String ssid = WiFi.SSID(i);
    Serial.println(String(i) + ": " + ssid);
    data.add(ssid);
  }
  Serial.print("Available networks ");
  Serial.println(n);
}

/**************************************************************************************************
 *    Function      : getSSIDList
 *    Description   : Sends a SSID List to the client
 *    Input         : none
 *    Output        : none
 *    Remarks       : none
 **************************************************************************************************/
void getSSIDList()
{
  String response;
  Serial.println("SSID list requested");
  DynamicJsonDocument root(2048);
  SSIDList(root.createNestedArray("data"));

  serializeJson(root, response);
  Serial.println("getSSIDList()");
  sendData(response);
}

/**************************************************************************************************
 *    Function      : setWiFiSettings
 *    Description   : Applies the WiFi settings
 *    Input         : none
 *    Output        : none
 *    Remarks       : Store the wifi settings configured on the webpage and restart the esp to connect to this network
 **************************************************************************************************/
void setWiFiSettings()
{
  credentials_t c = read_credentials();
  Serial.println("WiFi settings received, clear = " + server->arg("clear"));
  ssid = server->arg("ssid");
  pass = server->arg("pass");
  bool clear = false;

  if (server->arg("clear") == "true")
    clear = true;
  String response;
  if (clear)
  {
    response = "Current Network cleared. The Server will restarts in AP Mode! Connect to '" + APSSID + "' and select Network ...";
    c.ssid[0] = 0;
    c.pass[0] = 0;
  }
  else
  {
    response = "Attempting to connect to '" + ssid + "'. The WiFi module restarts and tries to connect to the network.";
    strncpy((char *)(c.ssid), (char *)(ssid.c_str()), 128);
    strncpy((char *)(c.pass), (char *)(pass.c_str()), 128);
  }
  sendData(response);
  Serial.println("Saving network credentials and restart.");

  Serial.printf("write ssid:%s ,pass:%s ,host:%s \n\r", c.ssid, c.pass, c.hostname);
  write_credentials(c);

  c = read_credentials();
  Serial.printf("read ssid:%s ,pass:%s ,host:%s \n\r", c.ssid, c.pass, c.hostname);
  /* if we do this we end up in flashloader */
  WiFi.softAPdisconnect(true);
  delay(2000);
  /* Any eSP8266 fix goes here */
  ESP.restart();
}

void setHostname()
{
  Serial.println("Hostname settings received");
  m_wifiHostname = server->arg("hostname");
  String response = "Attempting to connect to host '" + m_wifiHostname + "'. The WiFi module restarts and tries to connect to the network.";
  sendData(response);
  Serial.println("Saving hostname restart.");

  credentials_t c = read_credentials();
  strncpy((char *)(c.hostname), (char *)(m_wifiHostname.c_str()), 128);

  Serial.printf("write ssid:%s ,pass:%s ,host:%s \n\r", c.ssid, c.pass, c.hostname);
  write_credentials(c);

  c = read_credentials();
  Serial.printf("read ssid:%s ,pass:%s ,host:%s \n\r", c.ssid, c.pass, c.hostname);
  /* if we do this we end up in flashloader */
  WiFi.softAPdisconnect(true);
  delay(2000);
  /* Any eSP8266 fix goes here */
  ESP.restart();
}

// send the wifi settings to the connected client of the webserver
void getWiFiSettings()
{
  String response;
  Serial.println("WiFi settings requested");
  credentials_t crd = read_credentials();

  DynamicJsonDocument root(2048);

  root["ssid"] = crd.ssid;

  SSIDList(root.createNestedArray("data"));

  serializeJson(root, response);
  Serial.println("getWiFiSettings()");
  sendData(response);
}

/**************************************************************************************************
 *    Function      : setWiFiSettings
 *    Description   : Applies the WiFi settings
 *    Input         : none
 *    Output        : none
 *    Remarks       : restart the esp as requested on the webpage
 **************************************************************************************************/
void restart()
{
  sendData("Restart in progress\nyou will be disconnected from the " + APSSID + "\nPlease reload page!");
  /* delay(1000);
Any fixes for ESP8266 may be here */
  ESP.restart();
}

/**************************************************************************************************
 *    Function      : getContentType
 *    Description   : Gets the contenttype depending on a filename
 *    Input         : String
 *    Output        : String
 *    Remarks       : none
 **************************************************************************************************/
String getContentType(String filename)
{
  if (server->hasArg("download"))
    return "application/octet-stream";
  else if (filename.endsWith(".htm"))
    return "text/html";
  else if (filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".js"))
    return "application/javascript";
  else if (filename.endsWith(".png"))
    return "image/png";
  else if (filename.endsWith(".gif"))
    return "image/gif";
  else if (filename.endsWith(".jpg"))
    return "image/jpeg";
  else if (filename.endsWith(".ico"))
    return "image/x-icon";
  else if (filename.endsWith(".xml"))
    return "text/xml";
  else if (filename.endsWith(".pdf"))
    return "application/x-pdf";
  else if (filename.endsWith(".zip"))
    return "application/x-zip";
  else if (filename.endsWith(".gz"))
    return "application/x-gzip";
  return "text/plain";
}

/**************************************************************************************************
 *    Function      : sendFile
 *    Description   : Sends a requestedfile from SPIFFS
 *    Input         : none
 *    Output        : none
 *    Remarks       : send a file from the SPIFFS to the connected client of the webserver
 **************************************************************************************************/
void sendFile()
{
  String path = server->uri();
  Serial.println("Got request for: " + path);
  if (path.endsWith("/"))
    path += "index.html";
  String contentType = getContentType(path);
  Serial.println("Content type: " + contentType);
  if (SPIFFS.exists(path))
  {
    File file = SPIFFS.open(path, "r");
    server->streamFile(file, contentType);
    file.close();
    Serial.println("File " + path + " found & send");
  }
  else
  {
    Serial.println("File '" + path + "' doesn't exist");
    server->send(404, "text/plain", "The requested file doesn't exist");
  }
}

/**************************************************************************************************
 *    Function      : sendFile
 *    Description   : Sends a requestedfile from SPIFFS
 *    Input         : String
 *    Output        : none
 *    Remarks       : send data to the connected client of the webserver
 **************************************************************************************************/
void sendData(String data)
{
  server->send(200, "text/plain", data);
  Serial.println(data);
}

/**************************************************************************************************
 *    Function      : initWiFi
 *    Description   : initializes the WiFi
 *    Input         : String
 *    Output        : none
 *    Remarks       : initialize wifi by connecting to a wifi network or creating an accesspoint
 **************************************************************************************************/

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.println("Connected to AP successfully!");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.println("WiFi connected event");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);
}

void initWiFi()
{

  credentials_t c = read_credentials();
  // c.ssid[0] = 0;  write_credentials(c); //reset

  Serial.print("WiFi: ");

  if (0 == c.ssid[0]) // 1
  {
    // only once on init
    strncpy((char *)(c.hostname), (char *)(m_wifiHostname.c_str()), 128);
    Serial.printf("init hostname: %s  \n\r", c.hostname);
    write_credentials(c);
    Serial.println("AP");

    configureSoftAP();
  }
  else
  {
    Serial.println("STA");

    ssid = String(c.ssid);
    pass = String(c.pass);
    /*
        strncpy((char *)(c.hostname), (char *)(m_wifiHostname.c_str()), 128);
        Serial.printf("write hostname:%s  \n\r", c.hostname);
        write_credentials(c);
    */

    m_wifiHostname = String(c.hostname);
    Serial.printf("NTP-server hostname: %s\n", m_wifiHostname.c_str());

    if (true == connectWiFi())
    {
      configureServer();
    }
    else
    {
      configureSoftAP();
    }
  }
}

/**************************************************************************************************
 *    Function      : connectWiFi
 *    Description   : trys to establish a WiFi connection
 *    Input         : none
 *    Output        : bool
 *    Remarks       : connect the esp to a wifi network, retuns false if failed
 **************************************************************************************************/
void WifiSetMode(WiFiMode_t wifi_mode)
{
  if (WiFi.getMode() == wifi_mode)
  {
    return;
  }

  uint32_t retry = 2;
  while (!WiFi.mode(wifi_mode) && retry--)
  {
    Serial.println("Retry set Mode...");
    delay(100);
  }
}

int reconnectWifi = millis() + 1000 * 30;

bool connectWiFi()
{
  ipv4_settings nws;
  nws = read_ipv4_settings();

  if (ssid == "")
  {
    Serial.println("SSID unknown");

    return false;
  }
  //  Disable AP mode
  WifiSetMode(WIFI_STA);

  /*
 WiFi.setHostname - this seems to work
  WiFi.disconnect(true);  // Delete SDK wifi config
  delay(200);
   */

  WiFi.persistent(false); // Solve possible wifi init errors (re-address see TASMOTA at 6.2.1.16 #4044, #4083)
  // delete old config
  WiFi.disconnect(true);

  delay(1000);

  Serial.println("Attempting to connect to " + ssid + ", pass: xxxxxxxxxx "); // + pass
  if (false != nws.use_static)
  {
    IPAddress local_IP(nws.address);
    IPAddress gateway(nws.gateway);
    IPAddress subnet(nws.subnet);
    IPAddress primaryDNS(nws.dns0);   // optional
    IPAddress secondaryDNS(nws.dns1); // optional
    Serial.printf("use static local_IP: %s\n", local_IP.toString().c_str());

    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
    {
      Serial.println("STA Failed to configure");
    }
  }
  else
  {
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  }
  delay(15);
  if (WiFi.setHostname(m_wifiHostname.c_str()))
  {
    Serial.printf("\nHostname: %s\n", WiFi.getHostname());
  }
  //  WiFi.hostname(m_wifiHostname.c_str());

  WiFi.begin((char *)ssid.c_str(), (char *)pass.c_str(), 0);

  /*
  for (int timeout = 0; timeout < 15; timeout++)
  { // max 15 seconds
    int status = WiFi.status();
    if ((status == WL_CONNECTED) || (status == WL_NO_SSID_AVAIL) || (status == WL_CONNECT_FAILED))
      break;
    Serial.print(".");

    delay(1000);
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Failed to connect to " + ssid);
    Serial.println("Second attempt " + ssid);
    WiFi.disconnect(true);
    WiFi.begin((char *)ssid.c_str(), (char *)pass.c_str());
    for (int timeout = 0; timeout < 15; timeout++)
    { // max 15 seconds
      int status = WiFi.status();
      if ((status == WL_CONNECTED) || (status == WL_NO_SSID_AVAIL) || (status == WL_CONNECT_FAILED))
        break;
      Serial.print(".");
      delay(1000);
    }
  }
*/
  int count = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    count++;
    if (count > 50)
    {
      // ESP.restart();
      reconnectWifi = millis() + 1000 * 300;
      break;
    }
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Failed to connect to " + ssid);
    Serial.println("WiFi status: " + WiFiStatusToString());

    WiFi.disconnect();

    return false;
  }
  Serial.println("Connected to " + ssid);
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("gatewayIP: ");
  Serial.println(WiFi.gatewayIP());

  Serial.print("dnsIP: ");
  Serial.println(WiFi.dnsIP());

  Serial.print("Hostname: ");
  Serial.println(WiFi.getHostname());
  int wifiq = WifiGetRssiAsQuality(WiFi.RSSI());
  Serial.printf("Wifi Quality %d%%, %d dBm\n", wifiq, WiFi.RSSI());
  Serial.printf("Mac: %s \n", WiFi.macAddress().c_str());
  // Set up mDNS responder:
  // - first argument is the domain name, in this example
  //   the fully-qualified domain name is "esp32.local"
  // - second argument is the IP address to advertise
  //   we send our IP address on the WiFi network
  /*
  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  if (!MDNS.begin(WiFi.getHostname())) {
      Serial.println("Error setting up MDNS responder!");
      while(1) {
          delay(1000);
      }
  }
  Serial.println("mDNS responder started");
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
*/
  return true;
}

/**************************************************************************************************
 *    Function      : configureSoftAP
 *    Description   : Configures the ESP as SoftAP at IP:  192, 168, 4, 1
 *    Input         : none
 *    Output        : none
 *    Remarks       : configure the access point of the esp
 **************************************************************************************************/
void configureSoftAP()
{
  IsAP = true;
  Serial.println("Configuring AP: " + String(APSSID));

  /* This seems to stop crashing the ESP32 if in SoftAP mode */
  WiFi.disconnect(true);
  WiFi.enableAP(true);
  delay(100);
   WiFi.mode((WiFiMode_t)(WiFi.getMode() | WIFI_AP));
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
 // WiFi.softAP(APSSID, NULL, 1, 0, 1);
  delay(1500); // Without delay the IP address is sometimes blank

  Serial.print("AP IP: ");
  Serial.println(ip);

  // Start the mDNS responder for ntpserver.local
  if (!MDNS.begin("ntpserver"))
  {
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.println("mDNS responder started");
  configureServer();
  ip = WiFi.softAPIP();
}

/**************************************************************************************************
 *    Function      : configureServer
 *    Description   : Setup for the Webserver
 *    Input         : none
 *    Output        : none
 *    Remarks       : initialize the webserver on port 80
 **************************************************************************************************/
void configureServer()
{
  server = new WebServer(80);
  server->on("/setHostname", HTTP_GET, setHostname);
  server->on("/clients.dat", HTTP_GET, read_clients);
  server->on("/setWiFiSettings", HTTP_GET, setWiFiSettings);
  server->on("/getWiFiSettings", HTTP_GET, getWiFiSettings);
  server->on("/getSSIDList", HTTP_GET, getSSIDList);
  server->on("/restart", HTTP_GET, restart);
  server->on("/timesettings", HTTP_GET, response_settings);
  server->on("/settime.dat", HTTP_POST, settime_update);   /* needs to process date and time */
  server->on("/timezone.dat", timezone_update);            /* needs to handel timezoneid */
  server->on("/overrides.dat", timezone_overrides_update); /* needs to handle DLSOverrid,  ManualDLS, dls_offset, ZONE_OVERRRIDE and GMT_OFFSET */
  server->on("/notes.dat", HTTP_GET, read_notes);
  server->on("/notes.dat", HTTP_POST, update_notes);
  server->on("/gps/syncclock.dat", HTTP_POST, update_gps_syncclock);
  server->on("/gps/data", HTTP_GET, getGPS_Location);
  server->on("/display/settings", HTTP_GET, send_display_settings);
  server->on("/display/settings", HTTP_POST, update_display_settings);
  server->on("/ipv4settings.json", HTTP_GET, getipv4settings_settings);
  server->on("/ipv4settings.json", HTTP_POST, update_ipv4_settings);
  server->onNotFound(sendFile); // handle everything except the above things
  server->begin();
  Serial.println("Webserver started");
}

/**************************************************************************************************
 *    Function      : WiFiStatusToString
 *    Description   : Gives a string representing the WiFi status
 *    Input         : none
 *    Output        : none
 *    Remarks       : none
 **************************************************************************************************/
String WiFiStatusToString()
{
  switch (WiFi.status())
  {
  case WL_IDLE_STATUS:
    return "IDLE";
    break;
  case WL_NO_SSID_AVAIL:
    return "NO SSID AVAIL";
    break;
  case WL_SCAN_COMPLETED:
    return "SCAN COMPLETED";
    break;
  case WL_CONNECTED:
    return "CONNECTED";
    break;
  case WL_CONNECT_FAILED:
    return "CONNECT_FAILED";
    break;
  case WL_CONNECTION_LOST:
    return "CONNECTION LOST";
    break;
  case WL_DISCONNECTED:
    return "DISCONNECTED";
    break;
  case WL_NO_SHIELD:
    return "NO SHIELD";
    break;
  default:
    return "Undefined: " + String(WiFi.status());
    break;
  }
}

/**************************************************************************************************
 *    Function      : NetworkTask
 *    Description   : All things that needs to be done in the superloop
 *    Input         : none
 *    Output        : none
 *    Remarks       : none
 **************************************************************************************************/
void NetworkTask()
{

  if (server != NULL)
  {
    server->handleClient();
  }
  if (WiFi.status() != WL_CONNECTED && (reconnectWifi - millis() <= 0))
  {
    Serial.printf("restart:  %d \n", reconnectWifi - millis());
    ESP.restart();
  }
}
