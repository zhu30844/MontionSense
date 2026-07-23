document.addEventListener('DOMContentLoaded', function() {
    const languageToggle = document.getElementById('languageToggle');
    const calendarFrame = document.getElementById('calendarFrame');
    let currentLang = 'zh';

    document.getElementById('clickMe').addEventListener('click', function() {
        const replayUrl = '/components/stream.html';
        window.location.href = replayUrl;
    });

    languageToggle.addEventListener('click', function() {
        currentLang = currentLang === 'zh' ? 'en' : 'zh';
        updateLanguage(currentLang);
    });

    function updateLanguage(lang) {
        if (lang === 'zh') {
            document.getElementById('previewText').textContent = '摄像头预览';
            document.getElementById('clickMe').textContent = '点击预览';
            languageToggle.textContent = 'English';
        } else {
            document.getElementById('previewText').textContent = 'Camera Preview';
            document.getElementById('clickMe').textContent = 'Click to Preview';
            languageToggle.textContent = '简体中文';
        }

        calendarFrame.onload = function() {
            calendarFrame.contentWindow.updateCalendarLanguage(lang);
        }
        calendarFrame.src = calendarFrame.src; 
    }

    updateLanguage(currentLang); 
});
