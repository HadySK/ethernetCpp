#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <chrono>



class EthernetSocket {
private:
    //Hold socket file descriptor and interface index
    int sockfd = -1;
    int ifindex = -1;
    std::string interface;

public:
    //class destructor, close socket when program end or object destroyed
    ~EthernetSocket() { if (sockfd >= 0) close(sockfd); }

    bool init(const std::string& iface) {
        interface = iface;
        sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
        if (sockfd < 0) {
            perror("socket");
            return false;
        }
        ifindex = if_nametoindex(iface.c_str());

        if (ifindex == 0) {
            std::cerr << "Invalid interface: " << iface << std::endl;
            return false;
        }
        return true;
    }

    static bool parseMAC(const std::string& macStr, uint8_t* mac) {
        std::stringstream macStream(macStr);
        std::string token;
        int i = 0;
        while (std::getline(macStream, token, ':') && i < 6) {
            mac[i] = static_cast<uint8_t>(std::stoi(token, nullptr, 16));
            i++;
        }
        return (i == 6);
    }

    bool sendFrame(const uint8_t* destMac, const std::vector<uint8_t>& payload, uint16_t ethertype = 0x88B5) {
        if (sockfd < 0) return false;

        std::vector<uint8_t> frame;
        frame.reserve(14 + payload.size());

        frame.insert(frame.end(), destMac, destMac + 6);

        uint8_t srcMac[6];
        getOwnMAC(srcMac);
        frame.insert(frame.end(), srcMac, srcMac + 6);
        //little endian ethertype to big endian for network frame
        /*
        frame.push_back(ethertype >> 8);
        frame.push_back(ethertype & 0xFF);*/
        
        //alternative to code above using htons for portability
        uint16_t netTypeVar = htons(ethertype);

        frame.insert(frame.end(),
             reinterpret_cast<uint8_t*>(&netTypeVar),
             reinterpret_cast<uint8_t*>(&netTypeVar) + 2);

        frame.insert(frame.end(), payload.begin(), payload.end());

        struct sockaddr_ll socket_address{};
        socket_address.sll_ifindex = ifindex;
        socket_address.sll_halen = ETH_ALEN;
        std::memcpy(socket_address.sll_addr, destMac, 6);

        ssize_t sent = sendto(sockfd, frame.data(), frame.size(), 0,
                             (struct sockaddr*)&socket_address, sizeof(socket_address));

        if (sent < 0) {
            perror("sendto");
            return false;
        }
        std::cout << "Sent " << sent << " bytes\n";
        return true;
    }

    void receiveLoop(int delay_ms = 800) {
        uint8_t buffer[2048];
        struct sockaddr_ll saddr{};
        socklen_t saddr_len = sizeof(saddr);
        int packetCount = 0;

        std::cout << "=== Layer 2 Receiver on " << interface << " ===\n";
        std::cout << "Showing Custom (0x88b5) + Ping packets\n";
        std::cout << "Print delay: " << delay_ms << " ms\n";
        std::cout << "Press Ctrl+C to stop\n\n";

        while (true) {
            ssize_t len = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                  (struct sockaddr*)&saddr, &saddr_len);
            //Check minimum ethernet frame length 14 bytes(6 Dest MAC+ 6 Src MAC + 2 EtherType)
            if (len < 14) continue;

            //uint16_t ethertype = (buffer[12] << 8) | buffer[13];
            //alternative to line above
            uint16_t ethertype = ntohs(*reinterpret_cast<uint16_t*>(&buffer[12]));

            bool isCustom = (ethertype == 0x88b5);
            bool isPing = false;
            // check for IPv4 frame
            if (ethertype == 0x0800 && len > 34) {  
                uint8_t ip_hlen = (buffer[14] & 0x0F) * 4;
                if (len > 14 + ip_hlen + 1) {
                    //check the first byte of the payload,
                    //in ICMP protocol, first byte dictate message type (8 is ping request, 0 is ping reply)
                    uint8_t icmp_type = buffer[14 + ip_hlen];
                    if (icmp_type == 8 || icmp_type == 0) isPing = true;
                }
            }

            if (!isCustom && !isPing) continue;

            packetCount++;
            std::cout << "[" << packetCount << "] ";

            if (isCustom) std::cout << "Custom Packet (0x88b5)\n";
            else          std::cout << "Ping Packet (ICMP)\n";

            std::cout << "  Size   : " << len << " bytes\n";
            std::cout << "  From   : ";
            for (int i = 0; i < 6; ++i) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') 
                          << (int)saddr.sll_addr[i] << (i<5 ? ":" : "");
            }
            std::cout << std::dec << "\n";

            printEthernetHeader(buffer);

            std::cout << "  Data   : ";
            //check if custom frame then data will be after the Ethernet header (byte 14)
            //if ping then we need to skip the 20-byet IP header to look at the payload data
            int start = isCustom ? 14 : 34;
            //print a maxmimum of 64 elements, or packet length if it is less than 64
            for (int i = start; i < std::min(start + 64, (int)len); ++i) {
                std::cout << (isprint(buffer[i]) ? (char)buffer[i] : '.');
            }
            std::cout << "\n--------------------------------------------------\n";

            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
    }

private:
    void getOwnMAC(uint8_t* mac) {
        struct ifreq ifr{};
        std::strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ-1);
        if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == 0) {
            std::memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
        }
    }

    void printEthernetHeader(const uint8_t* frame) {
        std::cout << "  Dest   : ";
        for (int i = 0; i < 6; ++i) 
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)frame[i] 
                      << (i<5 ? ":" : "");
        std::cout << std::dec << "\n";

        std::cout << "  Src    : ";
        for (int i = 6; i < 12; ++i) 
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)frame[i] 
                      << (i<11 ? ":" : "");
        std::cout << std::dec << "\n";

        std::cout << "  Type   : 0x" << std::hex << ((frame[12]<<8)|frame[13]) << std::dec << "\n";
    }
};