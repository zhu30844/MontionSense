// MotionSense Stream App
class StreamApp {
    constructor() {
        this.streamImage = null;
        this.statusText = null;
        this.statusDot = null;
        this.isStreaming = false;
        this.retryCount = 0;
        this.maxRetries = 3;
        this.retryInterval = 5000; // 5 seconds
    }

    init() {
        this.streamImage = document.getElementById('streamImage');
        this.statusText = document.getElementById('statusText');
        this.statusDot = document.querySelector('.status-dot');
        
        if (this.streamImage) {
            this.initStream();
        }
    }

    initStream() {
        // Set up error handling for the stream image
        this.streamImage.addEventListener('load', () => {
            this.onStreamSuccess();
        });

        this.streamImage.addEventListener('error', () => {
            this.onStreamError();
        });

        // Start monitoring stream health
        this.startHealthCheck();
    }

    onStreamSuccess() {
        this.isStreaming = true;
        this.retryCount = 0;
        this.updateStatus('connected', '已连接');
        
        // Add success animation
        this.streamImage.style.opacity = '1';
        this.streamImage.style.transform = 'scale(1)';
    }

    onStreamError() {
        this.isStreaming = false;
        this.updateStatus('error', '连接失败');
        
        // Add error animation
        this.streamImage.style.opacity = '0.7';
        this.streamImage.style.transform = 'scale(0.98)';
        
        // Retry logic
        if (this.retryCount < this.maxRetries) {
            this.retryCount++;
            setTimeout(() => {
                this.retryStream();
            }, this.retryInterval);
        }
    }

    retryStream() {
        if (!this.isStreaming) {
            this.updateStatus('connecting', '重连中...');
            
            // Force reload the image
            const currentSrc = this.streamImage.src;
            this.streamImage.src = '';
            setTimeout(() => {
                this.streamImage.src = currentSrc + '?t=' + Date.now();
            }, 100);
        }
    }

    updateStatus(status, text) {
        if (this.statusText) {
            this.statusText.textContent = text;
        }
        
        if (this.statusDot) {
            this.statusDot.className = 'status-dot';
            this.statusDot.classList.add(`status-${status}`);
        }
    }

    startHealthCheck() {
        // Periodically check if the stream is still working
        setInterval(() => {
            if (this.isStreaming) {
                // Check if the image is still loading properly
                const img = new Image();
                img.onload = () => {
                    // Stream is still working
                    this.updateStatus('connected', '已连接');
                };
                img.onerror = () => {
                    // Stream has stopped working
                    this.onStreamError();
                };
                img.src = this.streamImage.src + '?health=' + Date.now();
            }
        }, 30000); // Check every 30 seconds
    }

    // Public method to manually refresh stream
    refreshStream() {
        if (this.streamImage) {
            this.updateStatus('connecting', '刷新中...');
            this.streamImage.src = this.streamImage.src + '?refresh=' + Date.now();
        }
    }

    // Public method to get stream status
    getStreamStatus() {
        return {
            isStreaming: this.isStreaming,
            retryCount: this.retryCount,
            maxRetries: this.maxRetries
        };
    }
}

// Initialize stream app
document.addEventListener('DOMContentLoaded', () => {
    window.streamApp = new StreamApp();
}); 