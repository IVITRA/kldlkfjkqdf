#pragma once
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <algorithm>
#include <cmath>
#include "context_protocol.h"

namespace asm_core {

// Expert Validation and Meta-Expert System
class ExpertValidator {
public:
    struct ValidationReport {
        std::string expert_id;
        bool language_coherence;
        bool fact_consistency;
        bool temporal_awareness;
        bool no_hallucination;
        float redundancy_score; // 0.0 = unique, 1.0 = duplicate
        bool is_valid;
        std::vector<std::string> warnings;
    };
    
    // Validate expert quality
    ValidationReport validate_expert(
        const std::string& expert_id,
        Domain domain,
        const std::vector<std::string>& test_outputs,
        int training_year) {
        
        ValidationReport report;
        report.expert_id = expert_id;
        report.is_valid = true;
        
        // 1. Language Coherence Test
        report.language_coherence = test_language_coherence(test_outputs, domain);
        if (!report.language_coherence) {
            report.warnings.push_back("⚠️  Poor language coherence detected");
            report.is_valid = false;
        }
        
        // 2. Fact Consistency (against known knowledge base)
        report.fact_consistency = test_fact_consistency(test_outputs, domain);
        if (!report.fact_consistency) {
            report.warnings.push_back("⚠️  Factual inconsistencies detected");
            report.is_valid = false;
        }
        
        // 3. Temporal Awareness
        report.temporal_awareness = test_temporal_awareness(training_year);
        if (!report.temporal_awareness) {
            report.warnings.push_back("⚠️  Outdated knowledge (training year: " + 
                                     std::to_string(training_year) + ")");
        }
        
        // 4. Hallucination Check
        report.no_hallucination = test_no_hallucination(test_outputs);
        if (!report.no_hallucination) {
            report.warnings.push_back("⚠️  Potential hallucinations detected");
            report.is_valid = false;
        }
        
        // 5. Redundancy Score
        report.redundancy_score = calculate_redundancy(expert_id, test_outputs);
        if (report.redundancy_score > 0.9f) {
            report.warnings.push_back("⚠️  High redundancy (" + 
                                     std::to_string((int)(report.redundancy_score * 100)) + 
                                     "%) - consider merging with similar expert");
        }
        
        return report;
    }
    
    // Meta-Expert: Reviews and validates other experts' outputs
    struct MetaExpertReview {
        bool approved;
        float quality_score;
        std::string feedback;
    };
    
    MetaExpertReview meta_review(
        const std::string& expert_output,
        Domain domain,
        float confidence) {
        
        MetaExpertReview review;
        review.quality_score = 0.5f; // Start neutral
        
        // Check logical consistency
        if (has_logical_fallacies(expert_output)) {
            review.quality_score -= 0.3f;
            review.feedback += "Logical inconsistencies detected. ";
        }
        
        // Check confidence calibration
        if (confidence > 0.9f && contains_uncertainty_markers(expert_output)) {
            review.quality_score -= 0.2f;
            review.feedback += "Overconfident despite uncertainty markers. ";
        }
        
        // Check domain appropriateness
        if (!is_domain_appropriate(expert_output, domain)) {
            review.quality_score -= 0.2f;
            review.feedback += "Content may not match domain expertise. ";
        }
        
        review.approved = (review.quality_score >= 0.6f);
        
        if (review.approved) {
            review.feedback = "✅ Approved: " + review.feedback;
        } else {
            review.feedback = "❌ Rejected: " + review.feedback;
        }
        
        return review;
    }
    
private:
    // Test language coherence
    bool test_language_coherence(const std::vector<std::string>& outputs, Domain domain) {
        if (outputs.empty()) return false;
        
        int coherent_count = 0;
        for (const auto& output : outputs) {
            // Check for basic grammatical structure
            if (has_proper_sentence_structure(output)) {
                coherent_count++;
            }
        }
        
        return (coherent_count / (float)outputs.size()) > 0.8f;
    }
    
