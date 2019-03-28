// M1GEO Word Clock (V2 PCB)
// First revision: 04 Feb 2019
// Compiled Arduino 1.8.8
// https://www.george-smart.co.uk/wordclock
// https://github.com/m1geo/WordClock

#include <DS3232RTC.h>          // https://github.com/JChristensen/DS3232RTC
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

#include "WordClock_Mapping.h"

// IO Pins
#define DISP_PIN     6
#define LDR          A0

// User Options
#define LONG_MONTH   1
#define SCROLL_DLY   100

// Display Config - DON'T TOUCH
#define DISP_LINES   9
#define DISP_WIDTH   15
#define MAX_BRIGHT   128
#define MIN_BRIGHT   16
#define LED_PER_CHAR 6


Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(DISP_WIDTH, DISP_LINES, DISP_PIN,
  NEO_MATRIX_TOP + NEO_MATRIX_LEFT +
  NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE,
  NEO_GRB + NEO_KHZ800);

const uint16_t colours[] =
{
  matrix.Color(255, 255, 255),  // white
  matrix.Color(255, 0, 0),      // red
  matrix.Color(0, 255, 0),      // green
  matrix.Color(0, 0, 255),      // blue
  matrix.Color(255, 255, 0),    // yellow (red + green)
  matrix.Color(0, 255, 255),    // cyan (green + blue)
  matrix.Color(255, 0, 255)     // magenta (red + blue)
};

unsigned int ldra[5] = {0, 0, 0, 0, 0};
unsigned int LDRVAL = 0;
unsigned int BRIGHT = MAX_BRIGHT; // average scaling between 0-255 for LED brightness.
unsigned int BRIGHTOLD = BRIGHT;
const unsigned int num_colours = sizeof(colours)/sizeof(uint16_t);
int x = matrix.width();
int pass = 0;
uint32_t z = 0;
char disp_str[15];

// ISR code to read LDR and respond to brightness changes
ISR(TIMER1_COMPA_vect) // interrupt 10Hz on Timer 1
{
  // LDR code
  int i=0;

  // Read the ADC (LDR value)
  LDRVAL = analogRead(LDR);

  // 5 point agerage
  ldra[0] = ldra[1];
  ldra[1] = ldra[2];
  ldra[2] = ldra[3];
  ldra[3] = ldra[4];
  ldra[4] = LDRVAL;
  
  // Average of ldra into LDRVAL
  LDRVAL = 0;
  for (i=0; i<(sizeof(ldra)/sizeof(ldra[0])); i++) {
    LDRVAL += ldra[i];
  }
  LDRVAL /= (sizeof(ldra)/sizeof(ldra[0]));
  
  if (LDRVAL>83) {
    BRIGHT = (LDRVAL-83)/2.4; // ADC<=200 is 1 PWM. ADC>=800 is 255 PWM - F(x) = 0.4233x - 83.67 ## 1/0.423 = 2.364
  } else {
    BRIGHT = MIN_BRIGHT;
  }
  if (BRIGHT < MIN_BRIGHT) {
    BRIGHT = MIN_BRIGHT;
  }
  if (BRIGHT > MAX_BRIGHT) {
    BRIGHT = MAX_BRIGHT;
  }

  if (BRIGHT != BRIGHTOLD)
  {
    matrix.setBrightness(BRIGHT);
    matrix.show();
    Serial.print(LDRVAL);
    Serial.print("/");
    Serial.println(BRIGHT);
  }
  BRIGHTOLD = BRIGHT;
}

void setup()
{
  Serial.begin(115200);
  // setSyncProvider() causes the Time library to synchronize with the
  // external RTC by calling RTC.get() every five minutes by default.
  setSyncProvider(RTC.get);
  Serial.print(F("RTC Sync"));
  if (timeStatus() != timeSet) {
    Serial.print(F(" FAILED"));
    scrollString("RTC SYNC FAIL", matrix.Color(255, 0, 0)); // red
  }
  Serial.println("");

  // seed the PRNG
  randomSeed(analogRead(0) + hour(now()) +  minute(now()) + second(now()));

  // interupts for reading the LDR regularly, and updating the brightness
  cli();//stop interrupts
  //set timer1 interrupt at 1Hz
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for 10hz increments
  OCR1A = 1562;// = (16*10^6) / (1*1024) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS12 and CS10 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  sei();//allow interrupts
  
  matrix.begin();
  matrix.setTextWrap(false);
  Twinkle();
  matrix.setBrightness(BRIGHT);
  dispWord(w_M1GEO, colours[random(0, num_colours)]); // random colour
  delay(1000);
  
  time_t t = now();
  float c = RTC.temperature() / 4.0;
  scrollTime(t, colours[random(0, num_colours)]); // random colour
  scrollDate(t, colours[random(0, num_colours)]); // random colour
  scrollTemp(c, colours[random(0, num_colours)]); // random colour
  
  dispWord(timeToWords(now()), colours[random(0, num_colours)]); // random colour
  dispWord(timeToWords(now()), colours[random(0, num_colours)]); // random colour
}

