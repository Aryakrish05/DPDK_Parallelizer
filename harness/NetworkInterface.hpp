#ifndef NETWORKINTERFACE_HPP
#define NETWORKINTERFACE_HPP

#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <net/if.h>
#include <arpa/inet.h>
#include <stdexcept>
#include "Packet.hpp"

// A non-blocking interface
class NetworkInterface{
public:
    
    explicit NetworkInterface(const std::string&);
    
    NetworkInterface(const NetworkInterface&) = delete;
    NetworkInterface& operator = (const NetworkInterface&) = delete;
    NetworkInterface(NetworkInterface&&) = delete;
    NetworkInterface& operator = (NetworkInterface&&) = delete;

    bool recvPacket(Packet&);
    bool sendPacket(const Packet&);
    bool discardPacket();
    ~NetworkInterface();
private:
    int sockfd_;
};

NetworkInterface::NetworkInterface(const std::string& intf_name){
    sockfd_ = socket(AF_PACKET, SOCK_RAW | SOCK_NONBLOCK, htons(ETH_P_ALL));
    if(sockfd_ < 0){
        throw std::runtime_error("Socket creation failed");
    }
    uint32_t if_index = if_nametoindex(intf_name.c_str());
    if(if_index == 0){
        close(sockfd_);
        throw std::runtime_error("Invalid interface");
    }
    struct sockaddr_ll sll = {
        .sll_family   = AF_PACKET,
        .sll_protocol = htons(ETH_P_ALL),
        .sll_ifindex  = static_cast<int>(if_index),
    };
    if(bind(sockfd_, reinterpret_cast<sockaddr *>(&sll), sizeof(sll))<0){
        close(sockfd_);
        throw std::runtime_error("Bind to interface failed");
    }
    
    // https://thomasw.dev/post/packet_ignore_outgoing/
    int ignore_outgoing = 1;
    if (setsockopt(sockfd_,SOL_PACKET,PACKET_IGNORE_OUTGOING,&ignore_outgoing,sizeof(ignore_outgoing)) < 0) {
        close(sockfd_);
        throw std::runtime_error("Ignoring Outgoing failed");
    }
}   

NetworkInterface::~NetworkInterface(){
    close(sockfd_);
}

bool NetworkInterface::recvPacket(Packet& packet){
    ssize_t n = recv(sockfd_, packet.buffer->bytes.data(), MAX_PACKET_SIZE, MSG_TRUNC);
    if(n>MAX_PACKET_SIZE || n<=0){
        return false;
    }
    packet.data = packet.buffer->bytes.data();
    packet.data_end = packet.data + n;
    return true;
}

bool NetworkInterface::sendPacket(const Packet& packet){
    ssize_t n = send(sockfd_,packet.data,packet.data_end-packet.data,0);
    if(n==(packet.data_end-packet.data)){
        return true;
    }
    return false;
}

// need to check if we actually need MSG_TRUNC here
bool NetworkInterface::discardPacket(){
    char buf[1];
    return recv(sockfd_,buf,1,MSG_TRUNC)>0;
}
#endif