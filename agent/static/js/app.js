// MotionSense App - Main Application Logic
class MotionSenseApp {
    constructor() {
        this.currentLang = 'en';
        this.currentView = 'live';
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
                statusText: '连接中...',
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
                statusText: 'Connecting...',
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
        this.fetchDeviceStatus();
        setInterval(() => this.fetchDeviceStatus(), 2000);
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

        // cpuTemp arrives formatted ("47.6°C"); parse only to pick a colour.
        const temp = parseFloat(s.cpuTemp);
        set('valCpuTemp', s.cpuTemp || '—',
            isNaN(temp) ? null : temp >= 75 ? 'alert' : temp >= 60 ? 'warn' : null);

        // NOTE: on this device the idle load average sits around 10. It is not
        // CPU: rockit keeps ten kernel threads (vsys, venc, vpss, rkisp-vir0,
        // vrga...) in uninterruptible sleep, which Linux counts towards
        // loadavg. Only one thread is ever runnable. These thresholds will
        // therefore read as alert whenever the media pipeline is up.
        const load = Number(s.workLoad) || 0;
        set('valWorkLoad', load.toFixed(2),
            load >= 4 ? 'alert' : load >= 2 ? 'warn' : null);

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
        // Keep the last known values on screen, greyed, rather than blanking
        // the card on a single failed poll.
        ['valCpuTemp', 'valWorkLoad', 'valMemory', 'valClients',
         'valSystemUptime', 'valLastRecord'].forEach(id => {
            const el = document.getElementById(id);
            if (el) el.classList.add('stale');
        });
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
        const statusDot = document.querySelector('.status-dot');
        const statusText = document.getElementById('statusText');
        
        // Simulate connection status
        setTimeout(() => {
            statusDot.style.background = '#4CAF50';
            statusText.textContent = this.currentLang === 'zh' ? '已连接' : 'Connected';
        }, 2000);
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