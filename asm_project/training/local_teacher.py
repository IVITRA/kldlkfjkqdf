"""
Local Teacher Model - Generates QA pairs for supervised training
Uses a local LLM (13B-70B) as teacher instead of GPT-4
Privacy-first: No internet connection required
"""

from transformers import pipeline
import torch
from typing import List, Dict
import re


class LocalTeacherModel:
    """
    Uses a local large model (13B-70B) as teacher
    for distillation into smaller experts (1B-2B)
    """
    
    def __init__(self, model_path: str = "TheBloke/Llama-2-13B-GPTQ",
                 use_4bit: bool = True):
        """
        Args:
            model_path: Compressed model (GPTQ or AWQ) that fits on one GPU
            use_4bit: Use 4-bit quantization for memory efficiency
        """
        print(f"Loading teacher model: {model_path}")
        
        self.generator = pipeline(
            "text-generation",
            model=model_path,
            model_kwargs={
                "torch_dtype": torch.float16,
                "load_in_4bit": use_4bit
            },
            device_map="auto",
        )
        
        print(f"✓ Teacher model loaded successfully")
        
    def generate_training_pairs(self, chunks: List[Dict]) -> List[Dict]:
        """
        Generates (question, answer) pairs from text chunks for supervised training
        """
        training_pairs = []
        
        print(f"\n📝 Generating QA pairs from {len(chunks)} chunks...")
        
        for i, chunk in enumerate(chunks):
            context = chunk['text']
            
            # Generate questions from this text
            prompt = self._create_qa_prompt(context)
            
            try:
                response = self.generator(
                    prompt,
                    max_new_tokens=300,
                    temperature=0.7,
                    do_sample=True,
                    top_p=0.9,
                    repetition_penalty=1.2
                )[0]['generated_text']
                
                # Parse QA pairs from response
                qa_pairs = self._parse_qa(response, context)
                training_pairs.extend(qa_pairs)
                
                if (i + 1) % 10 == 0:
                    print(f"  Processed {i + 1}/{len(chunks)} chunks...")
                    
            except Exception as e:
                print(f"  ⚠ Error processing chunk {i}: {e}")
                continue
        
        print(f"✓ Generated {len(training_pairs)} QA pairs")
        return training_pairs
    
    def generate_expert_evaluation(self, chunk: Dict, num_questions: int = 5) -> List[Dict]:
        """
        Generates evaluation questions for testing expert quality
        """
        context = chunk['text']
        
        prompt = f"""Based on the following text, generate {num_questions} challenging questions that test deep understanding:

Text: {context}

Generate questions that require:
1. Factual recall
2. Conceptual understanding
3. Application of knowledge
4. Critical thinking

Format:
Q1: [question]
A1: [answer]
Q2: [question]
A2: [answer]
...

Questions:"""

        response = self.generator(
            prompt,
            max_new_tokens=400,
            temperature=0.5,
            do_sample=True
        )[0]['generated_text']
        
        return self._parse_qa(response, context)
    
    def create_dialogue_dataset(self, chunks: List[Dict]) -> List[Dict]:
        """
        Creates multi-turn dialogue dataset for conversational training
        """
        dialogues = []
        
        for chunk in chunks:
            # Generate a conversation about this topic
            prompt = f"""Create a natural conversation between a student and expert about this topic:

Topic: {chunk['text']}

Format:
Student: [question]
Expert: [detailed answer]
Student: [follow-up question]
Expert: [detailed answer]

Make it educational and engaging. Include at least 3 turns.

Conversation:"""

            response = self.generator(
                prompt,
                max_new_tokens=500,
                temperature=0.8,
                do_sample=True
            )[0]['generated_text']
            
            # Parse dialogue turns
            turns = self._parse_dialogue(response)
            if turns:
                dialogues.append({
                    'turns': turns,
                    'source': chunk.get('source', ''),
                    'domain': chunk.get('domain', 'general')
                })
        
        print(f"✓ Created {len(dialogues)} dialogue samples")
        return dialogues
    
    def _create_qa_prompt(self, context: str) -> str:
        """
        Creates prompt for QA generation
        """
        return f"""Based on this text, generate 3 questions and their detailed answers:

Text: {context}

Instructions:
- Questions should be specific and test understanding
- Answers should be comprehensive and accurate
- Include both factual and conceptual questions

Format:
Q1: [question]
A1: [answer]
Q2: [question]
A2: [answer]
Q3: [question]
A3: [answer]

Questions:"""
    
    def _parse_qa(self, text: str, context: str) -> List[Dict]:
        """
        Extracts questions and answers from generated text
        """
        pairs = []
        lines = text.split('\n')
        
        current_q = None
        current_a = []
        
        for line in lines:
            line = line.strip()
            
            # Detect question
            if line.startswith('Q') or (line.endswith('؟') and '?' in line):
                # Save previous QA pair if exists
                if current_q and current_a:
                    pairs.append({
                        'input': current_q,
                        'output': ' '.join(current_a),
                        'context': context
                    })
                
                # Start new question
                current_q = line.split(':', 1)[-1].strip() if ':' in line else line
                current_a = []
            
            # Detect answer
            elif line.startswith('A') or line.startswith('Answer'):
                answer_text = line.split(':', 1)[-1].strip() if ':' in line else line
                if answer_text:
                    current_a.append(answer_text)
            
            # Continuation of answer
            elif current_q and line:
                current_a.append(line)
        
        # Don't forget the last pair
        if current_q and current_a:
            pairs.append({
                'input': current_q,
                'output': ' '.join(current_a),
                'context': context
            })
        
        return pairs
    
    def _parse_dialogue(self, text: str) -> List[Dict]:
        """
        Parses dialogue turns from generated text
        """
        turns = []
        lines = text.split('\n')
        
        for line in lines:
            line = line.strip()
            
            if line.startswith('Student:') or line.startswith('student:'):
                turns.append({
                    'role': 'student',
                    'text': line.split(':', 1)[-1].strip()
                })
            elif line.startswith('Expert:') or line.startswith('expert:'):
                turns.append({
                    'role': 'expert',
                    'text': line.split(':', 1)[-1].strip()
                })
        
        return turns


if __name__ == "__main__":
    # Example usage (requires GPU with sufficient VRAM)
    print("LocalTeacherModel ready!")
    print("\nExample usage:")
    print("""
    # Requires GPU with 8GB+ VRAM
    teacher = LocalTeacherModel(model_path="TheBloke/Llama-2-13B-GPTQ")
    
    chunks = [
        {'text': 'Quantum mechanics is a fundamental theory in physics...', 'domain': 'science'}
    ]
    
    # Generate QA pairs
    qa_pairs = teacher.generate_training_pairs(chunks)
    
    # Generate evaluation questions
    eval_questions = teacher.generate_expert_evaluation(chunks[0])
    
    # Create dialogue dataset
    dialogues = teacher.create_dialogue_dataset(chunks)
    """)
