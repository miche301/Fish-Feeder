# 🐟 ESP32 Fish Feeder Tracker

An ESP32 project that tracks when my fish were last fed. Press a button after feeding, and the device records the time, shows a live countdown on a round TFT display, serves a status page you can open from any phone on the same Wi-Fi, and reminds you when it's time to feed again.

The project has two versions:

| Version | What it adds | Folder |
|---|---|---|
| **v2** (current) | 1.28" round TFT with a countdown ring, feeding history on the web page | [`fish_feeder_v2/`](fish_feeder_v2/) |
| **v1** | Button, blinking reminder LED, web status page | [`fish_feeder/`](fish_feeder/) |

<!-- Add a photo or GIF of the v2 screen here, e.g.: ![v2 round display](images/v2.jpg) -->

## v2 Features

- **Countdown ring** on a round 240×240 display. It starts full right after a feeding and empties as the next one gets closer.
- **Big countdown** in the center (`23h 05m`, switching to seconds near the end) with the last feeding time underneath
- **"Feed me!" alert:** the ring turns orange, the fish icon blinks, and the screen shows how late the feeding is
- **Feeding history:** the web page lists the last 5 feedings
- **Flicker-free updates:** only the parts of the screen that changed get redrawn
- **One-switch test mode** that shortens the interval to 1 minute
- Everything from v1: physical button, reminder LED, phone status page, real time from the internet, and data saved across power loss. Saved v1 data carries over automatically.

## Hardware

| Part | Notes |
|---|---|
| ESP32 dev board | HiLetgo ESP-WROOM-32 (30-pin DevKit) |
| 1.28" round TFT display | 240×240, GC9A01 driver, SPI (v2 only) |
| Tactile push button | 4-pin, 6 mm |
| Breadboard + jumper wires | No soldering required |
| Built-in LED | Blue LED on GPIO 2, nothing to wire |

## Wiring

**Display (v2)**

| Display pin | ESP32 pin | Purpose |
|---|---|---|
| VCC | 3V3 | Power (3.3 V, not VIN) |
| GND | GND | Ground |
| SCL | D18 | SPI clock |
| SDA | D23 | SPI data (MOSI) |
| RES | TX2 (GPIO 17) | Reset |
| DC | RX2 (GPIO 16) | Data/command select |
| CS | D5 | Chip select |
| BLK | 3V3 | Backlight always on |

**Button (v1 and v2)**

| Button leg | ESP32 pin |
|---|---|
| One leg | D4 (GPIO 4) |
| Diagonally opposite leg | GND |

No external resistor is needed for the button. The code enables the ESP32's internal pull-up, so the pin reads `HIGH` normally and `LOW` when pressed. Using diagonal legs guarantees the two pins are on opposite sides of the switch.

## Setup

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) and add ESP32 support (Boards Manager → search **esp32** by Espressif).
2. For v2, install **Adafruit GC9A01A** from the Library Manager and accept its dependencies (Adafruit GFX, Adafruit BusIO).
3. Open `fish_feeder_v2/fish_feeder_v2.ino` and enter your Wi-Fi name and password at the top (2.4 GHz networks only).
4. Select **Tools → Board → ESP32 Dev Module** and the correct port.
5. Click **Upload**. If it hangs on `Connecting.....`, hold the **BOOT** button until the upload starts.
6. The screen shows the board's IP address for a few seconds at startup. It's also printed in the Serial Monitor at **115200 baud**.
7. Open that address (or `http://fishfeeder.local`) on a phone connected to the same Wi-Fi.


## Configuration

All settings are at the top of the sketch:

| Setting | Default | Meaning |
|---|---|---|
| `FEED_INTERVAL_HOURS` | `24` | Hours between feedings |
| `TEST_MODE` | `false` | `true` shortens the interval to 1 minute for testing |
| `TIMEZONE` | Pacific | POSIX timezone string |
| `HOSTNAME` | `fishfeeder` | Name for `http://<name>.local` |

## How It Works

- **Single source of truth for timing:** every part of the code (the due check, the countdown, and the ring) gets the interval from one `intervalSec()` function, so `TEST_MODE` changes them all at once.
- **Countdown ring:** drawn as a thick arc built from filled triangles in 3° steps, with the filled share equal to the fraction of time remaining.
- **Partial redraws:** the code remembers what is currently on screen and only redraws a ring, icon, or text line when its value or color changes. Text areas are cleared to a width that fits inside the circular screen.
- **Button input:** GPIO 4 uses `INPUT_PULLUP` with a non-blocking 50 ms debounce.
- **Timekeeping:** `configTzTime()` syncs the clock from NTP servers. Feedings are only recorded after the clock has synced, so every timestamp is real.
- **Storage:** the last 5 feedings are saved to flash with the `Preferences` library. On first boot, v2 imports the single saved time from v1.
- **Web server:** the built-in `WebServer` library serves the status page at `/` and accepts feedings via a POST to `/fed`.

## Demo (v1)

<img width="320" height="569" alt="demo" src="https://github.com/user-attachments/assets/d2ebabd5-00f4-4e15-bbd9-df87c1b8aef9" />
<img width="320" height="569" alt="fishfeederpic" src="https://github.com/user-attachments/assets/c5bc3888-305f-4feb-8887-6d128864551a" />
<img width="1920" height="1036" alt="2image" src="https://github.com/user-attachments/assets/ebc0cc44-473f-481e-a1f5-5d15504b4767" />
<img width="1917" height="1041" alt="image" src="https://github.com/user-attachments/assets/cbcfbe9f-85b9-47ab-ab05-fec81da8b1e2" />
This image shows "last fed 1 min ago" for testing.

*In v1, the blue LED blinks when feeding is due. One button press logs the feeding and turns it off.*

## Testing

- Prototyped v1 in the [Wokwi](https://wokwi.com) ESP32 simulator to check the button wiring, LED behavior, Wi-Fi connection, and NTP sync.
- Verified on hardware with `TEST_MODE` set to `true`, so the full cycle (fed → countdown → "Feed me!" → fed) takes about a minute.

## Acknowledgments

Code was drafted with help from Claude (AI). Wiring, simulation, hardware testing, and modifications were done by me.

## Author

Michelle Xie
