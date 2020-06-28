# esp8266_WeatherStation

## About
MQTT weather station using a esp8266 based board (Wemos D1 mini) and a DHT11 sensor. PlatformIO file project for convenience.

HTTP communication with the thingspeak API is commented out for convenience but should work without problems if an API key is available.

In the interest of consuming as little power as possible the device is going into deep sleep and reset every 15mins to update the MQTT broker with new temperature and pressure readings

## Requirements
+ Weemos D1 mini board
+ DHT11 Temperature/Humidity sensor
+ 3-5V supply or battery

![HW](HW.jpg)

+ [Visual Studio Code](https://code.visualstudio.com/) (or Codium) with:
    + [PlatformIO extension](https://platformio.org/) with platform Espressif 8266 installed (configuration for the board in [platformio.ini](platformio.ini))

## Demo
(Tested with MQTT_Dashboard)

![Demo](Demo.gif)

## References and links
+ Libraries and code based on examples from the Unoficial Develpment Kit for Espressif ESP8266, available at https://programs74.ru/udkew-en.html

## TODO
+ Implement initial SSID and password configuration through serial port
+ Implement initial sleep time configuration through serial port

## Contributing 
Feel free to drop a line/contact me if interested in this project