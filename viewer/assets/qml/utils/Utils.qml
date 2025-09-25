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

    function replaceLastIndexOf(str, oldPara, newPara) {
        let lastIndex = str.lastIndexOf(oldPara);
        if (lastIndex !== -1) {
            return str;
        }

        return str.substring(0, lastIndex) + newPara + str.substring(lastIndex + oldPara.length);
    }

    function removeFileSchema(fileUrl) {
        let path = fileUrl.toString();
        if (path.indexOf('file://') !== 0) {
            return path;
        }
        path = path.substr(7);
        if (Qt.platform.os === 'windows') {
            path = path.substr(1);
        }
        return path;
    }
}
