"""
Example: Training an Expert from Local Files

This example demonstrates the complete training workflow:
1. Process documents
2. Create semantic chunks
3. Train expert with LoRA
4. Save and validate
"""

import sys
from pathlib import Path

# Add training directory to path
sys.path.insert(0, str(Path(__file__).parent))

from local_file_processor import LocalDocumentProcessor
from chunking_engine import ExpertChunkingStrategy
from local_trainer import LocalFileTrainer


def main():
    print("="*70)
    print("🎓 ASM Training Example")
    print("="*70)
    
    # Configuration
    DATA_DIR = Path("./example_documents")
    OUTPUT_DIR = Path("./example_models")
    BASE_MODEL = "gpt2"  # Small model for quick testing
    
    # Create example documents if they don't exist
    if not DATA_DIR.exists():
        print("\n📝 Creating example documents...")
        DATA_DIR.mkdir(parents=True)
        
        # Create a sample medical text
        medical_text = """
# Medical Knowledge Base

## Cardiovascular System

The cardiovascular system is an organ system that permits blood to 
circulate and transport nutrients, oxygen, carbon dioxide, hormones, 
and other substances to and from the cells in the body.

### Heart Anatomy

The heart is a muscular organ located in the middle mediastinum. 
It pumps blood through the blood vessels of the circulatory system.

The heart has four chambers:
- Right atrium
- Right ventricle
- Left atrium
- Left ventricle

### Common Diseases

1. Coronary artery disease
2. Heart failure
3. Arrhythmias
4. Valvular heart disease
"""
        
        (DATA_DIR / "medical_sample.txt").write_text(medical_text, encoding='utf-8')
        print(f"✓ Created example document: {DATA_DIR / 'medical_sample.txt'}")
    
    # Step 1: Process documents
    print("\n📄 Step 1: Processing documents...")
    processor = LocalDocumentProcessor(ocr_enabled=False)
    documents = processor.process_directory(DATA_DIR)
    
    if not documents:
        print("❌ No documents found!")
        return
    
    print(f"✓ Processed {len(documents)} documents")
    
    # Step 2: Create semantic chunks
    print("\n✂️  Step 2: Creating semantic chunks...")
    chunker = ExpertChunkingStrategy(chunk_size=256, overlap=30)
    chunks = chunker.create_semantic_chunks(documents)
    
    print(f"✓ Created {len(chunks)} chunks")
    print(f"  Average chunk size: {sum(len(c['text'].split()) for c in chunks) // len(chunks)} words")
    
    # Step 3: Train expert
    print("\n🔥 Step 3: Training expert...")
    print("  (This may take a few minutes)")
    
    try:
        trainer = LocalFileTrainer(
            base_model_path=BASE_MODEL,
            use_8bit=False,  # Disable for CPU compatibility
            device="cpu"
        )
        
        expert_path = trainer.train_expert(
            chunks=chunks,
            expert_id="medical_example",
            output_dir=str(OUTPUT_DIR),
            num_epochs=1,  # Just 1 epoch for example
            batch_size=2,
            learning_rate=2e-4
        )
        
        print(f"\n✅ Expert trained successfully!")
        print(f"   Path: {expert_path}")
        
    except Exception as e:
        print(f"\n⚠️  Training failed (this is expected if PyTorch is not installed)")
        print(f"   Error: {e}")
        print(f"\nTo run training, install dependencies:")
        print(f"   pip install torch transformers peft")
        
        # Show what would have been created
        print(f"\n📋 Summary of what would be created:")
        print(f"   - Expert directory: {OUTPUT_DIR}/expert_medical_example")
        print(f"   - LoRA adapters: ~20MB")
        print(f"   - Training metadata: JSON file")
    
    # Step 4: Show results
    print("\n" + "="*70)
    print("📊 Training Summary")
    print("="*70)
    print(f"  Documents processed: {len(documents)}")
    print(f"  Chunks created:      {len(chunks)}")
    print(f"  Total words:         {sum(len(c['text'].split()) for c in chunks)}")
    print(f"  Domains detected:    {set(c['domain'] for c in chunks)}")
    print("="*70)
    
    print("\n🎉 Example complete!")
    print("\nNext steps:")
    print("  1. Add more documents to ./example_documents")
    print("  2. Run: python train_from_files.py --data-dir ./example_documents --output ./models")
    print("  3. Use trained experts with C++ inference engine")


if __name__ == "__main__":
    main()
