// MotionSense Playback App
class PlaybackApp {
    constructor() {
        this.currentDate = null;
        this.currentSegment = null;
        this.videoSegments = [];
        this.player = null;
        this.currentLang = 'en';
        
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
        // Get date from URL parameter
        const urlParams = new URLSearchParams(window.location.search);
        const dateParam = urlParams.get('date');
        
        if (dateParam) {
            const dateInput = document.getElementById('playbackDate');
            if (dateInput) {
                dateInput.value = dateParam;
            } else {
                console.error('Date input element not found');
            }
            this.loadDate(dateParam);
        } else {
        }
    }

    initPlayer() {
        const videoElement = document.getElementById('my-video');
        
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

        // Markers depend on the player's duration, so redraw once it is
        // known. This was on 'timeupdate', which fires several times a second
        // and rebuilt every marker element each time.
        if (this.player) {
            this.player.on('loadedmetadata', () => {
                this.updateMotionMarkers();
            });
        }
    }

    async loadDate(date) {
        
        if (!date) {
            this.showError('Please select a date');
            return;
        }

        this.currentDate = date;
        this.showLoading();

        try {
            // Load video segments
            await this.loadVideoSegments(date);
            
            this.hideLoading();
        } catch (error) {
            console.error('Error loading playback data:', error);
            this.showError('Failed to load playback data');
            this.hideLoading();
        }
    }

    async loadVideoSegments(date) {
        const response = await fetch(`/api/recordings/${date}`);

        const data = await response.json();

        if (data && Array.isArray(data.segments)) {
            // The API names these segment/startTime; the table below was
            // written against the older folder/start_time spelling.
            this.videoSegments = data.segments.map(s => ({
                folder: s.segment,
                start_time: s.startTime,
                total_frames: s.totalFrames,
                playlist_url: s.playlistUrl,
                duration_seconds: s.durationSeconds,
                motion_frames: s.motionFrames || [],
            }));
            this.generateSegmentsTable();
            
            // Load the first segment but do not start it: browsers reject
            // play() before the user has interacted with the page, and a
            // rejected promise here is not an error worth reporting.
            if (this.videoSegments.length > 0 && this.player && this.player.readyState() >= 1) {
                this.playVideoSegment(this.videoSegments[0].folder, 0, false);
            } else if (this.videoSegments.length > 0) {
                if (this.player) {
                    this.player.ready(() => {
                        this.playVideoSegment(this.videoSegments[0].folder, 0, false);
                    });
                }
            }
        } else {
            this.videoSegments = [];
            this.showNoSegments();
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

    playVideoSegment(folder, index, autoplay = true) {
        
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
        
        this.player.src({
            src: videoSource,
            type: 'application/x-mpegURL'
        });
        
        if (autoplay) {
            this.player.play().catch((error) => {
                // Autoplay policy, not a failure: the source is loaded and the
                // player's own control will start it.
                if (error && error.name === 'NotAllowedError') return;
                console.error('Error playing video:', error);
            });
        }
        
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

        const segment = this.videoSegments.find(s => s.folder === this.currentSegment);
        const motionFrames = (segment && segment.motion_frames) || [];
        const totalFrames = (segment && segment.total_frames) || 0;

        if (motionFrames.length === 0 || totalFrames === 0) {
            return;
        }

        const duration = this.player.duration();
        if (!duration || duration === Infinity) {
            return;
        }

        motionFrames.forEach(frame => {
            const marker = document.createElement('div');
            marker.className = 'motion-marker';

            // Position by share of frames rather than frame/fps: the recorder
            // varies its capture rate with motion but rewrites PTS to a
            // constant playback rate, so frame N sits at N/totalFrames of the
            // playback timeline whatever rate it was captured at.
            const share = frame / totalFrames;
            const motionTimeInSeconds = share * duration;
            const clampedPosition = Math.max(0, Math.min(100, share * 100));
            
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
            marker.title = `Motion detected at frame ${frame} (${motionTimeInSeconds.toFixed(1)}s)`;
            
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
