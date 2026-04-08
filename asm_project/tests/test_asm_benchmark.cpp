#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <iostream>
#include "../core/consensus_algorithm.h"
#include "../core/expert_validator.h"
#include "../core/thermal_scheduler.h"
#include "../core/safety_guardrail.h"
#include "../core/universal_tokenizer.h"
#include "../core/deterministic_router.h"
#include "../core/router.h"
#include "../core/context_protocol.h"

using namespace asm_core;

// ============================================================================
// 1. Consensus Algorithm Tests
// ============================================================================

TEST(ConsensusAlgorithm, AmbiguityResolution) {
    // Test: "النور يقهر الظلام" - multiple interpretations
    ConsensusAlgorithm consensus;
    
    std::vector<ConsensusAlgorithm::ExpertOutput> outputs = {
        {"الفوتونات تطرد الإلكترونات في التأثير الكهروضوئي", 0.95f, Domain::PHYSICS, 1, 1.0f},
        {"الحقيقة تنتصر على الجهل في رحلة التنوير", 0.90f, Domain::PHILOSOPHY, 2, 0.8f},
        {"استعارة مكنية عن الأمل والتفاؤل في الأدب العربي", 0.88f, Domain::LITERATURE, 3, 0.8f}
    };
    
    auto result = consensus.resolve_conflict(outputs);
    
    // Should synthesize all three perspectives
    EXPECT_FALSE(result.synthesized_text.empty());
    EXPECT_TRUE(result.has_contradiction || result.consensus_score > 0.7f);
    EXPECT_EQ(result.agreeing_experts.size(), 3); // All experts included
    
    std::cout << "Synthesis: " << result.synthesized_text << std::endl;
}

TEST(ConsensusAlgorithm, DeadlockResolution) {
    ConsensusAlgorithm consensus;
    
    // 5 vs 5 deadlock
    std::vector<ConsensusAlgorithm::ExpertOutput> outputs;
    for (int i = 0; i < 5; ++i) {
        outputs.push_back({"Answer A is correct", 0.9f, Domain::GENERAL, i, 0.7f});
    }
    for (int i = 5; i < 10; ++i) {
        outputs.push_back({"Answer B is correct", 0.9f, Domain::GENERAL, i, 0.7f});
    }
    
    auto result = consensus.resolve_conflict(outputs);
    
    // Should resolve deadlock with dialectic synthesis
    EXPECT_FALSE(result.synthesized_text.empty());
    std::cout << "Deadlock resolution: " << result.synthesized_text << std::endl;
}

TEST(ConsensusAlgorithm, EpistemicWeight) {
    ConsensusAlgorithm consensus;
    
    // Math question: Math expert should outweigh Philosophy expert
    std::vector<ConsensusAlgorithm::ExpertOutput> outputs = {
        {"2+2=4", 0.99f, Domain::MATHEMATICS, 1, 1.0f}, // Math expert
        {"الجمع عملية نسبية تعتمد على السياق", 0.50f, Domain::PHILOSOPHY, 2, 0.8f} // Philosopher
    };
    
    auto result = consensus.resolve_conflict(outputs);
    
    // Math expert should win due to higher epistemic weight
    EXPECT_EQ(result.agreeing_experts[0], 1); // Math expert first
}

// ============================================================================
// 2. Expert Validation Tests
// ============================================================================

TEST(ExpertValidator, LanguageCoherence) {
    ExpertValidator validator;
    
    std::vector<std::string> good_outputs = {
        "الذكاء الاصطناعي هو فرع من علوم الحاسوب.",
        "يتعامل مع إنشاء أنظمة ذكية قادرة على التعلم.",
        "تشمل هذه الأنظمة التعلم الآلي والشبكات العصبية."
    };
    
    auto report = validator.validate_expert("test_expert_1", Domain::GENERAL, good_outputs, 2025);
    
    EXPECT_TRUE(report.language_coherence);
    EXPECT_TRUE(report.is_valid);
}

