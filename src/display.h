
#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "timecore.h"
#include <TimeLib.h>

#include <Wire.h>       // Only needed for Arduino 1.6.5 and earlier
#include "SH1106Wire.h" // legacy include: `#include "SSD1306.h"`
#include "font.h"
#include "images.h"
#include <OLEDDisplay.h>
// #include <OLEDDisplayFonts.h>
//  Include the UI lib
#include "OLEDDisplayUi.h"
/* As we use a pointer to the oled we need to make sure it's the same type as out displays */
// U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled_left(U8G2_R0, /* reset=*/U8X8_PIN_NONE, /* clock=*/SCL_PIN, /* data=*/SDA_PIN);
// U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled_right(U8G2_R0, /* reset=*/U8X8_PIN_NONE, /* clock=*/SCL_PIN, /* data=*/SDA_PIN);
// U8G2_SSD1306_128X64_NONAME_F_HW_I2C *oled_ptr = NULL;
//  Initialize the OLED display using Wire library

extern Timecore timec;

typedef struct {
  int32_t day;
  int16_t hour;
  uint8_t minute;
  uint8_t second;
} uptime_t;

extern bool dsiplDirLeft;
void analogClockFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void digitalClockFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void locationFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void clientFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void statusFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void wifiFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void clockOverlay(OLEDDisplay *display, OLEDDisplayUiState *state);
String twoDigits(int digits);
void  initDisplay();
int uiLoop();
void printSysTime();
int getFrameCount();


#endif
