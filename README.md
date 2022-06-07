
# Water Leakage Detection System

## Problem

In European Union, more than half of the population lives in agglomerations or more than 150,000 population equivalent (PE), generating a daily amount of 41.5 million m3 of wastewater. Besides, an annual portion of 2.4% (counts for 1 billion m3) of treated domestic wastewater discharges contains reusable nutrients, organic carbon, lipids, and biosolids.
For that reason one of the big challenges for the smart cities of the future is to reduce the waste of water in the urban area. If monitoring an entire big water distribution network, find the leaks of water and locate them could be very challenging, monitoring the home wastewater could be easyer.
My water leakage detection system aims to detect a water leak in an office or home environment and notifies the problem to the user. The system is equipped of a buzzer that will signal the malfunctioning of a specific tap also with an acoustic signal.

## Sensors

- ***HC-SR501 Passive Infrared (PIR) Motion Sensor***: <img src="images/pir.jpg" width="100" align="right"/> <br/> This sensor is used to capture movements near the monitored tap. If there is movement near the tap it sends a positive signal otherwise 0. In our system the PIR is used to understand wether there is someone using the tap or not. The datasheet of the HC-SR501 PIR can be found here: [PIR Datasheet](https://pdf1.alldatasheet.com/datasheet-pdf/view/1131987/ETC2/HC-SR501.html).

- ***Water Flow Sensor***:<img src="images/wf.jpg" width="100" align="right"/> <br/> This sensor monitor wheter there is water flow exiting from the tap or not. If there is some water flow exiting from the tap it sends a positive signal otherwise it sends 0. The sensor has a small turbine that is activated by the flow. The movement of the turbine actives the magnetic Hall effect sensor, placed near the turbine, that generates the output signal. In our system this sensor is used to understand wether the tap is open or not. 



## Actuators
<br/>

- ***Led***: <img src="images/led.jpg" width="70" align="right"/> <br/> Classic led used to notify if the system is on. The led is always on while the system is working.

<br/>

- ***Active Buzzer***: <img src="images/active-buzzer.jpg" width="100" align="right"/> <br/> The active buzzer is turned on when a leakage is dected, in particular the usage of the active buzzer is to use the acoustic signa to keep the attention of someone that can close the tap stopping the leak of water. As soon as the tap is closed the buzzer stops. The datasheet of the active buzzer is available here: [Active Buzzer Datasheet](https://components101.com/sites/default/files/component_datasheet/Buzzer%20Datasheet.pdf).


## How the system works

The water leakage system proposed in this work is composed by the main mather board that is an STM32 Nucleo-F44ZE connected with two sensors, a PIR motion sensor and a water flow sensor, and two actuators, one led and one active buzzer. (Refer to [Technology]() document for details)

The two sensors monitor wether there is mouvement near the monitored tap and wether there is a water flow through the tap. If there is some water flow through the tap with no mouvement detected near the tap then the system notyfies a leakage of water. When the leakage of water is detected the buzzer is activated with the goal to draw someone’s attention that can easy solve the problem, for example a faucet left open can be closed easily and quickly by anyone.

In the meanwhile that the system monitors the environment it sends packets containing the collected data to the cloud. Each message sends to the cloud contains information about water flow, mouvement detected and if there is a leak of water. 
These information are written in a JSON format of the following form {"water": int, "mouvement": int, "leak": int} where the water and mouvement field contain the values reported by the sensors and the "leak" field is a binary value set to 1 if there is a leak of water otherwise 0.

## Network structure

In order to transmits the data collected by the sensors we need to connect the Nucleo board to a MQTT-SN broker thorugh which the RIOT-OS application will publish the sensor values over UDP/IPv6. We also need to set up an MQTT-SN/MQTT transparent bridge in order to subscribe to a predefined set of topics on the MQTT-SN broker and forward all messages received to the cloud service throug MQTT protocol. 
The following schema represents the network structure.

*Mettere immagine network*

More in detail as MQTT-SN broker I use the [mosquitto rsmb broker](https://github.com/eclipse/mosquitto.rsmb) that is a server implementation of the MQTT and MQTT-SN protocols. Since the RIOT-OS uses MQTT-SN it can send message to mosquitto rsmb broker that will forward all the incoming messages to [mosquitto broker](https://mosquitto.org/) through MQTT. 
The mosquitto broker needs to be set on the local machine and modifying its configuration file it can be connected to cloud service. Once connected to the cloud service mosquitto can act as MQTT-SN/MQTT transparent bridge by forwarding all the message received by the rsmb broker to the cloud.

For the cloud part I use [AWS](https://aws.amazon.com/it/) cloud services. All the messages received by the mosquitto broker are stored in a dynamoDb and in a S3 bucket.

## Evaluation

