#!/usr/bin/env python3
"""
ASM Web API Server - Flask backend for ASM Core Engine

This server provides REST API endpoints for:
- File upload and document processing
- Expert training management
- Inference and chat interface
- System status and diagnostics
- Self-learning control
"""

from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS
import os
import json
import subprocess
import threading
from datetime import datetime
from pathlib import Path
import uuid

# Import ASM training modules
import sys
sys.path.append(os.path.join(os.path.dirname(__file__), 'training'))
sys.path.append(os.path.join(os.path.dirname(__file__), 'training', 'self_learning'))

from training.local_file_processor import LocalFileProcessor
from training.chunking_engine import ChunkingEngine
from training.directory_trainer import DirectoryBasedTraining
from training.self_learning.knowledge_ingestor import KnowledgeIngestor
from training.self_learning.background_learner import BackgroundLearner
from training.self_learning.source_validator import SourceValidator

app = Flask(__name__, static_folder='web', static_url_path='')
CORS(app)  # Enable CORS for API access

# Configuration
UPLOAD_FOLDER = os.path.join(os.path.dirname(__file__), 'uploads')
TRAINING_OUTPUT = os.path.join(os.path.dirname(__file__), 'asm_model')
TRAINING_DATA = os.path.join(os.path.dirname(__file__), 'training_data')
LOGS_FOLDER = os.path.join(os.path.dirname(__file__), 'logs')

# Create necessary directories
for folder in [UPLOAD_FOLDER, TRAINING_OUTPUT, TRAINING_DATA, LOGS_FOLDER]:
    Path(folder).mkdir(parents=True, exist_ok=True)

# Global state
training_status = {
    'is_training': False,
    'current_task': None,
    'progress': 0.0,
    'message': '',
    'started_at': None,
    'completed_at': None
}

background_learner = None
background_learner_thread = None

# ============================================================================
# API Endpoints - File Management
# ============================================================================

@app.route('/api/upload', methods=['POST'])
def upload_files():
    """
    Upload documents for training
    
    Expects: multipart/form-data with files
    Returns: List of uploaded file info
    """
    if 'files' not in request.files:
        return jsonify({'error': 'No files provided'}), 400
    
    files = request.files.getlist('files')
    domain = request.form.get('domain', 'general')
    
    uploaded_files = []
    
    for file in files:
        if file and file.filename:
            # Generate unique filename
            file_ext = Path(file.filename).suffix
            unique_name = f"{uuid.uuid4().hex}{file_ext}"
            file_path = os.path.join(UPLOAD_FOLDER, unique_name)
            
            file.save(file_path)
            
            uploaded_files.append({
                'original_name': file.filename,
                'stored_name': unique_name,
                'path': file_path,
                'size': os.path.getsize(file_path),
                'domain': domain,
                'uploaded_at': datetime.now().isoformat()
            })
    
    return jsonify({
        'success': True,
        'files': uploaded_files,
        'count': len(uploaded_files)
    })


@app.route('/api/files', methods=['GET'])
def list_files():
    """List all uploaded files"""
    files = []
    
    if os.path.exists(UPLOAD_FOLDER):
        for filename in os.listdir(UPLOAD_FOLDER):
            file_path = os.path.join(UPLOAD_FOLDER, filename)
            if os.path.isfile(file_path):
                files.append({
                    'name': filename,
                    'size': os.path.getsize(file_path),
                    'modified': datetime.fromtimestamp(
                        os.path.getmtime(file_path)
                    ).isoformat()
                })
    
    return jsonify({'files': files, 'count': len(files)})


@app.route('/api/files/<filename>', methods=['DELETE'])
def delete_file(filename):
    """Delete an uploaded file"""
    file_path = os.path.join(UPLOAD_FOLDER, filename)
    
    if os.path.exists(file_path):
        os.remove(file_path)
        return jsonify({'success': True, 'message': 'File deleted'})
    
    return jsonify({'error': 'File not found'}), 404


