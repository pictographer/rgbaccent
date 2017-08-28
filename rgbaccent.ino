// -*-c++-*-
// Accent light USB-serial controller
//
// This firmware is part of a system to control and automate accent
// lighting in my home. The system is built using the FastLED library,
// APA102 RGB LEDs, Teensy 3.2 ARM Cortex-M4 development board, A5-V11
// WiFi router, OpenWrt, and Lua.
//
// The A5-V11 WiFi router is running OpenWrt in station mode. The
// A5-V11 is a client of our main WiFi router, just like a personal
// computer or tablet. The A5-V11 changes the lights according to the
// time of day. The A5-V11 also runs a webserver enabling anyone on
// our network to change the color and brightness of the lights. The
// A5-V11 has a USB-serial connection to the Teensy via an ordinary
// USB cable. The Teensy is connected to two momentary push buttons
// and an I/O buffer IC used to boost the 3.3V signals from the Teensy
// to 5V needed for the APA102 RGB LED strip.
//
// It is remarkable that the A5-V11 is available for less than $10
// including shipping. This device is about the size of a
// lighter. This is considerably less expensive, and more convenient
// than many of the so-called maker movement products with similar
// functionality from the big names like Arduino CC, Adafruit,
// SparkFun and the Raspberry Pi Foundation. The only drawback is that
// the A5-V11 is generic no-name electronics so sourcing models with
// the right ICs and firmware could be challenging. I haven't had
// trouble but others have.

// For the accent lighting control, the operations we want are:
//    set color
//    set animation
//    fade to color over interval
//    set pattern
//    rotate pattern over interval
//    define pattern
//    declare branch target
//    go to branch target
//    add a bump of a width and shape at a position
//    save the current configuration as a pattern

// since the LEDs are so well diffused, we we can use simple math for
// feathering the edges
//
// since there are so few LEDs relative to the processor speed and
// data rate, we can use whatever is most convenient to program

#include <cstddef>
#include <cctype>
#include <cstdint>

// The marvelous FastLED library provides a high-level interface to
// many kinds of RGB LEDs and drives them efficiently. See
// http://fastled.io
#include <FastLED.h>

#include <Bounce2.h>

// We'll use BOARD to announce the board type over the serial
// connection on startup.

#if defined(TEENSYDUINO)
    #if defined(__MK20DX128__)
        #define BOARD "Teensy 3.0"
    #elif defined(__MK20DX256__)
        #define BOARD "Teensy 3.1" // and Teensy 3.2
    #elif defined(__MKL26Z64__)
        #define BOARD "Teensy LC"
    #elif defined(__MK64FX512__)
        #define BOARD "Teensy 3.5"
    #elif defined(__MK66FX1M0__)
        #define BOARD "Teensy 3.6"
    #else
       #error "Unknown board"
    #endif
#endif

// Parameters for our LEDs
#define LED_TYPE APA102
#define COLOR_ORDER BGR
const int DATA_PIN = 11;
const int CLOCK_PIN = 14;
const unsigned int NUM_LEDS = 96;
const unsigned int CLOCK_MHZ = 12;

// Wire color and pin number for two momentary-contact push buttons.
const int YELLOW4 = 4;
const int WHITE5 = 5;

Bounce leftButton = Bounce(WHITE5, 10);   // 10 ms debounce
Bounce rightButton = Bounce(YELLOW4, 10); // 10 ms debounce

// Storage for LED colors.
CRGB leds[NUM_LEDS];

// Blink the built-in LED n times.
void debugBlink(int blinkCount) {
   const int blink_ms = 100;
   for (int i = 0; i < blinkCount; ++i) {
      digitalWriteFast(LED_BUILTIN, 1);
      delay(blink_ms);
      digitalWriteFast(LED_BUILTIN, 0);
      delay(blink_ms);
   }
   // Pause so that immediately consecutive calls can be distinguished
   // from a single call with greater blinkCount.
   delay(3 * blink_ms);
}

// Command argument structure
struct cmdarg_t {
   const char* const name;
   CRGB color1;
   long duration_ms;
};

// A command is an argument structure and a function.
class cmd_t {
public:
   cmdarg_t a;
   void (*fn)(cmdarg_t a);
};

// Set all the LEDs to color1.
void solidColor(struct cmdarg_t a) {
   if (a.duration_ms) {

      // Fade from the current leds to the new solid color.

      CRGB overlay[NUM_LEDS];
      fill_solid(overlay, NUM_LEDS, a.color1);
      for (int i = 0; i < 256; ++i) {
         nblend(leds, overlay, NUM_LEDS, 4);
         FastLED.delay(a.duration_ms); // Includes FastLED.show().
      }
      Serial.println("Done!");
   }
   fill_solid(leds, NUM_LEDS, a.color1);
}

