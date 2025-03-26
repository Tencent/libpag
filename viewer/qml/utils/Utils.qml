pragma Singleton
import QtQuick

QtObject {
    function msToTime(duration) {
        let seconds = parseInt((duration / 1000) % 60);
        let minutes = parseInt((duration / (1000 * 60)) % 60);
        minutes = (minutes < 10) ? "0" + minutes : minutes;
        seconds = (seconds < 10) ? "0" + seconds : seconds;
        return minutes + ":" + seconds;
    }

    function getFileDir(filePath) {
        let url = Qt.resolvedUrl(filePath);
        let urlObject = new URL(url);
        let directory = urlObject.pathname.substring(0, urlObject.pathname.lastIndexOf('/'));
        return directory;
    }

    function getFileName(filePath) {
        let url = Qt.resolvedUrl(filePath);
        let urlObject = new URL(url);
        let fileName = urlObject.pathname.split('/').pop();
        return fileName;
    }
}