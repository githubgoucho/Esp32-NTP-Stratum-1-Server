/*

    You should have received a copy of the GNU General Public License
    along with Firmware for Elektorproject 180662.  If not, see <https://www.gnu.org/licenses/>.

*/

/*
 * Hardware used:
 *
 *  ESP32 based module from Elektor ( https://www.elektor.com/wemos-lolin-esp32-oled-module-with-wifi )
 *  GPS Receiver ( https://www.elektor.com/open-smart-gps-serial-gps-module-for-arduino-apm2-5-flight-control )
 *  DS3231 bases I²C RTC Module, e.g those used for the Pi
 *
 *  License used: GPLv3
 *
 *
 *  start ATL Developement 08/07/2023
 *
 *  Version 2.0
 *
 *
 */
//
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_wifi.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Wire.h>
#include <ESPmDNS.h>

#include <TimeLib.h>
#include <Ticker.h>
#include <TinyGPS++.h>
// #include <RTClib.h>

// #include <U8g2lib.h>
// Include the correct display library
// For a connection via I2C using Wire include
#include "display.h" // Only needed for Arduino 1.6.5 and earlier

#include "ArduinoJson.h"

#include "datastore.h"

#include "ntp_server.h"
#include "network.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <SoftwareSerial.h>
#include "nvs_flash.h"
#include "nvs.h"

// #define SHOW_NMEA
#define VERSION (2.0)

#define GPSBAUD (9600)

#define MAX_SRV_CLIENTS (5) // telnet

// #define TELNET

#ifdef TELNET
/* For GPS Module Debug */
WiFiServer TelnetServer(23);
WiFiClient serverClients[MAX_SRV_CLIENTS];
#endif

hw_timer_t *timer = NULL;
uint32_t millisec;
Timecore timec;
bool dsiplDirLeft = true;
// RTC_DS3231 rtc_clock;
//  RTC_DS1307 rtc_clock;

Ticker TimeKeeper;
TinyGPSPlus gps;
NTP_Server NTPServer;

/* 63 Char max and 17 missign for the mac */
TaskHandle_t GPSTaskHandle;
SemaphoreHandle_t xSemaphore = NULL;
// SemaphoreHandle_t xi2cmtx = NULL;

// Used for the PPS interrupt
const byte interruptPin = 25;
// HardwareSerial now SowtwareS
EspSoftwareSerial::UART myPort;
const byte gpsRxPin = 16;
const byte gpsTxPin = 17;
char lastStart[1024];

volatile uint32_t UptimeCounter = 0; // 60*60*24 *5 -50;
volatile uint32_t GPS_Timeout = 0;
volatile uint32_t pps_counter = 0;
bool pps_active = false;
gps_settings_t gps_config;
int32_t restart_counter = 0; // value will default to 0, if not set yet in NVS

extern display_settings_t displayconfig;

void Display_Task(void *param);
uint32_t RTC_ReadUnixTimeStamp(bool *delayed_result);
void RTC_WriteUnixTimestamp(uint32_t ts);
void decGPSTimeout(void);
void _200mSecondTick(void);
void printSysTime();

#define NV_READ true

#define NV_WRITE false
void doNvram(String space, int *ptr, bool read_write);
nvs_handle_t my_handle;

/**************************************************************************************************
 *    Function      : GetUTCTime
 *    Description   : Reads the UTCTime
 *    Input         : none
 *    Output        : uint32_t
 *    Remarks       : none
 **************************************************************************************************/
uint32_t GetUTCTime(void);
int microDiff;
int microDrift;
/**************************************************************************************************
 *    Function      : handlePPSInterrupt
 *    Description   : Interrupt from the GPS module
 *    Input         : none
 *    Output        : none
 *    Remarks       : needs to be placed in RAM ans is only allowed to call functions also in RAM
 **************************************************************************************************/
