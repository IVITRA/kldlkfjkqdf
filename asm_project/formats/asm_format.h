#pragma once
#include <cstdint>
#include <array>
#include <cstring>
#include <vector>

namespace asm_format {

// Header ثابت الحجم (256 bytes)
struct ExpertHeader {
    char magic[4] = {'A', 'S', 'M', '1'};
    uint32_t version = 1;
    char expert_id[64] = {0};
    char domain[32] = {0};
    uint64_t num_params = 0;
    uint8_t quantization_type = 0; // 0=FP32, 1=INT8, 2=INT4, 3=Ternary(1.58bit)
    uint64_t weights_offset = 0;
    uint64_t weights_size_compressed = 0;
    uint64_t metadata_offset = 0;
    
    // للـ Router: Centroid جاهز
    float router_centroid[128] = {0}; // 512 bytes
    
    // Checksums للتكامل
    uint64_t crc32_weights = 0;
    uint64_t crc32_metadata = 0;
    
    static constexpr size_t HEADER_SIZE = 256;
    
    void set_expert_id(const char* id) {
        strncpy(expert_id, id, sizeof(expert_id) - 1);
        expert_id[sizeof(expert_id) - 1] = '\0';
    }
    
    void set_domain(const char* d) {
        strncpy(domain, d, sizeof(domain) - 1);
        domain[sizeof(domain) - 1] = '\0';
    }
    
    bool validate_magic() const {
        return magic[0] == 'A' && magic[1] == 'S' && 
               magic[2] == 'M' && magic[3] == '1';
    }
};

// Ternary Quantization (1.58-bit): -1, 0, +1
// كل بايت يخزن 5 قيم (لأن 3^5 = 243 < 256)
// packing: 2 bits لكل قيمة (00=-1, 01=0, 10=+1, 11=padding)
class TernaryStorage {
    static constexpr uint8_t PACKING_FACTOR = 5;
    static constexpr uint8_t BITS_PER_VALUE = 2;
    
public:
    // تحويل الأوزان من float إلى ternary مضغوط
    static size_t compress(const float* input, int8_t* output, size_t n) {
        size_t output_idx = 0;
        
        for (size_t i = 0; i < n; i += PACKING_FACTOR) {
            uint8_t packed_byte = 0;
            size_t values_in_this_byte = std::min(PACKING_FACTOR, static_cast<uint8_t>(n - i));
            
            for (size_t j = 0; j < values_in_this_byte; ++j) {
                int8_t ternary_value;
                if (input[i + j] > 0.05f) {
                    ternary_value = 1;
                } else if (input[i + j] < -0.05f) {
                    ternary_value = -1;
                } else {
                    ternary_value = 0;
                }
                
                // تحويل -1, 0, 1 إلى 00, 01, 10
                uint8_t bits = static_cast<uint8_t>(ternary_value + 1);
                packed_byte |= (bits << (j * BITS_PER_VALUE));
            }
            
            output[output_idx++] = static_cast<int8_t>(packed_byte);
        }
        
        return output_idx;
    }
    
    // فك الضغط من ternary إلى float
    static void decompress(const int8_t* input, float* output, size_t n, const float* scales) {
        size_t input_idx = 0;
        
        for (size_t i = 0; i < n; i += PACKING_FACTOR) {
            uint8_t packed_byte = static_cast<uint8_t>(input[input_idx++]);
            size_t values_in_this_byte = std::min(PACKING_FACTOR, static_cast<uint8_t>(n - i));
            
            for (size_t j = 0; j < values_in_this_byte; ++j) {
                uint8_t bits = (packed_byte >> (j * BITS_PER_VALUE)) & 0x03;
                
                // تحويل 00, 01, 10 إلى -1, 0, 1
                int8_t ternary_value = static_cast<int8_t>(bits) - 1;
                
                // تطبيق الـ scaling
                float scale = scales ? scales[i + j] : 1.0f;
                output[i + j] = static_cast<float>(ternary_value) * scale;
            }
        }
    }
    
    // حساب الحجم المضغوط
    static size_t compressed_size(size_t num_values) {
        return (num_values + PACKING_FACTOR - 1) / PACKING_FACTOR;
    }
};

// INT4 Quantization: 4-bit (-8 إلى +7)
class Int4Storage {
public:
    static size_t compress(const float* input, uint8_t* output, size_t n, 
                          float* scales, size_t scale_block_size = 16) {
        size_t num_blocks = (n + scale_block_size - 1) / scale_block_size;
        
        // حساب scales لكل block
        for (size_t b = 0; b < num_blocks; ++b) {
            size_t start = b * scale_block_size;
            size_t end = std::min(start + scale_block_size, n);
            
            float max_val = 0.0f;
            for (size_t i = start; i < end; ++i) {
                max_val = std::max(max_val, std::abs(input[i]));
            }
            scales[b] = max_val / 7.0f; // INT4 range: -8 إلى +7
        }
        
        // ضغط القيم
        for (size_t i = 0; i < n; i += 2) {
            size_t block = i / scale_block_size;
            float scale = scales[block];
            
            int8_t val1 = static_cast<int8_t>(std::round(input[i] / scale));
            val1 = std::clamp(val1, static_cast<int8_t>(-8), static_cast<int8_t>(7));
            
            int8_t val2 = 0;
            if (i + 1 < n) {
                val2 = static_cast<int8_t>(std::round(input[i + 1] / scale));
                val2 = std::clamp(val2, static_cast<int8_t>(-8), static_cast<int8_t>(7));
            }
            
            // Pack two 4-bit values into one byte
            output[i / 2] = static_cast<uint8_t>(
                (static_cast<uint8_t>(val1 & 0x0F)) | 
                (static_cast<uint8_t>(val2 & 0x0F) << 4)
            );
        }
        
        return (n + 1) / 2;
    }
    
    static size_t compressed_size(size_t num_values) {
        return (num_values + 1) / 2;
    }
};

} // namespace asm_format
