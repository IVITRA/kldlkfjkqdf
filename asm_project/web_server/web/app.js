// ============================================================================
// ASM Web Interface - JavaScript Client
// ============================================================================

const API_BASE = window.location.origin;

// ============================================================================
// Tab Navigation
// ============================================================================

document.addEventListener('DOMContentLoaded', () => {
    initializeTabs();
    initializeUploadZone();
    loadDashboardData();
    
    // Auto-refresh dashboard every 30 seconds
    setInterval(loadDashboardData, 30000);
});

function initializeTabs() {
    const navButtons = document.querySelectorAll('.nav-btn');
    
    navButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            const tabId = btn.dataset.tab;
            switchTab(tabId);
        });
    });
}

function switchTab(tabId) {
    // Update nav buttons
    document.querySelectorAll('.nav-btn').forEach(btn => {
        btn.classList.remove('active');
        if (btn.dataset.tab === tabId) {
            btn.classList.add('active');
        }
    });
    
    // Update tab content
    document.querySelectorAll('.tab-content').forEach(tab => {
        tab.classList.remove('active');
        if (tab.id === tabId) {
            tab.classList.add('active');
        }
    });
}

// ============================================================================
// File Upload
// ============================================================================

let selectedFiles = [];

function initializeUploadZone() {
    const uploadZone = document.getElementById('upload-zone');
    const fileInput = document.getElementById('file-input');
    
    uploadZone.addEventListener('click', () => {
        fileInput.click();
    });
    
    uploadZone.addEventListener('dragover', (e) => {
        e.preventDefault();
        uploadZone.classList.add('drag-over');
    });
    
    uploadZone.addEventListener('dragleave', () => {
        uploadZone.classList.remove('drag-over');
    });
    
    uploadZone.addEventListener('drop', (e) => {
        e.preventDefault();
        uploadZone.classList.remove('drag-over');
        handleFiles(e.dataTransfer.files);
    });
    
    fileInput.addEventListener('change', (e) => {
        handleFiles(e.target.files);
    });
}

function handleFiles(files) {
    selectedFiles = Array.from(files);
    displaySelectedFiles();
    document.getElementById('upload-btn').disabled = selectedFiles.length === 0;
}

function displaySelectedFiles() {
    const fileList = document.getElementById('file-list');
    fileList.innerHTML = '';
    
    selectedFiles.forEach((file, index) => {
        const fileItem = document.createElement('div');
        fileItem.className = 'file-item';
        fileItem.innerHTML = `
            <div class="file-info">
                <div class="file-name">${file.name}</div>
                <div class="file-meta">${formatFileSize(file.size)}</div>
            </div>
            <div class="file-actions">
                <button class="btn btn-danger" onclick="removeFile(${index})">❌ Remove</button>
            </div>
        `;
        fileList.appendChild(fileItem);
    });
}

function removeFile(index) {
    selectedFiles.splice(index, 1);
    displaySelectedFiles();
    document.getElementById('upload-btn').disabled = selectedFiles.length === 0;
}

async function uploadFiles() {
    if (selectedFiles.length === 0) {
        showToast('No files selected', 'error');
        return;
    }
    
    const domain = document.getElementById('upload-domain').value;
    const formData = new FormData();
    
    selectedFiles.forEach(file => {
        formData.append('files', file);
    });
    formData.append('domain', domain);
    
    try {
        showToast('Uploading files...', 'info');
        
        const response = await fetch(`${API_BASE}/api/upload`, {
            method: 'POST',
            body: formData
        });
        
        const result = await response.json();
        
        if (result.success) {
            showToast(`Successfully uploaded ${result.count} files`, 'success');
            selectedFiles = [];
            displaySelectedFiles();
            document.getElementById('upload-btn').disabled = true;
            loadFiles();
        } else {
            showToast(result.error || 'Upload failed', 'error');
        }
    } catch (error) {
        showToast(`Upload error: ${error.message}`, 'error');
    }
}

async function loadFiles() {
    try {
        const response = await fetch(`${API_BASE}/api/files`);
        const result = await response.json();
        
        const filesList = document.getElementById('uploaded-files-list');
        
        if (result.files.length === 0) {
            filesList.innerHTML = '<p class="text-muted text-center">No files uploaded yet</p>';
            return;
        }
        
        filesList.innerHTML = result.files.map(file => `
            <div class="file-item">
                <div class="file-info">
                    <div class="file-name">${file.name}</div>
                    <div class="file-meta">
                        ${formatFileSize(file.size)} | Modified: ${new Date(file.modified).toLocaleString()}
                    </div>
                </div>
                <div class="file-actions">
                    <button class="btn btn-danger" onclick="deleteFile('${file.name}')">🗑️ Delete</button>
                </div>
            </div>
        `).join('');
    } catch (error) {
        showToast(`Error loading files: ${error.message}`, 'error');
    }
}

