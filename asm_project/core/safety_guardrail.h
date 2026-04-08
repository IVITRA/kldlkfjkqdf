#pragma once
#include <string>
#include <vector>
#include <set>
#include <iostream>

namespace asm_core {

// Safety Guardrails to prevent harmful outputs
class SafetyGuardrail {
private:
    // Simple bloom filter for harmful content detection
    std::set<std::string> harmful_phrases;
    
public:
    SafetyGuardrail() {
        // Initialize with known harmful patterns
        harmful_phrases = {
            "اشرب البنزين", "drink gasoline", "kill yourself",
            "انتحر", "harm yourself", "self-harm"
        };
    }
    
    // 1. Output Filter - Check for harmful content
    bool contains_harmful_content(const std::string& text) {
        for (const auto& phrase : harmful_phrases) {
            if (text.find(phrase) != std::string::npos) {
                std::cerr << "⚠️  HARMFUL CONTENT DETECTED: " << phrase << std::endl;
                return true;
            }
        }
        return false;
    }
    
    // 2. Cross-Verification - Require secondary expert agreement
    struct VerificationResult {
        bool approved;
        float agreement_score;
        std::string warning_message;
    };
    
    VerificationResult verify_with_secondary_expert(
        const std::string& primary_text,
        const std::string& secondary_text,
        float threshold = 0.8f) {
        
        // Simple text similarity check (Jaccard similarity)
        std::set<std::string> words1 = tokenize(primary_text);
        std::set<std::string> words2 = tokenize(secondary_text);
        
        // Calculate intersection
        std::vector<std::string> intersection;
        for (const auto& word : words1) {
            if (words2.count(word) > 0) {
                intersection.push_back(word);
            }
        }
        
        // Jaccard similarity
        size_t union_size = words1.size() + words2.size() - intersection.size();
        float similarity = union_size > 0 ? 
            intersection.size() / (float)union_size : 0.0f;
        
        VerificationResult result;
        result.agreement_score = similarity;
        result.approved = (similarity >= threshold);
        
        if (!result.approved && similarity < 0.5f) {
            result.warning_message = "⚠️  Low agreement between experts (" + 
                                     std::to_string((int)(similarity * 100)) + 
                                     "%) - response may be unreliable";
        }
        
        return result;
    }
    
    // 3. Knowledge Freshness Check
    std::string check_expert_freshness(int training_year, const std::string& domain) {
        int current_year = 2026;
        int age = current_year - training_year;
        
        if (domain == "medical" || domain == "legal") {
            if (age > 5) {
                return "⚠️  WARNING: This " + domain + " expert is " + 
                       std::to_string(age) + " years old. Consult a professional for current information.";
            }
        } else if (age > 10) {
            return "⚠️  Note: This expert was trained " + std::to_string(age) + " years ago.";
        }
        
        return ""; // No warning needed
    }
    
    // 4. Comprehensive Safety Check
    struct SafetyReport {
        bool is_safe;
        std::vector<std::string> warnings;
        float confidence_score;
    };
    
    SafetyReport comprehensive_check(
        const std::string& output,
        const std::string& domain,
        int expert_training_year) {
        
        SafetyReport report;
        report.is_safe = true;
        report.confidence_score = 1.0f;
        
        // Check for harmful content
        if (contains_harmful_content(output)) {
            report.is_safe = false;
            report.warnings.push_back("🚫 Harmful content detected");
            report.confidence_score = 0.0f;
        }
        
        // Check freshness
        std::string freshness_warning = check_expert_freshness(
            expert_training_year, domain);
        if (!freshness_warning.empty()) {
            report.warnings.push_back(freshness_warning);
            report.confidence_score *= 0.8f;
        }
        
        return report;
    }
    
private:
    // Simple tokenizer for similarity check
    std::set<std::string> tokenize(const std::string& text) {
        std::set<std::string> words;
        std::string current_word;
        
        for (char c : text) {
            if (std::isalnum(c)) {
                current_word += std::tolower(c);
            } else if (!current_word.empty()) {
                words.insert(current_word);
                current_word.clear();
            }
        }
        
        if (!current_word.empty()) {
            words.insert(current_word);
        }
        
        return words;
    }
};

} // namespace asm_core
