# CloudBrick 2PWM/1iPWM and 2digital output

## Feature
* Wifi captive portal for configuration
* OTA update
* MQTTS communication on specific topic
* PWM outputs - multiple alternatives
  - mode "ipwm" to interface an H-bridge driver with 2 PWM inputs (like MAX1508)
  - mode "pwm" to interface an H-bridge driver with 1PWM input pin and 2 digital output to  (like L293D)
* 2 digital output (active HIGH) - for example, to drive leds.


## Overview
At the first execution the ESP8266 will configure as Wifi access point with SSID : `myBrickThing`, with password `mybrickthing`.
Once connected to this AP, the ESP will present a captive portal that allow you to configure these parameters:
* thing name - the name of this device, used to compose the MQTT TOPIC name
* AP password - the Admin password of this device, used to login and change configuration in the future, via http (username will be `admin`)
* wifi ssid - Wifi network name (SSID) 
* wifi pwd - password of the wifi
* mqttserver - fqdn of the MQTTS broker 
* mqttport - tcp port of the MQTTS broker
* mqttuser - username to authenticate on MQTTS broker
* mqttpassword - password to authenticate on MQTTS broker
* use ipwm [ ] - checkbox to select the PWM mode
  CHECKED  = ipwm, with 1 output that use 2 pwm output
  UNCHECKED  = pwm, with 2pwm output driven by 2 digital output and 1 pwm for each channel
  
After saving the configuration, the ESP will reboot and try to connect to the WIFI and register on the MQTTS broker.
By default this sketch will use these topics:
* SUBSCRIBE to : `/CloudBrick/BRC_ESP_`thingname`/Command`
* PUBLISH to : `/CloudBrick/BRC_ESP_`thingname`/Status`

For example, if the "thing name" configuration field is "myobject", the topics will be
* SUBSCRIBE to : `/CloudBrick/BRC_ESP_myobject/Command`
* PUBLISH to : `/CloudBrick/BRC_ESP_myobject/Status`

 
## Sample HW configurations/use
Configuration A
- use only 2 digital output to drive leds or an external driver (active HIGH)

Configuration B
- use 2 digital output to drive leds or an external driver (active HIGH)
- use one 2channel H-BRIDGE (2 digital output and 1 pwm output - for each channel)
  eg: L293D

Configuration C
- use 2 digital output to drive leds or an external driver (active HIGH)
- use one H-BRIDGE (2 pwm output)
  eg: MAX1508

  
## MQTT payloads/commands
Here some payload examples
### PWM mode (unchecked)
You could drive OUTPUT1 and OUTPUT2 (O1 and O2)
Activate PWM Output 1, setting direction, full throttle, and shutdown the output after 2 seconds 
`{
  "O1_pwr":1024,
  "O1_dir":0,
  "O1_timeout":2000
}`

Shutdown PWM Output 1
`{
  "O1_pwr":0,
}`

Activate PWM Output 1, setting direction (reverse direction), full throttle
`{
  "O1_pwr":1024,
  "O1_dir":1,
}`

### IPWM mode (checked)
Like PWM, but you can only address OUTPUT1 (O1).
Activate PWM Output 1, setting direction, full throttle, and shutdown the output after 2 seconds 
`{
  "O1_pwr":1024,
  "O1_dir":0,
  "O1_timeout":2000
}`

### Digital outputs
Activate OUTPUT3 for 7seconds
`{ 
  "O3_status":0,
  "O3_timeout" : 7000 
}`

### Digital outputs
Activate (HIGH) OUTPUT3 for 7seconds, then go LOW.
`{ 
  "O3_status":0,
  "O3_timeout" : 7000 
}`

Activate (HIGH) OUTPUT4, then go LOW.
`{ 
  "O4_status":0
}`

### Compose all the commands in one single JSON
`{
  "O1_pwr":1000,
  "O1_dir":0,
  "O1_timeout":2000,
  "O3_status":1,
  "O3_timeout":3000,
  "O4_status":0
}`