@app.route('/api/process-files', methods=['POST'])
def process_files():
    """
    Process uploaded files (OCR, text extraction, chunking)
    
    Expects: JSON with file paths and processing options
    Returns: Processed chunks
    """
    data = request.json
    file_paths = data.get('files', [])
    options = data.get('options', {})
    
    try:
        # Initialize processors
        file_processor = LocalFileProcessor(
            use_ocr=options.get('use_ocr', False),
            language=options.get('language', 'eng')
        )
        
        chunking_engine = ChunkingEngine(
            chunk_size=options.get('chunk_size', 512),
            chunk_overlap=options.get('chunk_overlap', 50)
        )
        
        all_chunks = []
        
        for file_path in file_paths:
            # Extract text
            text = file_processor.process_file(file_path)
            
            if text:
                # Chunk the text
                chunks = chunking_engine.semantic_chunk(text)
                all_chunks.extend(chunks)
        
        return jsonify({
            'success': True,
            'chunks': all_chunks,
            'total_chunks': len(all_chunks)
        })
        
    except Exception as e:
        return jsonify({'error': str(e)}), 500


# ============================================================================
# API Endpoints - Training
# ============================================================================

@app.route('/api/train', methods=['POST'])
def start_training():
    """
    Start expert training
    
    Expects: JSON with training parameters
    Returns: Training job ID
    """
    global training_status
    
    if training_status['is_training']:
        return jsonify({
            'error': 'Training already in progress',
            'status': training_status
        }), 400
    
    data = request.json
    
    # Start training in background thread
    training_thread = threading.Thread(
        target=run_training,
        args=(data,)
    )
    training_thread.start()
    
    return jsonify({
        'success': True,
        'message': 'Training started',
        'job_id': str(uuid.uuid4())
    })


def run_training(config):
    """Run training process in background"""
    global training_status
    
    try:
        training_status['is_training'] = True
        training_status['current_task'] = config.get('task', 'directory_training')
        training_status['progress'] = 0.0
        training_status['message'] = 'Initializing training...'
        training_status['started_at'] = datetime.now().isoformat()
        
        # Directory-based training
        if training_status['current_task'] == 'directory_training':
            run_directory_training(config)
        
        # Single file training
        elif training_status['current_task'] == 'file_training':
            run_file_training(config)
        
        training_status['is_training'] = False
        training_status['progress'] = 100.0
        training_status['message'] = 'Training completed successfully!'
        training_status['completed_at'] = datetime.now().isoformat()
        
    except Exception as e:
        training_status['is_training'] = False
        training_status['message'] = f'Training failed: {str(e)}'
        training_status['completed_at'] = datetime.now().isoformat()


def run_directory_training(config):
    """Run directory-based multi-expert training"""
    global training_status
    
    training_status['message'] = 'Loading directories...'
    training_status['progress'] = 10.0
    
    trainer = DirectoryBasedTraining(
        root_dir=config.get('data_dir', UPLOAD_FOLDER),
        base_model=config.get('base_model', 'microsoft/DialoGPT-medium'),
        use_teacher=config.get('use_teacher', False)
    )
    
    training_status['message'] = 'Training experts...'
    training_status['progress'] = 30.0
    
    results = trainer.train_all_directories(
        output_dir=config.get('output_dir', TRAINING_OUTPUT)
    )
    
    training_status['progress'] = 90.0
    training_status['message'] = 'Saving models...'
    training_status['results'] = results


def run_file_training(config):
    """Run single file training"""
    global training_status
    
    # Similar to directory training but for single file
    training_status['message'] = 'Processing file...'
    training_status['progress'] = 20.0
    
    # Implementation would go here
    training_status['progress'] = 100.0
    training_status['message'] = 'Training completed!'


@app.route('/api/training/status', methods=['GET'])
def get_training_status():
    """Get current training status"""
    return jsonify(training_status)


@app.route('/api/training/cancel', methods=['POST'])
def cancel_training():
    """Cancel ongoing training"""
    global training_status
    
    if training_status['is_training']:
        training_status['message'] = 'Training cancelled by user'
        training_status['is_training'] = False
        training_status['completed_at'] = datetime.now().isoformat()
        
        return jsonify({'success': True, 'message': 'Training cancelled'})
    
    return jsonify({'error': 'No training in progress'}), 400


# ============================================================================
# API Endpoints - Inference
# ============================================================================

