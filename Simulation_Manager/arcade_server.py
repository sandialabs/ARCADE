import select
import socket
import threading
import uuid
from Simulation_Manager import experiment_manager
import sys

class arcade_server:

    num_recieved = 0

    def __init__(self, em:experiment_manager, stop_event, port=12345) -> None:
        self.em = em
        self.HEADER = 64
        self.PORT = port
        self.stop_event = stop_event

        self.SERVER = socket.gethostbyname(socket.gethostname())
        self.ADDR = (self.SERVER, self.PORT)
        self.FORMAT = 'UTF-8'
        self.DISCONNECT_MESSAGE = "!DISCONNECT"

    def handle_client(self, conn, addr):
        input = []
        connected = True
        new_sim = False
        while connected:
            msg_length = conn.recv(self.HEADER).decode(self.FORMAT)
            if msg_length:
                msg_length = int(msg_length)
                msg = conn.recv(msg_length).decode(self.FORMAT)
                if msg == self.DISCONNECT_MESSAGE:
                    connected = False
                else:
                    input.append(msg)
                if "new_sim: True" or "new_sim: true" in msg:
                    new_sim = True
            #conn.send("Msg received".encode(self.FORMAT))
        self.num_recieved += 1
        if new_sim:
            uid = uuid.uuid4()
            conn.send(str(uid).encode(self.FORMAT))
            with open(f"/tmp/{uid}.yaml", "w+") as f:
                f.writelines(input)
                f.write("\n")
            self.em.create_simultation(f"/tmp/{uid}.yaml", uid)
        else:
            self.em.msg_from_sim(input)
        conn.close()

    def start(self):
        self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server.bind(self.ADDR)
        self.server.listen()
        print(f"[LISTENING]")
        try:
          inputs = [self.server]  # List of sockets to monitor for incoming connections
          while not self.stop_event.is_set():
            # Use select to wait for the server socket to be ready for reading
            readable, _, _ = select.select(inputs, [], [], 0.5)  # 0.5 sec timeout

            for s in readable:
              if s is self.server:
                conn, addr = self.server.accept()
                thread = threading.Thread(target=self.handle_client, args=(conn, addr), daemon=True)
                thread.start()
        except Exception as e:
          print(f'An expected error occurred in arcade_server.start {e}')
