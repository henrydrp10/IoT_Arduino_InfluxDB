// Libraries to include
#include <U8g2lib.h>

// Declare and initialise the display
u8g2.begin();

// Preparing the OLED display for writing
void u8g2_prepare(void) 
{
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
}

// Main code, draws something on the display
u8g2_prepare();
u8g2.clearBuffer();
u8g2.drawStr( 0, 32, "Hello World!");
u8g2.sendBuffer();