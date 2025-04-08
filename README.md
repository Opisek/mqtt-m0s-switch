# MQTT M0S Switch
## Purpose
To improve my bed occupation detection for smarthome automations, I decided to get an off-the-shelf pressure mat (as has been done by many people before me) and wire it up to a microcontroller.

This project arose from me finding out that my attempts to desolder pins from my beloved WeMos LOLIN D1 Mini microcontrollers without any desoldering gear resulted in nearly all my spares being fried and rendered useless.

I decided for this to be a learning opportunity and reached into my drawer full of Bouffalo Lab's M0S Dock microcontrollers, which I had feared ever since finding out that I cannot use the Arduino framework with them.

In the end, FreeRTOS does not seem as scary as before, but actually fun and powerful.

## How it works
The microcontroller attempt to connect to the WiFi network with credentials hard-coded in `config.h`. DHCP is required on the network. You can recognize the WiFi state based on the second LED as follows:
- Fast blinking: Connecting to WiFi
- Slow blinking: Error (The chip still continues to try to connect)
- Solid: Connected to WiFi

As long as WiFi is connected, a connection with the MQTT broker hard-coded in `config` is attempted. You can recognize the MQTT state based on the third LED as follows:
- Off: WiFi is not connected
- Fast blinking: Connecting to MQTT
- Slow blinking: Error (The chip still continues to try to connect)
- Solid: Connected to MQTT

Once MQTT is connected, the microcontroller sets its availability topic to `online` and publishes its will to set this topic to `offline` after 4 seconds of failed keep-alives. Furthermore, a Homeassistant discovery message is sent.

As long as MQTT is connected, the chip polls the state of GPIO pin 16 every 100ms and publishes `contact` when `IO16` and `GND` are shorted or otherwise `clear` on the state topic.

By connecting `IO16` and `GND` to two ends of a pressure sensor, like a pressure mat, you get a simple binary sensor inside of Homeassistant or any other software registered with your MQTT broker.

## Building
You need to set the environmental variable `BOUFFALO_SDK_PATH` to the downloaded [Bouffalo SDK](https://github.com/bouffalolab/bouffalo_sdk/).

Make sure to rename `config_example.h` to `config.h` and configure all the parameters.

To build, run `make` inside this repository.

To flash, hold the "Boot" button while connecting the M0S Dock to your computer. Next, run `make flash COMX=...` replacing the ellipsis with the location of your microcontroller, e.g. `COM4` on Windows or `/dev/ttyACM0` on Linux.

Disconnect the chip from your computer and connect it to any USB power supply et voilà.

## Why the code sucks
The codebase largery sucks. This is because it is my first ever contact with FreeRTOS and I'm mostly throwing code at the wall and see what sticks.

Furthermore, I found I was unable to properly set `GPIO_PULLDOWN` and `GPIO_PULLUP` for the pins, so `reset` and `set` had to be swapped.

In the future, I would like to look into GPIO interrupts for publishing state changes instead of continuous polling.

## Credit
The code is largely based on Bouffalo Lab's [MQTT](https://github.com/bouffalolab/bouffalo_sdk/tree/76ebf6ffcbc2a81d18dd18eb3a22810779edae1a/examples/wifi/sta/wifi_mqtt_pub) and [GPIO](https://github.com/bouffalolab/bouffalo_sdk/tree/76ebf6ffcbc2a81d18dd18eb3a22810779edae1a/examples/peripherals/gpio/gpio_input_output) licensed under Apache-2.0 with my modifications to connect MQTT and GPIO together.