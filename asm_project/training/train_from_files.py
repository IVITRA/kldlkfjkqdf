#!/usr/bin/env python3
"""
Complete Training Script from Local Files
Usage: python train_from_files.py --data-dir ./my_documents --output ./my_asm_model

This script implements the full training pipeline:
1. Document ingestion (PDF, TXT, DOCX, CSV, etc.)
2. Semantic chunking
3. Expert training with LoRA
4. Router building
5. Expert registry generation
"""

import argparse
import torch
from pathlib import Path
import sys
import os

# Add training directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from directory_trainer import DirectoryBasedTraining


def main():
    parser = argparse.ArgumentParser(
        description='Train ASM experts from local files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic training (CPU-friendly)
  python train_from_files.py --data-dir ./documents --output ./models
  
  # Training with teacher model (requires GPU)
  python train_from_files.py --data-dir ./documents --output ./models --use-teacher
  
  # Custom base model
  python train_from_files.py --data-dir ./documents --base-model gpt2 --output ./models
  
  # Training with custom parameters
  python train_from_files.py --data-dir ./documents --epochs 5 --batch-size 8
        """
    )
    
    parser.add_argument(
        '--data-dir',
        type=str,
        required=True,
        help='Directory containing documents (subdirectories = different experts)'
    )
    parser.add_argument(
        '--output',
        type=str,
        default='./asm_model',
        help='Output directory for trained experts'
    )
    parser.add_argument(
        '--base-model',
        type=str,
        default='microsoft/DialoGPT-medium',
        help='Base model to use for training'
    )
    parser.add_argument(
        '--use-teacher',
        action='store_true',
        help='Use local LLM as teacher (requires GPU with 8GB+ VRAM)'
    )
    parser.add_argument(
        '--teacher-model',
        type=str,
        default='TheBloke/Llama-2-13B-GPTQ',
        help='Teacher model path (if --use-teacher is enabled)'
    )
    parser.add_argument(
        '--epochs',
        type=int,
        default=3,
        help='Number of training epochs per expert'
    )
    parser.add_argument(
        '--batch-size',
        type=int,
        default=4,
        help='Training batch size'
    )
    parser.add_argument(
        '--learning-rate',
        type=float,
        default=2e-4,
        help='Learning rate for training'
    )
    parser.add_argument(
        '--chunk-size',
        type=int,
        default=512,
        help='Chunk size for text splitting'
    )
    parser.add_argument(
        '--no-ocr',
        action='store_true',
        help='Disable OCR for scanned PDFs'
    )
    parser.add_argument(
        '--device',
        type=str,
        default='auto',
        help='Device to use (cpu, cuda, auto)'
    )
    
    args = parser.parse_args()
    
    # Validate inputs
    data_dir = Path(args.data_dir)
    if not data_dir.exists():
        print(f"❌ Error: Data directory not found: {data_dir}")
        sys.exit(1)
    
    if not data_dir.is_dir():
        print(f"❌ Error: Path is not a directory: {data_dir}")
        sys.exit(1)
    
    # Print configuration
    print("\n" + "="*70)
    print("🚀 ASM TRAINING FROM LOCAL FILES")
    print("="*70)
    print(f"📁 Data directory:     {data_dir}")
    print(f"💾 Output directory:   {args.output}")
    print(f"🧠 Base model:         {args.base_model}")
    print(f"📚 Training epochs:    {args.epochs}")
    print(f"📦 Batch size:         {args.batch_size}")
    print(f"🎯 Learning rate:      {args.learning_rate}")
    print(f"✂️  Chunk size:         {args.chunk_size}")
    print(f"🔍 OCR enabled:        {not args.no_ocr}")
    print(f"🎓 Use teacher:        {args.use_teacher}")
    print(f"💻 Device:             {args.device}")
    print("="*70)
    
    # Check GPU availability
    if torch.cuda.is_available():
        print(f"\n✅ GPU detected: {torch.cuda.get_device_name(0)}")
        print(f"   VRAM: {torch.cuda.get_device_properties(0).total_mem / 1024**3:.1f} GB")
    else:
        print(f"\n⚠️  No GPU detected - training will use CPU (slower)")
        print(f"   Consider using a smaller base model for faster training")
    
    if args.use_teacher and not torch.cuda.is_available():
        print(f"\n❌ Error: Teacher model requires GPU")
        sys.exit(1)
    
    # Start training
    try:
        print(f"\n📋 Initializing training system...")
        
        trainer = DirectoryBasedTraining(
            root_dir=data_dir,
            base_model=args.base_model,
            use_teacher=args.use_teacher,
            teacher_model=args.teacher_model
        )
        
        print(f"\n✅ Training system initialized")
        print(f"   Found {len(list(data_dir.iterdir()))} directories to process")
        
        # Run training
        results = trainer.train_all_directories(output_dir=args.output)
        
        if not results:
            print(f"\n❌ No experts were trained. Check your data directory.")
            sys.exit(1)
        
        # Final summary
        print(f"\n{'='*70}")
        print(f"🎉 TRAINING COMPLETE!")
        print(f"{'='*70}")
        print(f"✅ Successfully trained {len(results)} experts")
        print(f"📊 Registry saved to: {args.output}/expert_registry.json")
        print(f"\n🚀 Next steps:")
        print(f"   1. Convert experts to ASM format for C++ inference")
        print(f"   2. Build router with expert centroids")
        print(f"   3. Run inference: ./asm_core --model {args.output}")
        print(f"{'='*70}")
        
    except KeyboardInterrupt:
        print(f"\n\n⚠️  Training interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n\n❌ Training failed with error:")
        print(f"   {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
