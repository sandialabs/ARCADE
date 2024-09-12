from client import send_file_to_server
import socket
def run_simulation(x1, x2, x3):
    print(f"Running with {x1} {x2} {x3}")
    file_name = "/home/tagray/mini-python/config.yaml"
    server = "127.0.1.1"
    port = 12345

    uid = send_file_to_server(file_name=file_name, port=port)
    print(uid)
    print("Simulation running...")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 54321))
        s.listen()
        conn, addr = s.accept()
        data = conn.recv(1024).decode('utf-8')
        print(data)

    return data
