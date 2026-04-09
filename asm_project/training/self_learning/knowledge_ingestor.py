#!/usr/bin/env python3
"""
Knowledge Ingestor - Converts web content into training samples for ASM

This module takes scraped web content and converts it into structured
training data that can be used to create or update ASM experts.

Features:
- Atomic fact extraction using spaCy
- Question generation with T5 model
- Training sample creation with metadata
- Domain detection and source tracking
"""

import spacy
from transformers import T5Tokenizer, T5ForConditionalGeneration
import json
import hashlib
from datetime import datetime
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass, asdict
import re

@dataclass
class TrainingSample:
    """Represents a single training sample"""
    question: str
    answer: str
    domain: str
    source_url: str
    source_title: str
    difficulty: str  # easy, medium, hard
    facts: List[str]
    timestamp: str
    sample_id: str


class KnowledgeIngestor:
    """
    Converts raw web content into structured training samples
    
    Pipeline:
    1. Text cleaning and normalization
    2. Atomic fact extraction (spaCy)
    3. Question generation (T5)
    4. Domain classification
    5. Training sample creation
    """
    
    def __init__(self, model_name='t5-small'):
        """
        Initialize knowledge ingestor
        
        Args:
            model_name: T5 model name for question generation
        """
        print("🔄 Loading spaCy model...")
        try:
            self.nlp = spacy.load("en_core_web_sm")
        except OSError:
            print("⚠️  spaCy model not found. Run: python -m spacy download en_core_web_sm")
            self.nlp = None
        
        print(f"🔄 Loading T5 model ({model_name})...")
        self.tokenizer = T5Tokenizer.from_pretrained(model_name)
        self.qg_model = T5ForConditionalGeneration.from_pretrained(model_name)
        
        print("✅ Knowledge Ingestor ready!")
    
    def ingest_content(self, text: str, source_url: str, source_title: str,
                      domain: Optional[str] = None) -> List[TrainingSample]:
        """
        Main pipeline: Convert web content to training samples
        
        Args:
            text: Raw text content
            source_url: Source URL
            source_title: Page title
            domain: Optional domain label (medical, legal, etc.)
        
        Returns:
            List of training samples
        """
        print(f"\n📥 Ingesting content from: {source_url}")
        print(f"   Text length: {len(text)} characters")
        
        # Step 1: Clean and normalize
        cleaned_text = self._clean_text(text)
        print(f"   Cleaned length: {len(cleaned_text)} characters")
        
        # Step 2: Extract atomic facts
        facts = self._extract_facts(cleaned_text)
        print(f"   Extracted {len(facts)} atomic facts")
        
        # Step 3: Detect domain if not provided
        if domain is None:
            domain = self._detect_domain(cleaned_text)
            print(f"   Detected domain: {domain}")
        
        # Step 4: Generate questions
        samples = self._generate_questions(cleaned_text, facts, domain, 
                                          source_url, source_title)
        print(f"   Generated {len(samples)} training samples")
        
        return samples
    
    def _clean_text(self, text: str) -> str:
        """Clean and normalize text"""
        # Remove extra whitespace
        text = re.sub(r'\s+', ' ', text)
        
        # Remove URLs
        text = re.sub(r'http\S+', '', text)
        
        # Remove special characters but keep punctuation
        text = re.sub(r'[^\w\s.,!?;:\-\']', '', text)
        
        # Normalize quotes
        text = text.replace('"', '"').replace('"', '"')
        text = text.replace(''', "'").replace(''', "'")
        
        return text.strip()
    
    def _extract_facts(self, text: str) -> List[str]:
        """
        Extract atomic facts using spaCy NER and dependency parsing
        
        Returns:
            List of atomic fact strings
        """
        if self.nlp is None:
            # Fallback: simple sentence extraction
            sentences = [s.strip() for s in text.split('.') if len(s.strip()) > 20]
            return sentences[:50]
        
        doc = self.nlp(text)
        facts = []
        
        # Extract named entities
        for ent in doc.ents:
            if ent.label_ in ['PERSON', 'ORG', 'GPE', 'DATE', 'EVENT']:
                fact = f"{ent.text} ({ent.label_})"
                facts.append(fact)
        
        # Extract key sentences (containing important information)
        for sent in doc.sents:
            sent_text = sent.text.strip()
            
            # Filter short or irrelevant sentences
            if len(sent_text) < 30:
                continue
            
            # Prioritize sentences with specific patterns
            if any(keyword in sent_text.lower() for keyword in 
                  ['is', 'are', 'was', 'were', 'defines', 'causes', 'creates',
                   'proves', 'shows', 'demonstrates', 'results in']):
                facts.append(sent_text)
        
        # Extract subject-verb-object triples
        for sent in doc.sents:
            triples = self._extract_triples(sent)
            facts.extend(triples)
        
        # Remove duplicates and limit
        unique_facts = list(dict.fromkeys(facts))
        return unique_facts[:100]
    
    def _extract_triples(self, sentence) -> List[str]:
        """Extract subject-verb-object triples from a sentence"""
        triples = []
        
        for token in sentence:
            # Look for root verbs
            if token.pos_ == 'VERB' and token.dep_ == 'ROOT':
                subject = ""
                obj = ""
                
                # Find subject
                for child in token.children:
                    if child.dep_ in ['nsubj', 'nsubjpass']:
                        subject = child.text
                        # Include compound words
                        for grandchild in child.children:
                            if grandchild.dep_ == 'compound':
                                subject = grandchild.text + " " + subject
                
                # Find object
                for child in token.children:
                    if child.dep_ in ['dobj', 'attr', 'pobj']:
                        obj = child.text
                        for grandchild in child.children:
                            if grandchild.dep_ == 'compound':
                                obj = grandchild.text + " " + obj
                
                if subject and obj:
                    triple = f"{subject} {token.text} {obj}"
                    triples.append(triple)
        
        return triples
    
    def _detect_domain(self, text: str) -> str:
        """Detect domain from text content"""
        text_lower = text.lower()
        
        # Domain keywords
        domain_keywords = {
            'medical': ['patient', 'disease', 'treatment', 'symptom', 'diagnosis',
                       'medicine', 'clinical', 'therapy', 'drug', 'hospital'],
            'legal': ['law', 'court', 'judge', 'legal', 'statute', 'regulation',
                     'legislation', 'compliance', 'attorney', 'litigation'],
            'physics': ['quantum', 'particle', 'energy', 'force', 'momentum',
                       'relativity', 'thermodynamics', 'electromagnetic'],
            'mathematics': ['equation', 'theorem', 'proof', 'integral', 'derivative',
                          'algebra', 'geometry', 'calculus', 'matrix'],
            'programming': ['code', 'function', 'algorithm', 'software', 'programming',
                          'compiler', 'debug', 'api', 'database'],
            'history': ['century', 'war', 'empire', 'king', 'revolution',
                       'ancient', 'medieval', 'modern era', 'dynasty'],
            'philosophy': ['philosophy', 'ethics', 'metaphysics', 'epistemology',
                          'logic', 'consciousness', 'existence', 'knowledge']
        }
        
        # Count keyword matches
        domain_scores = {}
        for domain, keywords in domain_keywords.items():
            score = sum(1 for keyword in keywords if keyword in text_lower)
            domain_scores[domain] = score
        
        # Return domain with highest score
        if max(domain_scores.values()) > 0:
            return max(domain_scores, key=domain_scores.get)
        
        return 'general'
    
    def _generate_questions(self, text: str, facts: List[str], domain: str,
                           source_url: str, source_title: str) -> List[TrainingSample]:
        """
        Generate question-answer pairs from text and facts
        
        Uses T5 model trained for question generation
        """
        samples = []
        
        # Split text into chunks for processing
        chunks = self._split_into_chunks(text, max_length=500)
        
        for i, chunk in enumerate(chunks):
            # Generate questions using T5
            questions = self._t5_generate_questions(chunk)
            
            for question in questions:
                # Find answer in text (simple extraction)
                answer = self._find_answer(question, chunk)
                
                if answer and len(answer) > 10:
                    # Create training sample
                    sample_id = self._generate_sample_id(question, source_url)
                    difficulty = self._assess_difficulty(question, answer)
                    
                    sample = TrainingSample(
                        question=question,
                        answer=answer,
                        domain=domain,
                        source_url=source_url,
                        source_title=source_title,
                        difficulty=difficulty,
                        facts=facts[:5],  # Keep top 5 facts
                        timestamp=datetime.now().isoformat(),
                        sample_id=sample_id
                    )
                    
                    samples.append(sample)
        
        # Also create fact-based QA pairs
        for fact in facts[:20]:  # Limit to top 20 facts
            question = self._fact_to_question(fact)
            if question:
                sample_id = self._generate_sample_id(question, source_url)
                
                sample = TrainingSample(
                    question=question,
                    answer=fact,
                    domain=domain,
                    source_url=source_url,
                    source_title=source_title,
                    difficulty='easy',
                    facts=[fact],
                    timestamp=datetime.now().isoformat(),
                    sample_id=sample_id
                )
                
                samples.append(sample)
        
        return samples
    
    def _split_into_chunks(self, text: str, max_length: int = 500) -> List[str]:
        """Split text into manageable chunks"""
        sentences = [s.strip() + '.' for s in text.split('.') if s.strip()]
        
        chunks = []
        current_chunk = ""
        
        for sentence in sentences:
            if len(current_chunk) + len(sentence) > max_length:
                if current_chunk:
                    chunks.append(current_chunk.strip())
                current_chunk = sentence
            else:
                current_chunk += " " + sentence
        
        if current_chunk:
            chunks.append(current_chunk.strip())
        
        return chunks
    
    def _t5_generate_questions(self, text: str) -> List[str]:
        """
        Generate questions using T5 model
        
        Format: "generate question: <statement>" -> "<question>"
        """
        questions = []
        
        # Prepare input for T5
        sentences = [s.strip() for s in text.split('.') if len(s.strip()) > 20]
        
        for sentence in sentences[:10]:  # Limit to 10 sentences per chunk
            input_text = f"generate question: {sentence}"
            
            inputs = self.tokenizer(input_text, return_tensors="pt", max_length=512, 
                                   truncation=True)
            
            outputs = self.qg_model.generate(
                inputs['input_ids'],
                max_length=64,
                num_beams=4,
                early_stopping=True,
                num_return_sequences=1
            )
            
            question = self.tokenizer.decode(outputs[0], skip_special_tokens=True)
            
            if question and '?' in question:
                questions.append(question)
        
        return questions
    
    def _find_answer(self, question: str, context: str) -> Optional[str]:
        """
        Simple answer extraction from context
        (In production, use a proper QA model like BERT)
        """
        # Extract key words from question
        question_words = set(question.lower().split())
        stop_words = {'what', 'when', 'where', 'who', 'why', 'how', 'is', 'are', 
                     'the', 'a', 'an', 'of', 'in', 'to', 'for'}
        question_words -= stop_words
        
        # Find sentence with most keyword overlap
        sentences = [s.strip() for s in context.split('.') if s.strip()]
        
        best_sentence = None
        best_score = 0
        
        for sentence in sentences:
            sentence_words = set(sentence.lower().split())
            overlap = len(question_words & sentence_words)
            
            if overlap > best_score:
                best_score = overlap
                best_sentence = sentence
        
        return best_sentence if best_score > 0 else None
    
    def _assess_difficulty(self, question: str, answer: str) -> str:
        """Assess difficulty level of a QA pair"""
        # Simple heuristic based on length and complexity
        question_len = len(question.split())
        answer_len = len(answer.split())
        
        # Count complex words (more than 3 syllables)
        complex_words = sum(1 for word in answer.split() 
                          if len(word) > 8)
        
        score = question_len + answer_len + (complex_words * 2)
        
        if score < 30:
            return 'easy'
        elif score < 60:
            return 'medium'
        else:
            return 'hard'
    
    def _fact_to_question(self, fact: str) -> Optional[str]:
        """Convert a fact statement into a question"""
        # Simple template-based conversion
        if ' is ' in fact:
            parts = fact.split(' is ', 1)
            return f"What is {parts[0]}?"
        elif ' are ' in fact:
            parts = fact.split(' are ', 1)
            return f"What are {parts[0]}?"
        elif ' was ' in fact:
            parts = fact.split(' was ', 1)
            return f"What was {parts[0]}?"
        elif ' causes ' in fact:
            parts = fact.split(' causes ', 1)
            return f"What does {parts[0]} cause?"
        elif ' defines ' in fact:
            parts = fact.split(' defines ', 1)
            return f"What does {parts[0]} define?"
        
        return None
    
    def _generate_sample_id(self, question: str, source_url: str) -> str:
        """Generate unique sample ID"""
        content = f"{question}:{source_url}"
        return hashlib.md5(content.encode()).hexdigest()[:12]
    
    def save_samples(self, samples: List[TrainingSample], output_file: str):
        """Save training samples to JSON file"""
        samples_dict = [asdict(s) for s in samples]
        
        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(samples_dict, f, indent=2, ensure_ascii=False)
        
        print(f"✅ Saved {len(samples)} samples to {output_file}")


if __name__ == "__main__":
    # Example usage
    print("=" * 60)
    print("Knowledge Ingestor - Test Mode")
    print("=" * 60)
    
    ingestor = KnowledgeIngestor()
    
    # Sample text
    sample_text = """
    Photosynthesis is the process by which green plants and some other organisms 
    use sunlight to synthesize foods with the help of chlorophyll pigments. 
    During photosynthesis, plants take in carbon dioxide and water, which are 
    converted into glucose and oxygen. This process occurs in the chloroplasts 
    of plant cells. Photosynthesis is essential for life on Earth as it produces 
    oxygen and forms the base of the food chain.
    """
    
    samples = ingestor.ingest_content(
        text=sample_text,
        source_url="https://example.com/photosynthesis",
        source_title="Understanding Photosynthesis",
        domain="biology"
    )
    
    print(f"\n📊 Generated {len(samples)} training samples:")
    for i, sample in enumerate(samples[:5], 1):
        print(f"\n{i}. [{sample.difficulty}] {sample.question}")
        print(f"   A: {sample.answer[:80]}...")
    
    # Save to file
    ingestor.save_samples(samples, "test_samples.json")
    print("\n✅ Test complete!")