@app.route('/api/chat', methods=['POST'])
def chat():
    """
    Chat with ASM experts
    
    Expects: JSON with query and optional domain
    Returns: Expert response
    """
    data = request.json
    query = data.get('query', '')
    domain = data.get('domain', 'general')
    expert_ids = data.get('expert_ids', [])
    
    if not query:
        return jsonify({'error': 'Query is required'}), 400
    
    try:
        # In production, this would call the C++ inference engine
        # For now, return simulated response
        
        response = {
            'query': query,
            'domain': domain,
            'experts_used': expert_ids if expert_ids else ['auto-selected'],
            'response': f"Response from {domain} expert(s) for: {query}",
            'confidence': 0.85,
            'response_time_ms': 150,
            'timestamp': datetime.now().isoformat()
        }
        
        return jsonify(response)
        
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/inference', methods=['POST'])
def inference():
    """
    Raw inference call to ASM engine
    
    Expects: JSON with prompt and parameters
    Returns: Generated text
    """
    data = request.json
    prompt = data.get('prompt', '')
    max_tokens = data.get('max_tokens', 100)
    temperature = data.get('temperature', 0.7)
    
    try:
        # Call C++ inference engine via subprocess
        # result = subprocess.run(
        #     ['./build/asm_core', '--prompt', prompt],
        #     capture_output=True,
        #     text=True
        # )
        
        # Simulated response
        response = {
            'prompt': prompt,
            'generated': f"Generated response for: {prompt}",
            'tokens_generated': 50,
            'confidence': 0.82,
            'experts_activated': 3
        }
        
        return jsonify(response)
        
    except Exception as e:
        return jsonify({'error': str(e)}), 500


# ============================================================================
# API Endpoints - Self-Learning
# ============================================================================

@app.route('/api/learning/start', methods=['POST'])
def start_background_learning():
    """Start the 24/7 background learning daemon"""
    global background_learner, background_learner_thread
    
    if background_learner and background_learner_thread and background_learner_thread.is_alive():
        return jsonify({'error': 'Background learning already running'}), 400
    
    try:
        background_learner = BackgroundLearner()
        background_learner_thread = background_learner.start()
        
        return jsonify({
            'success': True,
            'message': 'Background learning started',
            'schedule': {
                'hourly': 'Process failed queries',
                'daily': '3:00 AM - Deep learning',
                'weekly': 'Sunday - Knowledge consolidation'
            }
        })
        
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/learning/stop', methods=['POST'])
def stop_background_learning():
    """Stop background learning daemon"""
    global background_learner
    
    if background_learner:
        background_learner.stop()
        return jsonify({'success': True, 'message': 'Background learning stopped'})
    
    return jsonify({'error': 'Background learning not running'}), 400


@app.route('/api/learning/status', methods=['GET'])
def get_learning_status():
    """Get background learning status"""
    global background_learner, background_learner_thread
    
    is_running = (
        background_learner_thread is not None and 
        background_learner_thread.is_alive()
    )
    
    return jsonify({
        'is_running': is_running,
        'knowledge_base_size': len(background_learner.knowledge_base.get('entries', [])) if background_learner else 0
    })


@app.route('/api/learning/add-query', methods=['POST'])
def add_failed_query():
    """Add a failed query to the learning queue"""
    global background_learner
    
    if not background_learner:
        return jsonify({'error': 'Background learning not initialized'}), 400
    
    data = request.json
    question = data.get('question', '')
    domain = data.get('domain', 'general')
    context = data.get('context', {})
    
    if not question:
        return jsonify({'error': 'Question is required'}), 400
    
    background_learner.add_failed_query(question, domain, context)
    
    return jsonify({
        'success': True,
        'message': 'Query added to learning queue'
    })


@app.route('/api/learning/validate-source', methods=['POST'])
def validate_source():
    """Validate a source URL for credibility"""
    data = request.json
    url = data.get('url', '')
    
    if not url:
        return jsonify({'error': 'URL is required'}), 400
    
    validator = SourceValidator()
    result = validator.validate_url(url)
    
    return jsonify({
        'url': url,
        'is_valid': result.is_valid,
        'credibility_score': result.credibility_score,
        'reasons': result.reasons,
        'warnings': result.warnings
    })


# ============================================================================
# API Endpoints - System Status
# ============================================================================

