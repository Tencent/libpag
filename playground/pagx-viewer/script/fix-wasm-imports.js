import path from 'path';
import fs from 'fs';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Parse command line arguments manually
const args = process.argv.slice(2);
const argv = {};
for (let i = 0; i < args.length; i++) {
    if (args[i] === '-a' && args[i + 1]) {
        argv.a = args[i + 1];
        i++;
    }
}

function getFilesInDir(dir, extension) {
    let filesList = [];

    const files = fs.readdirSync(dir);
    files.forEach(file => {
        const fullPath = path.join(dir, file);
        const stat = fs.statSync(fullPath);

        if (stat.isFile() && fullPath.endsWith(extension)) {
            filesList.push(fullPath);
        }
        else if (stat.isDirectory()) {
            filesList = filesList.concat(getFilesInDir(fullPath, extension));
        }
    });

    return filesList;
}

function replaceInFile(filePath, searchString, replacement) {
    const data = fs.readFileSync(filePath, 'utf-8');
    if (data.includes(searchString)) {
        const updatedData = data.replaceAll(searchString, replacement);
        fs.writeFileSync(filePath, updatedData, 'utf-8');
        console.log(`In file ${filePath}, "${searchString}" was replaced with "${replacement}"`);
    }
}

function replaceFileNameInFiles() {
    const dir = path.resolve(__dirname, "../lib/");
    const extension = '.js';
    const searchString = 'pagx-viewer.js';
    const files = getFilesInDir(dir, extension);

    files.forEach(file => {
        const fileName = path.basename(file);
        replaceInFile(file, searchString, fileName);
    });
}

if(argv.a ==="wasm-mt"){
    replaceFileNameInFiles();
}
