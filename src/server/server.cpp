//When running for now simply takes in a line which is brought in, executes the command via executor and sends the result back, that is it
//For now only one connection possible!
#include "server.h"
#include <unistd.h>


TCPServer::TCPServer(){ 
    //initialize tcp server with a port to listen to
}

TCPServer::~TCPServer(){ 
    close(client_);//If a client exists 
    close(socket_);
}

