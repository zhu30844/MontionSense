// MotionSense Playback App
class PlaybackApp {
    constructor() {
        console.log('PlaybackApp constructor called');
        this.currentDate = null;
        this.currentSegment = null;
        this.videoSegments = [];
        this.motionPoints = [];
        this.player = null;
        this.currentLang = 'zh';
        
        this.translations = {
            zh: {
                powerOutage: '断电次数',
                recoverTime: '录制时间',
                segmentsTitle: '视频片段',
                noSegments: '该日期没有视频片段',
                motionEvents: '运动事件',
                loading: '加载中...',
                error: '加载失败',
                noMotionData: '无运动数据'
            },
            en: {
                powerOutage: 'Power Outage',
                recoverTime: 'Recover Time',
                segmentsTitle: 'Video Segments',
                noSegments: 'No video segments for this date',
                motionEvents: 'Motion Events',
                loading: 'Loading...',
                error: 'Load Failed',
                noMotionData: 'No motion data'
            }
        };
        
        // Initialize the app
        this.init();
    }

    init() {
        this.initPlayer();
        this.bindEvents();
        
        // Set default date to today
        const today = new Date().toISOString().split('T')[0];
        const dateInput = document.getElementById('playbackDate');
        if (dateInput) {
            dateInput.value = today;
        }
        
        // Check for auto-load immediately, don't wait for player
        this.checkAutoLoad();
    }
    
    checkAutoLoad() {
        console.log('checkAutoLoad called');
        // Get date from URL parameter
        const urlParams = new URLSearchParams(window.location.search);
        const dateParam = urlParams.get('date');
        console.log('URL params:', window.location.search);
        console.log('Date param:', dateParam);
        
        if (dateParam) {
            console.log('Auto-loading date from URL:', dateParam);
            const dateInput = document.getElementById('playbackDate');
            if (dateInput) {
                dateInput.value = dateParam;
                console.log('Date input set to:', dateParam);
            } else {
                console.error('Date input element not found');
            }
            this.loadDate(dateParam);
        } else {
            console.log('No date parameter found in URL');
        }
    }

    initPlayer() {
        console.log('Initializing video player...');
        const videoElement = document.getElementById('my-video');
        console.log('Video element:', videoElement);
        
        if (!videoElement) {
            console.error('Video element not found');
            return;
        }
        
        this.player = videojs('my-video', {
            controls: true,
            fluid: true,
            responsive: true,
            playbackRates: [0.5, 1, 1.25, 1.5, 2],
            controlBar: {
                children: [
                    'playToggle',
                    'volumePanel',
                    'currentTimeDisplay',
                    'timeDivider',
                    'durationDisplay',
                    'progressControl',
                    'playbackRateMenuButton',
                    'fullscreenToggle'
                ]
            }
        });
        
        console.log('Video.js player initialized:', this.player);

        // Add custom motion markers plugin
        this.initMotionMarkers();
    }

    initMotionMarkers() {
        // Create custom progress bar with motion markers
        const progressControl = this.player.controlBar.progressControl;
        const seekBar = progressControl.seekBar;
        
        // Add motion markers container
        const markersContainer = document.createElement('div');
        markersContainer.className = 'motion-markers';
        markersContainer.style.cssText = `
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            pointer-events: none;
            z-index: 10;
        `;
        
        seekBar.el().appendChild(markersContainer);
        this.markersContainer = markersContainer;
    }

    bindEvents() {
        // Load date button
        const loadBtn = document.getElementById('loadDateBtn');
        if (loadBtn) {
            loadBtn.addEventListener('click', () => {
                this.loadPlaybackDate();
            });
        }

        // Date input enter key
        const dateInput = document.getElementById('playbackDate');
        if (dateInput) {
            dateInput.addEventListener('keypress', (e) => {
                if (e.key === 'Enter') {
                    this.loadPlaybackDate();
                }
            });
        }

        // Player events
        if (this.player) {
            this.player.on('timeupdate', () => {
                this.updateMotionMarkers();
            });
        }
    }

    async loadDate(date) {
        console.log('PlaybackApp.loadDate called with date:', date);
        
        if (!date) {
            this.showError('Please select a date');
            return;
        }

        this.currentDate = date;
        this.showLoading();
        console.log('Loading video segments for date:', date);

        try {
            // Load video segments
            await this.loadVideoSegments(date);
            
            // Load motion points if available
            await this.loadMotionPoints(date);
            
            this.hideLoading();
            console.log('Successfully loaded playback data');
        } catch (error) {
            console.error('Error loading playback data:', error);
            this.showError('Failed to load playback data');
            this.hideLoading();
        }
    }

