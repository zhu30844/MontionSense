// MotionSense App - Main Application Logic
class MotionSenseApp {
    constructor() {
        this.currentLang = 'en';
        this.currentView = 'live';
        this.statusTimer = null;
        this.translations = {
            zh: {
                logoText: 'MotionSense',
                langText: 'English',
                navLive: '实时监控',
                navCalendar: '历史记录',
                streamTitle: '实时视频流',
                fullscreenText: '全屏',
                calendarTitle: '运动检测热力图',
                calendarDesc: '点击日期查看详细记录',
                statusText: '连接中',
                statusStreaming: '视频流正常',
                statusConnecting: '连接中',
                deviceOnline: '在线',
                deviceOffline: '离线',
                loadingText: '加载中...',
                statusCardTitle: '设备状态',
                labelCpuTemp: '温度',
                labelWorkLoad: '负载',
                labelMemory: '内存',
                labelClients: '观看数',
                labelSystemUptime: '系统运行',
                labelLastRecord: '最近录像'
            },
            en: {
                logoText: 'MotionSense',
                langText: '简体中文',
                navLive: 'Live Stream',
                navCalendar: 'History',
                streamTitle: 'Live Video Stream',
                fullscreenText: 'Fullscreen',
                calendarTitle: 'Motion Detection Heatmap',
                calendarDesc: 'Click on a date to view detailed records',
                statusText: 'Connecting',
                statusStreaming: 'Streaming',
                statusConnecting: 'Connecting',
                deviceOnline: 'Online',
                deviceOffline: 'Offline',
                loadingText: 'Loading...',
                statusCardTitle: 'Device Status',
                labelCpuTemp: 'Temperature',
                labelWorkLoad: 'Load',
                labelMemory: 'Memory',
                labelClients: 'Viewers',
                labelSystemUptime: 'Uptime',
                labelLastRecord: 'Last Recording'
            }
        };
        
        this.init();
    }

    init() {
        this.bindEvents();
        this.updateLanguage(this.currentLang);
        this.showView(this.currentView);
        this.initStreamStatus();
        this.initDeviceStatus();
    }

