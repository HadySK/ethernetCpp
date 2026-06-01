#include <iostream>
#include "ethernet.h"


int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage:\n"
                  << "  sudo " << argv[0] << " <interface> recv [delay_ms]\n"
                  << "  sudo " << argv[0] << " <interface> send <dest_mac> [message]\n"
                  << "Example:\n"
                  << "  sudo " << argv[0] << " eth0 recv 1200\n";
        return 1;
    }

    EthernetSocket eth;
    if (!eth.init(argv[1])) return 1;

    if (argc >= 3 && std::string(argv[2]) == "recv") {
        int delay = 800;
        if (argc >= 4) {
            delay = std::stoi(argv[3]);
        }
        eth.receiveLoop(delay);
    }
    else if (argc >= 3 && std::string(argv[2]) == "send") {
        // Send Packet
        if (argc < 4) { std::cerr << "Missing dest MAC\n"; return 1; }
        uint8_t dest[6];
        if (!EthernetSocket::parseMAC(argv[3], dest)) { std::cerr << "Bad MAC\n"; return 1; }

        std::vector<uint8_t> payload;
        for (int i = 4; i < argc; ++i) {
            payload.insert(payload.end(), argv[i], argv[i] + strlen(argv[i]));
            payload.push_back(' ');
        }
        if (!payload.empty()) payload.pop_back();

        eth.sendFrame(dest, payload);
    }

    return 0;
}