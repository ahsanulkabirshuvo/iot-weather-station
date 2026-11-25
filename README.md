# IoT Weather Station (NodeMCU ESP8266)

**A low-cost Wi-Fi weather station that measures temperature, humidity, air pressure, light level and rain, and shows the data on a built-in web page and on a Blynk IoT dashboard.**

![Board](https://img.shields.io/badge/Board-NodeMCU_ESP8266-blue)
![Framework](https://img.shields.io/badge/Framework-Arduino-00979d)
![Cloud](https://img.shields.io/badge/Dashboard-Blynk_IoT-23c48e)
![License](https://img.shields.io/badge/License-MIT-green)

## Overview

Weather data from official stations is often unavailable at a specific location. This project shows how a few cheap sensors and a Wi-Fi microcontroller can report local conditions live over the internet. The NodeMCU reads the sensors every 5 seconds and publishes the values in two ways:

- a **local web page** served by the board itself (open the board's IP address in any browser on the same network), plus a JSON endpoint for other tools, and
- an optional **Blynk IoT dashboard** for viewing the data on a phone from anywhere.

It was built as a course project for ME-3110 Measurement and Instrumentation Sessional at the Department of Mechanical and Production Engineering, Ahsanullah University of Science and Technology (AUST), and tested in August 2023.

## Measured parameters

| Parameter | Sensor | Interface |
|---|---|---|
| Temperature | DHT11 | Digital (single wire) |
| Relative humidity | DHT11 | Digital (single wire) |
| Air pressure | BMP180 | I2C |
| Light level | LDR (light dependent resistor) | Analog, 0–100 % |
| Rain (rain or dry) | Rain sensor module | Digital output |

## Hardware

| Part | Quantity |
|---|---|
| NodeMCU ESP8266 development board | 1 |
| DHT11 temperature and humidity sensor | 1 |
| BMP180 barometric pressure sensor | 1 |
| LDR and a 10 kΩ resistor | 1 each |
| Rain sensor module (with comparator board) | 1 |
| Breadboard, jumper wires, USB cable | as needed |

### Wiring

| Sensor pin | NodeMCU pin |
|---|---|
| DHT11 data | D5 |
| DHT11 VCC / GND | 3V3 / GND |
| BMP180 SDA | D2 |
| BMP180 SCL | D1 |
| BMP180 VCC / GND | 3V3 / GND |
| Rain module DO | D6 |
| Rain module VCC / GND | 3V3 / GND |
| LDR divider midpoint | A0 |

The LDR divider is: 3V3 → LDR → A0 → 10 kΩ → GND. Brighter light gives a higher reading. If your readings go the other way, set `LDR_INVERT` to `true` in the sketch.

The NodeMCU has only one analog input, so the LDR uses it and the rain module is read through its digital output. This gives a rain or dry signal, not a rainfall amount in millimetres.

## Quick start

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) and add ESP8266 board support (Boards Manager, package **esp8266 by ESP8266 Community**).
2. Install these libraries from the Library Manager:
   - **DHT sensor library** (Adafruit) and **Adafruit Unified Sensor**
   - **Adafruit BMP085 Library** (also works with the BMP180)
   - **Blynk** (only needed for the Blynk dashboard)
3. In [`firmware/weather_station`](firmware/weather_station), copy `config.example.h` to `config.h` and enter your Wi-Fi name and password. `config.h` is git-ignored, so your credentials stay private.
4. Select **NodeMCU 1.0 (ESP-12E Module)** as the board, then upload `weather_station.ino`.
5. Open the Serial Monitor at **115200 baud**. When the board connects, it prints its IP address and then one line of readings every 5 seconds.
6. Open `http://<the-printed-ip>/` in a browser on the same Wi-Fi network.

If the Wi-Fi does not connect, check the name and password in `config.h` and reset the board.

## Local web page and JSON

The board serves a live dashboard at `/` and the raw data at `/json`.

Example response from `/json`:

```json
{"temperature_c":30.5,"humidity_pct":79,"pressure_hpa":998.0,"light_pct":62,"raining":false}
```

A sensor that fails to read is reported as `null` instead of a wrong value.

## Blynk IoT dashboard (optional)

1. In [Blynk](https://blynk.io), create a template named **Weather Station** with hardware **ESP8266** and connection type **WiFi**.
2. Add these datastreams (type **Virtual Pin**):

   | Pin | Name | Type | Range |
   |---|---|---|---|
   | V0 | Temperature | Double | 0–100 |
   | V1 | Humidity | Double | 0–100 |
   | V2 | Rain | Integer | 0–100 |
   | V3 | Pressure | Double | 200–1100 |
   | V4 | Rain LED | Integer | 0–255 |
   | V5 | Light | Integer | 0–100 |

3. Add widgets (gauges for V0–V3 and V5, an LED for V4) and create a device from the template.
4. Copy the template ID, template name and auth token from the device into `config.h`.

To run without Blynk, comment out `#define USE_BLYNK` in `config.h`. The local web page works either way.

## Results from the original test run

Readings recorded during the August 2023 test run of the original prototype.

| Date and time | Temperature (°C) | Humidity (%) | Pressure (mbar) |
|---|---:|---:|---:|
| 5 Aug, 9:00 AM | 29.5 | 85 | 999 |
| 5 Aug, 11:00 AM | 29.6 | 86 | 1003 |
| 6 Aug, 9:00 AM | 30.1 | 82 | 997 |
| 6 Aug, 1:00 PM | 32.3 | 80 | 995 |
| 7 Aug, 9:00 AM | 31.2 | 78 | 997 |
| 7 Aug, 9:56 PM | 30.5 | 79 | 997.97 |

Over the three days the temperature stayed between 29.5 and 32.3 °C (the highest value was at 1:00 PM on 6 August), the humidity between 78 and 86 %, and the pressure between 995 and 1003 mbar.

## Limitations

- The DHT11 is a basic sensor, typically accurate to about ±2 °C and ±5 % relative humidity.
- The BMP180 reports absolute pressure at the station, not pressure adjusted to sea level.
- Rain is detected as present or absent. The project does not measure rainfall intensity or amount.
- The prototype was built on a breadboard without a weather-proof enclosure or radiation shield, so it is meant for demonstration and learning rather than outdoor deployment.
- The measurements were recorded at a few times of day over three days, which is too short for forecasting or trend analysis.

## About this repository

The sketch from the 2023 demonstration was not archived. The firmware in this repository is a clean re-implementation for the same hardware and the same dashboard layout (temperature, humidity, rain, pressure), written later and documented here so the project can be rebuilt. Pin assignments are defined by this firmware and listed in the wiring table above. It has been syntax-checked but not yet re-tested on the original hardware.

## Repository structure

```text
.
├── LICENSE
├── README.md
├── .gitignore
└── firmware/
    └── weather_station/
        ├── weather_station.ino
        └── config.example.h
```

## License

Released under the [MIT License](LICENSE).
