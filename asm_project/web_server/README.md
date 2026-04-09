# 🌐 ASM Web Interface

Complete web-based interface for the ASM (Adaptive Sparse Mind) system with REST API backend.

## 🚀 Quick Start

### 1. Install Dependencies

```bash
cd web_server
pip install -r requirements.txt
```

### 2. Download spaCy Model

```bash
python -m spacy download en_core_web_sm
```

### 3. Start the Server

```bash
python app.py
```

### 4. Open in Browser

Navigate to: **http://localhost:5000**

---

## 📋 Features

### 📤 File Upload & Management
- **Drag & Drop** upload interface
- Support for PDF, TXT, DOCX, MD, CSV files
- File management (view, delete)
- Domain classification (medical, legal, science, etc.)
- OCR support for images and scanned PDFs

### 🎓 Expert Training
- **Directory-based training** (multi-expert)
- Single file training
- Configurable base models (DialoGPT, GPT-2)
- Teacher model for QA generation
- Real-time progress monitoring
- Training cancellation support

### 💬 Chat Interface
- Real-time chat with ASM experts
- Domain-specific queries
- Confidence scoring display
- Expert activation tracking
- Chat history

### 🧠 Expert Management
- View all trained experts
- Expert metadata (size, creation date)
- Delete unused experts
- Expert statistics

### 🔄 Self-Learning System
- **24/7 background learning** daemon
- Hourly failed query processing
- Daily deep learning (3 AM)
- Weekly knowledge consolidation
- Source validation and credibility checking
- Add failed queries to learning queue

### 📊 System Dashboard
- Real-time CPU, memory, disk monitoring
- Training status tracking
- Learning system status
- File and expert counts
- Auto-refresh every 30 seconds

---

## 🏗️ Architecture

### File Structure

```
web_server/
├── app.py              # Flask API server (backend)
├── requirements.txt    # Python dependencies
├── README.md          # This file
└── web/               # Frontend files
    ├── index.html     # Main HTML page
    ├── styles.css     # Stylesheet (separate)
    └── app.js         # JavaScript client (separate)
```

### Separation of Concerns

- **HTML** (`index.html`): Structure and content only
- **CSS** (`styles.css`): All styling, responsive design
- **JavaScript** (`app.js`): API communication, UI logic
- **Python** (`app.py`): REST API, business logic

---

## 🔌 API Endpoints

### File Management

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/upload` | Upload files for training |
| GET | `/api/files` | List uploaded files |
| DELETE | `/api/files/<filename>` | Delete a file |
| POST | `/api/process-files` | Process files (OCR, chunking) |

### Training

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/train` | Start expert training |
| GET | `/api/training/status` | Get training progress |
| POST | `/api/training/cancel` | Cancel ongoing training |

### Inference

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/chat` | Chat with ASM experts |
| POST | `/api/inference` | Raw inference call |

### Self-Learning

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/learning/start` | Start background learning |
| POST | `/api/learning/stop` | Stop background learning |
| GET | `/api/learning/status` | Get learning status |
| POST | `/api/learning/add-query` | Add failed query |
| POST | `/api/learning/validate-source` | Validate source URL |

### System

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/status` | System status & resources |
| GET | `/api/diagnostics` | Run diagnostics |

### Experts

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/experts` | List trained experts |
| DELETE | `/api/experts/<name>` | Delete an expert |

---

## 💻 Usage Examples

### Upload Files via API

```bash
curl -X POST http://localhost:5000/api/upload \
  -F "files=@document1.pdf" \
  -F "files=@document2.txt" \
  -F "domain=medical"
```

### Start Training

```bash
curl -X POST http://localhost:5000/api/train \
  -H "Content-Type: application/json" \
  -d '{
    "task": "directory_training",
    "base_model": "microsoft/DialoGPT-medium",
    "use_teacher": true,
    "output_dir": "./asm_model"
  }'
```

### Chat with ASM

```bash
curl -X POST http://localhost:5000/api/chat \
  -H "Content-Type: application/json" \
  -d '{
    "query": "What is photosynthesis?",
    "domain": "science"
  }'
```

### Validate Source

```bash
curl -X POST http://localhost:5000/api/learning/validate-source \
  -H "Content-Type: application/json" \
  -d '{
    "url": "https://www.nature.com/articles/example"
  }'
```

---

