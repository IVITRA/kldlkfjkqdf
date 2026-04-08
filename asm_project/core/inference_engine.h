#pragma once
#include <vector>
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include "context_protocol.h"
#include "memory_manager.h"

namespace asm_core {

class ParallelInferenceEngine {
private:
    // Thread Pool ثابت 
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> task_queue;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> stop{false};
    
    HierarchicalMemoryManager& mem_manager;
    ContextSynthesizer synthesizer;
    
    // SIMD-optimized kernels
    void matmul_q4_simd(const uint8_t* weights, const float* input, 
                       float* output, int m, int n, int k);

public:
    ParallelInferenceEngine(HierarchicalMemoryManager& manager, size_t num_threads = 10);
    ~ParallelInferenceEngine();
    
    // تشغيل مجموعة خبراء بالتوازي
    std::vector<ContextSynthesizer::ExpertOutput> run_parallel(
        const std::vector<uint32_t>& expert_ids,
        const ThoughtPassport& context,
        const std::string& input_text);
    
    // Speculative Decoding: توليد 5 tokens دفعة واحدة
    std::vector<std::string> speculative_generate(
        const std::vector<uint32_t>& active_experts,
        const std::string& prompt,
        int max_tokens = 100);

private:
    // Expert Individual Inference
    ContextSynthesizer::ExpertOutput run_single_expert(uint32_t expert_id, 
                                  const ThoughtPassport& context,
                                  const std::string& input);
    
    // Worker thread function
    void worker_thread();
};

} // namespace asm_core
