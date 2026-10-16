# 🐟 ESP32 Fish Feeder Tracker

A small ESP32 project that tracks when my fish were last fed. Press a physical button (or tap a button on a web page) after feeding. The ESP32 records the time, serves a status page you can open from any phone on the same Wi-Fi, and blinks an LED when it's been more than 24 hours.

<img width="320" height="569" alt="demo" src="https://github.com/user-attachments/assets/d2ebabd5-00f4-4e15-bbd9-df87c1b8aef9" />
<img width="2160" height="2880" alt="fishfeederpic" src="https://github.com/user-attachments/assets/c5bc3888-305f-4feb-8887-6d128864551a" />
<img width="1920" height="1036" alt="2image" src="https://github.com/user-attachments/assets/ebc0cc44-473f-481e-a1f5-5d15504b4767" />
<img width="1917" height="1041" alt="image" src="https://github.com/user-attachments/assets/cbcfbe9f-85b9-47ab-ab05-fec81da8b1e2" />
This image shows "last fed 1 min ago" for testing.

## Features

- **Physical button** to log a feeding, with software debouncing
- **Mobile-friendly web page** showing the last feeding time and how long ago it was, with an "I just fed them" button
- **Reminder LED** that blinks when the fish are due to be fed (default: 24 hours)
- **Real time from the internet** via NTP, with Pacific time and daylight saving handled automatically
- **Saved across power loss.** The last feeding time is stored in the ESP32's flash memory.
- **Local hostname**: reachable at `http://fishfeeder.local` on most devices

## Hardware

| Part | Notes |
|---|---|
| ESP32 dev board | HiLetgo ESP-WROOM-32 (30-pin DevKit) |
| Tactile push button | 4-pin, 6 mm |
| Breadboard + jumper wires | No soldering required |
| Built-in LED | Blue LED on GPIO 2, nothing to wire |

## Wiring

| Button leg | ESP32 pin |
|---|---|
| One leg | D4 (GPIO 4) |
| Diagonally opposite leg | GND |

The button straddles the center gap of the breadboard. Using diagonal legs guarantees the two pins are on opposite sides of the switch. No external resistor is needed because the code enables the ESP32's internal pull-up, so the pin reads `HIGH` normally and `LOW` when pressed.

## Setup

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) and add ESP32 support (Boards Manager → search **esp32** by Espressif).
2. Open `fish_feeder/fish_feeder.ino`.
3. Enter your Wi-Fi name and password at the top of the file (2.4 GHz networks only).
4. Select **Tools → Board → ESP32 Dev Module** and the correct port.
5. Click **Upload**. If it hangs on `Connecting.....`, hold the **BOOT** button until the upload starts.
6. Open the Serial Monitor at **115200 baud** to see the board's IP address.
7. Open that address (or `http://fishfeeder.local`) on a phone connected to the same Wi-Fi.

## How It Works

- **Button input:** GPIO 4 uses `INPUT_PULLUP`. A non-blocking debounce (50 ms) makes sure one press counts once.
- **Timekeeping:** `configTzTime()` syncs the clock from NTP servers. Feedings are only recorded after the clock has synced, so timestamps are always real.
- **Storage:** the last feeding time is saved with the `Preferences` library (non-volatile flash).
- **Web server:** the built-in `WebServer` library serves the status page at `/` and accepts feedings via a POST to `/fed`. The page auto-refreshes every 30 seconds.
- **LED logic:** `millis()`-based blinking, so the web server and button stay responsive.

## Configuration

At the top of the sketch:

| Setting | Default | Meaning |
|---|---|---|
| `FEED_INTERVAL_HOURS` | `24` | Hours before the LED starts blinking |
| `TIMEZONE` | Pacific | POSIX timezone string |
| `HOSTNAME` | `fishfeeder` | Name for `http://<name>.local` |

## Testing

- Prototyped in the [Wokwi](https://wokwi.com) ESP32 simulator using its virtual `Wokwi-GUEST` network to check the button wiring, LED behavior, Wi-Fi connection, and NTP sync.
- To test the reminder quickly, temporarily change `3600UL` to `1UL` in `needsFeeding()` so the interval counts in seconds instead of hours.

## Future Ideas

- Feeding history (last several feedings)
- Separate tracking for water changes
- Temperature monitoring with a waterproof DS18B20 probe
- Custom PCB

## Acknowledgments

Code was drafted with help from Claude (AI). Wiring, simulation, hardware testing, and modifications were done by me.

## Author

Michelle Xie
