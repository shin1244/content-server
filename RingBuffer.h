#pragma once
#include <cstring>

class RingBuffer {
private:
    static const int BUFFER_SIZE = 4096;
    char buffer_[BUFFER_SIZE];
    int head_ = 0;
    int tail_ = 0;
public:
    int GetUsedSize();
    int GetEmptySize();
    
    int GetLinearUsedSize();
    int GetLinearEmptySize();

    char* GetHead();
    char* GetTail();

    void OnRead(int bytes);
    void OnWrite(int bytes);
    void Peek(char* dest, int len);
    bool Write(const char* data, int len);
    void Clear();
};