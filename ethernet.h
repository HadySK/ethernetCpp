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

        
        //############################
        //############################
        //############################
        std::cout << iface << std::endl;
        std::cout << ifindex << std::endl;
        int rr=0;
        std::cin>> rr;

        //############################

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
};