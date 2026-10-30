#include "ArgParse.hpp"
#include "Packet.hpp"
#include "NIC.hpp"
#include "NetworkInterface.hpp"
#include "PCAPInterface.hpp"
#include "Core.hpp"
#include "Queues.hpp"
#ifdef PROGRAM
#include PROGRAM
#else
#include "Program.hpp"
#endif
#include <thread>
#include <vector>
#include <deque>
#include <cassert>
#include <cstdlib>

// https://eli.thegreenplace.net/2016/c11-threads-affinity-and-hyperthreading/
// pin a thread to an lcore (for now we consider each hardware thread 
// as a core although two may share the same cpu)
void pin_pthread_to_lcore(pthread_t t, int lcore_no){
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(lcore_no, &cpuset);
    int rc = pthread_setaffinity_np(t,sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        std::cerr << "Error calling pthread_setaffinity_np for pthread id = " << t << " : " << rc << "\n";
    }
}

template<typename Interface>
void runHarness(const Config& config){
    std::shared_ptr<BufferPool> buffer_pool = std::make_shared<BufferPool>(config.buffer_pool_size);
    std::shared_ptr<SharedState> shared_state = std::make_shared<SharedState>();
    std::shared_ptr<Serializer> serializer = std::make_shared<Serializer>(config.serialFile());
    NIC<Interface> nic(config.intf_name,buffer_pool);
    std::deque<Core>cores;// std::mutex cannot be moved - so vector.emplace_back gives compilation error

    for(uint32_t i=0;i<config.num_cores;i++){
        std::shared_ptr<RXQueue> rx_queue = std::make_shared<RXQueue>(config.queue_size);
        std::shared_ptr<TXQueue> tx_queue = std::make_shared<TXQueue>(config.queue_size);
        CoreContext ctx{config.num_cores,serializer,config.dumpFile(i)};
        cores.emplace_back(rx_queue,tx_queue,shared_state,buffer_pool,ctx);
        nic.addRXQueue(rx_queue);
        nic.addTXQueue(tx_queue);
    }

    pin_pthread_to_lcore(pthread_self(),0);

    std::vector<std::jthread>core_threads;
    unsigned int num_lcores = std::thread::hardware_concurrency();
    assert(num_lcores);
    for(uint32_t i=0;i<config.num_cores;i++){
        core_threads.emplace_back(&Core::run,&cores[i]);
        pin_pthread_to_lcore(core_threads[i].native_handle(),(i+1)%num_lcores);
    }

    nic.run();
    //the cores loop forever, every packet (in pcap) is done once nic.run returns
    std::exit(0);
}

int main(int argc, char** argv){
    Config config = parseArgs(argc,argv);
    if(config.pcap){
        runHarness<PCAPInterface>(config);
    }
    else{
        runHarness<NetworkInterface>(config);
    }
}