void IRAM_ATTR handlePPSInterrupt()
{
  if (microDiff == 0) // test
  {
    microDiff = esp_timer_get_time();
  }
  microDrift = 1000000 - (esp_timer_get_time() - microDiff);
  //  Serial.println(microDrift);

  microDiff = esp_timer_get_time();

  if (gps_config.sync_on_gps)
  {
    timec.RTC_Tick();
    millisec = 0; // The problem is that we may be interrupte by the subsecond ISR
    pps_counter++;
    UptimeCounter++;
    decGPSTimeout();
    pps_active = true;
    // This needs to ne atomic
   // xSemaphoreGiveFromISR(xSemaphore, NULL);
  }
}

/**************************************************************************************************
 *    Function      : handleS
 *    Description   : Interrupt from internal Timer to get subseconds
 *    Input         : none
 *    Output        : none
 *    Remarks       : needs to be placed in RAM ans is only allowed to call functions also in RAM
 **************************************************************************************************/
void IRAM_ATTR handleSubSecondInterrupt()
{
  if (millisec < 999)
  {
    millisec++; // We coudl to some overhead and use a counting semaphore
  }
}

/**************************************************************************************************
 *    Function      : GetSubsecond
 *    Description   : Return the subseconds
 *    Input         : none
 *    Output        : uint32_t
 *    Remarks       : return 0 to 999
 **************************************************************************************************/
uint32_t GetSubsecond(void)
{
  // Serial.printf("millisec is %i\n\r", millisec);
  return millisec;
}

uptime_t GetUptime()
{
  uptime_t ut;
  ut.day = UptimeCounter / (3600 * 24);
  ut.hour = UptimeCounter / 3600 % 24;
  ut.minute = UptimeCounter % 3600 / 60;
  ut.second = UptimeCounter % 3600 % 60;
  return ut;
  //  sprintf(buffer, "Time:%02d:%02d:%02d", h, m, s);
}

/**************************************************************************************************
 *    Function      : GetUTCTime
 *    Description   : Returns the UTC Timestamp
 *    Input         : none
 *    Output        : none
 *    Remarks       : none
 **************************************************************************************************/
uint32_t GetUTCTime(void)
{
  uint32_t timest = 0;
  timest = timec.GetUTC();
  // Serial.printf("Timestamp is %i.%i\n\r", timest, millisec);
  return timest;
}

/**************************************************************************************************
 *    Function      : _200mSecondTick
 *    Description   : Runs all functions inside once a second
 *    Input         : none
 *    Output        : none
 *    Remarks       : none
 **************************************************************************************************/
void _200mSecondTick(void)
{
  // static inside a fkt. means init on first call
  static uint8_t callcount = 0;
  static uint8_t pps_check_cnt = 0;
  static uint32_t pps_count_last = 0;
  static bool last_pps_state = false;

  if ((last_pps_state != pps_active) && (true == pps_active))
  {
    Serial.println("Switch to PPS");
  }

  /* As long as the PPS is active we set the internal counter to zero */
  if (true == pps_active)
  {
    callcount = 0;
  }
  else
  {
    callcount++;
  }

  /* If we have no new PPS interrupt we increment the timout value */
  if (pps_count_last != pps_counter)
  {
    //  Serial.println(mymillidiff - UptimeCounter);
    pps_check_cnt = 0;
  }
  else
  {
    pps_check_cnt++;
  }

  /*  If we are running on internal clock
   *  we increment every  second the time
   *  also we give to the xSemaphore to inform
   *  that the display needs to update
   */
  if (callcount >= 5)
  {
    if (false == pps_active)
    {
      timec.RTC_Tick();
      //  GPS_Timeout = 0;
      UptimeCounter++;
//      xSemaphoreGive(xSemaphore);
    }
    callcount = 0;
  }

  /*
   * if the pps is active and over 1200ms since the last time
   * we switch back to the internal clock and also we
   * compensate the overdue by setting the next second
   * to be fired 600ms after this
   *
   */
  if ((pps_active == true) && (pps_check_cnt >= 7))
  { /*1400ms*/
    /* We assume that the pps is gone now and switch back to the internal timsource */
    /* Also we need to increment time here */
    pps_active = false;
    callcount = 2;
    timec.RTC_Tick();
    GPS_Timeout = 0;
    UptimeCounter++;
   // xSemaphoreGive(xSemaphore);
    /* The callcount shall now be 2 and we need  to set it to keep time*/
    Serial.println("Switch to internal clock");
  }

  pps_count_last = pps_counter;
  last_pps_state = pps_active;
}

