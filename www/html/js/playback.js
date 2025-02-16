document.addEventListener('DOMContentLoaded', function() {
    const urlParams = new URLSearchParams(window.location.search);
    const date = urlParams.get('date') || '1970-01-01'; // default to 1970-01-01
    console.log('target date:', date);

    const apiUrl = `/api/video_segments?date=${date}`;
    const player = videojs('my-video');

    // get json data from server /api/video_segments
    fetch(apiUrl)
        .then(response => response.json())
        .then(data => {
            console.log('Fetched data:', data); // debug
            if (data && Array.isArray(data.video_segments)) {
                generateTable(data.video_segments);
                // play the first video segment
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


    function generateTable(data) {
        const table = document.createElement('table');
        table.id = 'video-segments-table';

        //  create table header
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

        // create table body
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

            // add click event listener to each row
            row.addEventListener('click', function() {
                playVideoSegment(item.folder, index);
            });

            tbody.appendChild(row);
        });

        table.appendChild(tbody);
        document.body.insertBefore(table, document.getElementById('my-video'));
    }

    // play video segment
    function playVideoSegment(folder, index) {
        const videoSource = `/hls/${date}/${folder}/index.m3u8`;
        player.src({
            src: videoSource,
            type: 'application/x-mpegURL'
        });
        player.play();

        highlightRow(index);
    }

    // highlight the selected row
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
