document.addEventListener('DOMContentLoaded', function() {
    const calendar = document.getElementById('calendar');
    const daysOfWeekEn = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
    const daysOfWeekZh = ['周一', '周二', '周三', '周四', '周五', '周六', '周日'];
    let currentDate = new Date();
    let currentLang = 'zh';
    let motionCounts = {}; // 存储从后端获取的motion count数据
    let maxCount = 0;
    let minCount = 0;

    window.updateCalendarLanguage = function(lang) {
        currentLang = lang;
        createCalendar(currentDate.getFullYear(), currentDate.getMonth());
    }

    // 获取motion count数据
    function fetchMotionCounts() {
        fetch('/api/motion_counts')
            .then(response => response.json())
            .then(data => {
                motionCounts = data.motion_counts.reduce((acc, item) => {
                    acc[item.date] = item.motion_count;
                    return acc;
                }, {});
                
                // 获取所有的motion_count值
                const counts = data.motion_counts.map(item => item.motion_count);
                // 计算最大值和最小值
                maxCount = Math.max(...counts);
                minCount = Math.min(...counts);
                
                createCalendar(currentDate.getFullYear(), currentDate.getMonth());
            })
            .catch(error => console.error('Error fetching motion counts:', error));
    }
    

    function createCalendar(year, month) {
        const today = new Date();
        today.setHours(0,0,0,0); // 重置时间部分，确保日期比较准确
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
            dateObj.setHours(0,0,0,0); // 重置时间部分，确保日期比较准确
            const isFutureDate = dateObj > today;
            const isToday = dateObj.getTime() === today.getTime();
            const hasMotionData = motionCounts[currentDateStr] !== undefined;

            if (!isFutureDate && (hasMotionData || isToday)) {
                // 有 motion_count 数据的过去日期，或今天（无论是否有数据）
                if (hasMotionData && !isToday) {
                    // 对于有 motion_count 的日期，除了今天，设置背景色
                    const count = motionCounts[currentDateStr];
                    const opacity = count / 100;
                    day.style.backgroundColor = `rgba(255, 0, 0, ${opacity})`; // 根据需要调整颜色
                }

                // 添加点击事件
                day.addEventListener('click', function() {
                    const selectedDate = `${year}-${String(month + 1).padStart(2, '0')}-${String(i).padStart(2, '0')}`;
                    const replayUrl = `/components/playback.html?date=${selectedDate}`;
                    parent.window.location.href = replayUrl;
                });

                // 今天的日期样式处理
                if (isToday) {
                    day.style.border = '2px solid #4CAF50'; // 给今天加个绿色边框（可选）
                    day.style.fontWeight = 'normal'; // 今天的字体不加粗
                } else {
                    // 非今天的过去日期
                    day.style.fontWeight = 'bold'; // 将过去的日期字体加粗
                }
            } else {
                // 没有 motion_count 数据的日期或未来日期
                day.classList.add('disabled');
                day.style.color = '#A9A9A9'; // 灰色字体
                day.style.cursor = 'default'; // 光标为默认
                // 背景保持默认的白色
            }

            calendar.appendChild(day);
        }
    }

    fetchMotionCounts(); // 初始化时获取motion count数据
});
