#pragma once
#include "IPacketHandler.h"
#include "MPMCQueue.h"
#include "NetTypes.h" 

class QueueSink : public IPacketHandler {
public:
    QueueSink(MPMCQueue<Packet>* q) : queue_(q) {}

    void OnPacket(const Packet& pkt) override {
        queue_->Push(pkt);
    }
private:
    MPMCQueue<Packet>* queue_;
};