# DataBroker Quick Start

This guide describes how to build DataBroker and run `SampleSimulinkModel.slx`
from `Connectors/Simulink` on Linux and macOS systems.

## Requirements

- C compiler with C11 support
- Ninja
- ZeroMQ library (>= 4.0)
- Python 3 with PyZMQ
- MATLAB with Simulink and a configured MEX compiler

## Building DataBroker

From the repository root, build the broker:

```shell
ninja -C DataBroker/Unix
```

Until build configuration is implemented, you will need to manually remove
the `-lrt` linker flag to build on macOS. Once built, DataBroker will be
available at `DataBroker/Unix/DataBroker`

If you use Nix, a development shell can be entered using `nix develop`.

## Compiling the Connector

In MATLAB, enter `Connectors/Simulink`. On Linux, run:

```matlab
mex sfun_connector.c -lpthread -lrt
```

On macOS, omit `-lrt`.

## Configuration

The broker reads `input.json` from its working directory. The example
configuration in `Connectors/Simulink` contains:

```json
{
    "Simulator": [
        {
            "executableName": "Simulink",
            "hold": "false",
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

## Running the Example

1. In a terminal at the repository root, start the Python client:

   ```shell
   python3 Connectors/Simulink/zmq-client.py
   ```

2. In another terminal, start the broker from the example directory:

   ```shell
   cd Connectors/Simulink
   ../../DataBroker/Unix/DataBroker
   ```

3. When the broker reports that you may start the simulator, open and run
   `SampleSimulinkModel.slx` in MATLAB.

The Python client will report JSON objects indicating the model outputs,
and will send to the Simulink model random input values.

## Stopping the Example

To terminate the broker, enter `x` or `X` then press Enter. Alternatively,
you can send `SIGINT`/`SIGTERM`. The Python client can be stopped with Ctrl+C.
