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
import platform
import threading
import time
import socket
import sys
from Simulation_Manager import simulation
from Simulation_Manager import Errors

injectedAttacks = []

class experiment_manager:
    queued_simulations = []
    completed_simulations = 0
    active_simulations = {}
    minimega_connections = 0

    def __init__(self, max_active_simulations, wait_time = 1):
        # minimega executable
        self.mm = '/opt/minimega/bin/minimega'
        # wait time in minutes
        self.wait_time = max(1, wait_time)

        if 'Windows' == platform.system():
          # flownex
          self.os = 'windows'
          self.shell = 'powershell'
          self.drive = 'c:'
        elif 'Linux' == platform.system():
          # asherah
          self.os = 'linux'
          self.shell = 'bash'
          self.drive = ''

        self.has_terminal = sys.stdin.isatty() or sys.stdout.isatty() or sys.stderr.isatty()

        self.max_active_simulations = int(max_active_simulations)
        os.system(f'{self.mm} -e namespace\n')

    # create_simulation is being called from multiple threads from arcade_server
    # adding lock to ensure we don't lose simulations
    def create_simultation(self, config_file, uid):
        try:
            sim = simulation.simulation(config_file, uid)
        except ConfigError:
            print(f'create_simulation error with {uid} {config_file}')
            return
        self.queued_simulations.append(sim)
        position = len(self.queued_simulations)
        print(f'[+] Queued {sim.uid} (Total in queue: {position})')

    def print_config(self):
        print(self.config)

    def start_simulation(self, sim):
        self.minimega_connections += 1
        self.active_simulations[str(sim.uid)] = sim
        #print(self.active_simulations)
        os.system(f'{self.mm} -e namespace {sim.uid} read /tmp/{str(sim.uid)}.mm\n')
        t1 = threading.Thread(target=self.wait_for_end, args=(sim,),daemon=True)
        sim.thread = t1
        t1.start()
        print(f'[+] Started {sim.uid}')
        self.minimega_connections -= 1
        
    def inject_attack(self, temp_filepath, sim):
        with open(temp_filepath, "a") as file:
            file.write("clear cc filter\n")
            file.write(f"cc filter os={self.os}\n")
            file.write(f"cc exec {self.shell} -c 'echo {sim.uid} > {self.drive}/tmp/uid.txt'\n")
            file.write(f"cc filter name={sim.attack_simulator_name}\n")

            for file_info in sim.input_files:
                full_path = file_info["path"]
                filename = os.path.basename(full_path)
                vm = file_info["vm"]
                #print(vm)

                # Copy file to minimega shared directory
                os.system(f"cp {full_path} /tmp/minimega/files")
                file.write(f"cc filter name={vm}\ncc send /tmp/minimega/files/{filename}\n")

                if file_info["execute"]:
                    # Use `cc exec` for foreground execution with output
                    if filename.endswith(".sh"):
                        file.write(f"cc filter name={vm}\ncc exec {self.shell} -c /tmp/miniccc/files/{filename}\n")
                    elif filename.endswith(".ps1"):
                        file.write(f"cc filter name={vm}\ncc exec powershell -ExecutionPolicy Bypass -File /tmp/miniccc/files/{filename}\n")
                    else:
                        file.write(f"cc filter name={vm}\ncc exec /tmp/miniccc/files/{filename}\n")
          

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
                    print(f'[+] Parsed {sim.uid}')

    #Method is designed to be run in a thread, will wait for the simulation to reach the maximum runtime, retrive experiment results, and then kill it
    def wait_for_end(self, sim):
        global injectedAttacks
        # use wallclock_timeout since we don't have access to simulation time
        # max_simulation_runtime = sim.max_simulation_runtime
        wallclock_timeout = sim.wallclock_timeout
        start_time = time.perf_counter()

        while (time.perf_counter() - start_time) < wallclock_timeout:
            continue

        # Run staging script
        if sim.staging_script:
            print(f"[+] Running staging script: {sim.staging_script}")
            os.system(f'{self.mm} -e namespace {sim.uid} read {sim.staging_script}\n')

        # default wait time is 1 minute (60 seconds)
        print(f"[+] Waiting {self.wait_time} minute(s) before running collection")
        time.sleep(60 * self.wait_time)

        # Run collection script
        if sim.collection_script:
            print(f"[+] Running collection script: {sim.collection_script}")
            os.system(f'{self.mm} -e namespace {sim.uid} read {sim.collection_script}\n')

        # only run tree if a terminal is connected, if nohup, don't run tree
        if self.has_terminal:
          result = os.popen(f"tree /tmp/minimega/files/{sim.uid}/miniccc_responses/").read()
          # print("[+] result: " + result)

        self.kill_simulation(sim)
        self.completed_simulations += 1

    #Remove the simulation from the dictionary, pull the output file set in the yaml and then clear the simulation out of minimega
    def kill_simulation(self, sim):
        try:
            self.active_simulations.pop(str(sim.uid))
            #os.system(f"/opt/minimega/bin/minimega -e namespace {sim.uid} cc recv name={sim.attack['attack_simulator_name']} {sim.attack['output_path']}")
            os.system(f'{self.mm} -e clear namespace {sim.uid}\n')
            print(f"[+] Killed {sim.uid}")
        except KeyError as e:
            print(f'[-] {e}')
            print(f'[-] {str(sim.uid)} not active')
            print(self.active_simulations)

    #This will handle any messages coming from the simulation. Currently the only implemented message is to kill the simulation,
            #allowing it to end when the simulation deems it is done.
    def msg_from_sim(self, msg):
        if 'kill' in msg[0]:
            uid = msg[1].strip('\n')
            try:
                self.kill_simulation(self.active_simulations[uid])
            except KeyError:
                print(f'[-] {uid} not in the dictionary')
                print(self.active_simulations)

    def run(self):
        parse_thread = threading.Thread(target=self.parse, args=(), daemon=True)
        parse_thread.start()
        while True:
            if len(self.active_simulations) < self.max_active_simulations and len(self.queued_simulations) > 0 and self.queued_simulations[0].parsed == True and self.minimega_connections <= 1:
                sim = self.queued_simulations.pop(0)
                self.start_simulation(sim)
