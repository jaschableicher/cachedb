#ifndef SERVER_H
#define SERVER_H
#include <cstdint>

//Running TCP server which simply accepts commands
class TCPServer{
public:
    TCPServer();
    ~TCPServer();
    void run();
private:
    uint8_t socket_;
    uint8_t client_;
    uint8_t tcp_port_;
};


#endif