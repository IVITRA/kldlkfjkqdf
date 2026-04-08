#pragma once
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <numeric>
#include <map>
#include <cstdint>
#include "context_protocol.h"

namespace asm_core {

// Advanced Consensus Algorithm for Multi-Expert Agreement
class ConsensusAlgorithm {
public:
    struct ExpertOutput {
        std::string text;
        float confidence;
        Domain domain;
        uint32_t expert_id;
        float epistemic_weight; // Domain expertise weight
    };
    
    struct WeightedVote {
        std::string synthesized_text;
        float consensus_score; // 0.0 - 1.0
        bool has_contradiction;
        std::vector<uint32_t> agreeing_experts;
        std::vector<uint32_t> disagreeing_experts;
    };
    
    // Main conflict resolution
    WeightedVote resolve_conflict(std::vector<ExpertOutput> outputs) {
        if (outputs.empty()) {
            return {"", 0.0f, false, {}, {}};
        }
        
        if (outputs.size() == 1) {
            return {outputs[0].text, outputs[0].confidence, false, 
                    {outputs[0].expert_id}, {}};
        }
        
        // Step 1: Check for contradictions
        bool has_contradiction = detect_contradictions(outputs);
        
        // Step 2: Calculate epistemic weights
        calculate_epistemic_weights(outputs);
        
        // Step 3: If contradiction, use dialectic synthesis
        if (has_contradiction) {
            return dialectic_synthesis(outputs);
        }
        
        // Step 4: Otherwise, weighted consensus
        return weighted_consensus(outputs);
    }
    
private:
    // Detect if outputs contradict each other
    bool detect_contradictions(const std::vector<ExpertOutput>& outputs) {
        if (outputs.size() < 2) return false;
        
        // Check for semantic contradictions
        int positive_count = 0;
        int negative_count = 0;
        
        for (const auto& out : outputs) {
            if (contains_negative_marker(out.text)) {
                negative_count++;
            } else {
                positive_count++;
            }
        }
        
        // If split 50/50, it's a contradiction
        int total = positive_count + negative_count;
        if (total > 0) {
            float ratio = std::min(positive_count, negative_count) / (float)total;
            return ratio > 0.3f; // 30%+ disagreement = contradiction
        }
        
        return false;
    }
    
    // Dialectic Synthesis: Thesis + Antithesis → Synthesis
    WeightedVote dialectic_synthesis(const std::vector<ExpertOutput>& outputs) {
        // Group by domain expertise
        std::map<Domain, std::vector<const ExpertOutput*>> by_domain;
        for (const auto& out : outputs) {
            by_domain[out.domain].push_back(&out);
        }
        
        // Find primary and secondary perspectives
        const ExpertOutput* primary = nullptr;
        const ExpertOutput* secondary = nullptr;
        
        for (const auto& out : outputs) {
            if (!primary || out.epistemic_weight > primary->epistemic_weight) {
                secondary = primary;
                primary = &out;
            }
        }
        
        // Synthesize: acknowledge both perspectives
        std::string synthesis = "From multiple perspectives:\n";
        
        if (primary) {
            synthesis += "• Primary view: " + primary->text + "\n";
        }
        
        if (secondary) {
            synthesis += "• Alternative view: " + secondary->text + "\n";
        }
        
        synthesis += "\nThese perspectives can be reconciled by understanding the context and domain-specific nuances.";
        
        std::vector<uint32_t> all_experts;
        for (const auto& out : outputs) {
            all_experts.push_back(out.expert_id);
        }
        
        return {
            synthesis,
            0.7f, // Moderate confidence due to contradiction
            true,
            all_experts,
            {}
        };
    }
    
    // Weighted Consensus: Epistemic weight + Confidence
    WeightedVote weighted_consensus(const std::vector<ExpertOutput>& outputs) {
        // Sort by combined score (epistemic_weight * confidence)
        std::vector<const ExpertOutput*> sorted_outputs;
        for (const auto& out : outputs) {
            sorted_outputs.push_back(&out);
        }
        
        std::sort(sorted_outputs.begin(), sorted_outputs.end(),
            [](const ExpertOutput* a, const ExpertOutput* b) {
                return (a->epistemic_weight * a->confidence) > 
                       (b->epistemic_weight * b->confidence);
            });
        
        // Take weighted average of top responses
        std::string result = sorted_outputs[0]->text;
        float total_weight = 0.0f;
        float weighted_confidence = 0.0f;
        
        std::vector<uint32_t> agreeing;
        
        for (size_t i = 0; i < std::min(sorted_outputs.size(), size_t(3)); ++i) {
            float weight = sorted_outputs[i]->epistemic_weight * sorted_outputs[i]->confidence;
            total_weight += weight;
            weighted_confidence += weight * sorted_outputs[i]->confidence;
            agreeing.push_back(sorted_outputs[i]->expert_id);
        }
        
        if (total_weight > 0) {
            weighted_confidence /= total_weight;
        }
        
        return {
            result,
            weighted_confidence,
            false,
            agreeing,
            {}
        };
    }
    
    // Calculate epistemic weight based on domain relevance
    void calculate_epistemic_weights(std::vector<ExpertOutput>& outputs) {
        for (auto& out : outputs) {
            // Base weight on domain expertise
            switch(out.domain) {
                case Domain::MEDICAL:
                case Domain::LEGAL:
                case Domain::PHYSICS:
                    out.epistemic_weight = 1.0f; // High specialization
                    break;
                case Domain::PHILOSOPHY:
                case Domain::LITERATURE:
                    out.epistemic_weight = 0.8f; // Interpretative
                    break;
                case Domain::GENERAL:
                    out.epistemic_weight = 0.5f; // General knowledge
                    break;
                default:
                    out.epistemic_weight = 0.7f;
            }
        }
    }
    
    // Check for negative markers in text
    bool contains_negative_marker(const std::string& text) {
        std::vector<std::string> markers = {"لا", "ليس", "خطأ", "غير صحيح", "never", "not", "false"};
        for (const auto& marker : markers) {
            if (text.find(marker) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

} // namespace asm_core
