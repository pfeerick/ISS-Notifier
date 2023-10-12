[![Gitpod ready-to-code](https://img.shields.io/badge/Gitpod-ready--to--code-blue?logo=gitpod)](https://gitpod.io/#https://github.com/pfeerick/ISS-Notifier)

# ISS Notifier
Neopixel notifier for when the ISS goes overhead

This project is inspired and based on this [Hackster project](https://www.hackster.io/pollux-labs/these-cubes-notify-you-when-the-iss-is-overhead-6bfaf8), and is based on [the code here](https://gist.github.com/polluxlabs/1ba7824175c5e011565bd61af2fd1c6b).

> [!WARNING]  
> I've recently noticed this no longer works, due to http://open-notify.org/ no longer providing the pass predictions this relied on. I'm investigating alternatives, but I'm not holding my breath.

It has been adapted to be usable out of the box if you use PlatformIO, with the only changes needed being for you to update the `include/secrets.h` with your WiFi credentials, and location information, and the define flags at the top of the main code if you need different settings, such as which pin the status LED is on, or the length of the neopixel string you are using.

The code defaults to the Neopixel string (or ring) being connected to `D3`, in order to allow `LED_BUILTIN` (`D4` on the Wemos D1 Mini) to be used as a blinking indicator whilst the WiFi connection is being established. If you need to change this, or the length of the Neopixel string, you can change `NEOPIXEL_PIN` and `NUM_OF_NEOPIXELS` near the top of the main source code, as well as `STATUS_LED` and `STATUS_LED_INVERTED` which allow you to change the status LED and set if it is active high or active low.

There is also the startings of OLED display support, which is designed around a 0.91" 128x32 SSD1306 OLED display. If you enable support via the `USE_OLED` define, status messages will be shown on the display, and there will be a countdown shown for time remaining until the next pass or altenately the time remaining for hte current pass. The gap that is presently on the left side is where a mini ISS logo will eventually go.

The current UTC time is determined by querying the [World Time API](http://worldtimeapi.org), which is a service that provides the local-time for a given timezone in both unixtime and  ISO8601 format. This project runs at a loss, so if you like or found ISS Notifier useful, please consider a [small monthly donation](https://liberapay.com/WorldTimeAPI) to keep the World Time API going, so other projects like this can continue benefit from its continued operation.

There is now the beginnings of rudimentary timezone support. You can configure this also in the `include/secrets.h` file. The instructions on [how to format the timezone rule are here](https://github.com/JChristensen/Timezone#coding-timechangerules).

PlatformIO build environments can be used to set defines, and strings (via `build_flags`), so it is also possible to use different build environments for different SSIDs, configurations, etc. There is an example of this for the 17 pixel string I am using for testing, which also enables the OLED support.