// Limit the power drawn by the LEDs. It is possible to crash the
// router or Teensy by commanding the LEDs to get too bright.
//
//    Power supply           2000mA
//
//    A5-V11                  170mA
//    Teensy 3.2               35mA
//    MM74HTC245N Bus Buffer   70mA
//
//    Availble for LEDs      1725mA
//
// The buffer worst case is based on 35mA drive strength of each
// output and we're only using two. Actual current needed is
// whatever it takes to switch two inputs from the first LED in the
// chain. Probably under 10mA.
//
// We're guessing that the FastLED has a good estimate for the
// current draw of the APA102 RGB LEDs.
//
// TODO: Find a better way than #define. Using this for now because
// C++ lambda expressions can't have captures when used in a struct
// initializer.
#define LEDS_ON_2A 1500

// Limit the power drawn by the LEDs while plugged into a PC's USB 2.0
// port.
#define LEDS_ON_USB2 400

const unsigned long fade_ms = 32;

const cmd_t cmds[] = {

   // Control-related commands
   {{"startup", 0, 0},
    [] (cmdarg_t a) {
       // Set the first three LEDS to red, green, and blue respective.
       fill_solid(leds, NUM_LEDS, CRGB::Black);
       leds[0] = CRGB::Red;
       leds[1] = CRGB::Green;
       leds[2] = CRGB::Blue;
    }},
   {{"maxma", 0, 0},
    [] (cmdarg_t a) {
       FastLED.setMaxPowerInVoltsAndMilliamps(5, LEDS_ON_2A);
    }},
   {{"usbma", 0, 0},
    [] (cmdarg_t a) {
       FastLED.setMaxPowerInVoltsAndMilliamps(5, LEDS_ON_USB2);
    }},

   {{"dim",    0, 0}, [] (cmdarg_t a) { FastLED.setBrightness(8); }},
   {{"medium", 0, 0}, [] (cmdarg_t a) { FastLED.setBrightness(24); }},
   {{"bright", 0, 0}, [] (cmdarg_t a) { FastLED.setBrightness(32); }},
   {{"full",   0, 0}, [] (cmdarg_t a) { FastLED.setBrightness(48); }},

   // Scenes
   {{"black",  CRGB::Black,     fade_ms}, solidColor},
   {{"white",  CRGB::White,     fade_ms}, solidColor},
   {{"red",    CRGB::Red,       fade_ms}, solidColor},
   {{"orange", CRGB::OrangeRed, fade_ms}, solidColor},
   {{"yellow", CRGB::Yellow,    fade_ms}, solidColor},
   {{"green",  CRGB::Green,     fade_ms}, solidColor},
   {{"aqua",   CRGB::Aqua,      fade_ms}, solidColor},
   {{"blue",   CRGB::Blue,      fade_ms}, solidColor},
   {{"purple", CRGB::Purple,    fade_ms}, solidColor},
   {{"pink",   CRGB::Pink,      fade_ms}, solidColor},
   {{"rainbow", 0, 0},
    [] (cmdarg_t a) {
       fill_rainbow(leds, NUM_LEDS, HUE_RED, 1);
    }},
   {{"alarm", 0, 0}, [] (cmdarg_t) {}}
};

const size_t cmd_count = sizeof cmds / sizeof *cmds;

// Size of null-terminated command buffer.
const size_t cmax = 256;

size_t getCommandNumber(const char* const name) {
   for (size_t i = 0; i < cmd_count; ++i) {
// TODO strnlen
      if (strncmp(cmds[i].a.name, name, strlen(cmds[i].a.name)) == 0) {
         return i;
      }
   }
   return -1;
}

