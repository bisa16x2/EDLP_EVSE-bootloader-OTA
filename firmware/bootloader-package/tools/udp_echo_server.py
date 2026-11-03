import socket

HOST = "0.0.0.0"
PORT = 6000

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((HOST, PORT))

print(f"UDP echo server listening on {HOST}:{PORT}")

while True:
    data, addr = sock.recvfrom(2048)

    print(
        f"RX {len(data)} bytes from "
        f"{addr[0]}:{addr[1]} -> "
        f"{data.hex(' ')}"
    )

    sock.sendto(data, addr)

    print(
        f"TX {len(data)} bytes -> "
        f"{addr[0]}:{addr[1]}"
    )