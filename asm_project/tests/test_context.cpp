#include <gtest/gtest.h>
#include "../core/context_protocol.h"

using namespace asm_core;

TEST(ContextProtocolTest, PassportSerialization) {
    ThoughtPassport passport;
    passport.confidence = 0.95f;
    passport.source_expert_id = 42;
    
    // Fill intent with some values
    for (int i = 0; i < 128; ++i) {
        passport.intent[i] = static_cast<float>(i) / 128.0f;
    }
    
    // Serialize
    auto data = passport.serialize();
    
    // Deserialize
    auto deserialized = ThoughtPassport::deserialize(data);
    
    // Check critical fields
    EXPECT_FLOAT_EQ(deserialized.confidence, passport.confidence);
    EXPECT_EQ(deserialized.source_expert_id, passport.source_expert_id);
}

TEST(ContextProtocolTest, InterlinguaTranslation) {
    InterlinguaTranslator translator;
    
    ThoughtPassport input;
    input.confidence = 1.0f;
    input.source_expert_id = 1;
    
    // Translate from Medical to Physics
    auto output = translator.translate(input, Domain::MEDICAL, Domain::PHYSICS);
    
    // Should still be valid
    EXPECT_GT(output.confidence, 0.0f);
}

TEST(ContextProtocolTest, Synthesizer) {
    ContextSynthesizer synthesizer;
    
    std::vector<ContextSynthesizer::ExpertOutput> outputs;
    
    ContextSynthesizer::ExpertOutput out1;
    out1.text_fragment = "Expert 1 output";
    out1.weight = 0.8f;
    out1.passport.confidence = 0.9f;
    outputs.push_back(out1);
    
    ContextSynthesizer::ExpertOutput out2;
    out2.text_fragment = "Expert 2 output";
    out2.weight = 0.6f;
    out2.passport.confidence = 0.7f;
    outputs.push_back(out2);
    
    auto result = synthesizer.synthesize_final(outputs, "Test query");
    
    EXPECT_FALSE(result.empty());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
