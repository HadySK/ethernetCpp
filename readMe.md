Layer 2 Ethernet sockets in C++ on Linux (Raspberry Pi 5 running Debian 13)

The code will send and receive a packet over Ethernet using a device's MAC address.

Ethernet Frame as show below has 14 bytes for header+ CRC, plus a minimum of 46 bytes for payload.
We will build the ethernet frame manually in sendFrame function
![alt text](docs/EthernetFrame.png)  