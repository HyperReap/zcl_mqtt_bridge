Author: Petr Urbánek

This component should be used as c++ lightweight version of ZCL to MQTT bridge connected to Homeassistant. It uses MQTT Discovery for adding new devices.
For now it supports only some of Xiaomi Aqara devices, feel free to add more.

This component shall be integrated in time in ESPHome. However for now it must be used as custom_component.

ZclMqttBridge should work on ESP32 as well as ESP8266, though was tested only on ESP32.


Config:

in your config file, you must redefine wifi, mqtt and uart tags according to what you are using.

ESP32 must be connected to cc2530 via UART interface.
cc2530 shall have FW useable for UART communication, in this repository you can find FW I used (cc2530FW.hex).

| ESP32 | CC2530 |
| ------- | ------- |
| RX   |   P0.3 |
| TX   |   P0.2 |
| 3v3  |   VCC  |
| GND  |   GND  |
| GND  |  P2.0(CFG1) |




USE:

After connecting ESP32 to electric nwk, you should wait about 1 minute to let the component fully initialize.
Then after permiting joining devices - using switch 'PermitJoiningReq' - you can start creating your new IoT nwk.




Files Description:

zcl_mqtt_bridge.cpp contains the class for new ESPHome component (ZclMqttBridge).
zcl_mqtt_bridge.h cotains helpfull functions and classes, for easier programming.
black.yaml is ESPHome config file, containing all components
prototypeFW.hex is FW used for cc2530 board


---------------- WORK IN PROGRESS --------------------
------------ FEEL FREE TO CONTRIBUTE ----------------
