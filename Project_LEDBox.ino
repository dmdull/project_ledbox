/* RGB Remote Control
   by: Jim Lindblom
   SparkFun Electronics
   date: October 1, 2013

   This sketch uses Ken Shirriff's *awesome* IRremote library:
       https://github.com/shirriff/Arduino-IRremote

   The IR Remote's power button turns the LED on or off. The A, B, 
   and C buttons select a channel (red, green, or blue). The up
   and down arrows increment or decrement the LED brightness on that channel.
   The left and right arrows turn a channel to min or max, and
   circle set it to the middle.
 */

#include <IRremote.h> // Include the IRremote library
#include <SPI.h> // SPI uses pins 11..13
#include <TCL.h>

/* Setup constants for SparkFun's IR Remote: */
#define NUM_BUTTONS 9 // The remote has 9 buttons
/* Define the IR remote button codes. We're only using the
   least signinficant two bytes of these codes. Each one 
   should actually have 0x10EF in front of it. Find these codes
   by running the IRrecvDump example sketch included with
   the IRremote library.*/
const uint16_t BUTTON_POWER = 0xD827; // i.e. 0x10EFD827
const uint16_t BUTTON_A = 0xF807;
const uint16_t BUTTON_B = 0x7887;
const uint16_t BUTTON_C = 0x58A7;
const uint16_t BUTTON_UP = 0xA05F;
const uint16_t BUTTON_DOWN = 0x00FF;
const uint16_t BUTTON_LEFT = 0x10EF;
const uint16_t BUTTON_RIGHT = 0x807F;
const uint16_t BUTTON_CIRCLE = 0x20DF;

/* Connect the output of the IR receiver diode to */
int RECV_PIN = 2;
/* Initialize the irrecv part of the IRremote library */
IRrecv irrecv(RECV_PIN);

enum HEADING
{
  NORTH,
  WEST,
  EAST,
  SOUTH,
  NUM_DIRS,
};

enum COLORS
{
  RED,
  YELLOW,
  GREEN,
  BLUE,
  MAGENTA,
  BLACK,
  NUM_COLORS,
};

const byte red[NUM_COLORS] =
  {0xff, 0xff, 0x00, 0x00, 0xff, 0x00};
const byte green[NUM_COLORS] =
  {0x00, 0xb0, 0x80, 0x00, 0x00, 0x00};
const byte blue[NUM_COLORS] =
  {0x00, 0x00, 0x00, 0xff, 0x40, 0x00};

#define MAX_LEDS 50 // LED Total Control Lighting Strand
#define NUM_LEDS 49 // LED Total Control Lighting Strand
#define STRIP_LEN 7 // currently organized as N xN

/*
  What is currently does
  - random
  - snake
  - use IR

  What should this do?
  - orb (mood) light

  - slow/fast rate
  - use buttons/pot
*/

enum MODE
{
  MODE_RANDOM,
  MODE_RAIN,
  MODE_SNAKE,
  MODE_SPIRAL,
  MODE_TEST,
  NUM_MODES,
};

int update_interval = 1000; // Milliseconds between color updates
long nextupdate = 0; // Time when next update of colors should occur.
boolean power = true;

typedef struct
{
  uint8_t mode;
  uint8_t string[NUM_LEDS];
  uint8_t count;
  uint8_t locx;
  uint8_t locy;
  uint8_t color;
  uint8_t direction;
} LED;
LED led;

void setup()
{
  randomSeed(analogRead(0)); //
  Serial.begin(9600); // Use serial to debug. 
  irrecv.enableIRIn(); // Start the IR receiver

  TCL.begin(); // blank all LEDs
  TCL.sendEmptyFrame();
  for(int i=0; i<MAX_LEDS; i++) // blank the entire string!
  {
    TCL.sendColor(0x00, 0x00, 0x00);
  }
  TCL.sendEmptyFrame();

  clearLEDs(BLACK);
  led.mode = MODE_RANDOM;
}

void setMode(uint8_t new_mode)
{
  if (new_mode != led.mode)
  {
    {
      clearLEDs(BLACK);
    }
    led.mode = new_mode;
  }
}

