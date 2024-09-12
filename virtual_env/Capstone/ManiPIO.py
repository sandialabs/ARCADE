# Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC (NTESS). 
# Under the terms of Contract DE-NA0003525 with NTESS, the U.S. Government retains 
# certain rights in this software.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
# 
# ManiPIO - Manipulate Process IO V1.0
#
# This program reads an input script to construct complex Events on ICS networks.
# The classes for Events are made to handle ICS with the support of ICS classes structured in the way that the modbus
# class is written. The critical functions the Event and trigger classes need are: .connect(), .close(), .write(), and .read()
# the MB_PLC class formalizes how these functions need to work. This should allow flexibilty to add new ICS protocols, however the script 
# constructor that reads input files is a little more rigid and only uses the modbus PLC class as of now. Minor modifications to the script reader
# would allow more ICS classes by adding a PLC 'type' option.
# 
# ManiPIO is dependent on pymodbus.
# pip install  -U pymodbus
# 
from pymodbus.client import ModbusTcpClient as ModbusClient
from pymodbus.payload import BinaryPayloadBuilder, BinaryPayloadDecoder
from pymodbus.constants import Endian
import sys
import signal
import time
import threading
import argparse

#Define class for modbus PLCs
class MB_PLC:

    def __init__(self, IP, Port):
        self.ip = IP
        self.Mem_default = '32_float'
        self.port = Port
        self.client = ModbusClient(IP, port=self.port)
        self.byteOrder = Endian.BIG
        self.wordOrder = Endian.BIG
        self.mlock = threading.Lock()

    #Define how to connect with PLC
    def connect(self):
        client = self.client
        if not client.connect():
            print("Failed to connect to %s\n" % self.ip)

    #Define how to read values from PLCs
    def read(self, mem_addr, formating=None):
        #define decode options
        def float_64(decode):
            return decode.decode_64bit_float()
        def float_32(decode):
            return decode.decode_32bit_float()
        def float_16(decode):
            return decode.decode_16bit_float()
        def int_64(decode):
            return decode.decode_64bit_int()
        def int_32(decode):
            return decode.decode_32bit_int()
        def int_16(decode):
            return decode.decode_16bit_int()
        def uint_64(decode):
            return decode.decode_64bit_uint()
        def uint_32(decode):
            return decode.decode_32bit_uint()
        def uint_16(decode):
            return decode.decode_16bit_uint()

        #Check formatting and split off bit count
        if formating is None:
            formating = self.Mem_default
        Format = formating.split('_')

        if int(Format[0]) >= 16:  #determine number of registers to read
            count = int(int(Format[0])/16)
        else:
            count = 1

        client = self.client #define client

        #Need to used mutex's to lock read/writes
        #This is because conflicts were found to happen with multiple Events using same PLC
        self.mlock.acquire()
        try:
            results = client.read_holding_registers(mem_addr,count,unit=1) #read client PLC
        except:
            results = None
        self.mlock.release()

        #Set up decoder
        decoder = BinaryPayloadDecoder.fromRegisters(results.registers, byteorder=self.byteOrder, wordorder=self.wordOrder)

        #decoder dictionary
        Decode_dict = { '16_float':float_16, '32_float':float_32, '64_float':float_64, '16_int':int_16, '32_int':int_32, '64_int':int_64, '16_uint':uint_16, '32_uint':uint_32, '64_uint':uint_64 }

        return Decode_dict[formating](decoder)
        #return decoded value

    #define how to read coils from PLC
    def readcoil(self, mem_addr):
        client = self.client
        self.mlock.acquire()
        result = client.read_coils(mem_addr,1)
        self.mlock.release()
        return result.bits[0]

    #Define how to write to coils
    def writecoil(self, mem_addr, value):
        client = self.client
        self.mlock.acquire()
        client.write_coil(mem_addr, value)
        self.mlock.release()

    #define how to write to registers
    def write(self, mem_addr, value, formating=None):
        #define encode options
        def float_64(build, value):
            build.add_64bit_float(value)
        def float_32(build, value):
            build.add_32bit_float(value)
        def float_16(build, value):
            build.add_16bit_float(value)
        def int_16(build, value):
            build.add_16bit_int(value)
        def int_32(build, value):
            build.add_32bit_int(value)
        def int_64(build, value):
            build.add_64bit_int(value)
        def uint_16(build, value):
            build.add_16bit_uint(value)
        def uint_32(build, value):
            build.add_32bit_uint(value)
        def uint_64(build, value):
            build.add_64bit_uint(value)

        #Catch default format conditions and split bits value to determine register write count
        if formating is None:
            formating = self.Mem_default
        Format = formating.split('_')

        #Catch incorrect formating of ints
        if Format[1] == 'int' or Format[1] == 'uint':
            if type(value) is not int:
                value = int(value)


        if int(Format[0]) >= 16:  #determine number of registers to write
            count = int(Format[0])/16
        else:
            count = 1

        client = self.client #define client

        #start builder for writng to registers
        builder = BinaryPayloadBuilder(byteorder=self.byteOrder, wordorder=self.wordOrder)

        #encoder dictionary
        Encode_dict = { '16_float':float_16, '32_float':float_32, '64_float':float_64, '16_int':int_16, '32_int':int_32, '64_int':int_64, '16_uint':uint_16, '32_uint':uint_32, '64_uint':uint_64 }

        #Encode value with builder
        Encode_dict[formating](builder, value)

        payload = builder.to_registers()
        
        #Lock out read/write operations to stop conflicts
        self.mlock.acquire()
        try:
            Check_write = client.write_registers(mem_addr, payload)
        except:
            print("Write Check has failed!!\n")
            ### Add error checking ###
            pass

        while Check_write.isError():
            try:
                Check_write = client.write_registers(mem_addr, payload)
            except:
                pass
            
        self.mlock.release()

    #define how to close connection to PLC
    def close(self):
        client = self.client
        client.close()

    def __repr__(self):
        return "MB_PLC('{}','{}')".format(self.ip,self.port)