void setup() {
   // On-board orange LED for debugging. Start in a 'fault'
   // condition. Clear it once the USB-serial connection is available.
   pinMode(LED_BUILTIN, OUTPUT);
   digitalWriteFast(LED_BUILTIN, 1);

   // Wait for the USB-serial connection.
   while (!Serial && millis() < 1000) ;

  // Turn out the light to show successful serial connection.
  digitalWriteFast(LED_BUILTIN, 0);

  // Give a little provenance in hopes of making it easier to pick up
  // this project after a long while.
  Serial.printf("Serial connection time (ms): %d\n", millis());
  Serial.printf("Teensy board type: %s\n", BOARD);
  Serial.printf("Main source file: %s\n", __FILE__);
  Serial.printf("Data pin: %d\n", DATA_PIN);
  Serial.printf("Clock pin: %d\n", CLOCK_PIN);
  Serial.printf("LED count: %d\n", NUM_LEDS);

  // And now the star of the show. Initialize the LEDs.
  FastLED.addLeds<LED_TYPE, DATA_PIN, CLOCK_PIN,
      COLOR_ORDER, DATA_RATE_MHZ(CLOCK_MHZ)>(leds, NUM_LEDS);

  // Limit the power to a fraction of USB 2.0 power while theathe
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 400);

  // My strip seems to have a purple cast, at least at low brightness.
  FastLED.setCorrection(CRGB(0xFF, 0xFF, 0xCC));

  // Start with low brightness for safety sake.
  FastLED.setBrightness(8);

  // To keep the push button inputs from floating when the buttons
  // aren't pressed, enable a resistor connected to 3.3V to pull the
  // output up to a logic 1.
  pinMode(WHITE5, INPUT_PULLUP);  // White wire to Teensy pin 5
  pinMode(YELLOW4, INPUT_PULLUP); // Yellow wire to Teensy pin 4

  Serial.printf("Invoking command '%s'.\n", cmds[0].a.name);
  cmds[0].fn(cmds[0].a);
}

void loop() {

   // Handle USB-serial commands.

   static char cmdline[cmax];
   // TODO: need a better name/encapsulation of the index into the command string vs
   // index into the cmds array.
   static size_t cindex = 0;

   // Index into cmds array
   static size_t cmd = cmd_count;

   // Read a line into cmdline. Complain and truncate if longer than cmax - 1.
   while (Serial.available()) {
      unsigned int r = Serial.read();
      if (isprint(r)) {
         cmdline[cindex++] = r;
         cmdline[cindex] = 0;
         if (cindex == cmax - 1) {
            Serial.println("Command buffer overflow!");
            // The input isn't valid. Discard it.
            cindex = 0;
            cmdline[cindex] = 0;
            break;
         }
      }
   }
   if (cindex) {
      // TODO: better parser. match command based on first word (upto
      // whitespace or eol).  Remaining text is actual arguments.
      size_t candidate = getCommandNumber(cmdline);
      if (candidate != (size_t) -1) {
         cmd = candidate;
         Serial.printf("matched: %s\n", cmdline);
         cmds[cmd].fn(cmds[cmd].a);
      }
      cindex = 0;
   }

   // Handle button presses.
   if (leftButton.update() && leftButton.fallingEdge()) {
      // TODO: Single press toggles between off and the previous
      // brightness level. Holding the button cycles through
      // increasing brightness to off.
      Serial.println("left button WHITE5");
      if (FastLED.getBrightness()) {
         FastLED.setBrightness(0);
      } else {
         // TODO: The power limit doesn't appear to work. Otherwise we could put
         // 255 here and not have a problem, but we do.
         FastLED.setBrightness(32);
      }
      debugBlink(5);
   }

   if (rightButton.update() && rightButton.fallingEdge()) {
      // TODO Somehow pressing this button steps through the
      // colors/modes. Maybe adding the left button makes the cycling
      // faster or something. I'll have to find my documentation.
      Serial.println("right button YELLOW4");
      ++cmd;
      if (cmd_count <= cmd) cmd = getCommandNumber("white");
      Serial.printf("Setting color to %s (%d)\n", cmds[cmd].a.name, cmd);
      // Switch without the fade.
      fill_solid(leds, NUM_LEDS, cmds[cmd].a.color1);
      debugBlink(2);
   }


   // Handle work in progress.

   if (cmd == getCommandNumber("rainbow")) {
      // Slowly cycle a rainbow palette.
      static elapsedMillis rainbowDelay = 0;
      static uint8_t phase = 0;
      if (1000 < rainbowDelay) {
         ++phase;
         fill_rainbow(leds, NUM_LEDS, phase, 1);
         rainbowDelay = 0;
      }
   }

   if (cmd == getCommandNumber("alarm")) {
      // Quickly animate a block of red to the right.
      static elapsedMillis alarmDelay = 0;
      static uint8_t phase = 0;
      if (10 < alarmDelay) {
         ++phase;
         if (NUM_LEDS <= phase) phase = 0;
         fill_solid(leds, NUM_LEDS, CRGB::Black);
         for (size_t i = 0; i < 12; ++i) {
            leds[(phase + i) % NUM_LEDS] = CRGB::Red;
         }
         alarmDelay = 0;
      }
   }

   FastLED.show();
}