    async loadVideoSegments(date) {
        console.log('Loading video segments for date:', date);
        const response = await fetch(`/api/recordings/${date}`);
        console.log('Video segments API response status:', response.status);

        const data = await response.json();
        console.log('Video segments data:', data);

        if (data && Array.isArray(data.segments)) {
            // The API names these segment/startTime; the table below was
            // written against the older folder/start_time spelling.
            this.videoSegments = data.segments.map(s => ({
                folder: s.segment,
                start_time: s.startTime,
                total_frames: s.totalFrames,
                playlist_url: s.playlistUrl,
                duration_seconds: s.durationSeconds,
            }));
            console.log('Found', this.videoSegments.length, 'video segments');
            this.generateSegmentsTable();
            
            // Play first segment if available and player is ready
            if (this.videoSegments.length > 0 && this.player && this.player.readyState() >= 1) {
                console.log('Playing first segment:', this.videoSegments[0].folder);
                this.playVideoSegment(this.videoSegments[0].folder, 0);
            } else if (this.videoSegments.length > 0) {
                console.log('Player not ready yet, will play first segment when ready');
                // Wait for player to be ready
                if (this.player) {
                    this.player.ready(() => {
                        console.log('Player ready, now playing first segment');
                        this.playVideoSegment(this.videoSegments[0].folder, 0);
                    });
                }
            }
        } else {
            this.videoSegments = [];
            console.log('No video segments found');
            this.showNoSegments();
        }
    }

    async loadMotionPoints(date) {
        try {
            // No server-side endpoint for per-day motion points yet; the
            // timeline markers stay empty until one exists.
            this.motionPoints = [];
            this.updateMotionMarkers();
        } catch (error) {
            console.warn('Motion points API not available:', error);
            this.motionPoints = [];
        }
    }

    generateSegmentsTable() {
        const tableContainer = document.getElementById('segmentsContainer');
        if (!tableContainer) return;

        tableContainer.innerHTML = '';

        if (this.videoSegments.length === 0) {
            this.showNoSegments();
            return;
        }

        const table = document.createElement('table');
        table.className = 'segments-table';

        // Create header
        const thead = document.createElement('thead');
        const headerRow = document.createElement('tr');
        
        const thFolder = document.createElement('th');
        thFolder.textContent = 'Folder';
        
        const thStartTime = document.createElement('th');
        thStartTime.textContent = 'Start Time';
        
        const thDuration = document.createElement('th');
        thDuration.textContent = 'Duration';

        headerRow.appendChild(thFolder);
        headerRow.appendChild(thStartTime);
        headerRow.appendChild(thDuration);

        thead.appendChild(headerRow);
        table.appendChild(thead);

        // Create body
        const tbody = document.createElement('tbody');
        
        this.videoSegments.forEach((segment, index) => {
            const row = document.createElement('tr');
            row.dataset.index = index;
            row.className = 'segment-row';

            const tdFolder = document.createElement('td');
            tdFolder.textContent = segment.folder || '-';

            const tdStartTime = document.createElement('td');
            tdStartTime.textContent = segment.start_time || '-';

            const tdDuration = document.createElement('td');
            tdDuration.textContent = this.formatDuration(segment.duration_seconds || 0);

            const tdAction = document.createElement('td');
            const playButton = document.createElement('button');
            playButton.className = 'play-segment';
            playButton.textContent = 'Play';
            playButton.onclick = () => this.playVideoSegment(segment.folder, index);
            tdAction.appendChild(playButton);

            row.appendChild(tdFolder);
            row.appendChild(tdStartTime);
            row.appendChild(tdDuration);
            row.appendChild(tdAction);
            tbody.appendChild(row);
        });

        table.appendChild(tbody);
        tableContainer.appendChild(table);
    }

    showNoSegments() {
        const tableContainer = document.getElementById('segmentsContainer');
        if (!tableContainer) return;

        tableContainer.innerHTML = `
            <div class="no-segments">
                <i class="fas fa-video-slash"></i>
                <p>${this.translations[this.currentLang].noSegments}</p>
            </div>
        `;
    }

