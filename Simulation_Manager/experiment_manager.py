# Copyright 2024 National Technology & Engineering Solutions of Sandia, LLC (NTESS). 
# Under the terms of Contract DE-NA0003525 with NTESS, the U.S. Government retains 
# certain rights in this software.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import os
import threading
import time
import socket
from simulation import simulation
from Errors import *

injectedAttacks = []

class experiment_manager:
    queued_simulations = []
    completed_simulations = 0
    active_simulations = {}
    minimega_connections = 0
    
    def __init__(self, max_active_simulations):
        self.max_active_simulations = int(max_active_simulations)
        code = os.system("/opt/minimega/bin/minimega -e namespace")
        if code != 0:
            raise MinimegaNotActive

    def create_simultation(self, config_file, uid):
        try:
            sim = simulation(config_file, uid)
        except ConfigError:
            return
        self.queued_simulations.append(sim)
        print(f'Queued {sim.uid}')

    def print_config(self):
        print(self.config)

    def start_simulation(self, sim):
        self.minimega_connections += 1
        self.active_simulations[str(sim.uid)] = sim
        print(self.active_simulations)
        os.system(f"/opt/minimega/bin/minimega -e namespace {sim.uid} read /tmp/{str(sim.uid)}.mm")
        t1 = threading.Thread(target=self.wait_for_end, args=(sim,),daemon=True)
        sim.thread = t1
        t1.start()
        print(f'Started {sim.uid}')
        self.minimega_connections -= 1
        
    def inject_attack(self, temp_filepath, sim):
        global injectedAttacks
        files = sim.attack['input_files']
        #Open the minimega file for editing
        with open(temp_filepath, "a") as file:
            file.write(f"clear cc filter")
            file.write(f"\ncc filter name={sim.attack['attack_simulator_name']}")
            #filepath will just be the name of the variable in the config.yaml. We need the value stored in the variable
            for filepath in sim.files:
                to_copy = files[filepath]
                #Minimega can only copy files that are in the /tmp/minimega/files/ directory
                os.system(f"cp {to_copy} /tmp/minimega/files")
                #filename needs to be just the name of the file, for example /tmp/minimega/files/hello.txt will be hello.txt
                filename = to_copy.split("/")[-1]
                file.write(f"\ncc send /tmp/minimega/files/{filename}")
                #If the execute value is set to true and the extension is .sh then run the file
                if sim.attack["execute"] == True and len(filename.split(".")) > 1 and filename.split(".")[1] == "sh":
                    print(f"Executing: {filename}")
                    file.write(f"\ncc background su - root -c 'bash /tmp/miniccc/files/{filename}' &\n")
                    #injectedAttacks.append(f"/opt/minimega/bin/minimega -e namespace {sim.uid} cc exec su - root -c 'bash /tmp/miniccc/files/{filename}' &")            

    #Creates a unique tap name based on the uid of the namespace to prevent overlap in minimega
    def replace_tapid(self, fin, sim):
        with open(fin, "r") as file:
            lines = file.readlines()
        for i in range(len(lines)):
            if "tap create" in lines[i]:
                lines[i] = f"{lines[i].strip()}-{str(sim.uid).split('-')[1]} \n"
        with open(fin, "w") as file:
            file.writelines(lines)
        
    #Prep the minimega file for being run
    def parse(self):
        while True:
            for sim in self.queued_simulations:
                if sim.parsed == False:
                    temp_path = f"/tmp/{str(sim.uid)}.mm"
                    os.system(f"cp {sim.minimega_filepath} {temp_path}")
                    self.replace_tapid(temp_path, sim)
                    self.inject_attack(temp_path, sim)
                    sim.parsed = True
                    print(f'Parsed {sim.uid}')

    #Method is designed to be run in a thread, will wait for the simulation to reach the maximum runtime, retrive experiment results, and then kill it
    def wait_for_end(self, sim):
        global injectedAttacks
        max_simulation_runtime = sim.max_simulation_runtime
        start_time = time.perf_counter()

        while (time.perf_counter() - start_time) < max_simulation_runtime:
            continue

        #Data gathering functions
        os.system(f"/opt/minimega/bin/minimega -e namespace {sim.uid} cc exec python3 /tmp/miniccc/files/gatherResults.py")
        time.sleep(1)
        os.system(f"/opt/minimega/bin/minimega -e namespace {sim.uid} cc recv /tmp/miniccc/files/result.txt")
        result = os.popen(f"cat /tmp/minimega/files/{sim.uid}/miniccc_responses/36/*/tmp/miniccc/files/result.txt").read()

        print("result: " + result)
        os.system(f"/opt/minimega/bin/minimega -e namespace {sim.uid} cc delete command all")
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect(("127.0.0.1", 54321))
            s.send(result.encode('utf-8'))

        self.kill_simulation(sim)
        self.completed_simulations += 1

    #Remove the simulation from the dictionary, pull the output file set in the yaml and then clear the simulation out of minimega
    def kill_simulation(self, sim):
        try:
            self.active_simulations.pop(str(sim.uid))
            os.system(f"/opt/minimega/bin/minimega -e namespace {sim.uid} cc recv name={sim.attack['attack_simulator_name']} {sim.attack['output_path']}")
            os.system(f"/opt/minimega/bin/minimega -e clear namespace {sim.uid}")
            print(f"Killed {sim.uid}")
        except KeyError as e:
            print(e)
            print(str(sim.uid) + " not active")
            print(self.active_simulations)

    #This will handle any messages coming from the simulation. Currently the only implemented message is to kill the simulation,
            #allowing it to end when the simulation deems it is done.
    def msg_from_sim(self, msg):
        if 'kill' in msg[0]:
            uid = msg[1].strip('\n')
            try:
                self.kill_simulation(self.active_simulations[uid])
            except KeyError:
                print(f'{uid} not in the dictionary')
                print(self.active_simulations)

    def run(self):
        parse_thread = threading.Thread(target=self.parse, args=(), daemon=True)
        parse_thread.start()
        while True:
            if len(self.active_simulations) < self.max_active_simulations and len(self.queued_simulations) > 0 and self.queued_simulations[0].parsed == True and self.minimega_connections <= 1:
                sim = self.queued_simulations.pop(0)
                self.start_simulation(sim)
