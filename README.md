# VPN-VSwitch-Level2

Build Your Own Layer 2 VPN with Virtual Switch

Introduction
------------
This project implements a Layer 2 VPN, similar to Zerotier, with a Virtual Switch (VSwitch) that allows geographically distant devices to communicate as if they were on the same local network (LAN).

The system consists of a server (VSwitch) and clients (VPort) that connect through the internet to form a private virtual network. Devices connected to the VPN can exchange Ethernet frames directly, using MAC addresses.

Rappel
--------------------
What is a Virtual Switch?
- A network switch connects devices within a network and forwards data frames based on MAC addresses. A virtual switch (VSwitch) operates at OSI Layer 2, emulating this functionality over the internet.
- In this project, VSwitch maintains a MAC address table to track clients and forward frames only to the intended destinations.

What is a TAP Device?
- A TAP device is a virtual network interface that emulates a physical network adapter. TAP devices are often used in VPNs to simulate network interfaces for secure data transfer.
- Here, TAP devices connect each client’s operating system to VSwitch, enabling data forwarding over the virtual network.

System Architecture
-------------------
1. **Server (VSwitch)**:
   - Acts as a virtual switch.
   - Maintains a MAC address table to relay Ethernet frames between connected clients.
   - Receives frames from clients and forwards them based on MAC address matching.
   
2. **Clients (VPort)**:
   - Each client connects to VSwitch using a TAP device, capturing frames and forwarding them over a UDP connection to the server.
   - One end of VPort connects to the client OS via TAP, and the other end communicates with VSwitch over the internet.

Architecture Diagram:
---------------------
```
    +----------------------------------------------+
    |                   VSwitch                    |
    |                                              |
    |     +----------------+---------------+       |
    |     |            MAC Table           |       |
    |     |--------------------------------+       |
    |     |      MAC       |      VPort    |       |
    |     |--------------------------------+       |
    |     | 11:11:11:11:11 |   VClient-1   |       |
    |     |--------------------------------+       |
    |     | aa:aa:aa:aa:aa |   VClient-a   |       |
    |     +----------------+---------------+       |
    |                                              |
    +-----------|-----------------------|----------+
        +-------|--------+     +--------|-------+
        |       v        |     |        v       |
        | +------------+ |     | +------------+ |
        | | UDP Socket | |     | | UDP Socket | |
        | +------------+ |     | +------------+ |
        |       ^        |     |        ^       |
        |       |        |     |        |       |
        |(Ethernet Frame)|     |(Ethernet Frame)|
        |       |        |     |        |       |
  VPort |       v        |     |        v       | VPort
        | +------------+ |     | +------------+ |
        | | TAP Device | |     | | TAP Device | |
        | +------------+ |     | +------------+ |
        |       ^        |     |        ^       |
        +-------|--------+     +--------|-------+
                v                       v
  -------------------------   -------------------------
  Computer A's Linux Kernel   Computer B's Linux Kernel
```

How to Build and Deploy
-----------------------
### Prerequisites
- A server with a public IP address to run VSwitch.
- At least two clients with internet access to run VPort and connect to the server.
- Root privileges on each machine to configure TAP devices.

### Step 1: Clone the repository
On the server and each client:
```bash
git clone https://github.com/your-repo/virtual-switch cd virtual-switch
```

### Step 2: Build the Project
Compile the project on each machine by running:
make

vbnet
Copier le code

### Step 3: Run VSwitch on the Server
On the server, start VSwitch by specifying the port to listen on:
```python
python3 vswitch.py 5555
```

### Step 4: Run and Configure VPort on Each Client
On each client, run the following commands to connect to the VSwitch and configure the TAP device:

1. Run VPort with the server's IP and port:
```bash
sudo ./vport SERVER_IP 5555
```

3. Configure the TAP device with a unique IP in the same subnet:
```bash
sudo ip addr add 10.1.1.101/24 dev tapyuan sudo ip link set yourtap up
```

Repeat this for additional clients with different IPs (e.g., `10.1.1.102` for a second client).

### Step 5: Test Connectivity
Use `ping` from one client to another to confirm connectivity:
```bash
ping 10.1.1.102
```

Troubleshooting
-----------------------
- **Connection Issues**: Ensure that the server's IP and port are reachable and that firewalls allow UDP traffic.
- **Permission Errors**: Ensure you have root or sudo privileges to configure TAP devices.
- **Latency**: Expect some delay due to the geographic distance and internet speed.

Conclusion
----------
This project creates a Layer 2 VPN, allowing distant devices to communicate over the internet as if they were on the same LAN. It's useful for creating virtual network environments for testing or secure private communication.

**inspired from** https://github.com/peiyuanix/build-your-own-zerotier/tree/master
