import threading
import logging
import queue
import time
import socket
import sys
import os
import zmq
import json
from opcua import ua, Server, Client
from pymodbus.client import ModbusTcpClient as ModbusClient
from pymodbus.payload import BinaryPayloadBuilder, BinaryPayloadDecoder
from pymodbus.constants import Endian


class Data_Repo(object):
    def __init__(self,Tags):
        self.Tag_NoDuplicates = list(set(Tags))
        Val = [0.0] * len(self.Tag_NoDuplicates)
        self.Store = dict( zip(self.Tag_NoDuplicates, Val))

    def write(self, Tag, Value):
        self.Store[Tag] = Value

    def read(self, Tag):
        return self.Store[Tag]

    def UDP_TAGS(self):
        return self.Tag_NoDuplicates

#dictionary search util
def gen_dict_extract(key, var):
        if hasattr(var,'items'): # hasattr(var,'items') for python 3
            for k, v in var.items(): # var.items() for python 3
                if k == key:
                    yield v
                if isinstance(v, dict):
                    for result in gen_dict_extract(key, v):
                        yield result
                elif isinstance(v, list):
                    for d in v:
                        for result in gen_dict_extract(key, d):
                            yield result

class Config(object):
    def __init__(self):
        self.type_idx = {}
        self.config_dict = {}
    
    def import_config(self,config):
        self.config_dict = config
    
    def export_config(self):
        return self.config_dict

    def write(self,config_json):
        if 'Type' in config_json:
            Type_iter = config_json['Type']
        else:
            print("Undefined Type in config!")
            Type_iter = "Unknown"
        
        if Type_iter in self.type_idx:
            self.type_idx[Type_iter] += 1
        else:
            self.type_idx[Type_iter] = 1
            self.config_dict[Type_iter] = {}

        self.config_dict[Type_iter][self.type_idx[Type_iter]-1] = config_json

    def validate_type(self, type_idx, idx, values):
        i=0
        check = []
        for value in values:
            i += 1
            if value in self.config_dict[type_idx][idx]:
                check[i] = True
            else:
                check[i] = False
        valid = all(check)
        return valid
    
    def validate(self, values):
        i=0
        if type(values) == list:
            check = []
            for value in values:
                i += 1
                if value in self.config_dict:
                    check[i] = True
                else:
                    check[i] = False
            valid = all(check)
            return valid  
        else:
            valid = values in self.config_dict
            return valid
    
    def read(self, type_idx, idx, value):
        if type_idx in self.config_dict:
            if idx in self.config_dict[type_idx]:
                if value in self.config_dict[type_idx]:
                    return self.config_dict[type_idx][idx][value]
        else:
            return None
    
    def read_type(self, type_idx, idx):
        if type_idx in self.config_dict:
            if idx in self.config_dict[type_idx]:
                return self.config_dict[type_idx][idx]
        else:
            return None
    
    def print_conf(self):
        print(self.config_dict)

    def collect(self, value):
        return list(gen_dict_extract(value, self.config_dict))
    
    def collect_list(self, value):
        raw_list = list(gen_dict_extract(value, self.config_dict))
        new_list = []
        for x in raw_list:
            if type(x) is str:
                x = x.strip()
                sub_list = x.split(',')
                for y in sub_list:
                    y = y.strip()
                    new_list.append(y)
            else:
                new_list.append(x)
        return new_list
    
    def collect_list_dedup(self, value):
        return list(set(self.collect_list(value)))


