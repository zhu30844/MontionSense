document.addEventListener('DOMContentLoaded', function() {
    const urlParams = new URLSearchParams(window.location.search);
    const date = urlParams.get('date') || '2025-01-01'; // 使用默认日期
    console.log('target date:', date);

    const apiUrl = `/api/video_segments?date=${date}`;
    const player = videojs('my-video');

    // 从 /api/video_segments 获取 JSON 数据
    fetch(apiUrl)
        .then(response => response.json())
        .then(data => {
            console.log('Fetched data:', data); // 调试用，打印获取的数据
            if (data && Array.isArray(data.video_segments)) {
                // 生成表格
                generateTable(data.video_segments);
                // 默认播放第一段视频
                if (data.video_segments.length > 0) {
                    playVideoSegment(data.video_segments[0].folder, 0);
                }
            } else {
                console.error('Error: Fetched data is not in the expected format');
            }
        })
        .catch(error => {
            console.error('Error fetching video segments:', error);
        });

    // 生成表格函数
    function generateTable(data) {
        const table = document.createElement('table');
        table.id = 'video-segments-table';

        // 创建表头
        const thead = document.createElement('thead');
        const headerRow = document.createElement('tr');

        const thPowerOutage = document.createElement('th');
        thPowerOutage.textContent = '断电次数 (Power Outage)';
        const thRecoverTime = document.createElement('th');
        thRecoverTime.textContent = '录制时间 (Recover Time)';

        headerRow.appendChild(thPowerOutage);
        headerRow.appendChild(thRecoverTime);
        thead.appendChild(headerRow);
        table.appendChild(thead);

        // 创建表格主体
        const tbody = document.createElement('tbody');

        data.forEach((item, index) => {
            const row = document.createElement('tr');
            row.dataset.index = index;

            const tdPowerOutage = document.createElement('td');
            tdPowerOutage.textContent = item.folder;
            const tdRecoverTime = document.createElement('td');
            tdRecoverTime.textContent = item.start_time;

            row.appendChild(tdPowerOutage);
            row.appendChild(tdRecoverTime);

            // 行点击事件
            row.addEventListener('click', function() {
                playVideoSegment(item.folder, index);
            });

            tbody.appendChild(row);
        });

        table.appendChild(tbody);
        document.body.insertBefore(table, document.getElementById('my-video'));
    }

    // 播放视频段函数
    function playVideoSegment(folder, index) {
        const videoSource = `/hls/${date}/${folder}/index.m3u8`;
        player.src({
            src: videoSource,
            type: 'application/x-mpegURL'
        });
        player.play();

        highlightRow(index);
    }

    // 高亮当前行
    function highlightRow(index) {
        const rows = document.querySelectorAll('#video-segments-table tbody tr');
        rows.forEach((row, idx) => {
            if (idx === index) {
                row.classList.add('highlight');
            } else {
                row.classList.remove('highlight');
            }
        });
    }
});
