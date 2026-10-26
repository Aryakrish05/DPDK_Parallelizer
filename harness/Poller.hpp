#ifndef POLLER_HPP
#define POLLER_HPP

#include <functional>
#include <vector>

struct PollItem {
    std::function<void()> poll;
};

class Poller {
public:
    void addItem(PollItem);
    void run();
    void stop();
private:
    std::vector<PollItem> items_;
    bool stopped_ = false;
};

void Poller::addItem(PollItem item){
    items_.push_back(item);
}

void Poller::run(){
    while(!stopped_){
        for(size_t i=0;i<items_.size();i++){
            items_[i].poll();
        }
    }
}

void Poller::stop(){
    stopped_ = true;
}

#endif