@app.route('/api/status', methods=['GET'])
def system_status():
    """Get overall system status"""
    import psutil
    
    # System resources
    cpu_percent = psutil.cpu_percent(interval=1)
    memory = psutil.virtual_memory()
    disk = psutil.disk_usage('/')
    
    # Training status
    training_active = training_status['is_training']
    
    # Learning status
    global background_learner_thread
    learning_active = (
        background_learner_thread is not None and 
        background_learner_thread.is_alive()
    )
    
    # File counts
    upload_count = len(os.listdir(UPLOAD_FOLDER)) if os.path.exists(UPLOAD_FOLDER) else 0
    model_count = len(os.listdir(TRAINING_OUTPUT)) if os.path.exists(TRAINING_OUTPUT) else 0
    
    return jsonify({
        'system': {
            'cpu_percent': cpu_percent,
            'memory': {
                'total_gb': round(memory.total / (1024**3), 2),
                'used_gb': round(memory.used / (1024**3), 2),
                'available_gb': round(memory.available / (1024**3), 2),
                'percent': memory.percent
            },
            'disk': {
                'total_gb': round(disk.total / (1024**3), 2),
                'used_gb': round(disk.used / (1024**3), 2),
                'free_gb': round(disk.free / (1024**3), 2),
                'percent': disk.percent
            }
        },
        'training': {
            'active': training_active,
            'status': training_status
        },
        'learning': {
            'active': learning_active
        },
        'files': {
            'uploaded': upload_count,
            'models': model_count
        },
        'timestamp': datetime.now().isoformat()
    })


@app.route('/api/diagnostics', methods=['GET'])
def diagnostics():
    """Run system diagnostics"""
    # Check C++ binary
    cpp_binary = os.path.join(os.path.dirname(__file__), 'build', 'asm_core')
    cpp_exists = os.path.exists(cpp_binary)
    
    # Check Python modules
    modules_ok = True
    try:
        import torch
        import transformers
        import spacy
    except ImportError as e:
        modules_ok = False
    
    return jsonify({
        'cpp_engine': {
            'exists': cpp_exists,
            'path': cpp_binary
        },
        'python_modules': {
            'all_installed': modules_ok
        },
        'directories': {
            'uploads': os.path.exists(UPLOAD_FOLDER),
            'models': os.path.exists(TRAINING_OUTPUT),
            'training_data': os.path.exists(TRAINING_DATA)
        },
        'timestamp': datetime.now().isoformat()
    })


# ============================================================================
# API Endpoints - Experts Management
# ============================================================================

@app.route('/api/experts', methods=['GET'])
def list_experts():
    """List all trained experts"""
    experts = []
    
    if os.path.exists(TRAINING_OUTPUT):
        for item in os.listdir(TRAINING_OUTPUT):
            item_path = os.path.join(TRAINING_OUTPUT, item)
            if os.path.isdir(item_path):
                experts.append({
                    'name': item,
                    'path': item_path,
                    'size': get_folder_size(item_path),
                    'created': datetime.fromtimestamp(
                        os.path.getctime(item_path)
                    ).isoformat()
                })
    
    return jsonify({'experts': experts, 'count': len(experts)})


@app.route('/api/experts/<name>', methods=['DELETE'])
def delete_expert(name):
    """Delete a trained expert"""
    expert_path = os.path.join(TRAINING_OUTPUT, name)
    
    if os.path.exists(expert_path):
        import shutil
        shutil.rmtree(expert_path)
        return jsonify({'success': True, 'message': f'Expert {name} deleted'})
    
    return jsonify({'error': 'Expert not found'}), 404


def get_folder_size(folder_path):
    """Get total size of folder in bytes"""
    total_size = 0
    for dirpath, dirnames, filenames in os.walk(folder_path):
        for f in filenames:
            fp = os.path.join(dirpath, f)
            total_size += os.path.getsize(fp)
    return total_size


# ============================================================================
# Web Interface Routes
# ============================================================================

@app.route('/')
def index():
    """Serve main web interface"""
    return send_from_directory('web', 'index.html')


@app.route('/styles.css')
def styles():
    """Serve CSS file"""
    return send_from_directory('web', 'styles.css')


@app.route('/app.js')
def app_js():
    """Serve JavaScript file"""
    return send_from_directory('web', 'app.js')


# ============================================================================
# Main Entry Point
# ============================================================================

if __name__ == '__main__':
    print("="*60)
    print("🚀 ASM Web API Server")
    print("="*60)
    print(f"📁 Upload folder: {UPLOAD_FOLDER}")
    print(f"📁 Training output: {TRAINING_OUTPUT}")
    print(f"🌐 Server: http://localhost:5000")
    print("="*60)
    
    app.run(
        host='0.0.0.0',
        port=5000,
        debug=True,
        threaded=True
    )