    bindEvents() {
        // Language toggle
        document.getElementById('languageToggle').addEventListener('click', () => {
            this.toggleLanguage();
        });

        // Navigation
        document.querySelectorAll('.nav-item').forEach(item => {
            item.addEventListener('click', (e) => {
                const view = e.currentTarget.dataset.view;
                this.showView(view);
            });
        });

        // Fullscreen button
        document.getElementById('fullscreenBtn').addEventListener('click', () => {
            this.toggleFullscreen();
        });

        // Keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            this.handleKeyboardShortcuts(e);
        });
    }

    toggleLanguage() {
        this.currentLang = this.currentLang === 'zh' ? 'en' : 'zh';
        this.updateLanguage(this.currentLang);
        
        // Update calendar language if it exists
        if (window.calendarApp) {
            window.calendarApp.updateLanguage(this.currentLang);
        }
    }

    // ---- Device status card ----

    initDeviceStatus() {
        this.startDeviceStatus();

        // Stop polling whenever the card cannot be seen. Fullscreen covers it,
        // and so does switching to the calendar or backgrounding the tab.
        // Listening for fullscreenchange rather than hooking the button catches
        // Esc, F11 and anything the browser initiates on its own.
        document.addEventListener('fullscreenchange', () => this.syncDeviceStatusPolling());
        document.addEventListener('visibilitychange', () => this.syncDeviceStatusPolling());
    }

    // The card lives in the live view, is hidden by fullscreen, and is not
    // worth refreshing while the tab is in the background.
    deviceStatusVisible() {
        return !document.fullscreenElement &&
               !document.hidden &&
               this.currentView === 'live';
    }

    syncDeviceStatusPolling() {
        if (this.deviceStatusVisible()) {
            this.startDeviceStatus();
        } else {
            this.stopDeviceStatus();
        }
    }

    startDeviceStatus() {
        if (this.statusTimer) return;
        this.fetchDeviceStatus();
        this.statusTimer = setInterval(() => this.fetchDeviceStatus(), 2000);
    }

    stopDeviceStatus() {
        if (!this.statusTimer) return;
        clearInterval(this.statusTimer);
        this.statusTimer = null;
    }

    async fetchDeviceStatus() {
        try {
            const response = await fetch('/api/status');
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            this.renderDeviceStatus(await response.json());
        } catch (error) {
            this.markDeviceStatusStale();
        }
    }

    renderDeviceStatus(s) {
        const set = (id, text, level) => {
            const el = document.getElementById(id);
            if (!el) return;
            el.textContent = text;
            el.classList.remove('warn', 'alert', 'stale');
            if (level) el.classList.add(level);
        };

        // Reaching here means the agent answered, which is what the card
        // reports: the device is online when the browser can talk to Go. The
        // C daemon dying does not make it offline — the agent is still there,
        // and everything below is still true.
        set('valHeartbeat', this.translations[this.currentLang].deviceOnline);

        // frameFlow says whether the C daemon is still delivering frames.
        // That belongs to the video overlay, which is what the viewer checks
        // to tell a live picture from a frozen one.
        if (typeof s.frameFlow === 'boolean') {
            this.setStreamStatus(s.frameFlow ? 'connected' : 'disconnected');
        }

        // cpuTemp arrives formatted ("47.6°C"); parse only to pick a colour.
        const temp = parseFloat(s.cpuTemp);
        set('valCpuTemp', s.cpuTemp || '—',
            isNaN(temp) ? null : temp >= 75 ? 'alert' : temp >= 60 ? 'warn' : null);

        // Raw load average. It sits near 10 on an idle device: rockit parks
        // about ten kernel threads in uninterruptible sleep waiting on
        // hardware, and Linux counts those, while only one thread is ever
        // runnable. Thresholds are set above that baseline rather than
        // adjusting the number, which would need the D-state count and the
        // one-minute average to agree — they do not while the average is
        // still climbing after boot.
        const load = Number(s.workLoad) || 0;
        set('valWorkLoad', load.toFixed(2),
            load >= 16 ? 'alert' : load >= 13 ? 'warn' : null);

        const total = Number(s.totalram) || 0;
        const free = Number(s.freeram) || 0;
        if (total > 0) {
            const usedPct = Math.round((total - free) / total * 100);
            set('valMemory', `${this.formatBytes(total - free)} / ${this.formatBytes(total)}`,
                usedPct >= 90 ? 'alert' : usedPct >= 75 ? 'warn' : null);
        } else {
            set('valMemory', '—');
        }

        set('valClients', String(s.clients ?? 0));
        set('valSystemUptime', s.systemUptime || '—');
        set('valLastRecord', s.lastRecord || '—');

        const stamp = document.getElementById('statusUpdated');
        if (stamp) stamp.textContent = new Date().toLocaleTimeString();
    }

    markDeviceStatusStale() {
        // A failed poll is the device going offline, since the card's subject
        // is whether the browser can reach the agent at all. The other values
        // came from an agent that is no longer answering, so they are cleared
        // rather than left looking current.
        const set = (id, text, level) => {
            const el = document.getElementById(id);
            if (!el) return;
            el.textContent = text;
            el.classList.remove('warn', 'alert', 'stale');
            if (level) el.classList.add(level);
        };

        set('valHeartbeat', this.translations[this.currentLang].deviceOffline, 'alert');
        ['valCpuTemp', 'valWorkLoad', 'valMemory', 'valClients',
         'valSystemUptime', 'valLastRecord'].forEach(id => set(id, '—'));
    }

    formatBytes(bytes) {
        const mb = bytes / (1024 * 1024);
        return mb >= 1024 ? `${(mb / 1024).toFixed(1)} GB` : `${Math.round(mb)} MB`;
    }

    updateLanguage(lang) {
        const texts = this.translations[lang];
        
        // Update all text elements
        Object.keys(texts).forEach(key => {
            const element = document.getElementById(key);
            if (element) {
                element.textContent = texts[key];
            }
        });

        // statusText is driven by the stream, so restore it after the generic
        // id-based pass above has written "Connecting..." over it.
        if (this.streamState) this.setStreamStatus(this.streamState);

        // Update navigation items
        document.querySelectorAll('.nav-item span').forEach((span, index) => {
            const keys = ['navLive', 'navCalendar'];
            if (keys[index]) {
                span.textContent = texts[keys[index]];
            }
        });
    }

    showView(viewName) {
        // Hide all views
        document.querySelectorAll('.view').forEach(view => {
            view.classList.remove('active');
        });

        // Remove active class from all nav items
        document.querySelectorAll('.nav-item').forEach(item => {
            item.classList.remove('active');
        });

        // Show selected view
        const targetView = document.getElementById(viewName + 'View');
        if (targetView) {
            targetView.classList.add('active');
        }

        // Activate corresponding nav item
        const navItem = document.querySelector(`[data-view="${viewName}"]`);
        if (navItem) {
            navItem.classList.add('active');
        }

        this.currentView = viewName;
        this.syncDeviceStatusPolling();

        // Initialize view-specific functionality
        this.initView(viewName);
    }

    initView(viewName) {
        switch (viewName) {
            case 'live':
                this.initLiveView();
                break;
            case 'calendar':
                this.initCalendarView();
                break;
        }
    }

    initLiveView() {
        // Initialize live stream functionality
        if (window.streamApp) {
            window.streamApp.init();
        }
    }

    initCalendarView() {
        // Initialize calendar functionality
        if (window.calendarApp) {
            window.calendarApp.init();
        }
    }

    initStreamStatus() {
        const img = document.getElementById('streamImage');
        if (!img) return;

        // Track the real stream rather than a timer. An MJPEG response is a
        // long-lived multipart body in an <img>: load fires once the first
        // frame has arrived, error when the connection drops. The old code
        // just announced "connected" two seconds in, which was both late and
        // never wrong.
        img.addEventListener('load', () => this.setStreamStatus('connected'));
        img.addEventListener('error', () => this.setStreamStatus('disconnected'));

        // A cached or already-complete image fires no load event.
        if (img.complete && img.naturalWidth > 0) {
            this.setStreamStatus('connected');
        }
    }

    setStreamStatus(state) {
        this.streamState = state;
        const dot = document.querySelector('.status-dot');
        const text = document.getElementById('statusText');
        const t = this.translations[this.currentLang];

        if (dot) dot.style.background = state === 'connected' ? '#4CAF50' : '#f44336';
        if (text) {
            text.textContent = state === 'connected'
                ? t.statusStreaming
                : t.statusConnecting;
        }
    }

    toggleFullscreen() {
        const streamWrapper = document.querySelector('.stream-wrapper');
        
        if (!document.fullscreenElement) {
            streamWrapper.requestFullscreen().catch(err => {
                console.error('Error attempting to enable fullscreen:', err);
            });
        } else {
            document.exitFullscreen();
        }
    }

    handleKeyboardShortcuts(e) {
        // Ctrl/Cmd + 1-2 for view switching
        if ((e.ctrlKey || e.metaKey) && e.key >= '1' && e.key <= '2') {
            e.preventDefault();
            const views = ['live', 'calendar'];
            const viewIndex = parseInt(e.key) - 1;
            if (views[viewIndex]) {
                this.showView(views[viewIndex]);
            }
        }

        // F11 for fullscreen
        if (e.key === 'F11') {
            e.preventDefault();
            this.toggleFullscreen();
        }
    }

    showLoading() {
        document.getElementById('loadingOverlay').classList.add('show');
    }

    hideLoading() {
        document.getElementById('loadingOverlay').classList.remove('show');
    }

    showNotification(message, type = 'info') {
        // Create notification element
        const notification = document.createElement('div');
        notification.className = `notification notification-${type}`;
        notification.textContent = message;
        
        // Add styles
        notification.style.cssText = `
            position: fixed;
            top: 20px;
            right: 20px;
            background: ${type === 'error' ? '#f44336' : '#4CAF50'};
            color: white;
            padding: 1rem 1.5rem;
            border-radius: 8px;
            box-shadow: 0 4px 20px rgba(0,0,0,0.2);
            z-index: 1001;
            transform: translateX(100%);
            transition: transform 0.3s ease;
        `;
        
        document.body.appendChild(notification);
        
        // Animate in
        setTimeout(() => {
            notification.style.transform = 'translateX(0)';
        }, 100);
        
        // Remove after 3 seconds
        setTimeout(() => {
            notification.style.transform = 'translateX(100%)';
            setTimeout(() => {
                document.body.removeChild(notification);
            }, 300);
        }, 3000);
    }
}

// Initialize app when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    window.app = new MotionSenseApp();
}); 