#Define class for modbus PLCs
class MB_PLC:

    def __init__(self, config):
        # Set IP and port, crash if no IP
        self.ip = config['IP_PLC']
        if 'Port' in config:
            self.port = int(config['Port'])
        else:
            self.port = 502

        # memory format
        if 'MemFormat' in config:
            self.Mem_default = config['MemFormat']
        else:
            self.Mem_default = '32_float'
        
        #set endianness
        if 'Endianess' in config:
            # parse endianness
            Endian_Key = config['Endianess'].split(",")
            if len(Endian_Key) % 2:
                logging.info("Config Error!\nEndianness setting must be a pair! ByteOrder,WordOrder (Big,Big) ")
            else:
                Byte_order = Endian_Key[::2]
                Word_order = Endian_Key[1::2]
            
            if Byte_order[0].lower() == 'little':
                self.byteOrder = Endian.LITTLE
            else:
                self.byteOrder = Endian.BIG
        
            if Word_order[0].lower() == 'little':
                self.wordOrder = Endian.LITTLE
            else:
                self.wordOrder = Endian.BIG
        else:
            self.byteOrder = Endian.BIG
            self.wordOrder = Endian.BIG
        
        self.client = ModbusClient(self.ip, port=self.port)
        self.mlock = threading.Lock()

    #Define how to connect with PLC
    def connect(self):
        client = self.client
        client.connect()

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

        try:
            results = client.read_holding_registers(int(mem_addr),count,unit=1) #read client PLC
        except:
            results = None

        #decoder dictionary
        Decode_dict = { '16_float':float_16, '32_float':float_32, '64_float':float_64, '16_int':int_16, '32_int':int_32, '64_int':int_64, '16_uint':uint_16, '32_uint':uint_32, '64_uint':uint_64 }

        if results is not None:
            #Set up decoder
            decoder = BinaryPayloadDecoder.fromRegisters(results.registers, byteorder=self.byteOrder, wordorder=self.wordOrder)
            
            return Decode_dict[formating](decoder)
            #return decoded value
        else:
            #return a Nonetype
            return results

    #define how to read coils from PLC
    def readcoil(self, mem_addr):
        client = self.client
        self.mlock.acquire()
        result = client.read_coils(int(mem_addr),1)
        self.mlock.release()
        return result.bits[0]

    #Define how to write to coils
    def writecoil(self, mem_addr, value):
        client = self.client
        self.mlock.acquire()
        client.write_coil(int(mem_addr), value)
        self.mlock.release()

    #define how to write to registers
    def write(self, mem_addr, value, formating=None):
        #define encode options
        def float_64(build, value):
            build.add_64bit_float(float(value))
        def float_32(build, value):
            build.add_32bit_float(float(value))
        def float_16(build, value):
            build.add_16bit_float(float(value))
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
        
        #read/write operations
        
        #error check the write operation
        try:
            Check_write = client.write_registers(int(mem_addr), payload)
        except:
            Check_write = None
            print('First write failed - IP:%s\n' % self.ip)
            pass
        
        if Check_write is not None:
            while Check_write.isError():
                try:
                    Check_write = client.write_registers(int(mem_addr), payload)
                except:
                    pass
        else:
            print("Client Not Connected!")


    #define how to close connection to PLC
    def close(self):
        client = self.client
        client.close()

    def __repr__(self):
        return "MB_PLC('{}')".format(self.ip)

#Class for OPCUA Server Communication
class OPCUA_Server:
    def __init__(self, Conf):
        self.conf = Conf
        self.Lock = Lock
        self.Event = Event
        self.Time_Mem = -1
        self.Scan_Time = 0.1
        self.actuator = False
        self.sensor = False
        self.SensorTags = []
        self.ActuatorTags = []
        self.Actuator_String = ''
        self.Sensor_String = ''
        self.thread = None

#Class for OPCUA Client
class OPCUA_Client:
    def __init__(self, Conf):
        self.ip = Conf['IP_PLC']
        if 'Port' in Conf:
            self.port = int(Conf['Port'])
        else:
            self.port = 502
        self.conf = Conf
        self.Lock = Lock
        self.Event = Event
        self.Time_Mem = -1
        self.Scan_Time = 0.1
        self.actuator = False
        self.sensor = False
        self.SensorTags = []
        self.ActuatorTags = []
        self.Actuator_String = ''
        self.Sensor_String = ''
        self.thread = None