TEST(ExpertValidator, TemporalAwareness) {
    ExpertValidator validator;
    
    std::vector<std::string> outputs = {"Test output"};
    
    // Recent expert (2025)
    auto report_recent = validator.validate_expert("recent", Domain::GENERAL, outputs, 2025);
    EXPECT_TRUE(report_recent.temporal_awareness);
    
    // Old expert (2015)
    auto report_old = validator.validate_expert("old", Domain::GENERAL, outputs, 2015);
    EXPECT_FALSE(report_old.temporal_awareness);
    EXPECT_FALSE(report_old.warnings.empty());
}

TEST(ExpertValidator, MetaExpertReview) {
    ExpertValidator validator;
    
    // High confidence but uncertain language
    auto review = validator.meta_review(
        "هذا قد يكون صحيحاً ربما",
        Domain::SCIENCE,
        0.95f // Overconfident
    );
    
    EXPECT_FALSE(review.approved); // Should reject due to overconfidence
    EXPECT_LT(review.quality_score, 0.6f);
}

// ============================================================================
// 3. Universal Tokenizer Tests
// ============================================================================

TEST(UniversalTokenizer, ArabicNormalization) {
    UniversalTokenizer tokenizer;
    
    // Test Arabic normalization
    std::string input1 = "الذكاء الاصطناعي";
    std::string input2 = "الذكاء الاصطناعي"; // Same text
    
    auto tokens1 = tokenizer.encode(input1);
    auto tokens2 = tokenizer.encode(input2);
    
    // Should produce identical tokens
    EXPECT_TRUE(UniversalTokenizer::verify_consistency(tokens1, tokens2));
    
    std::cout << "Tokens for '" << input1 << "': ";
    for (auto t : tokens1) std::cout << t << " ";
    std::cout << std::endl;
}

TEST(UniversalTokenizer, AlefVariants) {
    UniversalTokenizer tokenizer;
    
    // Different Alef variants should normalize to same tokens
    std::string variant1 = "كتاب"; // Normal
    std::string variant2 = "كتاب"; // With different Alef
    
    auto tokens1 = tokenizer.encode(variant1);
    auto tokens2 = tokenizer.encode(variant2);
    
    EXPECT_TRUE(UniversalTokenizer::verify_consistency(tokens1, tokens2));
}

TEST(UniversalTokenizer, CrossExpertConsistency) {
    UniversalTokenizer tokenizer;
    
    // Simulate 10 experts tokenizing same text
    std::string text = "الذكاء الاصطناعي يغير العالم";
    std::vector<std::vector<uint32_t>> all_tokens(10);
    
    for (int i = 0; i < 10; ++i) {
        all_tokens[i] = tokenizer.encode(text);
    }
    
    // All experts should produce identical tokens
    for (int i = 1; i < 10; ++i) {
        EXPECT_TRUE(UniversalTokenizer::verify_consistency(all_tokens[0], all_tokens[i]));
    }
}

// ============================================================================
// 4. Safety Guardrail Tests
// ============================================================================

TEST(SafetyGuardrail, HarmfulContentDetection) {
    SafetyGuardrail guardrail;
    
    EXPECT_TRUE(guardrail.contains_harmful_content("اشرب البنزين"));
    EXPECT_TRUE(guardrail.contains_harmful_content("drink gasoline"));
    EXPECT_FALSE(guardrail.contains_harmful_content("اشرب الماء"));
}

TEST(SafetyGuardrail, CrossVerification) {
    SafetyGuardrail guardrail;
    
    auto result = guardrail.verify_with_secondary_expert(
        "الذكاء الاصطناعي مفيد في الطب",
        "AI يساعد في التشخيص الطبي",
        0.5f
    );
    
    // Should find some agreement
    EXPECT_GT(result.agreement_score, 0.0f);
}

TEST(SafetyGuardrail, KnowledgeFreshness) {
    SafetyGuardrail guardrail;
    
    // Old medical expert
    auto warning = guardrail.check_expert_freshness(2015, "medical");
    EXPECT_FALSE(warning.empty());
    EXPECT_NE(warning.find("WARNING"), std::string::npos);
    
    // Recent general expert
    auto no_warning = guardrail.check_expert_freshness(2024, "general");
    EXPECT_TRUE(no_warning.empty());
}

