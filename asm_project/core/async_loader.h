#pragma once
#include <thread>
#include <atomic>
#include <array>
#include <queue>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <chrono>
#include "memory_manager.h"

namespace asm_core {

// Triple Buffering Architecture
template<typename T>
class TripleBuffer {
    // Buffer States: 0=Display, 1=Ready, 2=Loading
    std::array<std::atomic<T*>, 3> buffers;
    std::atomic<int> state{0}; // 0: Display, 1: Ready, 2: Loading
    
public:
    TripleBuffer() {
        for (auto& buf : buffers) buf.store(nullptr);
    }
    
    // يُستدعى من Thread الـ Generation (Reader)
    T* get_display_buffer() {
        return buffers[0].load(std::memory_order_acquire);
    }
    
    // يُستدعى من Thread الـ Loader (Producer)
    void submit_new_buffer(T* new_data) {
        // الـ Loading (2) يصبح Ready (1)
        buffers[2].store(new_data, std::memory_order_release);
        
        // Swap: Ready يصبح Display، القديم يصبح Loading
        int old_state = state.exchange(2, std::memory_order_acq_rel);
        // old_state هو الذي كان Display، الآن هو Loading (2 في الـ exchange الجديد)
    }
    
    // يُستدعى من Thread الـ Generation لطلب Swap (V-Sync like)
    bool swap_if_ready() {
        T* ready = buffers[1].load(std::memory_order_acquire);
        if (ready == nullptr) return false;
        
        // Display (0) → Loading (2)
        // Ready (1) → Display (0)
        T* old_display = buffers[0].exchange(ready, std::memory_order_acq_rel);
        buffers[2].store(old_display, std::memory_order_release);
        return true;
    }
};

// Lock-Free Queue for requests (simplified version using mutex for safety)
class RequestQueue {
    struct Node {
        std::string expert_id;
        std::shared_ptr<HierarchicalMemoryManager::LoadTicket> ticket;
    };
    
    std::queue<Node> queue;
    std::mutex queue_mutex;
    std::condition_variable cv;

public:
    void enqueue(const std::string& id, std::shared_ptr<HierarchicalMemoryManager::LoadTicket> ticket) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            queue.push({id, ticket});
        }
        cv.notify_one();
    }
    
    bool dequeue(std::string& id, std::shared_ptr<HierarchicalMemoryManager::LoadTicket>& ticket, 
                 std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (cv.wait_for(lock, timeout, [this] { return !queue.empty(); })) {
            Node node = queue.front();
            queue.pop();
            id = node.expert_id;
            ticket = node.ticket;
            return true;
        }
        return false;
    }
    
    bool empty() {
        std::lock_guard<std::mutex> lock(queue_mutex);
        return queue.empty();
    }
};

class AsyncExpertLoader {
private:
    TripleBuffer<HotExpert> display_buffer; // للـ Expert الحالي يظهر للنظام
    
    RequestQueue load_queue;
    std::thread io_thread;
    std::thread decomp_thread; // Thread منفصل للـ Decompression (CPU heavy)
    
    std::atomic<bool> running{true};
    
    // Prefetching Predictor
    std::vector<std::string> predicted_next;
    std::mutex predictor_mutex;
    
    HierarchicalMemoryManager& mem_manager;

public:
    AsyncExpertLoader(HierarchicalMemoryManager& manager);
    ~AsyncExpertLoader();
    
    // Non-blocking request
    void prefetch(const std::string& expert_id);
    
    // Blocking get with timeout (للـ Emergency)
    std::shared_ptr<HotExpert> get_blocking(const std::string& expert_id, 
                                            std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));
    
    // Start background threads
    void start();
    
    // Stop background threads
    void stop();

private:
    void io_loop(); // يقرأ من SSD
    void decomp_loop(); // يفك الضغط (Ternary→4bit)
    void predict_next_experts(const std::string& current_expert);
};

} // namespace asm_core
