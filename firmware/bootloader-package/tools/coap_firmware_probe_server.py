import socket


HOST = "0.0.0.0"
PORT = 5684

TEST_PAYLOAD = b"TEST-FIRMWARE-BLOCK-0"


def handle_request(data: bytes, addr, sock: socket.socket):
    if len(data) < 4:
        print("Invalid CoAP packet")
        return

    first = data[0]

    version = (first >> 6) & 0x03
    msg_type = (first >> 4) & 0x03
    token_length = first & 0x0F

    code = data[1]

    message_id = int.from_bytes(
        data[2:4],
        byteorder="big",
    )

    if len(data) < 4 + token_length:
        print("Invalid token length")
        return

    token = data[
        4:4 + token_length
    ]

    print()
    print(
        f"RX {len(data)} bytes "
        f"from {addr[0]}:{addr[1]}"
    )

    print(
        f"CoAP version={version} "
        f"type={msg_type} "
        f"code=0x{code:02X}"
    )

    print(
        f"MID=0x{message_id:04X} "
        f"Token={token.hex(' ')}"
    )

    print(
        f"RAW={data.hex(' ')}"
    )

    # GET = 0.01
    if code != 0x01:
        print("Not a GET request")
        return

    #
    # ACK + 2.05 Content
    #
    # Version = 1
    # Type    = ACK (2)
    # TKL     = request와 동일
    #
    response_first = (
        0x40 |
        (2 << 4) |
        token_length
    )

    response = bytearray()

    response.append(response_first)

    # 2.05 Content
    response.append(0x45)

    response.extend(
        message_id.to_bytes(
            2,
            byteorder="big",
        )
    )

    response.extend(token)

    # Payload marker
    response.append(0xFF)

    response.extend(TEST_PAYLOAD)

    sock.sendto(
        response,
        addr,
    )

    print(
        f"TX 2.05 Content "
        f"{len(response)} bytes"
    )


def main():
    sock = socket.socket(
        socket.AF_INET,
        socket.SOCK_DGRAM,
    )

    sock.bind(
        (HOST, PORT)
    )

    print(
        f"CoAP firmware probe server "
        f"listening on {HOST}:{PORT}"
    )

    while True:
        data, addr = sock.recvfrom(2048)

        handle_request(
            data,
            addr,
            sock,
        )


if __name__ == "__main__":
    main()