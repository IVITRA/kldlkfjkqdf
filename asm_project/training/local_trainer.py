"""
Local Trainer - Trains experts on local files using LoRA
No internet connection required, privacy-first training
"""

import torch
from torch.utils.data import Dataset, DataLoader
from transformers import AutoTokenizer, AutoModelForCausalLM, TrainingArguments, Trainer
from peft import LoraConfig, get_peft_model, TaskType
from typing import List, Dict
import os
import json
from pathlib import Path


class TextDataset(Dataset):
    """
    Dataset for text training (Next Token Prediction)
    """
    def __init__(self, texts: List[str], tokenizer, max_length: int = 512):
        self.encodings = tokenizer(
            texts,
            truncation=True,
            padding=True,
            max_length=max_length,
            return_tensors='pt'
        )
        
    def __getitem__(self, idx):
        return {key: val[idx] for key, val in self.encodings.items()}
        
    def __len__(self):
        return len(self.encodings['input_ids'])


class LocalFileTrainer:
    """
    Trains experts on local files without internet connection
    Uses LoRA for parameter-efficient fine-tuning
    """
    
    def __init__(self, base_model_path: str = "microsoft/DialoGPT-medium",
                 use_8bit: bool = True, device: str = "auto"):
        """
        Args:
            base_model_path: Base open model (GPT-2, Llama, AraGPT, etc.)
            use_8bit: Use 8-bit quantization to save VRAM
            device: Device mapping strategy
        """
        print(f"Loading base model: {base_model_path}")
        
        self.tokenizer = AutoTokenizer.from_pretrained(base_model_path)
        
        # Add padding token if missing
        if self.tokenizer.pad_token is None:
            self.tokenizer.pad_token = self.tokenizer.eos_token
        
        self.base_model = AutoModelForCausalLM.from_pretrained(
            base_model_path,
            load_in_8bit=use_8bit,
            device_map=device
        )
        
        print(f"✓ Base model loaded successfully")
        print(f"  Parameters: {self.base_model.num_parameters():,}")
        
    def prepare_dataset(self, chunks: List[Dict]) -> TextDataset:
        """
        Converts text chunks to training format (Next Token Prediction)
        """
        formatted_texts = []
        
        for chunk in chunks:
            text = chunk['text']
            
            # Add metadata as prefix (helps expert know its knowledge source)
            domain_tag = f"[{chunk.get('domain', 'GENERAL').upper()}]"
            source_tag = f"<source>{chunk.get('source', 'unknown')}</source>"
            
            formatted = f"{domain_tag}\n{source_tag}\n{text}"
            formatted_texts.append(formatted)
        
        print(f"✓ Prepared {len(formatted_texts)} training samples")
        return TextDataset(formatted_texts, self.tokenizer)
    
    def train_expert(self, chunks: List[Dict], expert_id: str, output_dir: str,
                    num_epochs: int = 3, batch_size: int = 4, learning_rate: float = 2e-4):
        """
        Trains a single expert on a set of chunks
        """
        print(f"\n{'='*60}")
        print(f"Training Expert: {expert_id}")
        print(f"{'='*60}")
        
        # Prepare dataset
        dataset = self.prepare_dataset(chunks)
        
        # Setup LoRA (Low-Rank Adaptation) - trains only ~1% of parameters!
        lora_config = LoraConfig(
            r=16,  # rank
            lora_alpha=32,
            target_modules=["q_proj", "v_proj", "k_proj", "o_proj"],
            lora_dropout=0.05,
            bias="none",
            task_type=TaskType.CAUSAL_LM
        )
        
        model = get_peft_model(self.base_model, lora_config)
        model.print_trainable_parameters()
        
        # Training arguments
        training_args = TrainingArguments(
            output_dir=f"{output_dir}/expert_{expert_id}",
            num_train_epochs=num_epochs,
            per_device_train_batch_size=batch_size,
            gradient_accumulation_steps=4,
            learning_rate=learning_rate,
            save_steps=100,
            logging_steps=10,
            fp16=True,  # Mixed precision for speed
            optim="adamw_torch",
            weight_decay=0.01,
            warmup_steps=50,
            report_to="none",  # Disable wandb/tensorboard for privacy
        )
        
        # Data collator
        def data_collator(features):
            return {
                'input_ids': torch.stack([f['input_ids'] for f in features]),
                'attention_mask': torch.stack([f['attention_mask'] for f in features]),
                'labels': torch.stack([f['input_ids'] for f in features])
            }
        
        # Create trainer
        trainer = Trainer(
            model=model,
            args=training_args,
            train_dataset=dataset,
            data_collator=data_collator
        )
        
        # Train!
        print(f"\n🚀 Starting training...")
        trainer.train()
        
        # Save trained expert (LoRA adapters only - very small ~20MB)
        output_path = f"{output_dir}/expert_{expert_id}"
        model.save_pretrained(output_path)
        self.tokenizer.save_pretrained(output_path)
        
        # Save training metadata
        metadata = {
            'expert_id': expert_id,
            'num_chunks': len(chunks),
            'num_epochs': num_epochs,
            'batch_size': batch_size,
            'learning_rate': learning_rate,
            'base_model': self.base_model.config._name_or_path,
            'lora_config': lora_config.to_dict()
        }
        
        with open(f"{output_path}/training_metadata.json", 'w', encoding='utf-8') as f:
            json.dump(metadata, f, ensure_ascii=False, indent=2)
        
        print(f"\n✅ Expert saved to: {output_path}")
        print(f"   Size: ~{self._get_dir_size(output_path) / 1024 / 1024:.1f} MB")
        
        return output_path
    
    def merge_and_save(self, expert_path: str, output_path: str):
        """
        Merges LoRA adapters with base model and saves full model
        """
        print(f"\nMerging LoRA adapters with base model...")
        
        # Load base model
        base_model = AutoModelForCausalLM.from_pretrained(
            self.base_model.config._name_or_path
        )
        
        # Load and merge LoRA
        from peft import PeftModel
        model = PeftModel.from_pretrained(base_model, expert_path)
        model = model.merge_and_unload()
        
        # Save merged model
        model.save_pretrained(output_path)
        self.tokenizer.save_pretrained(output_path)
        
        print(f"✓ Merged model saved to: {output_path}")
        print(f"  Size: {self._get_dir_size(output_path) / 1024 / 1024:.1f} MB")
    
    def _get_dir_size(self, path: str) -> int:
        """Gets total size of directory in bytes"""
        total = 0
        with os.scandir(path) as it:
            for entry in it:
                if entry.is_file():
                    total += entry.stat().st_size
                elif entry.is_dir():
                    total += self._get_dir_size(entry.path)
        return total


if __name__ == "__main__":
    # Example usage
    print("LocalFileTrainer ready!")
    print("\nExample usage:")
    print("""
    trainer = LocalFileTrainer(base_model_path="gpt2")
    
    chunks = [
        {'text': 'Sample text about medicine...', 'domain': 'medical', 'source': 'book.pdf'},
        {'text': 'More medical information...', 'domain': 'medical', 'source': 'book.pdf'}
    ]
    
    expert_path = trainer.train_expert(
        chunks=chunks,
        expert_id="medical_expert",
        output_dir="./models/experts",
        num_epochs=3
    )
    """)
