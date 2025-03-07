# Overview

This file is responsible for delivering simulation data to OT endpoint systems. (usually PLCs) Must be ran in combination with the Data Broker System to be initialized properly. Standard operation consists of first running this file and then starting the Data Broker.


# Usage
*python3 Endpoint.py*

No arguments are supplied, as program settings are derived from initialization message
When run, the program waits for an initialization message from the Data Broker. This message is equivalent to the content found in the input.json file. The program then sends simulation information to one or multiple PLCs, and returns any specified values from PLCs back to the simulation engine. The endpoint has three modes of operation, selected based on the "Proto" tag in the initialization message. The "initialization message" in this context refers to the content of the "input.json" file used to run the Data Broker program component.


# Supported Modes of Operation

**Modbus**
- This option uses the Modbus protocol to directly update PLC memory registrars. Values to update are derived from the "SensorMem" tag in the initialization message.

**OPC-UA Server**
- This option has the Endpoint function as an OPC-UA server which may host client connections from PLCs. Note that this option requires a specific PLC configuration to support data transfer. The PLC must connect and update tag values using its own logic. 

**OPC-UA Client**
- This uses the OPC-UA Protocol as a client system for the updating of PLC node values. Values to update are derived from the "SensorTags" tag in the initialization message. Ensure your PLC uses an identical naming structure for OPC-UA Node IDs. 


# Additional Key Components
**UDP_Client**
- Handles the receiving updates from the physics simulation

**Connector**
- Class that contains PLC connection logic. Instantiated once per PLC Connection

**Data_Repo**
- Class that functions as a data storage module for received simulation data

