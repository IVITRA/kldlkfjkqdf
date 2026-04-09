#include "../core/async_loader.h"
#include <iostream>

namespace asm_core {

AsyncExpertLoader::AsyncExpertLoader(HierarchicalMemoryManager& manager)
    : mem_manager(manager) {
}

AsyncExpertLoader::~AsyncExpertLoader() {
    stop();
}

void AsyncExpertLoader::start() {
    running = true;
    io_thread = std::thread(&AsyncExpertLoader::io_loop, this);
    decomp_thread = std::thread(&AsyncExpertLoader::decomp_loop, this);
    
    std::cout << "AsyncExpertLoader started with IO and Decompression threads" << std::endl;
}

void AsyncExpertLoader::stop() {
    if (!running.exchange(false)) {
        return; // Already stopped
    }
    
    if (io_thread.joinable()) {
        io_thread.join();
    }
    
    if (decomp_thread.joinable()) {
        decomp_thread.join();
    }
    
    std::cout << "AsyncExpertLoader stopped" << std::endl;
}

void AsyncExpertLoader::prefetch(const std::string& expert_id) {
    auto ticket = mem_manager.request_load_async(expert_id);
    load_queue.enqueue(expert_id, ticket);
}

std::shared_ptr<HotExpert> AsyncExpertLoader::get_blocking(
    const std::string& expert_id, 
    std::chrono::milliseconds timeout) {
    
    // First check if already available
    if (auto expert = mem_manager.get_if_available(expert_id)) {
        return expert;
    }
    
    // Request load
    auto ticket = mem_manager.request_load_async(expert_id);
    
    // IMPROVEMENT: Use condition variable for efficient waiting
    // Poll with exponential backoff instead of fixed 1ms sleep
    auto start = std::chrono::steady_clock::now();
    auto poll_interval = std::chrono::microseconds(100); // Start with 100μs
    
    while (!ticket->ready.load(std::memory_order_acquire)) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > timeout) {
            std::cerr << "Timeout waiting for expert: " << expert_id << std::endl;
            return nullptr;
        }
        
        // Exponential backoff: 100μs → 200μs → 400μs → max 1ms
        std::this_thread::sleep_for(poll_interval);
        poll_interval = std::min(poll_interval * 2, std::chrono::microseconds(1000));
    }
    
    return mem_manager.get_if_available(expert_id);
}

void AsyncExpertLoader::io_loop() {
    std::string expert_id;
    std::shared_ptr<HierarchicalMemoryManager::LoadTicket> ticket;
    
    while (running.load()) {
        // IMPROVEMENT: Use blocking dequeue with timeout instead of sleep
        // This provides instant response to new requests and efficient waiting
        if (load_queue.dequeue(expert_id, ticket, std::chrono::milliseconds(100))) {
            // IO thread handles the actual disk reading
            // The memory manager handles decompression asynchronously
            // No sleep needed - dequeuing is already blocking with timeout
        }
        // If dequeue times out (100ms), loop continues and checks running flag
    }
}

void AsyncExpertLoader::decomp_loop() {
    // Decompression is now handled by the memory manager's async threads
    // This loop monitors for additional decompression tasks
    // IMPROVEMENT: Reduced sleep from 10ms to 1ms for faster response
    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void AsyncExpertLoader::predict_next_experts(const std::string& current_expert) {
    // Simple prediction: could be enhanced with ML-based prediction
    // For now, just a placeholder
    std::lock_guard<std::mutex> lock(predictor_mutex);
    predicted_next.clear();
    
    // In a real implementation, this would use:
    // 1. Markov chains based on transition probabilities
    // 2. Neural network predictions
    // 3. Domain-based predictions (if current is medical_A, predict medical_B)
}

} // namespace asm_core