/**************************************************************************************************
 *    Function      : decGPSTimeout
 *    Description   : decements the timputvaue for GPS Time Update
 *    Input         : none
 *    Output        : none
 *    Remarks       : none
 **************************************************************************************************/
void decGPSTimeout(void)
{
  if (GPS_Timeout > 0)
  {
    GPS_Timeout--;
  }
}

/**************************************************************************************************
 *    Function      : setup
 *    Description   : Get all components in ready state
 *    Input         : none
 *    Output        : none
 *    Remarks       : none
 **************************************************************************************************/
void setup()
{

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector

  /* First we setup the serial console with 115k2 8N1 */
  Serial.begin(115200);
  /* The next is to initilaize the datastore, here the eeprom emulation */
  datastoresetup();
  // change datastore to nvram ...
  doNvram("restart_counter", &restart_counter, NV_READ);
  restart_counter++;
  doNvram("restart_counter", &restart_counter, NV_WRITE);

  /* This is for the flash file system to access the webcontent */
  SPIFFS.begin();

  /* We read the Config from flash */
  Serial.println(F("Read Timecore Config"));
  timecoreconf_t cfg = read_timecoreconf();

  timec.SetConfig(cfg);

    
  /*
   * This delay the boot for a few seconds and will erase all config
   * if the boot btn is pressed
   */

  Serial.println(F("Booting..."));
  for (uint32_t i = 0; i < 25; i++)
  {
    if (digitalRead(0) == false)
    {
      Serial.println(F("Erase EEPROM"));
      erase_eeprom();
      break;
    }
    else
    {
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
  }
  /* We start to cpnfigure the WiFi */
  Serial.println(F("Init WiFi"));
  initWiFi();

#ifdef TELNET
  TelnetServer.begin();
  TelnetServer.setNoDelay(true);
#endif

  /*
   *  Next is to read how the GPS is configured
   *  here, if we use it for sync or not
   *
   */
  gps_config = read_gps_config();

  /* Last step is to get the NTP running */
  NTPServer.begin(123, GetUTCTime, GetSubsecond);
  /* Now we start with the config for the Timekeeping and sync */
  TimeKeeper.attach_ms(200, _200mSecondTick);

  /* We setup the PPS Pin as interrupt source */
  pinMode(interruptPin, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(interruptPin), handlePPSInterrupt, RISING);

  /* We will drain the RX buffer to avoid having old data in it*/

  myPort.begin(9600, SWSERIAL_8N1, gpsRxPin, gpsTxPin);
  if (!myPort)
  { // If the object did not initialize, then its configuration is invalid
    Serial.println("Invalid EspSoftwareSerial pin configuration, check config");
  }

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &handleSubSecondInterrupt, true);
  timerAlarmWrite(timer, 1000, true);
  timerAlarmEnable(timer);

  initDisplay();
}

/**************************************************************************************************
 *    Function      : doNvram
 *    Description   : read/write
 *    Input         : var-name , pointer-to, direction rw
 *    Output        : none
 *    Remarks       : none
 **************************************************************************************************/