    // Test fact consistency
    bool test_fact_consistency(const std::vector<std::string>& outputs, Domain domain) {
        // In production, check against Wikidata or knowledge graph
        // Simplified: check for internal contradictions
        for (size_t i = 0; i < outputs.size(); ++i) {
            for (size_t j = i + 1; j < outputs.size(); ++j) {
                if (are_contradictory(outputs[i], outputs[j])) {
                    return false;
                }
            }
        }
        return true;
    }
    
    // Test temporal awareness
    bool test_temporal_awareness(int training_year) {
        int current_year = 2026;
        return (current_year - training_year) <= 5; // Max 5 years old
    }
    
    // Test for hallucinations
    bool test_no_hallucination(const std::vector<std::string>& outputs) {
        // Check for uncited claims in specialized domains
        for (const auto& output : outputs) {
            if (contains_unverifiable_claim(output)) {
                return false;
            }
        }
        return true;
    }
    
    // Calculate redundancy against other experts
    float calculate_redundancy(const std::string& expert_id, 
                              const std::vector<std::string>& outputs) {
        // In production, compare with all other experts
        // Simplified: check internal consistency
        if (outputs.size() < 2) return 0.0f;
        
        float total_similarity = 0.0f;
        int comparisons = 0;
        
        for (size_t i = 0; i < outputs.size(); ++i) {
            for (size_t j = i + 1; j < outputs.size(); ++j) {
                total_similarity += text_similarity(outputs[i], outputs[j]);
                comparisons++;
            }
        }
        
        return comparisons > 0 ? total_similarity / comparisons : 0.0f;
    }
    
    // Helper functions
    bool has_proper_sentence_structure(const std::string& text) {
        return text.length() > 10 && 
               (text.back() == '.' || text.back() == '!' || text.back() == '?');
    }
    
    bool are_contradictory(const std::string& a, const std::string& b) {
        // Simple contradiction detection
        bool a_positive = !contains_negative_marker(a);
        bool b_positive = !contains_negative_marker(b);
        return (a_positive != b_positive) && 
               text_similarity(a, b) > 0.5f;
    }
    
    bool contains_negative_marker(const std::string& text) {
        std::vector<std::string> markers = {"لا", "ليس", "خطأ", "never", "not"};
        for (const auto& marker : markers) {
            if (text.find(marker) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
    
    bool contains_unverifiable_claim(const std::string& text) {
        // Check for claims without citations in specialized domains
        std::vector<std::string> claim_markers = {"according to", "研究显示", "أظهرت الدراسات"};
        bool has_claim = false;
        bool has_citation = false;
        
        for (const auto& marker : claim_markers) {
            if (text.find(marker) != std::string::npos) {
                has_claim = true;
                break;
            }
        }
        
        // Check for citations
        if (text.find("[") != std::string::npos && 
            text.find("]") != std::string::npos) {
            has_citation = true;
        }
        
        return has_claim && !has_citation;
    }
    
    bool has_logical_fallacies(const std::string& text) {
        std::vector<std::string> fallacy_markers = {
            "therefore it must be", "obviously", "everyone knows"
        };
        for (const auto& marker : fallacy_markers) {
            if (text.find(marker) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
    
    bool contains_uncertainty_markers(const std::string& text) {
        std::vector<std::string> uncertainty = {"maybe", "perhaps", "might", "قد", "ربما"};
        for (const auto& marker : uncertainty) {
            if (text.find(marker) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
    
    bool is_domain_appropriate(const std::string& text, Domain domain) {
        // Simplified check - in production, use domain-specific keyword matching
        return !text.empty();
    }
    
    float text_similarity(const std::string& a, const std::string& b) {
        // Simple Jaccard similarity
        std::set<char> set_a(a.begin(), a.end());
        std::set<char> set_b(b.begin(), b.end());
        
        std::vector<char> intersection;
        std::set_intersection(set_a.begin(), set_a.end(),
                            set_b.begin(), set_b.end(),
                            std::back_inserter(intersection));
        
        float union_size = set_a.size() + set_b.size() - intersection.size();
        return union_size > 0 ? intersection.size() / union_size : 0.0f;
    }
};

} // namespace asm_core