async function deleteFile(filename) {
    if (!confirm(`Are you sure you want to delete ${filename}?`)) {
        return;
    }
    
    try {
        const response = await fetch(`${API_BASE}/api/files/${filename}`, {
            method: 'DELETE'
        });
        
        const result = await response.json();
        
        if (result.success) {
            showToast('File deleted', 'success');
            loadFiles();
        } else {
            showToast(result.error || 'Delete failed', 'error');
        }
    } catch (error) {
        showToast(`Error deleting file: ${error.message}`, 'error');
    }
}

// ============================================================================
// Training
// ============================================================================

async function startTraining() {
    const config = {
        task: document.getElementById('training-task').value,
        base_model: document.getElementById('base-model').value,
        output_dir: document.getElementById('output-dir').value,
        use_teacher: document.getElementById('use-teacher').checked,
        use_ocr: document.getElementById('use-ocr').checked
    };
    
    try {
        showToast('Starting training...', 'info');
        
        const response = await fetch(`${API_BASE}/api/train`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(config)
        });
        
        const result = await response.json();
        
        if (result.success) {
            showToast('Training started successfully!', 'success');
            document.getElementById('start-training-btn').disabled = true;
            document.getElementById('cancel-training-btn').style.display = 'inline-flex';
            
            // Start monitoring progress
            monitorTrainingProgress();
        } else {
            showToast(result.error || 'Training failed to start', 'error');
        }
    } catch (error) {
        showToast(`Training error: ${error.message}`, 'error');
    }
}

async function monitorTrainingProgress() {
    const interval = setInterval(async () => {
        try {
            const response = await fetch(`${API_BASE}/api/training/status`);
            const status = await response.json();
            
            updateTrainingProgress(status);
            
            if (!status.is_training) {
                clearInterval(interval);
                document.getElementById('start-training-btn').disabled = false;
                document.getElementById('cancel-training-btn').style.display = 'none';
                
                if (status.message.includes('completed')) {
                    showToast('Training completed successfully!', 'success');
                } else if (status.message.includes('failed') || status.message.includes('cancelled')) {
                    showToast(status.message, 'error');
                }
            }
        } catch (error) {
            console.error('Error monitoring training:', error);
        }
    }, 2000);
}

function updateTrainingProgress(status) {
    const progressBar = document.getElementById('progress-bar');
    const progressText = document.getElementById('progress-text');
    const message = document.getElementById('training-message');
    
    progressBar.style.width = `${status.progress}%`;
    progressText.textContent = `${status.progress.toFixed(1)}%`;
    message.textContent = status.message || 'Training in progress...';
}

async function cancelTraining() {
    if (!confirm('Are you sure you want to cancel training?')) {
        return;
    }
    
    try {
        const response = await fetch(`${API_BASE}/api/training/cancel`, {
            method: 'POST'
        });
        
        const result = await response.json();
        
        if (result.success) {
            showToast('Training cancelled', 'warning');
        }
    } catch (error) {
        showToast(`Error cancelling training: ${error.message}`, 'error');
    }
}

// ============================================================================
// Chat
// ============================================================================

async function sendMessage() {
    const input = document.getElementById('chat-input');
    const domain = document.getElementById('chat-domain').value;
    const message = input.value.trim();
    
    if (!message) {
        return;
    }
    
    // Add user message
    addChatMessage('user', message);
    input.value = '';
    
    try {
        const response = await fetch(`${API_BASE}/api/chat`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                query: message,
                domain: domain
            })
        });
        
        const result = await response.json();
        
        if (result.response) {
            addChatMessage('bot', result.response, {
                confidence: result.confidence,
                experts: result.experts_used
            });
        } else {
            addChatMessage('bot', 'Error: ' + (result.error || 'No response'));
        }
    } catch (error) {
        addChatMessage('bot', `Error: ${error.message}`);
    }
}