#Begin Event class
class Event:
    def __init__(self, PLC):
        self.plc = PLC
        self.mem_addr = []
        self.mem_format = []
        self.Event = 'single'
        self.timing = 0
        self.timing_units = 'sec'
        self.time_delay = 0
        self.values = []
        self.persist = False
        self.thread = None
        self.thread_stop = False
    
    def add_mem_addr(self, addr=None, types=None): #add memory addresses to roster with memory format
        #Figure out and set memory formats
        if types is None and type(addr) is list:
            types = [self.plc.Mem_default for i in range(len(addr))]
        if types is None and type(addr) is not list:
            types = self.plc.Mem_default
        if addr is None:
            addr = []

        #Write new memory and formats to self if in list format
        if type(addr) is list:
            for i in range(len(addr)):
                self.mem_addr.append(addr[i])
                self.mem_format.append(types[i])
        
        #write new memory if single value
        if type(addr) is int or type(addr) is str:
            self.mem_addr.append(addr)
            self.mem_format.append(types)

    #Define method to change all options in Event
    def set_Event(self, **kwargs):
        #uses dictionary to parse kargs
        options = {
            'mem_addr':self.mem_addr,
            'mem_format':self.mem_format,
            'Event':self.Event,
            'timing':self.timing,
            'timing_units':self.timing_units,
            'time_delay':self.time_delay,
            'values':self.values,
            'persist':self.persist }
        options.update(kwargs)

        #setting memory format if none exist
        if len(options['mem_format'])==0:
            self.mem_format = [self.plc.Mem_default for i in range(len(options['mem_addr']))]

        elif len(options['mem_format']) != len(options['mem_addr']): #setting memory format if mismatch of input lengths
            self.mem_format.append([self.mem_format[-1] for i in range(len(options['mem_addr'])-len(options['mem_format']))])
            print('Wrong number of format elements, rest set to last format')

        else:
            self.mem_format = options['mem_format']

        #set options
        self.mem_addr = options['mem_addr']
        self.Event = options['Event']
        self.timing = options['timing']
        self.timing_units = options['timing_units']
        self.time_delay = options['time_delay']
        self.values = options['values']
        self.persist = options['persist']
        
    #define 'single' type Event
    def single(self):
        #Error checking
        Error_Check = True
        if type(self.mem_addr) is list and len(self.mem_addr) == 0:
            print('No memory address!')
            Error_Check = False
        if self.mem_addr == None:
            print('No memory address!')
            Error_Check = False
        if type(self.time_delay) is list:
            self.time_delay = self.time_delay[0]
        if type(self.timing) is list:
            timing = self.timing[0]
        else:
            timing = self.timing


        #time delay
        if self.time_delay != 0:
            time.sleep(self.time_delay)

        #connect to PLC
        PLC = self.plc 
        PLC.connect() 
        
        #Setup timing mechanism
        time_end = time.time()
        #check errors and start loop
        if Error_Check:
            while True:
                #if timing is set, start interval mechanism
                if self.timing != 0:
                    time1 = 0
                    while time1 < timing and self.thread_stop == False:
                        time1 = time.time() - time_end
                    
                #loop to write values out to all memory addresses specified
                for i in range(len(self.mem_addr)):
                    if len(self.values) == 0:
                        value = 0
                    elif len(self.values)-1 < i:
                        value = self.values[-1]
                    else:
                        value = self.values[i]
                    
                    PLC.write(self.mem_addr[i],value, self.mem_format[i])

                #if this is not a persistant Event, break out of loop
                if self.persist == False:
                    break
                time_end = time.time()
        #close PLC connection
        PLC.close()

    #setup 'ramp' type Event
    def ramp(self):

        Error_Check = True
        #check for invalid config
        if type(self.mem_addr) is list and len(self.mem_addr) == 0:
            print('No memory address!')
            Error_Check = False
        if self.mem_addr == None:
            print('No memory address!')
            Error_Check = False
        if type(self.timing) is not list and self.timing == 0:
            print('Timing for ramp cannot be 0')
            Error_Check = False
        if type(self.timing) is list:
            for i in range(len(self.timing)):
                if self.timing[i] == 0:
                    print('Timing for ramp cannot be 0')
                    Error_Check = False
        if type(self.time_delay) is list:
            self.time_delay = self.time_delay[0]
        if type(self.values) is not list or len(self.values) < 2:
            print('Not enough values to preform ramp!')
            Error_Check = False
        
        #time delay
        if int(self.time_delay) != 0:
            time.sleep(self.time_delay)
        
        PLC = self.plc
        PLC.connect()

        if Error_Check: #Check for errors
            #begin persistance loop
            while True:
                #begin assembling ramp info
                for i in range(len(self.values)-1):
                    #checking for timing variable and conditioning for all possible variances
                    if type(self.timing) is not list:
                        dt = float(self.values[i+1] - self.values[i])/float(self.timing)
                    elif len(self.timing) == 1:
                        dt = float(self.values[i+1] - self.values[i])/float(self.timing[0])
                    else:
                        if (i+1) > len(self.timing):
                            dt = float(self.values[i+1] - self.values[i])/float(self.timing[-1])
                        else:
                            dt = float(self.values[i+1] - self.values[i])/float(self.timing[i])

                    dV = float(self.values[i+1] - self.values[i])
                    value = self.values[i]
                    start_time = time.time()
                    #begin ramp function
                    while abs(dV) > abs((value - self.values[i])) and self.thread_stop == False:                        
                        value = self.values[i] + dt*(time.time() - start_time)
                        if abs(dV) < abs((value - self.values[i])):
                            value = self.values[i+1]
                        for n in range(len(self.mem_addr)):
                            PLC.write(self.mem_addr[n],value, self.mem_format[n])
                if self.persist == False:
                    break
        
        
        PLC.close()

    #Define how to run Event in seperate thread
    def run(self):
        Event_lib = {
            'single':self.single,
            'ramp':self.ramp
        }

        thread = threading.Thread(target=Event_lib[self.Event])
        thread.daemon = True
        thread.start()
        self.thread = thread
    #Define how to stop Event thread
    def stop(self):
        self.persist = False
        self.thread_stop = True
        self.thread.join()
    #wait for Event to finish
    def wait(self):
        self.thread.join()

    def __repr__(self):
        return "Event('{}')".format(self.plc)