void loop() {
  static time_t tLast;
  time_t t;
  tmElements_t tm;

  // check for input to set the RTC, minimum length is 12, i.e. yy,m,d,h,m,s
  if (Serial.available() >= 12) {
      // note that the tmElements_t Year member is an offset from 1970,
      // but the RTC wants the last two digits of the calendar year.
      // use the convenience macros from the Time Library to do the conversions.
      int y = Serial.parseInt();
      if (y >= 100 && y < 1000)
          Serial.println(F("Error: Year must be two digits or four digits!"));
      else {
          if (y >= 1000)
              tm.Year = CalendarYrToTm(y);
          else    // (y < 100)
              tm.Year = y2kYearToTm(y);
          tm.Month = Serial.parseInt();
          tm.Day = Serial.parseInt();
          tm.Hour = Serial.parseInt();
          tm.Minute = Serial.parseInt();
          tm.Second = Serial.parseInt();
          t = makeTime(tm);
          RTC.set(t);        // use the time_t value to ensure correct weekday is set
          setTime(t);
          Serial.println(F("RTC SET OK"));
          scrollString("RTC SET OK", matrix.Color(0, 255, 0)); // green
          // dump any extraneous input
          while (Serial.available() > 0) Serial.read();
          time_t t = now();
          float c = RTC.temperature() / 4.0;
          scrollTime(t, colours[random(0, num_colours)]); // random colour
          scrollDate(t, colours[random(0, num_colours)]); // random colour
          scrollTemp(c, colours[random(0, num_colours)]); // random colour
          
          dispWord(timeToWords(now()), colours[random(0, num_colours)]); // random colour
          dispWord(timeToWords(now()), colours[random(0, num_colours)]); // random colour
      }
  }

  // get the time
  t = now();

  // has the time changed?
  if (t != tLast) {
    tLast = t;
    if (second(t) == 0) // if it's a new minute
    {
      scrollTime(t, colours[random(0, num_colours)]); // random colour
      if ((minute(t) % 5) == 0) // if it's a fifth minute
      {
        float c = RTC.temperature() / 4.0;
        scrollDate(t, colours[random(0, num_colours)]); // random colour
        scrollTemp(c, colours[random(0, num_colours)]); // random colour
      }
      z = timeToWords(t);
      dispWord(z, colours[random(0, num_colours)]); // random colour
    }
  }
}

void scrollTime(time_t t, uint16_t colour)
{
  int scrolllimit = sprintf(disp_str, "%02u:%02u", hour(t), minute(t));
  scrollString(disp_str, colour);
}

void scrollDate(time_t t, uint16_t colour)
{
  int scrolllimit = 0;
  if (LONG_MONTH == 1) {
    scrolllimit = sprintf(disp_str, "%u %s %04u", day(t), monthStr(month(t)), year(t));
  } else {
    scrolllimit = sprintf(disp_str, "%u %s %04u", day(t), monthShortStr(month(t)), year(t));
  }
  scrollString(disp_str, colour);
}

void scrollTemp(float temp, uint16_t colour)
{
  int intC = (int) temp; // integer
  int fraC = (int) (temp*10) - (intC * 10); // 1 dec point
  int scrolllimit = sprintf(disp_str, "%d.%d C", intC, fraC); // to string
  scrollString(disp_str, colour);
}

