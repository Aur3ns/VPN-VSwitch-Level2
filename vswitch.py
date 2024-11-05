#!/usr/bin/env python3
import socket
import sys

if len(sys.argv) != 2:
    print("Usage: python3 vswitch.py {VSWITCH_PORT}")
    sys.exit(1)

try:
    server_port = int(sys.argv[1])
    server_addr = ("0.0.0.0", server_port)
except ValueError:
    print("Invalid port number.")
    sys.exit(1)

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as vserver_sock:
    vserver_sock.bind(server_addr)
    print(f"[VSwitch] Started at {server_addr[0]}:{server_addr[1]}")

    mac_table = {}
    while True:
        try:
            data, vport_addr = vserver_sock.recvfrom(1518)
            eth_dst, eth_src = ":".join(f"{x:02x}" for x in data[:6]), ":".join(f"{x:02x}" for x in data[6:12])
            print(f"[VSwitch] src<{eth_src}> dst<{eth_dst}> from {vport_addr}")

            mac_table[eth_src] = vport_addr
            if eth_dst in mac_table:
                vserver_sock.sendto(data, mac_table[eth_dst])
            elif eth_dst == "ff:ff:ff:ff:ff:ff":
                for dst in mac_table.values():
                    if dst != vport_addr:
                        vserver_sock.sendto(data, dst)
            else:
                print("Discarded unknown destination")
        except Exception as e:
            print(f"Error: {e}")
