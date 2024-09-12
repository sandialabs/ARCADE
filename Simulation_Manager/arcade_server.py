import socket
import threading
import uuid
import experiment_manager
import sys

class arcade_server:

    num_recieved = 0

    def __init__(self, em:experiment_manager, port=12345) -> None:
        self.em = em
        self.HEADER = 64
        self.PORT = port
        # SERVER = ""
        self.SERVER = socket.gethostbyname(socket.gethostname())
        self.ADDR = (self.SERVER, self.PORT)
        self.FORMAT = 'UTF-8'
        self.DISCONNECT_MESSAGE = "!DISCONNECT"
        

    def handle_client(self,conn, addr):
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
                if "new_sim: True" in msg:
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
        print(f"Listening on {self.SERVER}:{self.PORT}")
        self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server.bind(self.ADDR)
        self.server.listen()
        print(f"[LISTENING]")
        while True:
            conn, addr = self.server.accept()
            thread = threading.Thread(target=self.handle_client, args=(conn, addr),daemon=True)
            thread.start()