def initialization():
    context = zmq.Context()
    reciever = context.socket(zmq.REP)
    reciever.bind("tcp://*:6666")
    End_Command = False
    retrys_allowed = 3
    retry_attempts = 0
    sys_config = Config()

    while End_Command == False:
        #recieve message from ZMQ
        msgFromServer = reciever.recv()
        msg = msgFromServer.decode()

        if "END_MESSAGE" in msg or msg == '':
            End_Command = True
            reply = bytes("VALID",'utf-8')
            reciever.send(reply)
        else:
            #if end message not recieved, try to read json message
            try:
                msg_json = json.loads(msg)
                sys_config.write(msg_json)
                reply = bytes("VALID",'utf-8')
                reciever.send(reply)
            except:
                #if json cannot be read, try again, message could be corrupt.
                if retry_attempts < retrys_allowed:
                    retry_attempts += 1
                    print("JSON init is corrupt, attempting retry {} of {}}\n".format(retrys_allowed,retry_attempts))
                    reply = bytes("FAILED",'utf-8')
                    reciever.send(reply)
                else:
                    #Skip entry if we exceed the max attempts
                    print("Init JSON transmission failed. Attempting to continue.")
                    reply = bytes("SKIP",'utf-8')
                    reciever.send(reply)
                    retry_attempts = 0
    #close ZMQ connection
    reciever.close()
    
    return sys_config
    