function addChatMessage(role, content, metadata = {}) {
    const messagesContainer = document.getElementById('chat-messages');
    
    const messageDiv = document.createElement('div');
    messageDiv.className = `message ${role}`;
    
    let metaHtml = '';
    if (metadata.confidence) {
        metaHtml = `
            <div class="message-meta">
                Confidence: ${(metadata.confidence * 100).toFixed(1)}% | 
                Experts: ${Array.isArray(metadata.experts) ? metadata.experts.join(', ') : metadata.experts}
            </div>
        `;
    }
    
    messageDiv.innerHTML = `
        <div class="message-content">${content}</div>
        ${metaHtml}
    `;
    
    messagesContainer.appendChild(messageDiv);
    messagesContainer.scrollTop = messagesContainer.scrollHeight;
}

function handleChatKeypress(event) {
    if (event.key === 'Enter' && !event.shiftKey) {
        event.preventDefault();
        sendMessage();
    }
}

// ============================================================================
// Experts Management
// ============================================================================

async function loadExperts() {
    try {
        const response = await fetch(`${API_BASE}/api/experts`);
        const result = await response.json();
        
        const expertsList = document.getElementById('experts-list');
        
        if (result.experts.length === 0) {
            expertsList.innerHTML = '<p class="text-muted text-center">No experts trained yet</p>';
            return;
        }
        
        expertsList.innerHTML = result.experts.map(expert => `
            <div class="expert-card">
                <div class="expert-info">
                    <h4>🧠 ${expert.name}</h4>
                    <div class="expert-meta">
                        Size: ${formatFileSize(expert.size)} | 
                        Created: ${new Date(expert.created).toLocaleString()}
                    </div>
                </div>
                <div class="file-actions">
                    <button class="btn btn-danger" onclick="deleteExpert('${expert.name}')">🗑️ Delete</button>
                </div>
            </div>
        `).join('');
    } catch (error) {
        showToast(`Error loading experts: ${error.message}`, 'error');
    }
}

async function deleteExpert(name) {
    if (!confirm(`Are you sure you want to delete expert "${name}"?`)) {
        return;
    }
    
    try {
        const response = await fetch(`${API_BASE}/api/experts/${name}`, {
            method: 'DELETE'
        });
        
        const result = await response.json();
        
        if (result.success) {
            showToast(`Expert "${name}" deleted`, 'success');
            loadExperts();
        } else {
            showToast(result.error || 'Delete failed', 'error');
        }
    } catch (error) {
        showToast(`Error deleting expert: ${error.message}`, 'error');
    }
}

// ============================================================================
// Self-Learning
// ============================================================================

async function startLearning() {
    try {
        showToast('Starting background learning...', 'info');
        
        const response = await fetch(`${API_BASE}/api/learning/start`, {
            method: 'POST'
        });
        
        const result = await response.json();
        
        if (result.success) {
            showToast('Background learning started!', 'success');
            document.getElementById('start-learning-btn').style.display = 'none';
            document.getElementById('stop-learning-btn').style.display = 'inline-flex';
            updateLearningStatus(true);
        } else {
            showToast(result.error || 'Failed to start learning', 'error');
        }
    } catch (error) {
        showToast(`Error: ${error.message}`, 'error');
    }
}

async function stopLearning() {
    try {
        const response = await fetch(`${API_BASE}/api/learning/stop`, {
            method: 'POST'
        });
        
        const result = await response.json();
        
        if (result.success) {
            showToast('Background learning stopped', 'warning');
            document.getElementById('start-learning-btn').style.display = 'inline-flex';
            document.getElementById('stop-learning-btn').style.display = 'none';
            updateLearningStatus(false);
        }
    } catch (error) {
        showToast(`Error: ${error.message}`, 'error');
    }
}

function updateLearningStatus(isRunning) {
    const statusDisplay = document.getElementById('learning-status-display');
    statusDisplay.innerHTML = `
        <p><strong>Status:</strong> ${isRunning ? '✅ Running' : '❌ Not running'}</p>
        ${isRunning ? `
            <p class="text-muted mt-20">
                Schedule:<br>
                • Hourly: Process failed queries<br>
                • Daily (3 AM): Deep learning<br>
                • Weekly (Sunday): Knowledge consolidation
            </p>
        ` : ''}
    `;
}

