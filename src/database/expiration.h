#pragma once
#include "database.h"
#include <vector>
#include <limits>
#include <utility>
#include <cstddef>
#include <chrono>
class ExpirationHeap{
private:
    std::vector<ExpirationEntry*> heap_;
       bool less(size_t a, size_t b) const {
        return heap_[a]->expiry < heap_[b]->expiry;
    }

    void swapNodes(size_t a, size_t b) {
        std::swap(heap_[a], heap_[b]);
        heap_[a]->heapIndex = a;
        heap_[b]->heapIndex = b;
    }

    void siftUp(size_t i) {
        while (i > 0) {
            size_t parent = (i - 1) / 2;
            if (!less(i, parent)) break;
            swapNodes(i, parent);
            i = parent;
        }
    }

    void siftDown(size_t i) {
        const size_t n = heap_.size();

        while (i < n / 2) {
            size_t left = 2 * i + 1;
            size_t right = left + 1;
            size_t smallest = i;

            if (less(left, smallest)) smallest = left;
            if (right < n && less(right, smallest)) smallest = right;

            if (smallest == i) break;
            swapNodes(i, smallest);
            i = smallest;
        }
    }

    void repair(size_t i) {
        if (i > 0 && less(i, (i - 1) / 2))
            siftUp(i);
        else
            siftDown(i);
    }

public:
    void insert(ExpirationEntry& entry){
        entry.heapIndex=heap_.size();
        heap_.push_back(&entry);
        siftUp(entry.heapIndex);
    }
    void update(ExpirationEntry& entry, uint64_t newTime) {
        entry.expiry = newTime;

        if (entry.heapIndex == std::numeric_limits<size_t>::max())
            insert(entry);
        else
            repair(entry.heapIndex);
    }
    void remove(ExpirationEntry& entry){

    }
    ExpirationEntry* top() const{
        return heap_.empty()?nullptr:heap_.front();
    }
    void pop(){
        if(!heap_.empty()){
            remove(*heap_.front());
        }
    }

    bool empty(){
        return heap_.empty();
    }
    size_t size() const{
        return heap_.size();
    }

    void reserve(size_t n){
        heap_.reserve(n);
    }
};