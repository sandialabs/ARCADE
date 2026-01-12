import threading
import inspect
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

def findJsonFiles(directory):
    retVal = ""
    for root, dirs, files, in os.walk(directory):
        for file in files:
            if file.endswith('.json'):
                retVal = os.path.join(root, file)

    return retVal

def attackSim(actuatorList, currTime):
    j = open(findJsonFiles('/tmp'))
    attack_data = json.load(j)
    actuatorListNew = [0] * len(actuatorList)
    for i in attack_data.get("Attacks", []):
        attack_dict = dict(i)
        if attack_dict["Type"] == "PNN":
            if float(currTime) <  float(attack_dict["Time"]):
                for i in range (len(actuatorList)):
                    actuatorListNew[i] = -100000000000000.000000
                continue
            else:
                logging.info("Actuating attack at time: " + str(currTime))
                if attack_dict["Set_All_Values"] == "True":
                    for i in range(len(actuatorList)):
                        actuatorListNew[i] = float(attack_dict["Value1"])
                else:
                    for i in range (len(actuatorList)):
                        try:
                            actuatorListNew[i] = float(attack_dict["Value" + str((i+1))])
                        except Exception as e:
                            actuatorListNew[i] = -100000000000000.000000

        elif attack_dict["Type"] == "A2L":
            if float(currTime) <  float(attack_dict["Time"]):
                for i in range (len(actuatorList)):
                    actuatorListNew[i] = -100000000000000.000000
                continue
            else:
                logging.info("Actuating attack at time: " + str(currTime))
                if attack_dict["Set_All_Values"] == "True":
                    for i in range(len(actuatorList)):
                        actuatorListNew[i] = float(attack_dict["Value1"])
                else:
                    for i in range (len(actuatorList)):
                        try:
                            actuatorListNew[i] = float(attack_dict["Value" + str((i+1))])
                        except Exception as e:
                            actuatorListNew[i] = -100000000000000.000000

        else:
            logging.info('Attack type not defined, verify JSON file')
    
    return actuatorListNew

class Connector:
    def __init__(self,config,serAdd,Data,Lock,Event):
        self.config = Config()
        self.config.import_config(config)
        self.serAdd = serAdd
        self.Data = Data
        self.Lock = Lock
        self.Time_Mem = -1
        self.Scan_Time = 0.1
        self.actuator = False
        self.sensor = False
        self.SensorTags = []
        self.SensorMem = []
        self.ActuatorTags = []
        self.ActuatorMem = []
        self.thread = None
        self.Event = Event
        self.setup()

    def setup(self):
        #dict of all available PLC options
        
        #check if PLC option exists, else use modbus as default
        
        #setup loop to check all configs
        config_opts = ["Time_Mem","Scan_Time"]
        config_list_opts = ["SensorTags","SensorMem","ActuatorTags","ActuatorMem"]
        for c in config_opts:
            if self.config.validate(c):
                exec( 'self.' + c + '=self.config.collect(c)' )
        
        for c in config_list_opts:
            if self.config.validate(c):
                exec( 'self.' + c + '=self.config.collect_list(c)' )
        
        #check for actuator and sensor options
        if self.ActuatorTags:
            if len(self.ActuatorTags) == len(self.ActuatorMem):
                self.actuator = True
            else:
                print('Error: # of Actuator tags and # of Actuator memory addresses is not the same.')
        if self.SensorTags:
            if len(self.SensorTags) == len(self.SensorMem):
                self.sensor = True
            else:
                print('Error: # of Sensor tags and # of Sensor memory addresses is not the same.')
    
    #This is a helper function for browsing the OPC-UA Node Tree, returns a node of the given browse_name tag
    
    #define the thread that will run PLC comms
    def Agent(self):
        #initalize some values
        if self.sensor:
            Sensor_data = [0.0] * len(self.SensorTags)
        if self.actuator:
            Actuator_data = [0.0] * len(self.ActuatorTags)
            
        #ZMQ socket to talk to server
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
            
                #write out Time if requested
        
            #perform scan time delay if requested
            if self.Scan_Time != 0:
                    time1 = 0
                    while time1 < float(self.Scan_Time[0]) and not self.Event.is_set():
                        time1 = time.time() - time_end
        
            if self.actuator:
                #gather and report data from PLC
                #Add sensor data here as well for attackSim
                Actuator_data = attackSim(self.ActuatorTags, self.Data.read("Time"))
                for i in range(int(len(self.ActuatorTags))):
                    acutation_signal = bytes(str(self.ActuatorTags[i])+":"+str(Actuator_data[i])+" ",'utf-8')
                    DB.send(acutation_signal,zmq.NOBLOCK)
            else:
                print("No actuation tags!")
            time_end = time.time()
    
        #if actuator, then close connection
        if self.actuator:
            DB.close()
        #Inform everyone we have closed up
        logging.info('Thread stopped')
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

    pipeline = queue.Queue(maxsize=10)
    serverAddress = queue.Queue(maxsize=10)

    Event = threading.Event()
    Lock = threading.Lock()

    #Testing
    #print(attackSim([0,0], 10))
    
    # Initialize system config
    sys_config = initialization()
    sys_config.print_conf()

    # Initalize data repo
    Sensor_Data = Data_Repo(sys_config.collect_list_dedup('SensorTags'))
    Sensor_Data.write("Time",0.0)
    print(Sensor_Data.Tag_NoDuplicates)

    # Get # of PLCs if they exist
    if 'plc' in sys_config.type_idx:
        nPLC = sys_config.type_idx['plc']
    else:
        nPLC = 0
    
    com = Connector(sys_config.read_type('plc',0),serverAddress,Sensor_Data,Lock,Event)
        
    #setup UDP thread
    UDP_Thread = threading.Thread(target=UDP_Client, args=(Sensor_Data,serverAddress,Lock,nPLC,Event))
    UDP_Thread.daemon = True

    try:
        #Start threads and wait for completion
        UDP_Thread.start()
        com.run()

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
