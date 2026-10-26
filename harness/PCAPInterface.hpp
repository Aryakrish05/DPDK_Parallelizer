#ifndef PCAPINTERFACE_HPP
#define PCAPINTERFACE_HPP

#include <string>
#include <stdexcept>
#include <fstream>
#include "Packet.hpp"

//pcap format for the harness
//Timestamp (uint64_t),PacketSize (uint32_t),PacketContent(Variable number of bytes)
//The timestamps and PacketSize will be in little-endian and PacketContent will be in big endian

class PCAPInterface{
public:
    
    //comma separated file names
    explicit PCAPInterface(const std::string&);
    
    PCAPInterface(const PCAPInterface&) = delete;
    PCAPInterface& operator = (const PCAPInterface&) = delete;
    PCAPInterface(PCAPInterface&&) = delete;
    PCAPInterface& operator = (PCAPInterface&&) = delete;

    //pcap packets are not dropped just because cores are busy
    static constexpr bool lossless = true;

    bool recvPacket(Packet&);
    bool sendPacket(const Packet&);
    bool discardPacket();
    //true once the whole input has been read
    bool done();
private:
    //Useful reference: https://cppscripts.com/reading-binary-file-cpp
    std::string input_file_;
    std::string output_file_;
    std::ifstream input_;
    std::ofstream output_;
    bool pcap_done_ = false;
};

PCAPInterface::PCAPInterface(const std::string& files){
    size_t comma_pos = files.find(",");
    if(comma_pos==std::string::npos){
        throw std::runtime_error("Comma separated pcap file i/o needed"); 
    }
    input_file_ = files.substr(0,comma_pos);
    output_file_ = files.substr(comma_pos+1);

    input_.open(input_file_,std::ios::binary);
    if (!input_.is_open()) {
        throw std::runtime_error("pcap input opening failed");
    }
    output_.open(output_file_,std::ios::binary);
    if (!output_.is_open()) {
        throw std::runtime_error("pcap output opening failed");
    }
}   

//assumes that pcap is well-formed
bool PCAPInterface::recvPacket(Packet& packet){
    if(input_.read(reinterpret_cast<char*>(&packet.timestamp),sizeof(uint64_t))){
        uint32_t packet_size;
        input_.read(reinterpret_cast<char*>(&packet_size),sizeof(uint32_t));
        if(packet_size>static_cast<uint32_t>(MAX_PACKET_SIZE-HEADROOM)){
            throw std::runtime_error("pcap packet too large");
        }
        packet.data = packet.buffer->bytes.data()+HEADROOM;
        packet.data_end = packet.data+packet_size;
        input_.read(packet.buffer->bytes.data()+HEADROOM,packet_size);
        return true;
    }
    pcap_done_ = true;
    return false;
}

bool PCAPInterface::done(){
    return pcap_done_;
}
//assumes that the timestamp is not changed and the packet still contains the timestamp
//while being forwarded
bool PCAPInterface::sendPacket(const Packet& packet){
    uint64_t ts = packet.timestamp;
    output_.write(reinterpret_cast<char*>(&ts),sizeof(uint64_t));
    uint32_t packet_size = static_cast<uint32_t>(packet.data_end - packet.data);
    output_.write(reinterpret_cast<char*>(&packet_size),sizeof(uint32_t));
    output_.write(packet.data,packet_size);
    output_.flush();
    return static_cast<bool>(output_);
}

bool PCAPInterface::discardPacket(){
    return false;
}
#endif