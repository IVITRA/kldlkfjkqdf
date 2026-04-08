#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <algorithm>
#include <codecvt>
#include <locale>

namespace asm_core {

// Universal Tokenizer with Arabic Language Support
class UniversalTokenizer {
public:
    static constexpr size_t VOCAB_SIZE = 32000;
    
    // Tokenize text with Arabic normalization
    std::vector<uint32_t> encode(const std::string& text) {
        // Step 1: Normalize Arabic text
        std::string normalized = normalize_arabic(text);
        
        // Step 2: Apply BPE (Byte Pair Encoding)
        return bpe_encode(normalized);
    }
    
    // Decode tokens back to text
    std::string decode(const std::vector<uint32_t>& tokens) {
        std::string result;
        for (uint32_t token : tokens) {
            if (token < VOCAB_SIZE) {
                result += vocab_to_string(token);
            }
        }
        return result;
    }
    
    // Verify tokenization consistency across experts
    static bool verify_consistency(
        const std::vector<uint32_t>& tokens1,
        const std::vector<uint32_t>& tokens2) {
        return tokens1 == tokens2;
    }
    
private:
    // Arabic text normalization
    std::string normalize_arabic(const std::string& text) {
        std::string result;
        result.reserve(text.size());
        
        for (size_t i = 0; i < text.size(); ) {
            uint32_t ch = utf8_decode(text, i);
            
            // Normalize Alef variants (أإآا → ا)
            if (ch == 0x0623 || ch == 0x0625 || ch == 0x0622) {
                ch = 0x0627; // Alef
            }
            // Normalize Ha variants (ة → ه)
            else if (ch == 0x0629) {
                ch = 0x0647; // Ha
            }
            // Remove Tatweel (ـ)
            else if (ch == 0x0640) {
                continue; // Skip tatweel
            }
            // Normalize Ya variants (ى → ي)
            else if (ch == 0x0649) {
                ch = 0x064A; // Ya
            }
            // Remove diacritics (Tashkeel)
            else if (ch >= 0x064B && ch <= 0x0652) {
                continue; // Skip diacritics
            }
            
            utf8_encode(result, ch);
        }
        
        return result;
    }
    
    // BPE Encoding (simplified - in production, use trained BPE model)
    std::vector<uint32_t> bpe_encode(const std::string& text) {
        std::vector<uint32_t> tokens;
        
        // Split into words
        std::vector<std::string> words = split_words(text);
        
        for (const auto& word : words) {
            // Try to find word in vocabulary first
            uint32_t token = word_to_vocab_id(word);
            if (token < VOCAB_SIZE) {
                tokens.push_back(token);
            } else {
                // Fall back to character-level tokenization
                for (size_t i = 0; i < word.size(); ) {
                    uint32_t ch = utf8_decode(word, i);
                    tokens.push_back(char_to_vocab_id(ch));
                }
            }
        }
        
        return tokens;
    }
    
    // Split text into words
    std::vector<std::string> split_words(const std::string& text) {
        std::vector<std::string> words;
        std::string current_word;
        
        for (size_t i = 0; i < text.size(); ) {
            uint32_t ch = utf8_decode(text, i);
            
            if (is_word_character(ch)) {
                utf8_encode(current_word, ch);
            } else if (!current_word.empty()) {
                words.push_back(current_word);
                current_word.clear();
            }
        }
        
        if (!current_word.empty()) {
            words.push_back(current_word);
        }
        
        return words;
    }
    
    // Check if character is a word character (letter, digit, or Arabic letter)
    bool is_word_character(uint32_t ch) {
        // Arabic letters: 0600-06FF
        // Arabic extended: 0750-077F
        // Latin letters: 0041-005A, 0061-007A
        // Digits: 0030-0039
        return (ch >= 0x0600 && ch <= 0x06FF) ||
               (ch >= 0x0750 && ch <= 0x077F) ||
               (ch >= 0x0041 && ch <= 0x005A) ||
               (ch >= 0x0061 && ch <= 0x007A) ||
               (ch >= 0x0030 && ch <= 0x0039);
    }
    
    // Simple hash-based vocabulary mapping (in production, use trained vocabulary)
    uint32_t word_to_vocab_id(const std::string& word) {
        // Hash function for vocabulary lookup
        uint32_t hash = 0;
        for (char c : word) {
            hash = hash * 31 + static_cast<uint32_t>(c);
        }
        return hash % VOCAB_SIZE;
    }
    
    uint32_t char_to_vocab_id(uint32_t ch) {
        // Map Unicode character to vocabulary ID
        return (ch % 0x1000) + 1000; // Reserve first 1000 for special tokens
    }
    
    std::string vocab_to_string(uint32_t token) {
        // In production, use vocabulary lookup table
        return "[token:" + std::to_string(token) + "]";
    }
    
    // UTF-8 decode
    uint32_t utf8_decode(const std::string& text, size_t& pos) {
        uint8_t ch = static_cast<uint8_t>(text[pos]);
        uint32_t result = 0;
        
        if ((ch & 0x80) == 0) {
            // 1-byte character
            result = ch;
            pos += 1;
        } else if ((ch & 0xE0) == 0xC0) {
            // 2-byte character
            result = (ch & 0x1F) << 6;
            result |= (static_cast<uint8_t>(text[pos + 1]) & 0x3F);
            pos += 2;
        } else if ((ch & 0xF0) == 0xE0) {
            // 3-byte character (Arabic)
            result = (ch & 0x0F) << 12;
            result |= (static_cast<uint8_t>(text[pos + 1]) & 0x3F) << 6;
            result |= (static_cast<uint8_t>(text[pos + 2]) & 0x3F);
            pos += 3;
        } else if ((ch & 0xF8) == 0xF0) {
            // 4-byte character
            result = (ch & 0x07) << 18;
            result |= (static_cast<uint8_t>(text[pos + 1]) & 0x3F) << 12;
            result |= (static_cast<uint8_t>(text[pos + 2]) & 0x3F) << 6;
            result |= (static_cast<uint8_t>(text[pos + 3]) & 0x3F);
            pos += 4;
        }
        
        return result;
    }
    
    // UTF-8 encode
    void utf8_encode(std::string& result, uint32_t ch) {
        if (ch < 0x80) {
            result += static_cast<char>(ch);
        } else if (ch < 0x800) {
            result += static_cast<char>(0xC0 | (ch >> 6));
            result += static_cast<char>(0x80 | (ch & 0x3F));
        } else if (ch < 0x10000) {
            result += static_cast<char>(0xE0 | (ch >> 12));
            result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (ch & 0x3F));
        } else {
            result += static_cast<char>(0xF0 | (ch >> 18));
            result += static_cast<char>(0x80 | ((ch >> 12) & 0x3F));
            result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (ch & 0x3F));
        }
    }
};

} // namespace asm_core