void clearLEDs(uint8_t color)
{
  for(int i=0; i<NUM_LEDS; i++)
  {
    led.string[i] = color;
  }
  led.count = 0;
  led.color = color;
}

void updateLEDs()
{
  TCL.sendEmptyFrame();
  for(int i=0;i<NUM_LEDS;i++)
  {
    int ndx = led.string[i];
    TCL.sendColor(red[ndx],green[ndx],blue[ndx]);
  }
  TCL.sendEmptyFrame();
}

/*
  LEDs coordinate system, based on (x,y) location
  
      (0,6)    ...    (6,6)

               ...

      (0,0)    ...    (6,0)

  Actual LED position based on string location
  
      6   7    ...    48

               ...

      0  13    ...    42
 */
void setLED(uint8_t locx, uint8_t locy, uint8_t color)
{
  if (locx % 2) // odd column are decreasing led#
  {
    led.string[STRIP_LEN-1-locy + locx*STRIP_LEN] = color;
  }
  else // even column are increasing led#
  {
    led.string[locy + locx*STRIP_LEN] = color;
  }
}

uint8_t getLED(uint8_t locx, uint8_t locy)
{
  int color;
  if (locx % 2) // odd column are decreasing led#
  {
    color = led.string[STRIP_LEN-1-locy + locx*STRIP_LEN];
  }
  else // even column are increasing led#
  {
    color = led.string[locy + locx*STRIP_LEN];
  }
  return color;
}

void loop() 
{
  readIR();

  long time = millis(); // Find the current time
  if (time > nextupdate)
  {
    if (power)
    switch (led.mode)
    {
      case MODE_RANDOM:
        leds_random();
        break;
      case MODE_RAIN:
        leds_rain();
        break;
      case MODE_SNAKE:
        leds_snake();
        break;
      case MODE_SPIRAL:
        leds_spiral();
        break;
      case MODE_TEST:
        leds_test();
        break;
      default:
        break;
    }

    update_interval = random(100, 1001);
    nextupdate = time + update_interval; // Set the time of the next update
  }
}

uint16_t lastCode = 0; // This keeps track of the last code RX'd
// constantly checks for any received IR codes
void readIR()
{
  decode_results results; // This will store our IR received codes

  if (irrecv.decode(&results)) 
  {
    /* read the RX'd IR into a 16-bit variable: */
    uint16_t resultCode = (results.value & 0xFFFF);

    /* The remote will continue to spit out 0xFFFFFFFF if a 
     button is held down. If we get 0xFFFFFFF, let's just
     assume the previously pressed button is being held down */
    if (resultCode == 0xFFFF)
      resultCode = lastCode;
    else
      lastCode = resultCode;

    // This switch statement checks the received IR code against
    // all of the known codes. Each button press produces a 
    // serial output, and has an effect on the LED output.
    switch (resultCode)
    {
      case BUTTON_POWER:
        //power ^= true;
        Serial.println("Power");
        break;
      case BUTTON_A:
        setMode(MODE_RANDOM);
        Serial.println("A");
        break;
      case BUTTON_B:
        setMode(MODE_RAIN);
        Serial.println("B");
        break;
      case BUTTON_C:
        setMode(MODE_TEST);
        Serial.println("C");
        break;
      case BUTTON_UP:
        Serial.println("Up");
        break;
      case BUTTON_DOWN:
        Serial.println("Down");
        break;
      case BUTTON_LEFT:
        setMode(MODE_SNAKE);
        Serial.println("Left");
        break;
      case BUTTON_RIGHT:
        setMode(MODE_SPIRAL);
        Serial.println("Right");
        break;
      case BUTTON_CIRCLE:
        Serial.println("Circle");
        break;
      default:
        Serial.print("Unrecognized code received: 0x");
        Serial.println(results.value, HEX);
        break;        
    }    
    irrecv.resume(); // Receive the next value
  }
}

/*
 * RANDOM: show random color in random location
 */
void leds_random()
{
  int locx, locy;
  do
  {
    locx = random(0, STRIP_LEN);
    locy = random(0, STRIP_LEN);
  }
  while (getLED(locx, locy) != BLACK);
  setLED(locx, locy, random(0, NUM_COLORS-1)); // skip BLACK
  led.count++;

  updateLEDs();

  // is it time to reset?
  if (led.count >= NUM_LEDS)
  {
    clearLEDs(BLACK);
  }
} /* leds_random() */

