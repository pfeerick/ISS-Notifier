# ISS Notifier
Neopixel notifier for when the ISS goes overhead

This project is inspired and based on this [Hackster project](https://www.hackster.io/pollux-labs/these-cubes-notify-you-when-the-iss-is-overhead-6bfaf8), and is based on [the code here](https://gist.github.com/polluxlabs/1ba7824175c5e011565bd61af2fd1c6b).

It has been adapted to be usable out of the box if you use PlatformIO, with the only changes needed being for you to update the `include/secrets.h` with your WiFi credentials, and location information.

The code defaults to the Neopixel ring being connected to `D3`, in order to allow `LED_BUILTIN` (`D4` on the Wemos D1 Mini) to be used as a blinking indicator whilst the WiFi connection is being established.

If you need to change this, or the length of the Neopixel string, you can change `NEOPIXEL_PIN` and `NUM_OF_NEOPIXELS` near the top of the main source code, as well as `STATUS_LED` and `STATUS_LED_INVERTED` which allow you to change the status LED and set if it is active high or active low.

There is also the startings of OLED display support, which is designed around a 0.91" 128x32 SSD1306 OLED display. If you enable suppot via the `USE_OLED` define, messages will be shown during startup, and the countdown till the next pass, and until the current pass is over will be shown. The gap that is presently on the left side is where a mini ISS logo will eventually go.

The current UTC time is determined by querying the [World Time API](http://worldtimeapi.org), which is a service that provides the local-time for a given timezone in both unixtime and  ISO8601 format. This project runs at a loss, so if you like or found ISS Notifier useful, please consider a [small monthly donation](https://liberapay.com/WorldTimeAPI) toto keep the World Time API going, so other projects like this can continue benefit from its continued operation.
