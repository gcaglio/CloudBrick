# CloudBrick
This project will provide you information, electronic schematics and Arduino sketches to let you build mini devices for LEGOⓇ management and integration with an MQTT broker.
The CloudBrick is an electronic device made basically by an ESP8266 chip that provide the programmable logic and a power driver (H-Bridge) to let you pilot the required power for motors or lights.
If well assembled, you will have a small board of about 2 x 6 x 1 LEGOⓇ studs, that you could easily hide inside your model, behind a wall, and so on.

# What you can do with a CloudBrick
You can remotely control LEGOⓇ models (trains, cars, lights, and so on) using a smartphone and an internet connection.
You can control many devices from your smartphone with a single app, without the need to disconnect and reconnect to each different model.
You can also script complex scenarios using your preferred programming language because this project use the standard and very supported protocol MQTT.

# Example
You built a LEGOⓇ train diorama and you want to manage your train motors and lights from one central panel with all the control of your models.
In this example, you only need to:
1. build one "cloudbrick" for each model you want to control
2. configure "cloudbrick(s)" to your wifi and to the mqtt broker you choose (in cloud or installed on your pc or on a single board computer like a RaspberryPi)
3. draw the interface on your smartphone, adding button(s) and slider(s) as you want.

# Hardware requirements to build a CloudBrick
* 1x ESP8266 chip (12-E or 12-F are widely tested)
* 1x Dual H-Bridge chip L293D
* 1x PCB breadboard (to solder component onto)
* 1x Voltage regulator (9V to 3.3V - since ESP8266 need to be powered with 3.3V only)   

You will also need :
* soldering iron and tin
* some little wires to build the connections
