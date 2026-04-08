"""
Semantic Chunking Engine - Intelligent Text Splitting
Cuts documents into meaningful chunks based on semantics, not just size
"""

import re
import numpy as np
from typing import List, Dict, Optional
from sentence_transformers import SentenceTransformer


class ExpertChunkingStrategy:
    """
    Chunks documents optimally for specialized experts
    Uses semantic similarity to find natural breakpoints
    """
    
    def __init__(self, chunk_size: int = 512, overlap: int = 50, 
                 model_name: str = 'paraphrase-multilingual-mpnet-base-v2'):
        self.chunk_size = chunk_size
        self.overlap = overlap
        self.embedder = SentenceTransformer(model_name)
        
        # Arabic continuation markers
        self.continuation_markers = [
            'هذا', 'هذه', 'ذلك', 'التي', 'الذي', 'كذلك', 'أيضاً', 'لذلك',
            'وهذا', 'وهذه', 'كما', 'بالإضافة', 'علاوة', 'ثم', 'بعد ذلك'
        ]
        
        # Domain keywords for automatic classification
        self.domain_keywords = {
            'medical': ['مريض', 'دواء', 'جراحة', 'علاج', 'طبيب', 'diagnosis', 
                       'symptom', 'hospital', 'medicine', 'prescription'],
            'legal': ['قانون', 'محكمة', 'عقد', 'دعوى', 'قاضي', 'constitution',
                     'law', 'court', 'contract', 'lawsuit', 'judge'],
            'programming': ['code', 'function', 'bug', 'python', 'برمجة', 'دالة',
                           'algorithm', 'variable', 'class', 'method', 'API'],
            'science': ['فيزياء', 'كيمياء', 'تجربة', 'نظرية', 'physics',
                       'chemistry', 'experiment', 'theory', 'hypothesis'],
            'history': ['تاريخ', 'حضارة', 'حرب', 'ثورة', 'history',
                       'civilization', 'war', 'revolution', 'empire'],
            'philosophy': ['فلسفة', 'منطق', 'أخلاق', 'philosophy',
                          'logic', 'ethics', 'metaphysics', 'epistemology'],
            'mathematics': ['رياضيات', 'معادلة', 'نظرية', 'mathematics',
                           'equation', 'theorem', 'calculus', 'algebra']
        }
        
    def create_semantic_chunks(self, documents: List[Dict]) -> List[Dict]:
        """
        Chunks based on meaning, not just size
        """
        chunks = []
        
        for doc in documents:
            text = self._extract_full_text(doc)
            
            if not text or len(text.strip()) == 0:
                continue
            
            # Step 1: Split by headings first (Hierarchical Splitting)
            sections = self._split_by_headings(text, doc)
            
            for section in sections:
                # Step 2: If section is too large, split it semantically
                if len(section['text']) > self.chunk_size * 2:
                    sub_chunks = self._semantic_split(section['text'])
                    chunks.extend(sub_chunks)
                else:
                    # Detect domain for this chunk
                    domain = self._detect_domain(section['text'])
                    
                    chunks.append({
                        'text': section['text'],
                        'heading': section.get('heading', ''),
                        'source': doc.get('source', ''),
                        'domain': domain,
                        'word_count': len(section['text'].split())
                    })
        
        print(f"✓ Created {len(chunks)} semantic chunks")
        return chunks
    
    def _semantic_split(self, text: str) -> List[Dict]:
        """
        Splits at topic shifts using semantic similarity
        """
        # Split into sentences
        sentences = re.split(r'(?<=[.!?؟])\s+', text)
        
        chunks = []
        current_chunk = []
        current_tokens = 0
        
        for i, sent in enumerate(sentences):
            sent_tokens = len(sent.split())
            
            if current_tokens + sent_tokens > self.chunk_size:
                # Check: does the next sentence continue the idea?
                if current_chunk and i < len(sentences) - 1:
                    if self._is_continuation(current_chunk[-1], sentences[i]):
                        current_chunk.append(sent)
                        current_tokens += sent_tokens
                        continue
                
                # Create chunk
                chunk_text = ' '.join(current_chunk)
                chunks.append({
                    'text': chunk_text,
                    'token_count': current_tokens,
                    'word_count': len(chunk_text.split()),
                    'heading': '',
                    'source': '',
                    'domain': self._detect_domain(chunk_text)
                })
                
                current_chunk = [sent]
                current_tokens = sent_tokens
            else:
                current_chunk.append(sent)
                current_tokens += sent_tokens
        
        # Don't forget the last chunk
        if current_chunk:
            chunk_text = ' '.join(current_chunk)
            chunks.append({
                'text': chunk_text,
                'token_count': current_tokens,
                'word_count': len(chunk_text.split()),
                'heading': '',
                'source': '',
                'domain': self._detect_domain(chunk_text)
            })
        
        return chunks
    
    def _is_continuation(self, prev_sent: str, curr_sent: str) -> bool:
        """
        Determines if current sentence continues the previous one
        Uses coreference resolution (simplified)
        """
        if not curr_sent.strip():
            return False
            
        curr_start = curr_sent.split()[0] if curr_sent.split() else ""
        
        # Check for continuation markers
        return any(marker in curr_start for marker in self.continuation_markers)
    
    def _detect_domain(self, text: str) -> str:
        """
        Automatically detects domain (medical, legal, programming, etc.)
        """
        text_lower = text.lower()
        scores = {}
        
        for domain, keywords in self.domain_keywords.items():
            score = sum(1 for kw in keywords if kw in text_lower)
            scores[domain] = score
        
        # Return domain with highest score if > 0
        if max(scores.values()) > 0:
            return max(scores, key=scores.get)
        
        return 'general'
    
    def _split_by_headings(self, text: str, doc: Dict = None) -> List[Dict]:
        """
        Splits text by headings while preserving structure
        """
        sections = []
        
        # Try to detect markdown headings
        heading_pattern = re.compile(r'^(#{1,6})\s+(.+)$', re.MULTILINE)
        matches = list(heading_pattern.finditer(text))
        
        if matches:
            # Split by headings
            for i, match in enumerate(matches):
                start = match.start()
                end = matches[i + 1].start() if i + 1 < len(matches) else len(text)
                
                section_text = text[start:end].strip()
                heading = match.group(2)
                
                sections.append({
                    'text': section_text,
                    'heading': heading,
                    'level': len(match.group(1))
                })
        else:
            # No headings found, treat as single section
            sections.append({
                'text': text,
                'heading': '',
                'level': 0
            })
        
        return sections
    
    def _extract_full_text(self, doc: Dict) -> str:
        """
        Extracts full text from various document formats
        """
        if 'text' in doc:
            return doc['text']
        elif 'paragraphs' in doc:
            return '\n'.join([p['text'] for p in doc['paragraphs']])
        elif 'pages' in doc:
            return '\n'.join([p['text'] for p in doc['pages']])
        else:
            return ''
    
    def calculate_semantic_similarity(self, text1: str, text2: str) -> float:
        """
        Calculates semantic similarity between two texts
        """
        embeddings = self.embedder.encode([text1, text2])
        similarity = np.dot(embeddings[0], embeddings[1]) / (
            np.linalg.norm(embeddings[0]) * np.linalg.norm(embeddings[1])
        )
        return float(similarity)
    
    def find_similar_chunks(self, chunks: List[Dict], threshold: float = 0.85) -> List[tuple]:
        """
        Finds redundant/similar chunks that could be merged
        """
        texts = [chunk['text'] for chunk in chunks]
        embeddings = self.embedder.encode(texts)
        
        similar_pairs = []
        for i in range(len(chunks)):
            for j in range(i + 1, len(chunks)):
                similarity = np.dot(embeddings[i], embeddings[j]) / (
                    np.linalg.norm(embeddings[i]) * np.linalg.norm(embeddings[j])
                )
                
                if similarity > threshold:
                    similar_pairs.append((i, j, float(similarity)))
        
        return similar_pairs


if __name__ == "__main__":
    # Test the chunking engine
    chunker = ExpertChunkingStrategy(chunk_size=512)
    
    # Example usage:
    # documents = [...]  # List of processed documents
    # chunks = chunker.create_semantic_chunks(documents)
    # print(f"Created {len(chunks)} chunks")
    
    print("ExpertChunkingStrategy ready!")
    print(f"Default chunk size: {chunker.chunk_size} tokens")
    print(f"Overlap: {chunker.overlap} tokens")