#define trigger class that will launch Event when conditions are met        
class Trigger:
    def __init__(self, Event):
        self.plc = []
        self.Event = Event
        self.trigger_mem = []
        self.mem_alloc = []
        self.trigger_format = []
        self.trigger_value = []
        self.trigger_conditions = []
        self.thread_stop = False
        self.threaded = None
    
    #define method to set all trigger options
    def set_trigger(self, **kwargs):
        options = {
            'plc':self.plc,
            'Event':self.Event,
            'mem_alloc':self.mem_alloc,
            'trigger_mem':self.trigger_mem,
            'trigger_format':self.trigger_format,
            'trigger_value':self.trigger_value,
            'trigger_conditions':self.trigger_conditions }
        options.update(kwargs)

        #memory format checking
        if len(options['trigger_format'])==0:
            if type(options['plc']) is list:
                plc = options['plc'][-1]
            else:
                plc = options['plc']
            self.trigger_mem = [plc.Mem_default for i in range(len(options['trigger_mem']))]

        elif len(options['trigger_format']) != len(options['trigger_mem']):
            self.trigger_mem.append([options['trigger_format'][-1] for i in range(len(options['trigger_mem'])-len(options['trigger_format']))])
            print('Wrong number of format elements, rest set to last format')

        else:
            self.trigger_mem.append(options['trigger_mem'])

        self.plc = options['plc']
        self.Event = options['Event']
        self.mem_alloc = options['mem_alloc']
        self.trigger_mem = options['trigger_mem']
        self.trigger_format = options['trigger_format']
        self.trigger_value = options['trigger_value']
        self.trigger_conditions = options['trigger_conditions']

    #Method to set up new PLCs and conditions on that PLC
    def set_plc(self, PLC, mem_addr, conditions, values):
        #check to make sure there are values for each input
        if PLC is None:
            print('Must include PLC!')
        if mem_addr is None:
            print('Must include memory addresses!')
        if conditions is None:
            print('Must include conditions!')
        if values is None:
            print('Must include values!')
        
        #append list of PLCs
        self.plc.append(PLC)
        
        #append memory ranges
        for i in range(len(mem_addr)):
            self.trigger_mem.append(mem_addr[i]) 

        #memory allocation tells the program which memory values in the list go to which PLC
        if type(self.mem_alloc) is list and len(self.mem_alloc) == 0:
            self.mem_alloc = [len(mem_addr)]
        elif type(self.mem_alloc) is not list and self.mem_alloc is not None and self.mem_alloc != 0:
            self.mem_alloc = [self.mem_alloc, len(mem_addr)]
        else:
            self.mem_alloc.append(len(mem_addr))
        
        #set conditions for starting Event
        if type(conditions) is list and len(conditions) < len(mem_addr):
            conditions.append([conditions[-1] for i in range(len(mem_addr)-len(conditions))])
            for i in range(len(conditions)):
                self.trigger_conditions.append(conditions[i])
        elif type(conditions) is not list and type(mem_addr) is list and len(mem_addr) > 1:
            conditions = [conditions for i in range(len(mem_addr))]
            for i in range(len(conditions)):
                self.trigger_conditions.append(conditions[i])
        else:
            for i in range(len(conditions)):
                self.trigger_conditions.append(conditions[i])
        
        #set values for conditional chceks
        if type(values) is list and len(values) < len(mem_addr):
            values.append([values[-1] for i in range(len(mem_addr)-len(values))])
            for i in range(len(values)):
                self.trigger_value.append(values[i])
            self.trigger_value.append(values)
        elif type(values) is not list and type(mem_addr) is list and len(mem_addr) > 1:
            values = [values for i in range(len(mem_addr))]
            for i in range(len(values)):
                self.trigger_value.append(values[i])
        else:
            for i in range(len(values)):
                self.trigger_value.append(values[i])
    
    #define the thread for the trigger
    def thread(self):
        Error_Check = True
        #error checking

        if len(self.trigger_value) != len(self.trigger_mem) or len(self.trigger_value) != len(self.trigger_conditions):
            Error_Check=False
            print('Incorrect number of arguments.')
        #if sum(self.mem_alloc) != len(self.trigger_mem):
        #    Error_Check=False
        #    print('Memory allocation does not match number of memory addresses.')

        #figure out the number of PLCs
        if type(self.plc) is not list:
            N_PLC = 1
        else:
            N_PLC = len(self.plc)

        #truth table setup and value table setup
        T_Table = [False for i in range(len(self.trigger_mem))]
        VAL = [0 for i in range(len(self.trigger_mem))]
        
        #if no errors start loop
        if Error_Check == True:
            while True:
                #begin loop to check all PLCs
                for i in range(N_PLC):
                    # find the correct way to pull PLCs from the list of PLCs
                    if type(self.plc) is not list:
                        PLC = self.plc
                    elif N_PLC == 1:
                        PLC = self.plc[0]
                    else:
                        PLC = self.plc[i]

                    # find correct way to pull memory addresses
                    N_mem = 0 
                    if i > 0:
                        for n in range(0,i+1):
                            N_mem = N_mem + self.mem_alloc[n]
                    else:
                        N_mem = self.mem_alloc[0]

                    if N_PLC == 1:
                        N_mem_s = 0
                    else:
                        N_mem_s = N_mem - self.mem_alloc[i]
                    
                    #connect to PLC
                    PLC.connect()               
                    m = 0
                    #loop to poll all PLC memory addresses for this PLC
                    for m in range(N_mem_s, N_mem):
                        Read_True = False
                        #set up check to see if we actually read a memory address
                        try:
                            VAL[m] = PLC.read(int(self.trigger_mem[m]))
                            Read_True = True
                        except:
                            print("Read Failed on PLC IP: %s Mem Address %u" % (PLC.ip, self.trigger_mem[m]))
                            Read_fail = False  
                        
                        #if the read fails, dont update the truth table
                        if Read_True:
                            if self.trigger_conditions[m] == '>':
                                T_Table[m] = VAL[m] > self.trigger_value[m]
                            elif self.trigger_conditions[m] == '<':
                                T_Table[m] = VAL[m] < self.trigger_value[m]

                #if truth table is true or we are stopping the thready, break out of loop
                if all(T_Table) == True:
                    break
                if self.thread_stop:
                    break
        #if we are not stoping the thread, start the Event
        if self.thread_stop == False and Error_Check == True:
            self.Event.run()
            self.Event.wait()

    #define how to run trigger thread
    def run(self):
        threaded = threading.Thread(target=self.thread)
        threaded.daemon = True
        threaded.start()
        self.threaded = threaded

    #define how to stop thread
    def stop(self):
        self.thread_stop = True
        self.threaded.join()

    #wait for trigger thread to finish
    def wait(self):
        self.threaded.join()

    def __repr__(self):
        return "Trigger('{}')".format(self.Event)

    #debug settings printer
    def show(self):
        print("PLC: " + str(self.plc))
        print("Trigger Conditions: " + str(self.trigger_conditions))
        print("Trigger Values: " + str(self.trigger_value))
        print("Trigger Memory: " + str(self.trigger_mem))