void doNvram(String space, int *ptr, bool read_write)
{
  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  // Open
  printf("\n");
  printf("Opening (NVS) %s ", space.c_str());
  err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK)
  {
    printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  }
  else
  {
    printf("Done\n");
    if (read_write)
    {
      // Read
      printf("NVS Handle %04x ", my_handle);

      // value will default to 0, if not set yet in NVS
      *ptr = 0;
      err = nvs_get_i32(my_handle, space.c_str(), ptr);
      switch (err)
      {
      case ESP_OK:
        printf("Done\n");
        printf("read = %" PRIu32 "\n", *ptr);
        break;
      case ESP_ERR_NVS_NOT_FOUND:
        printf("The value is not initialized yet!\n");
        break;
      default:
        printf("Error (%s) reading!\n", esp_err_to_name(err));
      }
    }
    else
    {

      // Write
      printf("Updating %s NVS...", space.c_str());
      // restart_counter++;
      err = nvs_set_i32(my_handle, space.c_str(), *ptr);
      printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

      // Commit written value.
      // After setting any values, nvs_commit() must be called to ensure changes are written
      // to flash storage. Implementations may write to storage at other times,
      // but this is not guaranteed.
      printf("Committing updates in NVS ... ");
      err = nvs_commit(my_handle);
      printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
    }
  }

  // Close
  nvs_close(my_handle);
  printf("\n");
}
/**************************************************************************************************
 *    Function      : loop
 *    Description   : Superloop
 *    Input         : none
 *    Output        : none
 *    Remarks       : none
 **************************************************************************************************/

int lastRestart;
void loop()
{


  NetworkTask();

  int remainingTimeBudget = uiLoop();
  // You can do some work here
  // Don't do stuff if you are below your
  // time budget.
  /* Process all networkservices */
  if (remainingTimeBudget > 0)
  {

#ifdef TELNET
    TelnetDebugService();
#endif
    /* timeupdate done here is here */
    if (!gps_config.sync_on_gps)
    {
    }
    else
      while (myPort.available())
      {
        // Serial.print(",");
        int16_t Data = myPort.read();
        gps.encode(Data);

        if (displayconfig.show_nmea)
          Serial.print(char(Data));

        //  We check here if we have a new timestamp, and a valid GPS position
        if ((gps.date.isValid() == true) && (gps.time.isValid() == true) && (gps.location.isValid() == true))
        {
          if (pps_active != true)
          {
            // This needs to ne atomic
            millisec = 0; // The problem is that we may be interrupte by the subsecond ISR
          }

          if (gps.time.isUpdated() == true)
          {
            uint32_t ts_age = gps.time.age(); // this are milliseconds and should be lower than 1 second
            if (ts_age > 1000)
            {
              Serial.println("Warning GPS Timestamp older than a second");
            }
            // We need to feed the gps task
            datum_t newtime;
            newtime.year = gps.date.year();
            if (newtime.year >= 2000)
            {
              newtime.year -= 2000;
            }
            newtime.month = gps.date.month();
            newtime.day = gps.date.day();
            newtime.dow = 0;
            newtime.hour = gps.time.hour();
            newtime.minute = gps.time.minute();
            newtime.second = gps.time.second();

            uint32_t newtimestamp = timec.TimeStructToTimeStamp(newtime);

            /* Added by T. Godau DL9SEC 30.05.2020
            // Fix for older GPS with the week number rollover problem (see https://en.wikipedia.org/wiki/GPS_Week_Number_Rollover)
            if (gps_config.rollover_cnt > 0)
            {
              newtimestamp += (SECS_PER_WEEK * 1024 * gps_config.rollover_cnt);
            }
            */

            // We print the time for debug
            if ((true == gps_config.sync_on_gps) && (GPS_Timeout <= 0))
            {
              if (restart_counter != lastRestart)
              {
                lastRestart = restart_counter;
                sprintf(lastStart, "%d @ GPS: %i/%i/%i at %i:%i:%i \r\n", restart_counter, newtime.year + 2000, newtime.month, newtime.day, newtime.hour, newtime.minute, newtime.second);
              }
              // This function is overloaded and takes timestamps as time_t structs
              Serial.printf("GPS: %i/%i/%i at %i:%i:%i \r\n", newtime.year + 2000, newtime.month, newtime.day, newtime.hour, newtime.minute, newtime.second);
              timec.SetUTC(newtimestamp, GPS_CLOCK);
              GPS_Timeout = 600; // 10 Minute timeout
            }
            else
            {
              // Do nothing
            }
          }
        }
        // else
        // printSysTime();

#ifdef TELNET
        TelenetDebugServerTx(Data);
#endif
      }

    /* Display boot splash to the oled */

    // printSysTime();

    /* MQTT Stuff
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
    delay(remainingTimeBudget);
    */
  }
}