## 🎨 UI Features

### Responsive Design
- Works on desktop, tablet, and mobile
- Adaptive grid layouts
- Touch-friendly interface

### Modern UI Components
- Drag & drop file upload
- Real-time progress bars
- Toast notifications
- Smooth animations
- Card-based layouts

### Dark/Light Mode Ready
- CSS variables for easy theming
- Consistent color scheme
- Accessible contrast ratios

---

## 🔧 Configuration

### Environment Variables

Create a `.env` file in `web_server/`:

```env
# Server Configuration
HOST=0.0.0.0
PORT=5000
DEBUG=True

# Paths
UPLOAD_FOLDER=./uploads
TRAINING_OUTPUT=./asm_model
TRAINING_DATA=./training_data

# API Keys (if needed)
OPENAI_API_KEY=your_key_here
```

### Server Configuration

Edit `app.py` to change:

```python
# File storage paths
UPLOAD_FOLDER = './uploads'
TRAINING_OUTPUT = './asm_model'

# Server settings
app.run(host='0.0.0.0', port=5000, debug=True)
```

---

## 🚀 Deployment

### Development

```bash
python app.py
```

### Production (with Gunicorn)

```bash
pip install gunicorn
gunicorn -w 4 -b 0.0.0.0:5000 app:app
```

### Docker (Optional)

Create `Dockerfile`:

```dockerfile
FROM python:3.10-slim

WORKDIR /app
COPY requirements.txt .
RUN pip install -r requirements.txt
RUN python -m spacy download en_core_web_sm

COPY . .

EXPOSE 5000
CMD ["python", "app.py"]
```

Build and run:

```bash
docker build -t asm-web .
docker run -p 5000:5000 asm-web
```

---

## 📊 System Requirements

### Minimum
- Python 3.8+
- 4GB RAM
- 10GB disk space

### Recommended
- Python 3.10+
- 8GB RAM
- 20GB disk space
- GPU (for faster training)

---

## 🐛 Troubleshooting

### Port Already in Use

```bash
# Windows
netstat -ano | findstr :5000
taskkill /PID <PID> /F

# Linux/Mac
lsof -ti:5000 | xargs kill -9
```

### Module Not Found

```bash
pip install -r requirements.txt --upgrade
```

### spaCy Model Missing

```bash
python -m spacy download en_core_web_sm
```

### Permission Errors

```bash
# Windows (Run as Administrator)
# Linux/Mac
sudo chown -R $USER:$USER ./uploads ./asm_model
```

---

## 📝 Development

### Add New API Endpoint

1. Edit `app.py`:

```python
@app.route('/api/new-endpoint', methods=['POST'])
def new_endpoint():
    data = request.json
    # Your logic here
    return jsonify({'success': True})
```

2. Add JavaScript function in `app.js`:

```javascript
async function callNewEndpoint() {
    const response = await fetch(`${API_BASE}/api/new-endpoint`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ key: 'value' })
    });
    return await response.json();
}
```

### Modify UI

- **HTML**: Edit `web/index.html`
- **CSS**: Edit `web/styles.css`
- **JavaScript**: Edit `web/app.js`

---

## 🔐 Security

### Production Checklist

- [ ] Set `DEBUG=False`
- [ ] Use environment variables for secrets
- [ ] Enable HTTPS
- [ ] Add authentication/authorization
- [ ] Implement rate limiting
- [ ] Sanitize file uploads
- [ ] Set CORS restrictions

### File Upload Security

- File type validation
- Size limits (configure in Flask)
- Unique filename generation
- Stored outside web root

---

## 📖 Additional Resources

- [ASM Core Documentation](../README.md)
- [Training Guide](../training/TRAINING_GUIDE.md)
- [API Documentation](#-api-endpoints)
- [Web Agent Guide](../WEB_AGENT_IMPLEMENTATION.md)

---

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

---

## 📄 License

This project is part of the ASM (Adaptive Sparse Mind) system.

---

## 🎯 Roadmap

- [ ] User authentication system
- [ ] Multi-user support
- [ ] Advanced analytics dashboard
- [ ] Model comparison tools
- [ ] Export/import functionality
- [ ] WebSocket support for real-time updates
- [ ] Mobile app (React Native)
- [ ] API rate limiting
- [ ] Comprehensive logging

---

**Made with ❤️ for the ASM Project**