#define the constructor that will read scripts and build Events
def constructor(FILE_PATH):
    #define dictionaries and number of objects
    PLCS = {}
    NPLC = 0
    Events = {}
    NEvent = 0
    Triggers = {}
    Ntrigger = 0
    
    #define what to do if you find 'PLC' in the script
    def PLC_Text(line, file):
        nonlocal NPLC
        nonlocal PLCS
        NPLC = NPLC +1

        #set up local vars
        IP = ''
        port = 502
        byte_order = Endian.BIG
        word_order = Endian.BIG

        PLC_Lib ={
            'ip':IP,
            'port':port,
            'byteorder':byte_order,
            'wordorder':word_order
        }

        line = file.readline()
        #until we hit a blank line keep reading, might be comments between 'PLC' and 'IP'
        while line:
            if line in ['\n', '\r\n']:
                break
            #split things up, looking for key values
            Key = line.split(':',1)
            Keys = [x.lower() for x in Key]
            if Keys[0] in PLC_Lib: #if we find a key value
                #format values
                values = Keys[1].strip(' []')
                values_splt = values.split(',')
                values = [x.strip() for x in values_splt]

                if Keys[0] == 'ip': #find the IP
                    PLC_Lib['ip'] = values
                if Keys[0] == 'byteorder' or Keys[0] == 'wordorder':
                    if values[0] == 'big':
                        PLC_Lib[Keys[0]] = Endian.BIG
                    elif values[0] == 'little':
                        PLC_Lib[Keys[0]] = Endian.LITTLE
                    else:
                        print('Error: Wordorder and Byteorder must be either Big or Little!\n Defaulted to Big\n')
                if Keys[0] == 'port':
                    PLC_Lib[Keys[0]] = int(values[0])
                    
            line = file.readline()

        #put PLC in PLC dict
        PLCS[NPLC] = MB_PLC(PLC_Lib['ip'][0], PLC_Lib['port'])
        PLCS[NPLC].wordOrder = PLC_Lib['wordorder']
        PLCS[NPLC].byteOrder = PLC_Lib['byteorder']

    #what to do if we find 'Event' in script
    def Event_Text(line,file):
        nonlocal NEvent, Events, PLCS
        #set up variables
        plc = 0
        Val = 0.0
        Mem = []
        Mem_form = ''
        time = 0.0
        time_d = 0.0
        att_type = 'single'
        persistance = False
        
        NEvent = NEvent + 1
        #set up dict of valid operators
        Att_Lib = {
            'plc':plc,
            'values':Val,
            'mem':Mem,
            'format':Mem_form,
            'timing':time,
            'delay':time_d,
            'type':att_type,
            'persist':persistance
            }

        line = file.readline()
        while line: #until a blank line keep reading
            if line in ['\n', '\r\n']:
                break
            #split things up, looking for key values
            Key = line.split(':',1)
            Keys = [x.lower() for x in Key]
            if Keys[0] in Att_Lib: #if we find a key value
                #format values
                values = Keys[1].strip(' []')
                values_splt = values.split(',')
                values = [x.strip() for x in values_splt]

                #depending on the type of the key, format the value correctly
                if type(Att_Lib[Keys[0]]) is str:
                    Att_Lib[Keys[0]] = values
                elif type(Att_Lib[Keys[0]]) is bool:
                    if values[0] == 'true' or values[0] == 't':
                         Att_Lib[Keys[0]] = True
                elif type(Att_Lib[Keys[0]]) is float:
                    Att_Lib[Keys[0]] = [float(x) for x in values]
                else:
                    Att_Lib[Keys[0]] = [int(x) for x in values]
            line = file.readline()

        #setup Event and put it in the Event dict
        #print(PLCS[Att_Lib['plc'][0]])
        Events[NEvent] = Event(PLCS[Att_Lib['plc'][0]])
        #set all Event options
        #print(Att_Lib['format'])
        Events[NEvent].set_Event(values = Att_Lib['values'], mem_addr = Att_Lib['mem'], mem_format = Att_Lib['format'], time_delay = Att_Lib['delay'], timing = Att_Lib['timing'], persist = Att_Lib['persist'], Event = Att_Lib['type'][0] )

    #what to do if 'trigger' is found in script
    def Trigger_Text(line,file):
        nonlocal Ntrigger, Triggers, Events, PLCS
        Ntrigger = Ntrigger + 1

        plc_count = 0
        Event = 0
        plc = 0
        Val = 0.0
        Mem = []
        Mem_form = ''
        conditions = ''

        #set up 2 libraries one for keywords
        #the other is to assemble a library of plcs
        PLC_Lib = {}
        Trig_Lib = {
            'plc':plc,
            'event':Event,
            'values':Val,
            'mem':Mem,
            'format':Mem_form,
            'conditions':conditions,
            }

        line = file.readline()
        while line:
            if line in ['\n', '\r\n']:
                break
            Key = line.split(':',1)
            Keys = [x.lower() for x in Key]
            if Keys[0] in Trig_Lib:
                #if we find a plc, incriment the plc library index
                if Keys[0] == 'plc':
                    plc_count = plc_count + 1
                
                values = Keys[1].strip(' []')
                values_splt = values.split(',')
                values = [x.strip() for x in values_splt]

                #if the keyword is anything but 'Event' put it in the plc dict
                if type(Trig_Lib[Keys[0]]) is str:
                    PLC_Lib[(plc_count,Keys[0])] = values
                elif type(Trig_Lib[Keys[0]]) is float:
                    PLC_Lib[(plc_count,Keys[0])] = [float(x) for x in values]
                elif Keys[0] == 'event':
                    Trig_Lib[Keys[0]] = [int(x) for x in values]
                else:
                    PLC_Lib[(plc_count,Keys[0])] = [int(x) for x in values]
            line = file.readline()

        #set up the trigger
        Triggers[Ntrigger] = Trigger(Events[Trig_Lib['event'][0]])
        #set up each plc in the trigger
        for i in range(plc_count):
            n = i + 1
            Triggers[Ntrigger].set_plc(PLCS[PLC_Lib[(n,'plc')][0]], PLC_Lib[(n,'mem')], PLC_Lib[(n,'conditions')], PLC_Lib[(n,'values')])
            
    #what to do if we find 'start' in the script
    def Start_Text(line,file):
        nonlocal PLCS, Events, Triggers
        nonlocal NPLC, NEvent, Ntrigger
        #make lists of trigger and Event indexes 
        ATT = []
        TRG = []
        
        line = file.readline()
        while line:
            if line in ['\n', '\r\n']:
                break
            low_line = line.lower()
            values_splt = low_line.split(',')
            Keys = [x.strip() for x in values_splt]
            Keys = [x.split(' ') for x in Keys]

            #if we find Event or trigger, put its index number in the lists
            for i in range(len(Keys)):
                if Keys[i][0] == 'event':
                    if len(Keys[i]) > 1 and int(Keys[i][1]) != 0: #make sure their is a value following the keyword
                        ATT.append(int(Keys[i][1]))
                elif Keys[i][0] == 'trigger':
                    if len(Keys[i]) > 1 and int(Keys[i][1]) != 0:
                        TRG.append(int(Keys[i][1]))
            
            line = file.readline()
        
        #now start the Events and triggers and wait for completion
        for i in range(len(ATT)):
            Events[ATT[i]].run()
            
        for i in range(len(TRG)):
            Triggers[TRG[i]].run()

        for i in range(len(ATT)):
            Events[ATT[i]].wait()
            
        for i in range(len(TRG)):
            Triggers[TRG[i]].wait()

    #library of base keywords    
    Base_lib = {
        'plc':PLC_Text,
        'event':Event_Text,
        'trigger':Trigger_Text,
        'start':Start_Text
    }

    #open the file and begin parsing lines
    FILE = open(FILE_PATH, 'r')
    Line = FILE.readline()
    while Line:
        #if line is not empty and does not start with comment
        if Line and Line[0] != '#':
            nline = Line.strip('\n')
            Keys = nline.split(' ', 1)
            Key = [x.lower() for x in Keys]
            if Key[0] in Base_lib: #if we find a keyword execute the function associated with it
                print(Key)
                Base_lib[Key[0]](Line, FILE)
                Line = FILE.readline()
            else:
                Line = FILE.readline()
        else:
            Line = FILE.readline()

#main program
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='ManiPIO - Manipulate Process IO')
    parser.add_argument('file', nargs='+', help='Path to Event script')
    args_namespace = parser.parse_args()
    args = vars(args_namespace)['file']
    #parse arguments and find file location to pluggin to Event constructor
    constructor(args[0])
