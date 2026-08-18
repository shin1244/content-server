#pragma once
#include "IPacketHandler.h"
#include "MPMCQueue.h"
#include "NetTypes.h" 

class QueueSink : public IPacketHandler {
public:
    QueueSink(MPMCQueue<RecvEvent>* q) : queue_(q) {}

    void OnPacket(uint64_t sessionId, const Packet& pkt) override {
        queue_->Push({ sessionId, pkt });
    }
private:
    MPMCQueue<RecvEvent>* queue_;
};