class Connector:
    def __init__(self,config,serAdd,Data,Lock,Event):
        self.config = Config()
        self.config.import_config(config)
        self.PLC = object
        self.serAdd = serAdd
        self.Data = Data
        self.Lock = Lock
        self.Time_Mem = -1
        self.TimeNS = "3"
        self.TimeScaling = 1
        self.Scan_Time = 0.1
        self.actuator = False
        self.sensor = False
        self.SensorTags = []
        self.SensorMem = []
        self.SensorNS = []
        self.SensorOffset = []
        self.SensorScaling = []
        self.ActuatorTags = []
        self.ActuatorMem = []
        self.ActuatorNS = []
        self.ActuatorOffset = []
        self.ActuatorScaling = []
        self.thread = None
        self.Event = Event
        self.setup()

    def setup(self):
        #dict of all available PLC options
        proto_dict = { 
            'modbus':MB_PLC,
            'opc_server':OPCUA_Server,
            'opc_client':OPCUA_Client
        }
        
        #check if PLC option exists, else use modbus as default
        if self.config.validate('Proto'):
            try:
                self.PLC = proto_dict[self.config.config_dict['Proto'].lower()](self.config.export_config())
            except:
                logging.error("Failed: %s PLC failed setup." % self.config.config_dict['Proto'])
        else:
            logging.error('Error: No PLC protocol specified, using Modbus as default.')
            self.PLC = MB_PLC(self.config.export_config())
        
        #setup loop to check all configs
        config_opts = ["Time_Mem","Scan_Time","TimeNS","TimeScaling"]
        config_list_opts = ["SensorTags","SensorMem","SensorNS","SensorOffset","SensorScaling","ActuatorTags","ActuatorMem","ActuatorNS","ActuatorOffset","ActuatorScaling"]
        config_list_defaults = {"SensorNS" : "3","SensorOffset" : 0,"SensorScaling" : 1,"ActuatorNS" : "3","ActuatorOffset" : 0,"ActuatorScaling" : 1}
        for c in config_opts:
            if self.config.validate(c):
                exec( 'self.' + c + '=self.config.collect(c)' )
        
        for c in config_list_opts:
            if self.config.validate(c):
                exec( 'self.' + c + '=self.config.collect_list(c)' )
        
        #check for actuator and sensor options
        if self.ActuatorTags:
            if len(self.ActuatorTags) == len(self.ActuatorMem):
                if len(self.ActuatorTags) > 0:
                    self.actuator = True
            else:
                logging.error('Error: # of Actuator tags and # of Actuator memory addresses is not the same.')
        if self.SensorTags:
            if len(self.SensorTags) == len(self.SensorMem):
                if len(self.SensorTags) > 0:
                    self.sensor = True
            else:
                logging.error('Error: # of Sensor tags and # of Sensor memory addresses is not the same.')

        #check NameSpace and Scaling settings
        for c in config_list_opts:
            if c[:6] == 'Sensor' and self.sensor and c != "SensorTags" and c != "SensorMem":
                if eval("len(self."+c+") != len(self.SensorMem) and len(self."+c+") > 0"):
                    exec("self."+c+".append(self."+c+"(-1) for i in range(len(self.SensorMem) - len(self."+c+")))")
                elif eval("len(self."+c+") <= 0"):
                    exec("self."+c+" = [config_list_defaults["+c+"] for i in range(len(self.SensorMem))]")
            elif c[:8] == 'Actuator' and self.actuator and c != "ActuatorTags" and c != "ActuatorMem":
                if eval("len(self."+c+") != len(self.ActuatorMem) and len(self."+c+") > 0"):
                    exec("self."+c+".append(self."+c+"(-1) for i in range(len(self.ActuatorMem) - len(self."+c+")))")
                elif eval("len(self."+c+") <= 0"):
                    exec("self."+c+" = [config_list_defaults["+c+"] for i in range(len(self.ActuatorMem))]")
            #make sure that all the floats are actually converted to floats
            if c[-6:] == 'Offset' or c[-6:] == 'caling':
                exec("self."+c+" = [float(x) for x in self."+c+"]")

    #This is a helper function for browsing the OPC-UA Node Tree, returns a node of the given browse_name tag
    def checkNodeExists(self, node, name):
        for var in node.get_variables():
            if var.get_browse_name().Name == name:
                return var
        return None
    
    #define the thread that will run PLC comms
    def Agent(self):
        #initalize some values
        if self.sensor:
            Sensor_data = [0.0] * len(self.SensorTags)
        if self.actuator:
            Actuator_data = [0.0] * len(self.ActuatorTags)
        
        #Check whether we're using an OPC or Modbus Connection - can expand later for additional protocol support
        if (isinstance(self.PLC, OPCUA_Server)):
            self.server = Server()
            self.server.set_endpoint("opc.tcp://0.0.0.0:4840/freeopcua/server/")

            # setup our own namespace, not really necessary but should, as it could be changed later
            self.uri = "http://examples.freeopcua.github.io"
            self.idx = self.server.register_namespace(self.uri)

            # Build Object Root Nodes for Sensor and Actuator tags
            dataObj = self.server.nodes.objects.add_object(self.idx, "SenseRoot")
            dataObj2 = self.server.nodes.objects.add_object(self.idx, "ActRoot")
            self.OPC_Sensor_Nodes = []
            self.OPC_Actuator_Nodes = []
        
            # populating our address space for sensors and actuators
            for i in range(len(Sensor_data)):
                self.OPC_Sensor_Nodes.append(dataObj.add_variable(self.idx, str(self.SensorTags[i]), Sensor_data[i]))
                self.OPC_Sensor_Nodes[len(self.OPC_Sensor_Nodes)-1].set_writable()
        
            for i in range(len(Actuator_data)):
                self.OPC_Actuator_Nodes.append(dataObj2.add_variable(self.idx, str(self.ActuatorTags[i]), Actuator_data[i]))
                self.OPC_Actuator_Nodes[len(self.OPC_Actuator_Nodes)-1].set_writable()

            # starting!
            self.server.start()
            print("OPC-UA server started successfully, ensure your PLC has a running client!")
            
            #Setting Up ZMQ Connection
            if self.actuator:
                context = zmq.Context()
                DB = context.socket(zmq.PUSH)
                serverAddress = self.serAdd.get(block=True)
                DB.connect("tcp://"+serverAddress+":5555")
                logging.info("Successfully connected to server: " + serverAddress)
                
            #Setup timing mechanism
            time_end = time.time()
            try:
                while True:
                    time.sleep(1)
                    if self.sensor:
                        with self.Lock:
                            #Update tag values
                            for i in range(len(self.SensorTags)):
                                curNode = self.checkNodeExists(dataObj, self.SensorTags[i])
                                curNode.set_value(self.Data.read(self.SensorTags[i]))
                                print(curNode.get_value())
                            Time_stamp = self.Data.read("Time")
                            
                        #Append time if requested
                        if self.Time_Mem != -1:
                            curTime = self.checkNodeExists(dataObj, "Time")
                            if curTime != None:
                                curTime.set_value(Time_stamp)
                            else:
                                self.OPC_Sensor_Nodes.append(dataObj2.add_variable(self.idx, "Time", Time_stamp))
                                self.OPC_Actuator_Nodes.append(dataObj2.add_variable(self.idx, "Time", Time_stamp))
                    
                    #perform scan time delay if requested
                    if self.Scan_Time != 0:
                            time1 = 0
                            while time1 < self.Scan_Time and not self.Event.is_set():
                                time1 = time.time() - time_end
                    #!!!!!!!!!!!!!!!CHANGE ME!!!!!!!!!!!!!!
                    if self.actuator:
                        #gather and report data from PLC
                        for i in range(int(len(self.ActuatorTags))):
                            Actuator_data[i] = self.checkNodeExists(dataObj2, self.ActuatorTags[i]).get_value()
                            if Actuator_data[i] is not None:
                                acutation_signal = bytes(self.ActuatorTags[i]+":"+str(Actuator_data[i])+" ",'utf-8')
                                DB.send(acutation_signal,zmq.NOBLOCK)
                            else:
                                print("No Accessible Actuation Data")
                                
                    #We don't write to PLC because we are a server
            
            finally:
                time_end = time.time()
                
                #if actuator, then close connection
                if self.actuator:
                    DB.close()
                    
                #Inform everyone we have closed up
                logging.info('Thread stopped for PLC IP:%s' % self.PLC.ip)
                
                #close connection, remove subscriptions, etc
                self.server.stop()
                
        elif(isinstance(self.PLC, OPCUA_Client)):
            #Define PLC address
            client = Client(self.PLC.ip)
            #client = Client("opc.tcp://" + self.PLC.ip + ":14840/groov")
            try:
                client.connect()
            except:
                logging.error("Error: Cannot connect to OPCUA client at address " + str(self.PLC.ip))
                self.Event.set()

            nodeList = [None] * len(self.SensorTags)
            
            #Setup timing mechanism
            time_end = time.time()
            
            if self.actuator!=False:
                context = zmq.Context()
                DB = context.socket(zmq.PUSH)
                serverAddress = self.serAdd.get(block=True)
                DB.connect("tcp://"+serverAddress+":5555")
                logging.info("Successfully connected to server: " + serverAddress)
            
            #physicsDB = []
            
            while not self.Event.is_set():

                #gather data if sensor is active
                if self.sensor:
                    #get data lock and release
                    with self.Lock:
                        for i in range(len(self.SensorTags)):
                            curList = []
                            Sensor_data[i] = self.Data.read(self.SensorTags[i])
                            #*****EDIT THIS LINE BASED ON PLC NAMING CONVENTION*****
                            nodeList[i] = client.get_node('ns=' + self.SensorNS[i] + ';s=' + self.SensorMem[i])
                            #nodeList[i] = client.get_node('ns=3;s="OPC_UA_DB"."OPC_Data"."' + self.SensorTags[i] + '"')
                            #nodeList[i] = client.get_node('ns=2;s=Devices/[ETE]/OptoMMP/Modules/Channels/AO-5/Value')
                            #curList.append(nodeList[i].get_value())
                        Time_stamp = self.Data.read("Time")
                
                    #physicsDB.append(curList)
                
                    #write out to PLC
                    for i in range(len(nodeList)):
                        dv = ua.DataValue(ua.Variant((Sensor_data[i] + self.SensorOffset[i]) * self.SensorScaling[i], ua.VariantType.Float))
                        dv.ServerTimestamp = None
                        dv.SourceTimestamp = None
                        nodeList[i].set_value(dv)
                
                    #write out Time if requested
                    if self.Time_Mem != -1:
                        client.get_node('ns=' + self.Time_NS + ';s=' + self.Time_Mem).set_value(Time_stamp * self.TimeScaling)
            
                #perform scan time delay if requested
                if self.Scan_Time != 0:
                        time1 = 0
                        while time1 < self.Scan_Time and not self.Event.is_set():
                            time1 = time.time() - time_end
            
                if self.actuator:
                    #gather and report data from PLC
                    for i in range(int(len(self.ActuatorTags))):
                        Actuator_data[i] = (client.get_node('ns=' + self.ActuatorNS[i] + ';s=' + self.ActuatorMem[i]).get_value() + self.ActuatorOffset[i]) * self.ActuatorScaling[i]
                        #Actuator_data[i] = nodeList[i] = client.get_node('ns=3;s="OPC_UA_DB"."OPC_Data"."' + self.ActuatorTags[i] + '"').get_value()
                        #Actuator_data[i] = nodeList[i] = client.get_node('ns=2;s=Devices/[ETE]/OptoMMP/Modules/Channels/AI-0/Value').get_value()
                        if Actuator_data[i] is not None:
                            acutation_signal = bytes(self.ActuatorTags[i]+":"+str(Actuator_data[i])+" ",'utf-8')
                            DB.send(acutation_signal,zmq.NOBLOCK)
                        else:
                            print("Read Failure on IP: %s" % self.PLC.ip)

                time_end = time.time()
        else:
            #connect to the PLC
            self.PLC.connect()

            #  ZMQ socket to talk to server
            if self.actuator:
                context = zmq.Context()
                DB = context.socket(zmq.PUSH)
                serverAddress = self.serAdd.get(block=True)
                DB.connect("tcp://"+serverAddress+":5555")
                logging.info("Successfully connected to server: " + serverAddress)
        
            #Setup timing mechanism
            time_end = time.time()

            while not self.Event.is_set():

                #gather data if sensor is active
                if self.sensor:
                    #get data lock and release
                    with self.Lock:
                        for i in range(len(self.SensorTags)):
                            Sensor_data[i] = self.Data.read(self.SensorTags[i])
                        Time_stamp = self.Data.read("Time")
                
                    #write out to PLC
                    for i in range(len(self.SensorTags)):
                        self.PLC.write(int(self.SensorMem[i]),Sensor_data[i])
                
                    #write out Time if requested
                    if self.Time_Mem != -1:
                        self.PLC.write(int(self.Time_Mem),Time_stamp)
            
                #perform scan time delay if requested
                if self.Scan_Time != 0:
                        time1 = 0
                        while time1 < self.Scan_Time and not self.Event.is_set():
                            time1 = time.time() - time_end
            
                if self.actuator:
                    #gather and report data from PLC
                    for i in range(int(len(self.ActuatorTags))):
                        Actuator_data[i] = self.PLC.read(int(self.ActuatorMem[i]))
                        if Actuator_data[i] is not None:
                            acutation_signal = bytes(self.ActuatorTags[i]+":"+str(Actuator_data[i])+" ",'utf-8')
                            DB.send(acutation_signal,zmq.NOBLOCK)
                        else:
                            print("Read Failure on IP: %s" % self.PLC.ip)

                time_end = time.time()
        
            #close up shop
            self.PLC.close()
            #if actuator, then close connection
            if self.actuator:
                DB.close()
            #Inform everyone we have closed up
            logging.info('Thread stopped for PLC IP:%s' % self.PLC.ip)
            sys.exit(0)

    #Define how to start the connector thread
    def run(self):
        self.thread = threading.Thread(target=self.Agent)
        self.thread.daemon = True
        self.thread.start()
    #Define how to stop Event thread
    def stop(self):
        self.Event.set()
        self.thread.join()
    #wait for Event to finish
    def wait(self):
        self.thread.join()

    def __repr__(self):
        return "Connector('{},{},{},{},{}')".format(self.config.export_config(),self.serAdd,self.Data,self.Lock,self.Event)
               
