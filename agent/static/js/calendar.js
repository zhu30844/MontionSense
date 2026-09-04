// MotionSense Calendar App
class CalendarApp {
    constructor() {
        this.currentDate = new Date();
        this.currentLang = 'zh';
        this.motionCounts = {};
        this.maxCount = 0;
        this.minCount = 0;
        this.calendar = null;
        
        this.translations = {
            zh: {
                previous: '上个月',
                next: '下个月',
                daysOfWeek: ['周一', '周二', '周三', '周四', '周五', '周六', '周日'],
                noData: '无数据',
                motionEvents: '运动事件'
            },
            en: {
                previous: 'Previous',
                next: 'Next',
                daysOfWeek: ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'],
                noData: 'No Data',
                motionEvents: 'Motion Events'
            }
        };
    }

    init() {
        this.calendar = document.getElementById('calendar');
        if (!this.calendar) return;
        
        this.fetchMotionCounts();
        this.createCalendar(this.currentDate.getFullYear(), this.currentDate.getMonth());
    }

    updateLanguage(lang) {
        this.currentLang = lang;
        this.createCalendar(this.currentDate.getFullYear(), this.currentDate.getMonth());
    }

    async fetchMotionCounts() {
        try {
            const response = await fetch('/api/recordings/calendar');
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}`);
            }
            const days = await response.json() || [];

            this.motionCounts = days.reduce((acc, item) => {
                acc[item.date] = item.motionCount;
                return acc;
            }, {});

            // Calculate max and min for heatmap scaling
            const counts = days.map(item => item.motionCount);
            this.maxCount = Math.max(...counts, 1);
            this.minCount = Math.min(...counts, 0);
            
            this.createCalendar(this.currentDate.getFullYear(), this.currentDate.getMonth());
        } catch (error) {
            console.error('Error fetching motion counts:', error);
            this.showError('Failed to load motion data');
        }
    }

    createCalendar(year, month) {
        if (!this.calendar) return;
        
        const today = new Date();
        today.setHours(0, 0, 0, 0);
        
        this.calendar.innerHTML = '';

        // Create header with navigation
        const headerRow = this.createCalendarHeader(year, month, today);
        this.calendar.appendChild(headerRow);

        // Create day headers
        const daysOfWeek = this.translations[this.currentLang].daysOfWeek;
        daysOfWeek.forEach(day => {
            const dayHeader = document.createElement('div');
            dayHeader.classList.add('day', 'header');
            dayHeader.textContent = day;
            this.calendar.appendChild(dayHeader);
        });

        // Calculate calendar grid
        const firstDay = new Date(year, month, 1);
        const lastDay = new Date(year, month + 1, 0);
        const startingDay = (firstDay.getDay() === 0) ? 6 : firstDay.getDay() - 1;

        // Add empty cells for days before the first day of the month
        for (let i = 0; i < startingDay; i++) {
            const emptyDay = document.createElement('div');
            emptyDay.classList.add('day', 'empty');
            this.calendar.appendChild(emptyDay);
        }

        // Add days of the month
        for (let i = 1; i <= lastDay.getDate(); i++) {
            const day = this.createDayElement(year, month, i, today);
            this.calendar.appendChild(day);
        }
    }

    createCalendarHeader(year, month, today) {
        const headerRow = document.createElement('div');
        headerRow.classList.add('header-row');

        // Previous month button
        const prevButton = document.createElement('button');
        prevButton.classList.add('btn', 'btn-secondary', 'nav-btn');
        prevButton.innerHTML = `<i class="fas fa-chevron-left"></i> ${this.translations[this.currentLang].previous}`;
        prevButton.addEventListener('click', () => {
            this.currentDate.setMonth(this.currentDate.getMonth() - 1);
            this.createCalendar(this.currentDate.getFullYear(), this.currentDate.getMonth());
        });
        headerRow.appendChild(prevButton);

        // Month and year display
        const monthYear = document.createElement('div');
        monthYear.classList.add('month-year');
        monthYear.textContent = this.currentLang === 'zh' 
            ? `${year}年 ${month + 1}月` 
            : `${year}-${String(month + 1).padStart(2, '0')}`;
        headerRow.appendChild(monthYear);

        // Next month button
        const nextButton = document.createElement('button');
        nextButton.classList.add('btn', 'btn-secondary', 'nav-btn');
        nextButton.innerHTML = `${this.translations[this.currentLang].next} <i class="fas fa-chevron-right"></i>`;
        nextButton.disabled = year > today.getFullYear() || (year === today.getFullYear() && month >= today.getMonth());
        if (!nextButton.disabled) {
            nextButton.addEventListener('click', () => {
                this.currentDate.setMonth(this.currentDate.getMonth() + 1);
                this.createCalendar(this.currentDate.getFullYear(), this.currentDate.getMonth());
            });
        }
        headerRow.appendChild(nextButton);

        return headerRow;
    }

    createDayElement(year, month, day, today) {
        const dayElement = document.createElement('div');
        dayElement.classList.add('day');
        
        const currentDateStr = `${year}-${String(month + 1).padStart(2, '0')}-${String(day).padStart(2, '0')}`;
        const dateObj = new Date(year, month, day);
        dateObj.setHours(0, 0, 0, 0);
        
        const isFutureDate = dateObj > today;
        const isToday = dateObj.getTime() === today.getTime();
        const hasMotionData = this.motionCounts[currentDateStr] !== undefined;
        const motionCount = this.motionCounts[currentDateStr] || 0;

        // Add classes based on date properties
        if (isToday) dayElement.classList.add('today');
        if (isFutureDate) dayElement.classList.add('future');
        if (hasMotionData) dayElement.classList.add('has-data');

        // Create day content
        const dayNumber = document.createElement('span');
        dayNumber.classList.add('day-number');
        dayNumber.textContent = day;
        dayElement.appendChild(dayNumber);

        // Add motion indicator if there's data
        if (hasMotionData && motionCount > 0) {
            const motionIndicator = document.createElement('div');
            motionIndicator.classList.add('motion-indicator');
            
            // Calculate heatmap intensity
            const intensity = this.calculateHeatmapIntensity(motionCount);
            motionIndicator.style.background = this.getHeatmapColor(intensity);
            
            // Add tooltip
            motionIndicator.title = `${currentDateStr}: ${motionCount} ${this.translations[this.currentLang].motionEvents}`;
            
            dayElement.appendChild(motionIndicator);
        }

        // Add click event for date selection
        if (!isFutureDate) {
            dayElement.classList.add('clickable');
            dayElement.addEventListener('click', () => {
                this.selectDate(currentDateStr);
            });
        }

        return dayElement;
    }

    calculateHeatmapIntensity(count) {
        if (this.maxCount === this.minCount) return 0.5;
        return (count - this.minCount) / (this.maxCount - this.minCount);
    }

    getHeatmapColor(intensity) {
        // Create a gradient from green (low) to red (high)
        const r = Math.round(255 * intensity);
        const g = Math.round(255 * (1 - intensity));
        const b = 0;
        return `rgba(${r}, ${g}, ${b}, 0.8)`;
    }

    selectDate(dateStr) {
        // Open playback in new tab
        const playbackUrl = `${window.location.origin}/playback.html?date=${dateStr}`;
        window.open(playbackUrl, '_blank');
    }

    showError(message) {
        if (window.app && window.app.showNotification) {
            window.app.showNotification(message, 'error');
        }
    }
}

// Initialize calendar app
document.addEventListener('DOMContentLoaded', () => {
    window.calendarApp = new CalendarApp();
});