async function addFailedQuery() {
    const question = document.getElementById('failed-query').value.trim();
    const domain = document.getElementById('failed-query-domain').value;
    
    if (!question) {
        showToast('Please enter a question', 'error');
        return;
    }
    
    try {
        const response = await fetch(`${API_BASE}/api/learning/add-query`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                question: question,
                domain: domain
            })
        });
        
        const result = await response.json();
        
        if (result.success) {
            showToast('Query added to learning queue', 'success');
            document.getElementById('failed-query').value = '';
        } else {
            showToast(result.error || 'Failed to add query', 'error');
        }
    } catch (error) {
        showToast(`Error: ${error.message}`, 'error');
    }
}

async function validateSource() {
    const url = document.getElementById('source-url').value.trim();
    
    if (!url) {
        showToast('Please enter a URL', 'error');
        return;
    }
    
    try {
        showToast('Validating source...', 'info');
        
        const response = await fetch(`${API_BASE}/api/learning/validate-source`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ url: url })
        });
        
        const result = await response.json();
        
        const resultDiv = document.getElementById('validation-result');
        resultDiv.className = `validation-result ${result.is_valid ? 'valid' : 'invalid'}`;
        
        resultDiv.innerHTML = `
            <h4>${result.is_valid ? '✅ Valid Source' : '❌ Invalid Source'}</h4>
            <p><strong>Credibility Score:</strong> ${(result.credibility_score * 100).toFixed(1)}%</p>
            ${result.reasons.length > 0 ? `
                <p><strong>Reasons:</strong></p>
                <ul>
                    ${result.reasons.map(r => `<li>${r}</li>`).join('')}
                </ul>
            ` : ''}
            ${result.warnings.length > 0 ? `
                <p><strong>Warnings:</strong></p>
                <ul style="color: var(--danger-color);">
                    ${result.warnings.map(w => `<li>${w}</li>`).join('')}
                </ul>
            ` : ''}
        `;
    } catch (error) {
        showToast(`Error: ${error.message}`, 'error');
    }
}

// ============================================================================
// Dashboard
// ============================================================================

async function loadDashboardData() {
    try {
        const response = await fetch(`${API_BASE}/api/status`);
        const status = await response.json();
        
        // Update stats
        document.getElementById('cpu-usage').textContent = `${status.system.cpu_percent}%`;
        document.getElementById('memory-usage').textContent = `${status.system.memory.used_gb} GB`;
        document.getElementById('disk-usage').textContent = `${status.system.disk.percent}%`;
        document.getElementById('experts-count').textContent = status.files.models;
        document.getElementById('files-count').textContent = status.files.uploaded;
        document.getElementById('learning-status').textContent = status.learning.active ? '✅ Active' : '❌ Inactive';
        
        // Update status display
        const statusDiv = document.getElementById('system-status');
        statusDiv.innerHTML = `
            <p><strong>Training:</strong> ${status.training.active ? '🔄 Active' : '⏸️ Idle'}</p>
            <p><strong>Learning:</strong> ${status.learning.active ? '🧠 Running' : '⏹️ Stopped'}</p>
            <p><strong>Files:</strong> ${status.files.uploaded} uploaded, ${status.files.models} models</p>
            <p class="text-muted mt-20">Last updated: ${new Date(status.timestamp).toLocaleString()}</p>
        `;
    } catch (error) {
        console.error('Error loading dashboard:', error);
    }
}

async function refreshStatus() {
    showToast('Refreshing status...', 'info');
    await loadDashboardData();
    showToast('Status updated', 'success');
}

// ============================================================================
// Utility Functions
// ============================================================================

function formatFileSize(bytes) {
    if (bytes === 0) return '0 Bytes';
    
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    
    return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
}

function showToast(message, type = 'info') {
    const container = document.getElementById('toast-container');
    
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.innerHTML = `<div class="toast-message">${message}</div>`;
    
    container.appendChild(toast);
    
    // Remove after 4 seconds
    setTimeout(() => {
        toast.style.opacity = '0';
        toast.style.transform = 'translateX(100%)';
        setTimeout(() => toast.remove(), 300);
    }, 4000);
}

function showAbout() {
    alert('ASM - Adaptive Sparse Mind\n500B Parameters on 4GB RAM\n\nA revolutionary AI system that runs on weak hardware using sparse expert activation and advanced quantization.');
}

function showHelp() {
    alert('Help:\n\n1. Upload files in the Upload tab\n2. Train experts in the Training tab\n3. Chat with ASM in the Chat tab\n4. Manage trained experts in the Experts tab\n5. Enable self-learning for continuous improvement\n\nFor more help, check the documentation.');
}
