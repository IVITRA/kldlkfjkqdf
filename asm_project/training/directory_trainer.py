"""
Directory-Based Training System
Trains one expert per folder:
/data/medical/ → Medical expert
/data/legal/   → Legal expert
"""

from pathlib import Path
import json
from typing import List, Dict
from datetime import datetime

from local_file_processor import LocalDocumentProcessor
from chunking_engine import ExpertChunkingStrategy
from local_trainer import LocalFileTrainer
from local_teacher import LocalTeacherModel


class DirectoryBasedTraining:
    """
    Automatically trains experts for each directory
    Organizes training by domain/folder structure
    """
    
    def __init__(self, root_dir: Path, 
                 base_model: str = "microsoft/DialoGPT-medium",
                 use_teacher: bool = False,
                 teacher_model: str = "TheBloke/Llama-2-13B-GPTQ"):
        """
        Args:
            root_dir: Root directory containing domain subdirectories
            base_model: Base model for training
            use_teacher: Whether to use local LLM as teacher
            teacher_model: Path to teacher model (if use_teacher=True)
        """
        self.root_dir = Path(root_dir)
        self.processor = LocalDocumentProcessor()
        self.chunker = ExpertChunkingStrategy()
        self.trainer = LocalFileTrainer(base_model_path=base_model)
        
        if use_teacher:
            self.teacher = LocalTeacherModel(model_path=teacher_model)
        else:
            self.teacher = None
        
        self.results = []
        
    def train_all_directories(self, output_dir: str = "./models/experts"):
        """
        Iterates through each directory and trains an expert for it
        """
        print(f"\n{'='*70}")
        print(f"🚀 Starting Directory-Based Training")
        print(f"{'='*70}")
        print(f"Root directory: {self.root_dir}")
        print(f"Output directory: {output_dir}")
        print(f"{'='*70}\n")
        
        output_path = Path(output_dir)
        output_path.mkdir(parents=True, exist_ok=True)
        
        # Process each subdirectory
        for domain_dir in sorted(self.root_dir.iterdir()):
            if not domain_dir.is_dir():
                continue
            
            if domain_dir.name.startswith('.'):
                continue
                
            print(f"\n{'='*70}")
            print(f"📚 Training expert for domain: {domain_dir.name}")
            print(f"{'='*70}")
            
            try:
                # Step 1: Collect all files in this directory
                print(f"\n📄 Step 1: Collecting documents...")
                files = list(domain_dir.rglob('*'))
                documents = []
                
                for file in files:
                    if file.suffix.lower() in ['.pdf', '.txt', '.docx', '.md', '.csv', '.json']:
                        try:
                            doc = self.processor.process_file(file)
                            documents.append(doc)
                        except Exception as e:
                            print(f"  ⚠ Error processing {file.name}: {e}")
                
                if not documents:
                    print(f"  ⚠ No valid documents found in {domain_dir.name}")
                    continue
                
                print(f"  ✓ Loaded {len(documents)} documents")
                
                # Step 2: Chunk the content
                print(f"\n✂️ Step 2: Chunking content...")
                chunks = self.chunker.create_semantic_chunks(documents)
                print(f"  ✓ Created {len(chunks)} chunks")
                
                # Check for redundant chunks
                similar_chunks = self.chunker.find_similar_chunks(chunks, threshold=0.9)
                if similar_chunks:
                    print(f"  ⚠ Found {len(similar_chunks)} highly similar chunks (>{90}%)")
                    print(f"    Consider merging to avoid redundancy")
                
                # Step 3: Optional - Use teacher model to generate QA pairs
                if self.teacher:
                    print(f"\n🎓 Step 3: Generating QA pairs with teacher...")
                    qa_pairs = self.teacher.generate_training_pairs(chunks)
                    
                    # Convert QA pairs to chunks format
                    qa_chunks = [
                        {
                            'text': f"Q: {pair['input']}\nA: {pair['output']}",
                            'domain': chunk.get('domain', 'general'),
                            'source': chunk.get('source', '')
                        }
                        for pair, chunk in zip(qa_pairs, chunks[:len(qa_pairs)])
                    ]
                    
                    # Combine original chunks with QA pairs
                    all_chunks = chunks + qa_chunks
                    print(f"  ✓ Total training samples: {len(all_chunks)}")
                else:
                    all_chunks = chunks
                
                # Step 4: Train expert for this domain
                print(f"\n🔥 Step 4: Training expert...")
                expert_path = self.trainer.train_expert(
                    chunks=all_chunks,
                    expert_id=domain_dir.name,
                    output_dir=output_dir,
                    num_epochs=3,
                    batch_size=4,
                    learning_rate=2e-4
                )
                
                # Record results
                result = {
                    'domain': domain_dir.name,
                    'expert_path': expert_path,
                    'num_files': len(documents),
                    'num_chunks': len(chunks),
                    'total_words': sum(len(c['text'].split()) for c in chunks),
                    'timestamp': datetime.now().isoformat()
                }
                
                if self.teacher:
                    result['qa_pairs_generated'] = len(qa_pairs)
                    result['total_training_samples'] = len(all_chunks)
                
                self.results.append(result)
                
                print(f"\n✅ Expert '{domain_dir.name}' training complete!")
                
            except Exception as e:
                print(f"\n❌ Error training expert for {domain_dir.name}: {e}")
                import traceback
                traceback.print_exc()
                continue
        
        # Save expert registry
        self._save_registry(output_dir)
        
        # Print summary
        self._print_summary()
        
        return self.results
    
    def train_single_directory(self, dir_path: Path, expert_id: str, 
                               output_dir: str = "./models/experts"):
        """
        Trains expert for a single directory
        """
        print(f"\n📚 Training expert: {expert_id}")
        print(f"   Directory: {dir_path}")
        
        # Process all files
        documents = self.processor.process_directory(dir_path)
        
        if not documents:
            print(f"⚠ No valid documents found")
            return None
        
        # Chunk content
        chunks = self.chunker.create_semantic_chunks(documents)
        
        # Train expert
        expert_path = self.trainer.train_expert(
            chunks=chunks,
            expert_id=expert_id,
            output_dir=output_dir
        )
        
        result = {
            'domain': expert_id,
            'expert_path': expert_path,
            'num_files': len(documents),
            'num_chunks': len(chunks),
            'timestamp': datetime.now().isoformat()
        }
        
        self.results.append(result)
        return result
    
    def _save_registry(self, output_dir: str):
        """
        Saves expert registry JSON file
        """
        registry_path = Path(output_dir) / "expert_registry.json"
        
        registry = {
            'created_at': datetime.now().isoformat(),
            'num_experts': len(self.results),
            'experts': self.results
        }
        
        with open(registry_path, 'w', encoding='utf-8') as f:
            json.dump(registry, f, ensure_ascii=False, indent=2)
        
        print(f"\n💾 Expert registry saved to: {registry_path}")
    
    def _print_summary(self):
        """
        Prints training summary
        """
        print(f"\n{'='*70}")
        print(f"📊 TRAINING SUMMARY")
        print(f"{'='*70}")
        print(f"Total experts trained: {len(self.results)}")
        print(f"")
        
        for result in self.results:
            print(f"  📚 {result['domain']}:")
            print(f"     Files: {result['num_files']}")
            print(f"     Chunks: {result['num_chunks']}")
            if 'total_words' in result:
                print(f"     Words: {result['total_words']:,}")
            if 'qa_pairs_generated' in result:
                print(f"     QA Pairs: {result['qa_pairs_generated']}")
            print(f"     Path: {result['expert_path']}")
            print(f"")
        
        print(f"{'='*70}")
        print(f"✅ All experts trained successfully!")
        print(f"{'='*70}")


if __name__ == "__main__":
    # Example usage
    print("DirectoryBasedTraining system ready!")
    print("\nExample usage:")
    print("""
    # Train experts for all subdirectories
    trainer = DirectoryBasedTraining(
        root_dir="./my_documents",
        base_model="gpt2",
        use_teacher=False
    )
    
    results = trainer.train_all_directories(output_dir="./models/experts")
    """)
