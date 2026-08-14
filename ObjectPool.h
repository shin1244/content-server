#pragma once

#include <vector>
#include <stack>
#include <mutex>

template <typename T, int N>
class ObjectPool {
private:
    std::vector<T> items;
    std::stack<int> freeList;
    std::mutex lock;

public:
    ObjectPool()
        : items(N)
    {
        for (int i = N - 1; i >= 0; --i)
            freeList.push(i);
    }

    int Alloc() {
        std::lock_guard<std::mutex> g(lock);

        if (freeList.empty())
            return -1;

        int i = freeList.top();
        freeList.pop();

        return i;
    }

    void Free(int i) {
        std::lock_guard<std::mutex> g(lock);
        freeList.push(i);
    }

    T& operator[](int i) {
        return items[i];
    }

    T* begin() {
        return items.data();
    }

    T* end() {
        return items.data() + items.size();
    }
};