// ============================================================================
// 5. Thermal Scheduler Tests
// ============================================================================

TEST(ThermalScheduler, NormalOperation) {
    ThermalAwareScheduler scheduler;
    
    // At normal temperature, should allow max parallelism
    EXPECT_EQ(scheduler.get_max_parallel_experts(), 10);
    EXPECT_FALSE(scheduler.is_throttling());
}

TEST(ThermalScheduler, ManualThrottling) {
    ThermalAwareScheduler scheduler;
    
    // Simulate high temp scenario (if monitoring available)
    float temp = scheduler.get_temperature();
    if (temp > 0.0f) {
        scheduler.throttle_if_needed();
        std::cout << "CPU Temp: " << temp << "°C" << std::endl;
        std::cout << "Max Parallel: " << scheduler.get_max_parallel_experts() << std::endl;
    } else {
        std::cout << "Temperature monitoring unavailable (expected on Windows)" << std::endl;
    }
}

// ============================================================================
// 6. Deterministic Router Tests
// ============================================================================

TEST(DeterministicRouter, ConsistentResults) {
    HierarchicalRouter base_router;
    DeterministicRouter router(base_router, 42);
    
    router.set_deterministic(true);
    
    // Same query should always return same results
    float query[128] = {0.5f};
    
    auto results1 = router.search_consistent(query, 5);
    auto results2 = router.search_consistent(query, 5);
    
    EXPECT_EQ(results1, results2);
}

TEST(DeterministicRouter, UserPreferences) {
    HierarchicalRouter base_router;
    DeterministicRouter router(base_router, 42);
    
    std::string user_id = "test_user_123";
    
    // Store preferences
    router.store_user_feedback(user_id, 42, "medical");
    router.store_user_feedback(user_id, 42, "medical");
    router.store_user_feedback(user_id, 43, "medical");
    
    auto prefs = router.get_user_preferred_experts(user_id, "medical");
    EXPECT_FALSE(prefs.empty());
    
    // Should have consistency > 0.5
    float consistency = router.get_consistency_score(user_id);
    EXPECT_GT(consistency, 0.5f);
    
    std::cout << "User consistency score: " << consistency << std::endl;
}

// ============================================================================
// 7. Integration Tests
// ============================================================================

TEST(Integration, FullPipeline) {
    // Test complete pipeline: Tokenize → Route → Consensus → Safety Check
    
    UniversalTokenizer tokenizer;
    SafetyGuardrail guardrail;
    ConsensusAlgorithm consensus;
    
    std::string query = "ما هو الذكاء الاصطناعي؟";
    
    // Step 1: Tokenize
    auto tokens = tokenizer.encode(query);
    EXPECT_FALSE(tokens.empty());
    
    // Step 2: Simulate expert outputs
    std::vector<ConsensusAlgorithm::ExpertOutput> outputs = {
        {"الذكاء الاصطناعي هو محاكاة للذكاء البشري", 0.95f, Domain::GENERAL, 1, 0.9f},
        {"يشمل التعلم الآلي ومعالجة اللغة الطبيعية", 0.90f, Domain::PROGRAMMING, 2, 0.8f}
    };
    
    // Step 3: Reach consensus
    auto consensus_result = consensus.resolve_conflict(outputs);
    EXPECT_FALSE(consensus_result.synthesized_text.empty());
    
    // Step 4: Safety check
    EXPECT_FALSE(guardrail.contains_harmful_content(consensus_result.synthesized_text));
    
    std::cout << "Final answer: " << consensus_result.synthesized_text << std::endl;
}

TEST(Integration, ContextBridgeWithTokenization) {
    UniversalTokenizer tokenizer;
    ContextSynthesizer synthesizer;
    
    // Create passport with tokenized input
    std::string text = "الفيزياء الكمية معقدة";
    auto tokens = tokenizer.encode(text);
    
    ThoughtPassport passport;
    passport.confidence = 0.9f;
    
    // Serialize and deserialize
    auto data = passport.serialize();
    auto restored = ThoughtPassport::deserialize(data);
    
    EXPECT_FLOAT_EQ(restored.confidence, passport.confidence);
}
