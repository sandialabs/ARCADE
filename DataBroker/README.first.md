# DataBroker Quick Start

This is minimalistic quick start guide for using DataBroker. It describes how
to build `DataBroker` and run a simple Simulink example
`SampleSimulinkModel.slx` with it. The Simulink example is shipped with
`DataBroker` and is located in `Connectors/Simulink` directory. Instructions
here apply for Linux and MacOS systems.

## Requirements

Following are requirements to build and run DataBroker:
- C compiler (a reasonably recent version, I used GCC 13.4)
- Matlab with Simulink (a reasonably recent version)
- ZeroMQ library (>= 4.0)
- Python 3

## Building DataBroker Code

To compile `DataBroker` edit script [`compile.sh`](Linux/compile.sh) to match
your compiler and find ZeroMQ library at your system and run it within
`DataBroker/Linux` directory (works for Linux and MacOS). An executable
`DB` will be created in the same directory.

Next, create a mex file. In Matlab console change into directory
`Connectors/Simulink` and compile `sfun_connector.c` into a `mex` file be
executing
```shell
>> mex sfun_connector.c
```
The output should look like this
```shell
Building with 'Xcode with Clang'.
MEX completed successfully.
>> 
```
and a file `sfun_connector.mexa64` should be created in the same directory.

## Configuring DataBroker Simulation

In the directory where you run `DB` executable there should be a JSON
configuration file with content like this:
```json
{
    "Simulator": [
        {
            "executableName": "Simulink",
            "hold_for_dante": "false",
            "co_sim_enable": "true"
        }
    ],
    "cosim": [
        {
            "sync_enable": "true",
            "outputs": "Output_Value_1,Output_Value_2"
        }
    ]
}
```

## Running a Simple Example

To start the simple co-simulation, follow these simple steps:
- Launch `DataBroker` in a shell by executing `./DB` in `DataBroker/Linux` directory.
- In another shell, start Python client [`zmq-client.py`](../Connectors/Simulink/zmq-client.py).
- Open `SampleSimulinkModel.slx` and run it from Simulink.

In the shell where `DataBroker` is running you should see output like this:
<details>

```shell
$ ./DB
Semaphores Initialized
Flag hold_for_dante = false 
Flag co_sim_enable = true 
Flag sync_enable = true 
Flag realtime_timestep not in config!
Endpoint Initialization Complete
Co-Simulation Enabled
Starting Shm_Interface
waiting for DA *********************
Done waiting for DA
Wait for Semaphore
Entering loop
Semaphores created by Data_Aggregator
DA WAITING ON SHMExecutable Name = Simulink 
External simulator selected. 
****You may now start the simulator****
***Enter X to stop simulation***

Flag outputs = Output_Value_1,Output_Value_2 
Semaphore captured
Semaphore captured
Update Points: 2
Publish Points: 2
Timestep Size 0.200000
DA Semaphore captured
Init data written to shared memory
Received from Shm_Interface: PUB = 2, UP = 2, TimeStep = 0.200000
Semaphore captured
Output_Value_1  0.000000 0.000000 sec 
Output_Value_2  0.000000 0.000000 sec 
Input_Value_1 DOUBLE -100000000000000.000000 0.000000 sec 
Input_Value_2 DOUBLE -100000000000000.000000 0.000000 sec 
Output_Value_1  1.000000 0.000000 sec 
Output_Value_2  2.000000 0.000000 sec 
Input_Value_1 DOUBLE 8.000000 0.000000 sec 
Input_Value_2 DOUBLE 20.000000 0.000000 sec 

***Press X then Enter to stop simulation***
Output_Value_1  1.000000 0.200000 sec 
Output_Value_2  2.000000 0.200000 sec 
Input_Value_1 DOUBLE 1.000000 0.000000 sec 
Input_Value_2 DOUBLE 16.000000 0.000000 sec 

***Press X then Enter to stop simulation***
Output_Value_1  1.000000 0.400000 sec 
Output_Value_2  2.000000 0.400000 sec 
Input_Value_1 DOUBLE 10.000000 0.000000 sec 
Input_Value_2 DOUBLE 14.000000 0.000000 sec 

***Press X then Enter to stop simulation***
Output_Value_1  160.000000 0.600000 sec 
Output_Value_2  28.000000 0.600000 sec 
Input_Value_1 DOUBLE 6.000000 0.000000 sec 
Input_Value_2 DOUBLE 11.000000 0.000000 sec 

***Press X then Enter to stop simulation***
Output_Value_1  16.000000 0.800000 sec 
Output_Value_2  17.000000 0.800000 sec 
Input_Value_1 DOUBLE 1.000000 0.000000 sec 
Input_Value_2 DOUBLE 13.000000 0.000000 sec 

 ...
 ```
 </details>

<br>

In the console where Python client is run, the output should look like this:
<details>

```shell
$ python3 ../../../broker.py 
ZMQ test server is running on tcp://*:5556...
[Simulink → External Sim] Received: {"Output_Value_1":1,"Output_Value_2":2}
[External Sim → Simulink] Sent: {'Input_Value_1': 8.0, 'Input_Value_2': 20.0}

[Simulink → External Sim] Received: {"Output_Value_1":1,"Output_Value_2":2}
[External Sim → Simulink] Sent: {'Input_Value_1': 1.0, 'Input_Value_2': 16.0}

[Simulink → External Sim] Received: {"Output_Value_1":1,"Output_Value_2":2}
[External Sim → Simulink] Sent: {'Input_Value_1': 10.0, 'Input_Value_2': 14.0}

[Simulink → External Sim] Received: {"Output_Value_1":160,"Output_Value_2":28}
[External Sim → Simulink] Sent: {'Input_Value_1': 6.0, 'Input_Value_2': 11.0}

[Simulink → External Sim] Received: {"Output_Value_1":16,"Output_Value_2":17}
[External Sim → Simulink] Sent: {'Input_Value_1': 1.0, 'Input_Value_2': 13.0}

...
```
</details>

<br>

Note that the Python client is specific to the Simulink example. It simply
receives two numbers from the example and sends two random numbers back.