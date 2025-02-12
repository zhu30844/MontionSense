document.addEventListener('DOMContentLoaded', function() {
    const calendar = document.getElementById('calendar');
    const daysOfWeekEn = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
    const daysOfWeekZh = ['周一', '周二', '周三', '周四', '周五', '周六', '周日'];
    let currentDate = new Date();
    let currentLang = 'zh';
    let motionCounts = {}; // 存储从后端获取的motion count数据

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
                createCalendar(currentDate.getFullYear(), currentDate.getMonth());
            })
            .catch(error => console.error('Error fetching motion counts:', error));
    }

    function createCalendar(year, month) {
        const today = new Date();
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

            if (motionCounts[currentDateStr] !== undefined) {
                day.style.backgroundColor = `rgba(255, 0, 0, ${motionCounts[currentDateStr] / 100})`; // 用motion_count计算背景色的透明度
            }

            if (year < today.getFullYear() || (year === today.getFullYear() && month < today.getMonth()) || (year === today.getFullYear() && month === today.getMonth() && i <= today.getDate())) {
                day.addEventListener('click', function() {
                    const selectedDate = `${year}-${month + 1}-${i}`;
                    const replayUrl = `/components/playback.html?date=${selectedDate}`;
                    parent.window.location.href = replayUrl;
                });
                if (!(year === today.getFullYear() && month === today.getMonth() && i === today.getDate())) {
                    day.style.fontWeight = 'bold'; // 将过去的日期字体加粗，今天的不加粗
                }
            } else {
                day.classList.add('disabled');
                day.style.color = '#A9A9A9'; // 将今天之后的日期字体变成深灰色
            }
            calendar.appendChild(day);
        }
    }

    fetchMotionCounts(); // 初始化时获取motion count数据
});



function fetchMotionCounts() {
    fetch('/motion.json')
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









document.addEventListener('DOMContentLoaded', function() {
    const calendar = document.getElementById('calendar');
    const daysOfWeekEn = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
    const daysOfWeekZh = ['周一', '周二', '周三', '周四', '周五', '周六', '周日'];
    let currentDate = new Date();
    let currentLang = 'zh';
    let motionCounts = {}; // 存储后端获取的motion_count数据
    let maxCount = 0; // motion_count的最大值
    let minCount = 0; // motion_count的最小值

    window.updateCalendarLanguage = function(lang) {
        currentLang = lang;
        createCalendar(currentDate.getFullYear(), currentDate.getMonth());
    }

    // 获取motion_count数据
    // function fetchMotionCounts() {
    //     fetch('/api/motion_counts')
    //         .then(response => response.json())
    //         .then(data => {
    //             motionCounts = data.motion_counts.reduce((acc, item) => {
    //                 acc[item.date] = item.motion_count;
    //                 return acc;
    //             }, {});

    //             // 获取所有的motion_count值
    //             const counts = data.motion_counts.map(item => item.motion_count);
    //             // 计算最大值和最小值
    //             maxCount = Math.max(...counts);
    //             minCount = Math.min(...counts);

    //             createCalendar(currentDate.getFullYear(), currentDate.getMonth());
    //         })
    //         .catch(error => console.error('Error fetching motion counts:', error));
    // }
    function fetchMotionCounts() {
        fetch('/motion.json')
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

    function getColorForValue(value, min, max) {
        // 定义浅蓝色和黄色的RGB值
        const startColor = { r: 173, g: 216, b: 230 }; // #ADD8E6
        const endColor = { r: 255, g: 255, b: 0 };     // #FFFF00

        // 防止除以零的情况
        if (max === min) {
            return `rgb(${startColor.r}, ${startColor.g}, ${startColor.b})`;
        }

        // 计算当前值在最小值和最大值之间的比例
        const ratio = (value - min) / (max - min);

        // 根据比例计算当前颜色的RGB值
        const r = Math.round(startColor.r + (endColor.r - startColor.r) * ratio);
        const g = Math.round(startColor.g + (endColor.g - startColor.g) * ratio);
        const b = Math.round(startColor.b + (endColor.b - startColor.b) * ratio);

        return `rgb(${r}, ${g}, ${b})`;
    }

    function createCalendar(year, month) {
        const today = new Date();
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

            if (motionCounts[currentDateStr] !== undefined) {
                // 有motion_count数据
                const count = motionCounts[currentDateStr];
                const color = getColorForValue(count, minCount, maxCount);
                day.style.backgroundColor = color;

                // 添加点击事件
                day.addEventListener('click', function() {
                    const selectedDate = `${year}-${month + 1}-${i}`;
                    const replayUrl = `/components/playback.html?date=${selectedDate}`;
                    parent.window.location.href = replayUrl;
                });

                // 设置字体加粗
                if (!(year === today.getFullYear() && month === today.getMonth() && i === today.getDate())) {
                    day.style.fontWeight = 'bold'; // 将过去的日期字体加粗，今天的不加粗
                }

            } else {
                // 没有motion_count数据，设置为灰色，禁用点击事件
                day.classList.add('disabled');
                day.style.color = '#A9A9A9'; // 深灰色字体
                day.style.backgroundColor = '#E0E0E0'; // 灰色背景
                day.style.cursor = 'default'; // 光标为默认

                // 禁用点击事件（不添加任何事件）
            }

            // 处理未来的日期
            if (year > today.getFullYear() || (year === today.getFullYear() && month > today.getMonth()) || (year === today.getFullYear() && month === today.getMonth() && i > today.getDate())) {
                day.classList.add('disabled');
                day.style.color = '#A9A9A9'; // 深灰色字体
                day.style.backgroundColor = '#E0E0E0'; // 灰色背景
                day.style.cursor = 'default'; // 光标为默认

                // 禁用点击事件（不添加任何事件）
            }

            calendar.appendChild(day);
        }
    }

    fetchMotionCounts(); // 初始化时获取motion_count数据
});
