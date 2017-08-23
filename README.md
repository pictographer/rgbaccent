# rgbaccent
*Accent lighting controller for Teensy 3.2 driving a strip of APA102 RGB LEDs*

This firmware is part of a system to control and automate accent lighting in my home. The system is built using the FastLED library, APA102 RGB LEDs, Teensy 3.2 ARM Cortex-M4 development board, A5-V11 WiFi router, OpenWrt, and Lua.

The A5-V11 WiFi router is running OpenWrt in station mode. The A5-V11 is a client of our main WiFi router, just like a personal
computer or tablet. The A5-V11 changes the lights according to the time of day. The A5-V11 also runs a webserver enabling anyone on
our network to change the color and brightness of the lights. The A5-V11 has a USB-serial connection to the Teensy via an ordinary
USB cable. The Teensy is connected to two momentary push buttons and an I/O buffer IC used to boost the 3.3V signals from the Teensy to 5V needed for the APA102 RGB LED strip.

It is remarkable that the A5-V11 is available for less than $10 including shipping. This device is about the size of a lighter. This is considerably less expensive, and more convenient than many of the so-called maker movement products with similar functionality from the big names like Arduino CC, Adafruit, SparkFun and the Raspberry Pi Foundation. The only drawback is that the A5-V11 is generic no-name electronics so sourcing models with the right ICs and firmware could be challenging. I haven't had trouble but others have.
