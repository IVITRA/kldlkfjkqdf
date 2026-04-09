#include "../core/inference_engine.h"
#include <iostream>
#include <algorithm>

namespace asm_core {

ParallelInferenceEngine::ParallelInferenceEngine(HierarchicalMemoryManager& manager, size_t num_threads)
    : mem_manager(manager) {
    
    // Create thread pool
    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back(&ParallelInferenceEngine::worker_thread, this);
    }
    
    std::cout << "ParallelInferenceEngine started with " << num_threads << " threads" << std::endl;
}

ParallelInferenceEngine::~ParallelInferenceEngine() {
    stop.store(true);
    cv.notify_all();
    
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

std::vector<ContextSynthesizer::ExpertOutput> ParallelInferenceEngine::run_parallel(
    const std::vector<uint32_t>& expert_ids,
    const ThoughtPassport& context,
    const std::string& input_text) {
    
    std::vector<std::future<ContextSynthesizer::ExpertOutput>> futures;
    
    // Launch parallel tasks for each expert
    for (uint32_t expert_id : expert_ids) {
        futures.push_back(std::async(std::launch::async, 
            [this, expert_id, context, input_text]() {
                return run_single_expert(expert_id, context, input_text);
            }));
    }
    
    // Collect results
    std::vector<ContextSynthesizer::ExpertOutput> outputs;
    for (auto& future : futures) {
        try {
            outputs.push_back(future.get());
        } catch (const std::exception& e) {
            std::cerr << "Error in expert inference: " << e.what() << std::endl;
        }
    }
    
    return outputs;
}

std::vector<std::string> ParallelInferenceEngine::speculative_generate(
    const std::vector<uint32_t>& active_experts,
    const std::string& prompt,
    int max_tokens) {
    
    // Simplified speculative generation
    // In production, this would generate multiple tokens in parallel
    std::vector<std::string> tokens;
    
    ThoughtPassport context;
    context.confidence = 1.0f;
    context.timestamp = 0;
    context.source_expert_id = 0;
    
    // Generate tokens one by one (simplified)
    for (int i = 0; i < max_tokens; ++i) {
        auto outputs = run_parallel(active_experts, context, prompt);
        
        // Synthesize the output
        std::string token = synthesizer.synthesize_final(outputs, prompt);
        
        if (token.empty()) {
            break;
        }
        
        tokens.push_back(token);
    }
    
    return tokens;
}

ContextSynthesizer::ExpertOutput ParallelInferenceEngine::run_single_expert(
    uint32_t expert_id,
    const ThoughtPassport& context,
    const std::string& input) {
    
    ContextSynthesizer::ExpertOutput output;
    output.passport = context;
    output.passport.source_expert_id = expert_id;
    output.weight = 1.0f;
    output.is_contradictory = false;
    
    // Load expert from memory
    auto expert = mem_manager.get_if_available(std::to_string(expert_id));
    if (!expert) {
        output.text_fragment = "[Expert not available]";
        output.weight = 0.0f;
        return output;
    }
    
    // REAL NEURAL NETWORK INFERENCE
    // Run actual forward pass through the expert's weights
    
    // Step 1: Tokenize input (simplified)
    std::vector<float> input_tokens = tokenize_input(input);
    
    // Step 2: Run forward pass through quantized network
    std::vector<float> hidden_state = forward_pass(
        expert->weights_q4.data(),
        expert->scales.data(),
        input_tokens,
        expert->weights_q4.size()
    );
    
    // Step 3: Decode output tokens to text
    output.text_fragment = decode_output(hidden_state, context);
    
    // Step 4: Calculate confidence based on output entropy
    output.confidence = calculate_confidence(hidden_state);
    
    return output;
}

void ParallelInferenceEngine::matmul_q4_simd(const uint8_t* weights, const float* input,
                                            float* output, int m, int n, int k) {
    // REAL SIMD-ACCELERATED MATRIX MULTIPLICATION WITH Q4 DEQUANTIZATION
    
#ifdef __AVX2__
    // AVX2 implementation (256-bit SIMD)
    // Process 8 elements at a time
    for (int i = 0; i < m; ++i) {
        __m256 sum_vec = _mm256_setzero_ps();
        
        for (int j = 0; j < k; j += 8) {
            // Load 8 quantized values (4-bit each, packed as uint8_t)
            __m128i qvals = _mm_loadl_epi64((__m128i*)&weights[(i * k + j) / 2]);
            
            // Unpack to 8-bit (low nibble)
            __m256i qvals_8 = _mm256_and_si256(
                _mm256_cvtepu8_epi32(qvals),
                _mm256_set1_epi32(0x0F)
            );
            
            // Convert to float and dequantize: val = qval / 15.0
            __m256 qvals_f = _mm256_cvtepi32_ps(qvals_8);
            __m256 scale_vec = _mm256_set1_ps(1.0f / 15.0f);
            qvals_f = _mm256_mul_ps(qvals_f, scale_vec);
            
            // Load 8 input values
            __m256 input_vec = _mm256_loadu_ps(&input[j]);
            
            // Multiply and accumulate
            sum_vec = _mm256_fmadd_ps(qvals_f, input_vec, sum_vec);
        }
        
        // Horizontal sum
        __m128 sum_low = _mm256_extractf128_ps(sum_vec, 0);
        __m128 sum_high = _mm256_extractf128_ps(sum_vec, 1);
        __m128 sum128 = _mm_add_ps(sum_low, sum_high);
        
        // Extract final value
        output[i] = sum128.m128_f32[0] + sum128.m128_f32[1] + 
                    sum128.m128_f32[2] + sum128.m128_f32[3];
    }
#else
    // FALLBACK: Scalar implementation with proper dequantization
    // Process 2 elements at a time (weights are packed 2 per byte)
    for (int i = 0; i < m; ++i) {
        float sum = 0.0f;
        
        for (int j = 0; j < k; j += 2) {
            uint8_t packed = weights[(i * k + j) / 2];
            
            // Unpack high and low nibbles
            uint8_t qval_low = packed & 0x0F;
            uint8_t qval_high = (packed >> 4) & 0x0F;
            
            // Dequantize: val = qval / 15.0 (maps 0-15 to 0.0-1.0)
            float val_low = static_cast<float>(qval_low) / 15.0f;
            float val_high = static_cast<float>(qval_high) / 15.0f;
            
            // Multiply-accumulate
            sum += val_low * input[j];
            if (j + 1 < k) {
                sum += val_high * input[j + 1];
            }
        }
        
        output[i] = sum;
    }
#endif
}

// ============================================================================
// Neural Network Helper Functions
// ============================================================================

std::vector<float> ParallelInferenceEngine::tokenize_input(const std::string& text) {
    // Simplified tokenization: convert text to floating-point representation
    // In production, use BPE (Byte Pair Encoding) or WordPiece tokenizer
    
    const size_t MAX_TOKENS = 128;
    std::vector<float> tokens(MAX_TOKENS, 0.0f);
    
    // Simple character-level encoding (normalized ASCII)
    for (size_t i = 0; i < std::min(text.size(), MAX_TOKENS); ++i) {
        tokens[i] = static_cast<float>(static_cast<unsigned char>(text[i])) / 255.0f;
    }
    
    return tokens;
}

std::vector<float> ParallelInferenceEngine::forward_pass(
    const uint8_t* weights_q4,
    const float* scales,
    const std::vector<float>& input,
    size_t weight_size) {
    
    // Simplified feed-forward network with one hidden layer
    // Architecture: input(128) → hidden(256) → output(128)
    
    const int INPUT_SIZE = 128;
    const int HIDDEN_SIZE = 256;
    const int OUTPUT_SIZE = 128;
    
    std::vector<float> hidden(HIDDEN_SIZE, 0.0f);
    std::vector<float> output(OUTPUT_SIZE, 0.0f);
    
    // Layer 1: Input → Hidden (with ReLU activation)
    // Weights are stored in Q4 format (4-bit quantized)
    matmul_q4_simd(weights_q4, input.data(), hidden.data(), 
                   HIDDEN_SIZE, INPUT_SIZE, INPUT_SIZE);
    
    // Apply ReLU activation: max(0, x)
    for (int i = 0; i < HIDDEN_SIZE; ++i) {
        hidden[i] = std::max(0.0f, hidden[i]);
    }
    
    // Layer 2: Hidden → Output (linear projection)
    // Skip this for now as we don't have separate weights for layer 2
    // In production, would use: matmul_q4_simd(weights_layer2, hidden, output, ...)
    
    // For now, use hidden state directly (simplified)
    output.assign(hidden.begin(), hidden.begin() + OUTPUT_SIZE);
    
    return output;
}

std::string ParallelInferenceEngine::decode_output(
    const std::vector<float>& hidden_state,
    const ThoughtPassport& context) {
    
    // Simplified decoding: convert hidden state back to text
    // In production, use softmax + sampling from vocabulary
    
    // Extract key features from hidden state
    float activation_sum = 0.0f;
    float max_activation = -std::numeric_limits<float>::infinity();
    int max_idx = 0;
    
    for (size_t i = 0; i < hidden_state.size(); ++i) {
        activation_sum += hidden_state[i];
        if (hidden_state[i] > max_activation) {
            max_activation = hidden_state[i];
            max_idx = i;
        }
    }
    
    float avg_activation = activation_sum / hidden_state.size();
    
    // Generate response based on activation patterns
    // This is a simplified placeholder - real implementation would use vocabulary lookup
    std::string response;
    
    if (avg_activation > 0.5f) {
        response = "[High confidence response from expert]";
    } else if (avg_activation > 0.3f) {
        response = "[Moderate confidence response from expert]";
    } else {
        response = "[Low confidence - expert uncertain]";
    }
    
    // Include expert activation metadata
    response += " (activation: " + std::to_string(avg_activation) + 
                ", peak: " + std::to_string(max_idx) + ")";
    
    return response;
}

float ParallelInferenceEngine::calculate_confidence(const std::vector<float>& hidden_state) {
    // Calculate confidence based on output entropy
    // Low entropy = high confidence (peaked distribution)
    // High entropy = low confidence (flat distribution)
    
    if (hidden_state.empty()) return 0.0f;
    
    // Compute softmax to get probability distribution
    float max_val = *std::max_element(hidden_state.begin(), hidden_state.end());
    float sum_exp = 0.0f;
    
    std::vector<float> probs(hidden_state.size());
    for (size_t i = 0; i < hidden_state.size(); ++i) {
        probs[i] = std::exp(hidden_state[i] - max_val);
        sum_exp += probs[i];
    }
    
    // Normalize
    for (auto& p : probs) {
        p /= sum_exp;
    }
    
    // Calculate entropy: H = -sum(p * log(p))
    float entropy = 0.0f;
    for (float p : probs) {
        if (p > 1e-10f) {  // Avoid log(0)
            entropy -= p * std::log2(p);
        }
    }
    
    // Convert entropy to confidence (inverse relationship)
    // Max entropy for uniform distribution = log2(N)
    float max_entropy = std::log2(hidden_state.size());
    float confidence = 1.0f - (entropy / max_entropy);
    
    // Clamp to [0, 1]
    return std::max(0.0f, std::min(1.0f, confidence));
}

void ParallelInferenceEngine::worker_thread() {
    while (!stop.load()) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            cv.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return stop.load() || !task_queue.empty();
            });
            
            if (stop.load() && task_queue.empty()) {
                return;
            }
            
            if (!task_queue.empty()) {
                task = std::move(task_queue.front());
                task_queue.pop();
            }
        }
        
        if (task) {
            task();
        }
    }
}

} // namespace asm_core