/*
 * RAIN: colors down the face
 */
void leds_rain()
{
  if (led.count == 0)
  {
    led.color = BLUE;
  }
  int ndx = STRIP_LEN * random(0, 7); // column
  if (ndx % 2) // odd column are decreasing led#
  {
    for (int j=STRIP_LEN-1; j>0; j--)
      led.string[ndx+j] = led.string[ndx+j-1];
    led.string[ndx] = led.color;
  }
  else // even column are increasing led#
  {
    for (int j=0; j<STRIP_LEN-1; j++)
      led.string[ndx+j] = led.string[ndx+j+1];
    led.string[ndx+STRIP_LEN-1] = led.color;
  }
  led.count++;

  updateLEDs();

  // is it time to reset?
  ndx = 0;
  for (int j=0; j<NUM_LEDS; j+=STRIP_LEN)
  {
    if (j % 2)
    {
      if (led.string[j+STRIP_LEN-1] != BLACK)
        ndx++;
    }
    else
    {
      if (led.string[j] != BLACK)
        ndx++;
    }
  }
  if (ndx >= (STRIP_LEN/2))
  {
    clearLEDs(BLACK);
  }
} /* leds_rain() */

/*
 * SNAKE: random line around
 */
void leds_snake()
{
  if (led.count == 0)
  {
    led.color = random(0, NUM_COLORS-1); // skip BLACK
    led.direction = random(0, NUM_DIRS);
    led.locx = led.locy = 0;
  }
  else
  {
  }
  setLED(led.locx, led.locy, led.color);
  led.count++;

  updateLEDs();
  // is it time to reset?
  if (false)
  {
    clearLEDs(BLACK);
  }
} /* leds_snake() */

/*
 * SPIRAL: entire unit from a random corner
 */
void leds_spiral()
{
  if (led.count == 0)
  {
    led.color = random(0, NUM_COLORS-1); // skip BLACK
    led.direction = NORTH;
    led.locx = led.locy = 0;
  }
  else
  {
    int border = BLACK;

    switch (led.direction)
    { // this only works for CW!
      case NORTH:
        if (((led.locy+1) % STRIP_LEN) && (getLED(led.locx, led.locy+1) == border))
        {
          led.locy++;
        }
        else
        {
          led.locx++;
          led.direction = EAST;
        }
        break;
  
      case EAST:
        if (((led.locx+1) % STRIP_LEN) && (getLED(led.locx+1, led.locy) == border))
        {
          led.locx++;
        }
        else
        {
          led.locy--;
          led.direction = SOUTH;
        }
        break;
  
      case SOUTH:
        if ((led.locy > 0) && (getLED(led.locx, led.locy-1) == border))
        {
          led.locy--;
        }
        else
        {
          led.locx--;
          led.direction = WEST;
        }
        break;
  
      case WEST:
        if ((led.locx > 0) && (getLED(led.locx-1, led.locy) == border))
        {
          led.locx--;
        }
        else
        {
          led.locy++;
          led.direction = NORTH;
        }
        break;
    }
  }
  setLED(led.locx, led.locy, led.color);
  led.count++;

  updateLEDs();

  // is it time to reset?
  if ((led.locx == 3) && (led.locy == 3)) // center location
  {
    clearLEDs(BLACK);
  }
} /* leds_spiral() */

/*
 * TEST: verify and validate the (x,y) routines
 */
void leds_test()
{
  int color = random(0, NUM_COLORS-1); // skip BLACK

  if (led.count == 0)
  {
    led.locx = led.locy = 0;
  }
  else
  {
    if ((led.locy+1) < STRIP_LEN)
    {
      led.locy++;
    }
    else
    {
      led.locx++;
      led.locy = 0;
    }
  }
  setLED(led.locx, led.locy, color);
  led.count++;

  updateLEDs();

  // is it time to reset?
  if (led.count >= NUM_LEDS)
  {
    clearLEDs(BLACK);
  }
} /* leds_test() */

