# CYD (Cheap Yellow Display) Spec

The display screen is controllable and can be used for APP remote control, remote environmental data collection and remote. Data , remote parameter setting and other batch development applications.

ESP32-2432S028 development board is based on the ESP32-DOWDQ6 controller, low-power, dual-core CPU, clock frequency up to 240MHZ, integrates a wealth of resource peripherals, high-speed SDIO,SPI, UART and other functions

Application: Home smart device image transmission, Wireless monitoring, Smart agriculture QR wireless recognition, Wireless positioning system signal, And other IoT applications

ESP32 WIFI&Bluetooth Development Board 2.8 " 240*320 Smart Display Screen 2.8inch LCD TFT Module With Touch WROOM; Support: 1: UART/SPI/I2C/PWM/ADC/DAC and other interfaces 2:OV2640 and OV7670 cameras, built-in flash 3:picture WiFI upload 4:TF card 5:multiple sleep modes 6:Embedded Lwip and FreeRTOS 7:STA/AP/STA+AP working mode 8:Smart Config 9:AirKiss one-click network configuration 10:secondary development

The ESP32-24325028 development board is based on the company's ESP32-D0WDQ6 controller, with dual core CPU and clock frequency up to 240MHz. It integrates peripherals, high-speed SDO, SP, UART and other functions, and supports automatic download.


## User Comments


* These work fairly well, although the touch screen did not work with the standard tft_espi.h library.. I tried multiple pin assignments, but ended up having to use the xpt2046_touchscreen.h library with the standard pin assignments.. no idea why ... would buy these again.. I am using it for a personal project, not one of the many online projects... oh yeah i had to set # Define tft_inversion_on in the user_setup.h file for the colours to work properly.


* Successfully flashed one with nerdminer v2. Be aware - these boards have both usb-c and micro-usb power connectors, so are the 'version 3' variant of the board with the st7789 display driver.


## Source

AliExpress: [ESP32 Development Board ESP32-2432S028R WiFi Bluetooth 2.8 Inch 240X320 Smart Display TFT Module Touch Screen for Arduino](https://www.aliexpress.com/item/1005010525144441.html) for ~$11 each (2026-03-22)


## More info

[CYD on GitHub](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)
