/*
 */
#include "display.h"
#include "datastore.h"
#include <WiFi.h>
#include <TinyGPS++.h>
#include "ntp_server.h"
#include <ESP32Ping.h>

extern int WifiGetRssiAsQuality(int rssi);
extern client_settings_t ntp_clients[];
extern int totalClients;
extern uptime_t GetUptime();
int lastsek = -1;

// Timecore timec;

// U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled_left(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
// U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled_right(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
uint8_t SDA_PIN = 5;
uint8_t SCL_PIN = 4;
SH1106Wire display(0x3c, SDA_PIN, SCL_PIN); // ADDRESS, SDA, SCL  -  SDA and SCL usually populate automatically based on your board's pins_arduino.h e.g. https://github.com/esp8266/Arduino/blob/master/variants/nodemcu/pins_arduino.h
OLEDDisplayUi ui(&display);

int screenW = 128;
int screenH = 64;
int clockCenterX = screenW / 2;
int clockCenterY = ((screenH) / 2); // top yellow part is 16 px height
int clockRadius = 38;
// This array keeps function pointers to all frames
// frames are the single views that slide in
FrameCallback frames[] = {digitalClockFrame, analogClockFrame, locationFrame, clientFrame, statusFrame, wifiFrame}; //, temperatureFrame

// how many frames are there?
int frameCount = 6;

// Overlays are statically drawn on top of a frame eg. a clock
OverlayCallback overlays[] = {clockOverlay};
int overlaysCount = 1;
// struct tm timeinfo;
bool swap;
extern TinyGPSPlus gps;
extern int totalClients;
extern display_settings_t displayconfig;
extern int microDrift;
int oldsec;
String twoDigits(int digits)
{
    if (digits < 10)
    {
        String i = '0' + String(digits);
        return i;
    }
    else
    {
        return String(digits);
    }
}

void clockOverlay(OLEDDisplay *display, OLEDDisplayUiState *state)
{
}

void initDisplay()
{
    // The ESP is capable of rendering 60fps in 80Mhz mode
    // but that won't give you much time for anything else
    // run it in 160Mhz mode or just set it to 30 fps
    ui.setTargetFPS(60);
    ui.setTimePerFrame(5000);
    // Customize the active and inactive symbol
    ui.setActiveSymbol(noSymbol);
    ui.setInactiveSymbol(noSymbol);

    // You can change this to
    // TOP, LEFT, BOTTOM, RIGHT
    ui.setIndicatorPosition(LEFT);

    // Defines where the first frame is located in the bar.
    ui.setIndicatorDirection(LEFT_RIGHT);

    // You can change the transition that is used
    // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN
    ui.setFrameAnimation(SLIDE_LEFT);

    // Add frames
    ui.setFrames(frames, frameCount);

    // Add overlays
    ui.setOverlays(overlays, overlaysCount);

    ui.disableAutoTransition();

    // Initialising the UI will init the display too.
    ui.init();

    displayconfig = read_display_config();
    display.flipScreenVertically();
    Serial.println("init display");
}
datum_t timeinfo;

extern int reconnectWifi;
String status = "discon";
int pingme = 120;
int uiLoop()
{
    /**/
    if (WiFi.getMode() != WIFI_MODE_AP)
    {
        /* wifi check*/
        timeinfo = timec.GetLocalTimeDate();
        if (oldsec != timeinfo.second )
        {
            oldsec = timeinfo.second;
            pingme--;
            if(pingme <= 0){
                pingme = 120;
                String str;
                printf("Ping %02d:%02d:%02d success ...\n" , timeinfo.hour, timeinfo.minute , timeinfo.second);
                
                if (!Ping.ping("fritz.box"))
                {
                    Serial.println("WIFI_MODE_STA reconnect ...");
                    ESP.restart();
                }
            }
        }
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        if (reconnectWifi <= millis())
        {
            Serial.println("WIFI_MODE_STA reconnect ...");
            // reconnectWifi = millis() + 1000 * 30;
            //ESP.restart();
        }
    }
    else
        status = "conn";

    if (displayconfig.swap_display)
    {
        swap = displayconfig.swap_display;
        if (ui.getUiState()->currentFrame + 1 != displayconfig.display_info && ui.getUiState()->frameState == FrameState::FIXED)
        {
            if (dsiplDirLeft)
                ui.nextFrame();
            else
                ui.previousFrame();
        }
    }
    else
    {
        if (swap != displayconfig.swap_display) // switch off
            initDisplay();
        swap = displayconfig.swap_display;
        return 1;
    }

    if (digitalRead(0) == 0 && digitalRead(0) == 0) // prell
    {
        ui.nextFrame();
    }
    // Serial.println();
    ui.update(); // < 0 if we need to much time e.g. 20 client.ip's
    return 1;    // so don't block the rest
}

void wifiFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    char buffer[50];
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(ArialMT_Plain_16);

    sprintf(buffer, "Wifi Quality");
    display->drawString(clockCenterX + x, y, buffer);

    display->setFont(ArialMT_Plain_24);
    int wifiq = WifiGetRssiAsQuality(WiFi.RSSI());
    sprintf(buffer, "%d%%", wifiq);
    display->drawString(clockCenterX + x, y + 18, buffer);
    // sprintf(buffer, "%d dB", WiFi.RSSI());

    sprintf(buffer, "Stat %s", status);
    // printf(buffer, "Stat %d", WiFi.status and 0xff);
    display->drawString(clockCenterX + x, y + 40, buffer);
}

void statusFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    char buffer[50];
    uptime_t ui = GetUptime();
    /**/
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(Lato_Hairline_18);
    sprintf(buffer, "Total UpTime");
    display->drawString(clockCenterX + x, y, buffer);

    sprintf(buffer, "Days: %03d ", ui.day);
    display->drawString(clockCenterX + x, y + 20, buffer);

    sprintf(buffer, "T: %02d:%02d:%02d ", ui.hour, ui.minute, ui.second);
    display->drawString(clockCenterX + x, y + 40, buffer);
}

void locationFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{

    char buffer[50];
    display->setFont(ArialMT_Plain_16);
    if (!gps.location.isValid())
    {
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->drawString(clockCenterX + x, y, "Position");
        display->drawString(clockCenterX + x, 20 + y, "not valid!");
        sprintf(buffer, "hdop %d", gps.hdop.value() / 100);
        display->drawString(clockCenterX + x, 40 + y, buffer);
    }
    else
    {
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        char indi = 'N';
        if (gps.location.lat() < 0)
        {
            indi = 'S';
        }
        sprintf(buffer, "Lat %0.6f %c", abs(gps.location.lat()), indi);
        display->drawString(clockCenterX + x, 0, buffer);
        // Serial.println(gps.location.lat());

        indi = 'E';
        if (gps.location.lng() < 0)
        {
            indi = 'W';
        }
        sprintf(buffer, "Lng %0.6f %c", abs(gps.location.lng()), indi);
        display->drawString(clockCenterX + x, 20 + y, buffer);
        sprintf(buffer, "Altitude: %0.1f m", gps.altitude.meters());
        display->drawString(clockCenterX + x, 40 + y, buffer);
    }
}

int idx = 0;
bool dir = false;

void clientFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    int lines = totalClients; // 14;
    int lineHeight = 12;
    int totalY = lines * lineHeight;
    if (dir)
    {
        idx++;
        if (idx >= totalY - 64 + lineHeight)
            dir = !dir;
    }
    else
    {
        idx--;
        if (idx <= -lineHeight)
            dir = !dir;
    }
    if (lines > 5)
        y = -(idx);

    char buffer[50];
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(ArialMT_Plain_10);

    for (int i = 0; i < lines; i++)
    {
        const time_t t = ntp_clients[i].last_stamp;
        struct tm *ti;
        ti = localtime(&t);
        sprintf(buffer, "%d %s %02d/%02d %02d:%02d", (i + 1), ntp_clients->clientIP.toString().c_str(), ti->tm_mon, ti->tm_mday, ti->tm_hour, ti->tm_min); // ntp_clients->syncs
        /*
        sprintf(buffer, "%d %s",i + 1, "asdf asd fsdsafsaf 4e ref");
        */
        display->drawString(clockCenterX + x, y + (i)*lineHeight, buffer);
    }

    //   sprintf(buffer, "total syncs. %d", idx);
    //    int wifiq = WifiGetRssiAsQuality(WiFi.RSSI());
    //    sprintf(buffer, "Wifi Quality %d%%, %d dBm", wifiq, WiFi.RSSI());
}

void analogClockFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    //  ui.disableIndicator();

    // Draw the clock face
    //  display->drawCircle(clockCenterX + x, clockCenterY + y, clockRadius);
    display->drawCircle(clockCenterX + x, clockCenterY + y, 2);
    //
    // hour ticks
    for (int z = 0; z < 360; z = z + 30)
    {
        // Begin at 0° and stop at 360°
        float angle = z;
        angle = (angle / 57.29577951); // Convert degrees to radians
        int x2 = (clockCenterX + (sin(angle) * clockRadius));
        int y2 = (clockCenterY - (cos(angle) * clockRadius));
        int x3 = (clockCenterX + (sin(angle) * (clockRadius - (clockRadius / 8))));
        int y3 = (clockCenterY - (cos(angle) * (clockRadius - (clockRadius / 8))));
        display->drawLine(x2 + x, y2 + y, x3 + x, y3 + y);
    }

    // display second hand
    float angle = timeinfo.second * 6;
    angle = (angle / 57.29577951); // Convert degrees to radians
    int x3 = (clockCenterX + (sin(angle) * (clockRadius - (clockRadius / 5))));
    int y3 = (clockCenterY - (cos(angle) * (clockRadius - (clockRadius / 5))));
    display->drawLine(clockCenterX + x, clockCenterY + y, x3 + x, y3 + y);
    //
    // display minute hand
    angle = timeinfo.minute * 6;
    angle = (angle / 57.29577951); // Convert degrees to radians
    x3 = (clockCenterX + (sin(angle) * (clockRadius - (clockRadius / 4))));
    y3 = (clockCenterY - (cos(angle) * (clockRadius - (clockRadius / 4))));
    display->drawLine(clockCenterX + x, clockCenterY + y, x3 + x, y3 + y);
    //
    // display hour hand
    angle = timeinfo.hour * 30 + int((timeinfo.minute / 12) * 6);
    angle = (angle / 57.29577951); // Convert degrees to radians
    x3 = (clockCenterX + (sin(angle) * (clockRadius - (clockRadius / 2))));
    y3 = (clockCenterY - (cos(angle) * (clockRadius - (clockRadius / 2))));
    display->drawLine(clockCenterX + x, clockCenterY + y, x3 + x, y3 + y);
}

String s[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
String m[] = {"Jan", "Feb", "Mar", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dez"};

void digitalClockFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{

    //    datum_t timeinfo = timec.GetLocalTimeDate();

    String timenow;
    if (timeinfo.second % 2)
        timenow = twoDigits(timeinfo.hour) + ":" + twoDigits(timeinfo.minute);
    else
    {
        timenow = twoDigits(timeinfo.hour) + " " + twoDigits(timeinfo.minute);
    }

    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(DSEG7_Modern_Mini_Bold_38);
    display->drawString(clockCenterX + x, 22, timenow);
    display->setFont(Lato_Hairline_18);
    // Serial.println(timeinfo.dow);
    String datenow = s[timeinfo.dow - 1] + ". " + twoDigits(timeinfo.day) + ". " + m[timeinfo.month - 1] + ".";
    display->drawString(clockCenterX - 20 + x, 0, datenow);

    display->setFont(DSEG7_Modern_Mini_Bold_16);
    display->drawString(clockCenterX + 50 + x, 0, twoDigits(timeinfo.second));
}
/**************************************************************************************************
 *    Function      : printSysTime
 *    Description   : Will output datetime to console
 *    Input         : none
 *    Output        : none
 *    Remarks       : none
 **************************************************************************************************/

void printSysTime()
{
    datum_t timeinfo = timec.GetLocalTimeDate();

    // time_t now = time(nullptr);
    // tm = *localtime(&now);
    // getLocalTime(&tm); // esp32
    if (timeinfo.minute != lastsek)
    {
        lastsek = timeinfo.minute;
        String datenow = twoDigits(timeinfo.hour) +
                         ":" + twoDigits(timeinfo.minute) +
                         ":" + twoDigits(timeinfo.second) +
                         "." + String(millis() % 1000) +
                         " " + s[timeinfo.dow] +
                         ". " + twoDigits(timeinfo.day) +
                         "." + m[timeinfo.month - 1] +
                         " " + (timeinfo.year);
        Serial.println(datenow);
    }
}

int getFrameCount()
{
    return frameCount;
}