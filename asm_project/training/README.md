# 🎓 ASM Training Pipeline

**Train specialized AI experts from your local documents - Privacy-first, no internet required!**

---

## 🚀 Quick Start

### 1. Install Dependencies

```bash
pip install -r requirements.txt
```

### 2. Organize Your Documents

```
my_documents/
├── medical/
│   ├── textbook.pdf
│   └── research.pdf
├── legal/
│   └── contracts.docx
└── programming/
    └── guide.pdf
```

### 3. Train Experts

```bash
python train_from_files.py --data-dir ./my_documents --output ./models
```

**That's it!** Each folder becomes a specialized expert. 🎉

---

## 📁 File Structure

```
training/
├── local_file_processor.py    # Document ingestion (PDF, TXT, DOCX, etc.)
├── chunking_engine.py         # Semantic text splitting
├── local_trainer.py           # LoRA-based expert training
├── local_teacher.py           # Optional teacher model for QA generation
├── directory_trainer.py       # Automated multi-expert training
├── train_from_files.py        # Main CLI script (one-click training)
├── example_training.py        # Example usage
├── requirements.txt           # Python dependencies
└── TRAINING_GUIDE.md          # Comprehensive documentation
```

---

## ✨ Features

- ✅ **Multi-Format Support**: PDF, TXT, DOCX, CSV, JSON, Markdown
- ✅ **OCR Support**: Handles scanned documents (Arabic + English)
- ✅ **Semantic Chunking**: Smart text splitting based on meaning
- ✅ **LoRA Training**: Parameter-efficient (~20MB per expert)
- ✅ **Automatic Domains**: One expert per folder
- ✅ **Privacy-First**: 100% local, no internet needed
- ✅ **Arabic Support**: Full Arabic language handling

---

## 📖 Documentation

See [TRAINING_GUIDE.md](TRAINING_GUIDE.md) for:
- Detailed usage instructions
- Hardware requirements
- Advanced features
- Troubleshooting
- Performance benchmarks

---

## 🎯 Example Usage

### Basic Training

```bash
python train_from_files.py --data-dir ./documents --output ./models
```

### With Custom Parameters

```bash
python train_from_files.py \
    --data-dir ./documents \
    --output ./models \
    --base-model gpt2 \
    --epochs 5 \
    --batch-size 8
```

### With Teacher Model (Higher Quality)

```bash
python train_from_files.py \
    --data-dir ./documents \
    --output ./models \
    --use-teacher \
    --teacher-model TheBloke/Llama-2-13B-GPTQ
```

### Run Example

```bash
python example_training.py
```

---

## 💻 Hardware Requirements

### CPU Training
- RAM: 8GB minimum
- Time: ~30 min per expert
- Expert size: ~20MB

### GPU Training (Recommended)
- VRAM: 4GB minimum, 8GB+ recommended
- Time: ~2 min per expert (RTX 3060)
- Supports quantized models

---

## 🔧 API Usage

### Process Documents

```python
from local_file_processor import LocalDocumentProcessor

processor = LocalDocumentProcessor(ocr_enabled=True)
documents = processor.process_directory("./my_documents/medical")
```

### Create Chunks

```python
from chunking_engine import ExpertChunkingStrategy

chunker = ExpertChunkingStrategy(chunk_size=512)
chunks = chunker.create_semantic_chunks(documents)
```

### Train Expert

```python
from local_trainer import LocalFileTrainer

trainer = LocalFileTrainer(base_model_path="gpt2")
expert_path = trainer.train_expert(
    chunks=chunks,
    expert_id="medical_expert",
    output_dir="./models"
)
```

---

## 📊 Output

After training:

```
models/
├── expert_registry.json
├── expert_medical/
│   ├── adapter_model.bin (~20MB)
│   ├── adapter_config.json
│   └── training_metadata.json
├── expert_legal/
│   └── ...
└── expert_programming/
    └── ...
```

---

## 🤝 Integration with C++ Inference

After training Python experts, convert them for C++ inference:

1. Quantize to 1.58-bit ternary format
2. Extract expert centroids
3. Package into `.asm` binary files
4. Use with `asm_core.exe`

---

## 🆘 Troubleshooting

See [TRAINING_GUIDE.md](TRAINING_GUIDE.md#troubleshooting) for common issues and solutions.

---

## 📄 License

Part of the ASM (Adaptive Sparse Mind) project.

---

**Happy Training! 🎓✨**
