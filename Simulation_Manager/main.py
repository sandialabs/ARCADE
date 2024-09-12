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

from experiment_manager import experiment_manager
from arcade_server import arcade_server
import sys
import threading

#Server that listens for message from client and sims
def start_server(exp_man, port):
    server1 = arcade_server(exp_man, int(port))
    server_thread = threading.Thread(target=server1.start, args=())
    server_thread.start()
    return server_thread

#Experiment manager that maintains the simulations
def start_exp_man(max_active_simulations):
    exp_man = experiment_manager(max_active_simulations)
    exp_man_thread = threading.Thread(target=exp_man.run, args=(),daemon=True)
    exp_man_thread.start()
    return exp_man

if len(sys.argv) < 3:
    print ("Usage main.py <PORT> <MAX SIMULATIONS>")
    exit(1)

port = sys.argv[1]
max_active_simulations = sys.argv[2]

print("[STARTING]")
exp_man = start_exp_man(max_active_simulations)
thread = start_server(exp_man, port)