def UDP_Client(Data,serAdd,Lock,nPLCs,Event):

    Event = threading.Event()
    bufferSize          = 128*1000
    serverAddressPort   = ("", 8000)
    # Create a UDP socket at client side
    UDPClientSocket = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
    UDPClientSocket.bind(serverAddressPort)
    UDPClientSocket.settimeout(30)

    #grab tag names from Data class
    with Lock:
        Tags = Data.UDP_TAGS()

    #initalize values
    nTags = len(Tags)
    Values = [0.0] * nTags
    Time_Stamp = 0.0
    First_Time = True

    while not Event.is_set():
        #recieve message from UDP
        attempts = 0
        while attempts < 5 and not Event.is_set():
            try:
                msgFromServer,address = UDPClientSocket.recvfrom(bufferSize)
                break
            except:
                attempts += 1
                logging.info("UDP Client timeout %i of 5" % attempts)
                if attempts > 5:
                    Event.set()
                    break

        if Event.is_set():
            break

        #check if its the first update to pass the server IP to PLC threads
        if First_Time:
            for i in range(nPLCs):
                serAdd.put(address[0]) #there has to be a better way to do this
            First_Time = False

        #Decode message
        msg = str(msgFromServer,'UTF-8')
        msg_split = msg.split()

        #print(msg_split)
        #See if a stop was requested
        if msg_split[0] == "STOP":
            Event.set()
            logging.info("UDP Client was sent stop request from DataBroker.")
            break

        #get and store values from msg
        for i in range(nTags):
            try:
                IDX = msg_split.index(Tags[i])
                #print(msg_split)
                Values[i] = float(msg_split[IDX+1])
                Time_Stamp = float(msg_split[IDX+2])
            except:
                logging.info("Tag: %s not in UDP message..." % Tags[i])
        
        #plop data in data repo
        with Lock:
            for i in range(nTags):
                Data.write(Tags[i],Values[i])
                Data.write("Time",Time_Stamp)

    UDPClientSocket.close()
    logging.info("UDP Client received event. Exiting")
    Event.set()
    sys.exit(0)

