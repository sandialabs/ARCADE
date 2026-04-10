import zmq
import json
import time
import random
 
def main():
    context = zmq.Context()
    socket = context.socket(zmq.REP)  # REP = reply
    socket.bind("tcp://*:5556")
 
    print("ZMQ test server is running on tcp://*:5556...")
 
    try:
        while True:
            # 1. Receive data from DataBroker
            message = socket.recv_string()
            print(f"[Simulink → External Sim] Received: {message}")
            #time.sleep(0.5)  # Simulate processing time
 
            # 2. Respond with new data
            response = {
                "Input_Value_1": round(random.uniform(1.0, 10.0), 0),
                "Input_Value_2": round(random.uniform(10.0, 20.0), 0)
            }
            socket.send_string(json.dumps(response))
            print(f"[External Sim → Simulink] Sent: {response}\n")
    except KeyboardInterrupt:
        print("Shutting down test server...")
    finally:
        socket.close()
        context.term()
 
if __name__ == "__main__":
    main()