// disp_str length sets maximum string length
void scrollString(char * stringarr, uint16_t colour)
{
  int scrolllimit = strlen(stringarr);
  byte run_loop = 1;
  matrix.setTextColor(colour);
  while (run_loop)
  {
    matrix.fillScreen(0);
    matrix.setCursor(x, 1);
    matrix.print(disp_str);
    if(--x < -(scrolllimit*LED_PER_CHAR)) {
      x = matrix.width();
      run_loop = 0;
    }
    matrix.show();
    delay(SCROLL_DLY);
  }
}

void dispWord(uint32_t wrds, uint16_t colour)
{
  matrix.fillScreen(0);
  for (int i = 0; i < 32; i++) {
    if (bitRead(wrds, i)) // if word at index 'i' should be on
    {
      uint8_t line   = word_pixel_data[i][0];
      uint8_t pixel  = word_pixel_data[i][1];
      uint8_t pixlen = word_pixel_data[i][2];
      //uint16_t colour   = colours[random(0, num_colours)]; // random colour per word
      //sprintf(disp_str, "i=%d line=%u, pixel=%u, pixlen=%u, col=%u \n", i, line, pixel, pixlen, colour);
      //Serial.print(disp_str);
      for (int j = pixel; j < (pixel + pixlen); j++) {
        //uint16_t colour   = colours[random(0, num_colours)]; // random colour per letter
        matrix.drawPixel(j, line, colour);
      }
    }
  }
  matrix.show();
}

void Twinkle()
{
  matrix.setBrightness(BRIGHT/3);
  for(int t = 0; t < 15; t++)
    {
    for(int l = 0; l < DISP_LINES; l++)
    {
      for(int p = 0; p < DISP_WIDTH; p++)
      {
        matrix.drawPixel(p, l, colours[random(0, num_colours)]);
      }
    }
    matrix.show();
    delay(75);
  }
  matrix.setBrightness(BRIGHT);
}

// This function could do with being tidied, but it works well and is relatively easy to follow.
uint32_t timeToWords(time_t t)
{
  uint32_t frame = 0;
  byte m = minute(t);
  byte h = hour(t);
  byte h2 = h;
  byte pm = 0;
  
  frame |= w_IT | w_IS;

  // set minute modifiers
  switch (m) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
      // Nothing to add for "on the hour"
      break;
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
      frame |= w_FIVE_A | w_PAST;
      break;
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
      frame |= w_TEN_A | w_PAST;
      break;
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
      frame |= w_QUARTER | w_PAST;
      break;
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
      frame |= w_TWENTY | w_PAST;
      break;
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
      frame |= w_TWENTY | w_FIVE_A | w_PAST;
      break;
    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
      frame |= w_HALF | w_PAST;
      break;
    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
      frame |= w_TWENTY | w_FIVE_A | w_TO;
      h2 = h + 1;
      break;
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
      frame |= w_TWENTY | w_TO;
      h2 = h + 1;
      break;
    case 45:
    case 46:
    case 47:
    case 48:
    case 49:
      frame |= w_QUARTER | w_TO;
      h2 = h + 1;
      break;
    case 50:
    case 51:
    case 52:
    case 53:
    case 54:
      frame |= w_TEN_A | w_TO;
      h2 = h + 1;
      break;
    case 55:
    case 56:
    case 57:
    case 58:
    case 59:
      frame |= w_FIVE_A | w_TO;
      h2 = h + 1;
      break;
  }

  if (h2 > 12)
  {
    h2 -= 12;
    pm = 1;
  }
  
  // set hours
  switch (h2) {
    case 0:
    case 12:
      frame |= w_TWELVE;
      break;
    case 1:
      frame |= w_ONE;
      break;
    case 2:
      frame |= w_TWO;
      break;
    case 3:
      frame |= w_THREE;
      break;
    case 4:
      frame |= w_FOUR;
      break;
    case 5:
      frame |= w_FIVE;
      break;
    case 6:
      frame |= w_SIX;
      break;
    case 7:
      frame |= w_SEVEN;
      break;
    case 8:
      frame |= w_EIGHT;
      break;
    case 9:
      frame |= w_NINE;
      break;
    case 10:
      frame |= w_TEN;
      break;
    case 11:
      frame |= w_ELEVEN;
      break;
  }

  frame |= w_IN | w_THE;
  
  // set morning, afternoon, evening
  if (pm == 0)
  {
    frame |= w_MORNING;
  } else if (h >= 6) 
  {
    frame |= w_EVENING;
  } else {
    frame |= w_AFTERNOON;
  }
  
  return frame;
}
