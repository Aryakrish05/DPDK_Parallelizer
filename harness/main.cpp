#include "ArgParse.hpp"
#include "Packet.hpp"
#include "NIC.hpp"
#include "Core.hpp"
#include "Queues.hpp"
#include "Program.hpp"
#include <thread>
#include <vector>

int main(int argc, char** argv){
    Config config = parseArgs(argc,argv);
    std::shared_ptr<BufferPool> buffer_pool = std::make_shared<BufferPool>(config.buffer_pool_size);
    std::shared_ptr<SharedState> shared_state = std::make_shared<SharedState>();
    NIC nic(config.intf_name,buffer_pool);
    std::vector<Core>cores;

    for(size_t i=0;i<config.num_cores;i++){
        std::shared_ptr<RXQueue> rx_queue = std::make_shared<RXQueue>(config.queue_size);
        std::shared_ptr<TXQueue> tx_queue = std::make_shared<TXQueue>(config.queue_size);
        cores.emplace_back(rx_queue,tx_queue,shared_state,buffer_pool);
        nic.addRXQueue(rx_queue);
        nic.addTXQueue(tx_queue);
    }

    std::vector<std::jthread>core_threads;
    for(size_t i=0;i<config.num_cores;i++){
        core_threads.emplace_back(&Core::run,&cores[i]);
    }

    nic.run();
}