# Simulation Manager

*** UNDER CONSTRUCTION ***

## Components

### main.py
The entry point for starting the server and the experiment manager.

### arcade_server.py
Handles client connections and manages incoming messages.

### experiment_manager.py
Manages the lifecycle of simulations, including creation, execution, and termination.

## main.py
This script initializes and starts the server and the experiment manager. It takes two command-line arguments: the port number for the server to listen on and the maximum number of active simulations allowed.

### Usage
```bash
python main.py <PORT> <MAX_SIMULATIONS>
```
- `<PORT>`: The port number for the server to listen on.
- `<MAX_SIMULATIONS>`: The maximum number of simulations that can run concurrently.

By sending a message formatted as follows to the server, a simulation instance can be cleanly shutdown. This will still collect files that were listed to be collected.
```
kill
<UID>
```
Example config file to send to simulation manager to spin up a new instance.

```yaml
new_sim: True
exp_name: exp_1
minimega_filepath: /opt/minimega/script.mm
max_simulation_runtime: 400
attack:
  attack_simulator_name: UCA_Activator
  input_files:
    file_1: /path/to/files/file1.zip
    file_2: /path/to/files/curl
    file_4: /path/to/files/UCA_Activator.sh
  execute: True
  output_path: /tmp/whoami.txt
```
new_sim: Is this creating a new experiment instance or a message to a simulation?
exp_name: Name for the experiment
minimega_filepath: The path to the minimega script file
max_simulation_runtime: How long to run the simulation before automatically terminating it, in seconds.
attack: Instructions detailing files to inject and run
    attack_simulator_name: Name of the VM that is acting as the attacker machine
    input_files: Files to inject
        file_X: List of files to inject, these will be copied to a temporary location on the host, then injected into the miniccc location on the target machine.
    execute: Run .sh files or not
    output_path: File to extract from the attacker machine at the end of the simluation
### Functions
- **start_server(exp_man, port)**: Initializes and starts the server in a separate thread.
- **start_exp_man(max_active_simulations)**: Initializes and starts the experiment manager in a separate thread.

## arcade_server.py
This module defines the `arcade_server` class, which handles client connections and processes incoming messages. It listens for messages from clients and simulations and can start new simulations or handle messages from existing ones.

### Key Methods
- **__init__(self, em, port=12345)**: Initializes the server with the given experiment manager and port.
- **handle_client(self, conn, addr)**: Handles communication with a connected client.
- **start(self)**: Starts the server and listens for incoming connections.

## experiment_manager.py
This module defines the `experiment_manager` class, which manages the lifecycle of simulations. It handles the creation, execution, and termination of simulations and ensures that the number of active simulations does not exceed the specified limit.

### Key Methods
- **__init__(self, max_active_simulations)**: Initializes the experiment manager with the maximum number of active simulations.
- **create_simulation(self, config_file, uid)**: Creates a new simulation based on the provided configuration file.
- **start_simulation(self, sim)**: Starts a simulation and monitors its execution.
- **inject_attack(self, temp_filepath, sim)**: Injects attack parameters into the simulation configuration.
- **replace_tapid(self, fin, sim)**: Ensures unique tap names for simulations to prevent overlap.
- **parse(self)**: Prepares simulations for execution.
- **wait_for_end(self, sim)**: Waits for a simulation to reach its maximum runtime and then terminates it.
- **kill_simulation(self, sim)**: Terminates a simulation and cleans up resources.
- **msg_from_sim(self, msg)**: Handles messages from simulations.
- **run(self)**: Main loop for managing simulations.

## Getting Started

### Install Dependencies
Ensure that minimega is installed and accessible at `/opt/minimega/bin/minimega`.

### Run the Server
Start the server and experiment manager by running `main.py` with the appropriate arguments.

``` bash
python main.py <PORT> <MAX_SIMULATIONS>
```

### Connect Clients
Clients can connect to the server on the specified port to start new simulations or send messages to existing ones.
