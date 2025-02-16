document.addEventListener('DOMContentLoaded', function() {
    const calendar = document.getElementById('calendar');
    const daysOfWeekEn = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
    const daysOfWeekZh = ['周一', '周二', '周三', '周四', '周五', '周六', '周日'];
    let currentDate = new Date();
    let currentLang = 'zh';
    let motionCounts = {}; 
    let maxCount = 0;
    let minCount = 0;

    window.updateCalendarLanguage = function(lang) {
        currentLang = lang;
        createCalendar(currentDate.getFullYear(), currentDate.getMonth());
    }

    // get motion counts from server
    function fetchMotionCounts() {
        fetch('/api/motion_counts')
            .then(response => response.json())
            .then(data => {
                motionCounts = data.motion_counts.reduce((acc, item) => {
                    acc[item.date] = item.motion_count;
                    return acc;
                }, {});
                
                // get max and min motion counts
                const counts = data.motion_counts.map(item => item.motion_count);
                maxCount = Math.max(...counts);
                minCount = Math.min(...counts);
                
                createCalendar(currentDate.getFullYear(), currentDate.getMonth());
            })
            .catch(error => console.error('Error fetching motion counts:', error));
    }
    

    function createCalendar(year, month) {
        const today = new Date();
        today.setHours(0,0,0,0); 
        calendar.innerHTML = '';

        const headerRow = document.createElement('div');
        headerRow.classList.add('header-row');
        
        const prevButton = document.createElement('button');
        prevButton.textContent = currentLang === 'zh' ? '上个月' : 'Previous';
        prevButton.addEventListener('click', function() {
            currentDate.setMonth(currentDate.getMonth() - 1);
            createCalendar(currentDate.getFullYear(), currentDate.getMonth());
        });
        headerRow.appendChild(prevButton);

        const monthYear = document.createElement('div');
        monthYear.classList.add('month-year');
        monthYear.textContent = currentLang === 'zh' ? `${year}年 ${month + 1}月` : `${year}-${String(month + 1).padStart(2, '0')}`;
        headerRow.appendChild(monthYear);

        const nextButton = document.createElement('button');
        nextButton.textContent = currentLang === 'zh' ? '下个月' : 'Next';
        nextButton.disabled = year > today.getFullYear() || (year === today.getFullYear() && month >= today.getMonth());
        nextButton.addEventListener('click', function() {
            if (!nextButton.disabled) {
                currentDate.setMonth(currentDate.getMonth() + 1);
                createCalendar(currentDate.getFullYear(), currentDate.getMonth());
            }
        });
        headerRow.appendChild(nextButton);

        calendar.appendChild(headerRow);

        const daysOfWeek = currentLang === 'zh' ? daysOfWeekZh : daysOfWeekEn;

        for (let day of daysOfWeek) {
            const dayHeader = document.createElement('div');
            dayHeader.classList.add('day', 'header');
            dayHeader.textContent = day;
            calendar.appendChild(dayHeader);
        }

        const firstDay = new Date(year, month, 1);
        const lastDay = new Date(year, month + 1, 0);
        const startingDay = (firstDay.getDay() === 0) ? 6 : firstDay.getDay() - 1;

        for (let i = 0; i < startingDay; i++) {
            const emptyDay = document.createElement('div');
            emptyDay.classList.add('day');
            calendar.appendChild(emptyDay);
        }

        for (let i = 1; i <= lastDay.getDate(); i++) {
            const day = document.createElement('div');
            day.classList.add('day');
            day.textContent = i;
            const currentDateStr = `${year}-${String(month + 1).padStart(2, '0')}-${String(i).padStart(2, '0')}`;
            const dateObj = new Date(year, month, i);
            dateObj.setHours(0,0,0,0); 
            const isFutureDate = dateObj > today;
            const isToday = dateObj.getTime() === today.getTime();
            const hasMotionData = motionCounts[currentDateStr] !== undefined;

            if (!isFutureDate && (hasMotionData || isToday)) {
                if (hasMotionData && !isToday) {
                    // fill color for past dates with motion count data
                    const count = motionCounts[currentDateStr];
                    const opacity = count / 100;
                    day.style.backgroundColor = `rgba(255, 0, 0, ${opacity})`; // 根据需要调整颜色
                }


                day.addEventListener('click', function() {
                    const selectedDate = `${year}-${String(month + 1).padStart(2, '0')}-${String(i).padStart(2, '0')}`;
                    const replayUrl = `/components/playback.html?date=${selectedDate}`;
                    parent.window.location.href = replayUrl;
                });

                // style for today's date
                if (isToday) {
                    day.style.border = '2px solid #4CAF50'; // a green border
                    day.style.fontWeight = 'normal'; 
                } else {
                    // past dates
                    day.style.fontWeight = 'bold'; // bold font
                }
            } else {
                // no motion data or future dates
                day.classList.add('disabled');
                day.style.color = '#A9A9A9'; // gray color
                day.style.cursor = 'default'; 
            }

            calendar.appendChild(day);
        }
    }

    fetchMotionCounts(); 
});