if __name__ == "__main__":
    format = "%(asctime)s: %(message)s"
    logging.basicConfig(format=format, level=logging.INFO,
                        datefmt="%H:%M:%S")

    pipeline = queue.Queue(maxsize=100)
    serverAddress = queue.Queue(maxsize=10)

    Event = threading.Event()
    Lock = threading.Lock()

    # Initialize system config
    sys_config = Config()
    sys_config = initialization()
    sys_config.print_conf()

    # Initalize data repo
    Sensor_Data = Data_Repo(sys_config.collect_list_dedup('SensorTags'))
    Sensor_Data.write("Time",0.0)
    print(Sensor_Data.Tag_NoDuplicates)

    # GEt # of PLCs if they exist
    if 'plc' in sys_config.type_idx:
        nPLC = sys_config.type_idx['plc']
    else:
        nPLC = 0
    
    Comms = []
    for i in range(nPLC):
        Comms.append(Connector(sys_config.read_type('plc',i),serverAddress,Sensor_Data,Lock,Event))
        
    #setup UDP thread
    UDP_Thread = threading.Thread(target=UDP_Client, args=(Sensor_Data,serverAddress,Lock,nPLC,Event))
    UDP_Thread.daemon = True

    try:
        #Start threads and wait for completion
        UDP_Thread.start()
        for c in Comms:
            c.run()

        UDP_Thread.join()
        time.sleep(2)
    
    except KeyboardInterrupt:
        print('Interrupted')
        try:
            Event.set()
            sys.exit(0)
        except SystemExit:
            Event.set()
            os._exit(0)