    playVideoSegment(folder, index) {
        console.log('playVideoSegment called with folder:', folder, 'index:', index);
        console.log('currentDate:', this.currentDate);
        console.log('player:', this.player);
        
        if (!this.currentDate) {
            console.error('No current date set');
            return;
        }

        if (!this.player) {
            console.error('Video player not initialized');
            return;
        }

        // Use the URL the API gave us rather than rebuilding the path here;
        // it was constructed as /hls/... while the server serves /media/...,
        // so every segment 404'd.
        const segment = this.videoSegments.find(s => s.folder === folder);
        const videoSource = segment && segment.playlist_url
            ? segment.playlist_url
            : `/media/${this.currentDate}/${folder}/index.m3u8`;
        console.log('Video source:', videoSource);
        
        this.player.src({
            src: videoSource,
            type: 'application/x-mpegURL'
        });
        
        console.log('Video source set, attempting to play...');
        this.player.play().then(() => {
            console.log('Video play started successfully');
        }).catch((error) => {
            console.error('Error playing video:', error);
        });
        
        this.currentSegment = folder;
        this.highlightRow(index);
        
        // Update motion markers for this segment
        this.updateMotionMarkers();
    }

    highlightRow(index) {
        const rows = document.querySelectorAll('.segment-row');
        rows.forEach((row, idx) => {
            if (idx === index) {
                row.classList.add('highlight');
            } else {
                row.classList.remove('highlight');
            }
        });
    }

    updateMotionMarkers() {
        if (!this.markersContainer || !this.currentSegment) return;

        // Clear existing markers
        this.markersContainer.innerHTML = '';

        // Filter motion points for current segment
        const segmentMotionPoints = this.motionPoints.filter(point => 
            point.folder === this.currentSegment
        );

        if (segmentMotionPoints.length === 0) {
            console.log('No motion points found for segment:', this.currentSegment);
            return;
        }

        console.log('Found motion points for segment:', segmentMotionPoints.length);

        // Get video duration
        const duration = this.player.duration();
        if (!duration || duration === Infinity) {
            console.log('Video duration not available yet');
            return;
        }

        // Create markers for each motion point
        segmentMotionPoints.forEach(point => {
            const marker = document.createElement('div');
            marker.className = 'motion-marker';
            
            // Calculate position based on motion_time (frame number) and video duration
            // Assuming 30fps for frame to time conversion
            const fps = 30;
            const motionTimeInSeconds = point.motion_time / fps;
            const position = (motionTimeInSeconds / duration) * 100;
            
            // Ensure position is within bounds
            const clampedPosition = Math.max(0, Math.min(100, position));
            
            marker.style.cssText = `
                position: absolute;
                left: ${clampedPosition}%;
                top: 0;
                width: 2px;
                height: 100%;
                background: #ff4444;
                border-radius: 1px;
                box-shadow: 0 0 2px rgba(255, 68, 68, 0.8);
                pointer-events: auto;
                cursor: pointer;
                transition: all 0.2s ease;
                z-index: 10;
            `;
            
            // Add tooltip
            marker.title = `Motion detected at frame ${point.motion_time} (${motionTimeInSeconds.toFixed(1)}s)`;
            
            // Add click event to seek to position
            marker.addEventListener('click', () => {
                this.player.currentTime(motionTimeInSeconds);
            });
            
            // Hover effect
            marker.addEventListener('mouseenter', () => {
                marker.style.width = '4px';
                marker.style.background = '#ff6666';
                marker.style.boxShadow = '0 0 4px rgba(255, 68, 68, 1)';
            });
            
            marker.addEventListener('mouseleave', () => {
                marker.style.width = '2px';
                marker.style.background = '#ff4444';
                marker.style.boxShadow = '0 0 2px rgba(255, 68, 68, 0.8)';
            });
            
            this.markersContainer.appendChild(marker);
        });
        
        console.log('Created', segmentMotionPoints.length, 'motion markers');
    }

    formatDuration(seconds) {
        const total = Math.round(seconds);
        const hours = Math.floor(total / 3600);
        const minutes = Math.floor((total % 3600) / 60);
        const secs = total % 60;
        
        if (hours > 0) {
            return `${hours}:${minutes.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
        } else {
            return `${minutes}:${secs.toString().padStart(2, '0')}`;
        }
    }

    loadPlaybackDate() {
        const dateInput = document.getElementById('playbackDate');
        const date = dateInput ? dateInput.value : null;
        this.loadDate(date);
    }

    showLoading() {
        if (window.app) {
            window.app.showLoading();
        }
    }

    hideLoading() {
        if (window.app) {
            window.app.hideLoading();
        }
    }

    showError(message) {
        if (window.app && window.app.showNotification) {
            window.app.showNotification(message, 'error');
        }
    }

    updateLanguage(lang) {
        this.currentLang = lang;
        // Re-generate table with new language
        if (this.videoSegments.length > 0) {
            this.generateSegmentsTable();
        }
    }
}