/**************************************************************************************************
 *    Function      : RTC_ReadUnixTimeStamp
 *    Description   : Writes a UTC Timestamp to the RTC
 *    Input         : bool*
 *    Output        : uint32_t
 *    Remarks       : Requiered to do some conversation
uint32_t RTC_ReadUnixTimeStamp(bool *delayed_result)
{
  DateTime now = time(0);
  if (true == xSemaphoreTake(xi2cmtx, (100 / portTICK_PERIOD_MS)))
  {
    now = rtc_clock.now();
    xSemaphoreGive(xi2cmtx);
  }
  *delayed_result = false;
  return now.unixtime();
}
 **************************************************************************************************/

/**************************************************************************************************
 *    Function      : RTC_WriteUnixTimestamp
 *    Description   : Writes a UTC Timestamp to the RTC
 *    Input         : uint32_t
 *    Output        : none
 *    Remarks       : Requiered to do some conversation
void RTC_WriteUnixTimestamp(uint32_t ts)
{
  uint32_t start_wait = millis();
  if (true == xSemaphoreTake(xi2cmtx, (40 / portTICK_PERIOD_MS)))
  {
    ts = ts + ((millis() - start_wait) / 1000);
    rtc_clock.adjust(DateTime(ts));
    DateTime now = rtc_clock.now();
    Serial.println("Update RTC");
    if (ts != now.unixtime())
    {
      Serial.println(F("I2C-RTC W-Fault"));
    }
    xSemaphoreGive(xi2cmtx);
  }
}
 **************************************************************************************************/

#ifdef TELNET

/**************************************************************************************************
 *    Function      : TelnetDebugService
 *    Description   : Cares for new and old connections to keep them alive
 *    Input         : none
 *    Output        : none
 *    Remarks       : none
 **************************************************************************************************/
void TelnetDebugService(void) // This keeps the connections alive
{
  uint8_t con = 0;
  if (TelnetServer.hasClient())
  {
    for (con = 0; con < MAX_SRV_CLIENTS; con++)
    {
      // find free/disconnected spot
      if (!serverClients[con] || !serverClients[con].connected())
      {
        if (serverClients[con])
          serverClients[con].stop();
        serverClients[con] = TelnetServer.available();
        if (!serverClients[con])
          Serial.println("available broken");
        Serial.print("New client: ");
        Serial.print(con);
        Serial.print(' ');
        Serial.println(serverClients[con].remoteIP());
        break;
      }
    }
    if (con >= MAX_SRV_CLIENTS)
    {
      // no free/disconnected spot so reject
      TelnetServer.available().stop();
    }
  }

  for (uint8_t i = 0; i < MAX_SRV_CLIENTS; i++)
  {
    if (serverClients[i] && serverClients[i].connected())
    {
      if (serverClients[i].available())
      {
        // get data from the telnet client and flush it
        serverClients[i].read();
      }
    }
    else
    {
      if (serverClients[i])
      {
        serverClients[i].stop();
      }
    }
  }
}

/**************************************************************************************************
 *    Function      : TelenetDebugServerTx
 *    Description   : Will send one byte to teh conncted clients
 *    Input         : none
 *    Output        : none
 *    Remarks       : none
 **************************************************************************************************/
void TelenetDebugServerTx(int16_t Data)
{
  if (Data < 0)
  {
    return;
  }
  // This is really inefficent but shall work
  uint8_t sbuf[1];
  sbuf[0] = Data;
  // push UART data to all connected telnet clients
  for (uint8_t i = 0; i < MAX_SRV_CLIENTS; i++)
  {
    if (serverClients[i] && serverClients[i].connected())
    {
      serverClients[i].write(sbuf, 1);
      delay(1); //?
    }
  }
}

#endif
