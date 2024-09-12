import socket

HEADER = 64
FORMAT = 'UTF-8'
DISCONNECT_MESSAGE = "!DISCONNECT"

class Client:
    def __init__(self, server, port):
        self.server = server
        self.port = port
        self.addr = (self.server, self.port)
        self.client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.client.connect(self.addr)
    
    def send(self, msg):
        message = msg.encode(FORMAT)
        msg_length = len(message)
        send_length = str(msg_length).encode(FORMAT)
        send_length += b' ' * (HEADER - len(send_length))
        self.client.send(send_length)
        self.client.send(message)
    
    def disconnect(self):
        self.send(DISCONNECT_MESSAGE)
        self.uid = self.client.recv(2048).decode(FORMAT)
        self.client.close()

def send_file_to_server(file_name, server=None, port=None):
    if server is None:
        server = socket.gethostbyname(socket.gethostname())
    if port is None:
        raise ValueError("Port number must be provided")

    client = Client(server, port)
    
    with open(file_name) as f:
        lines = f.readlines()
    
    for line in lines:
        client.send(line)
    
    client.disconnect()
    return client.uid

if __name__ == "__main__":
    import sys
    port = int(sys.argv[1])
    file_name = sys.argv[2]
    print(send_file_to_server(file_name, port=port))
