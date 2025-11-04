const { exec } = require('child_process');
const path = require('path');
const fs = require('fs');
const filePath = path.resolve(__dirname, '../src/pag.ts');
const argv = require('minimist')(process.argv.slice(2));

function restoreFile(file) {
    const command = `git restore ${file}`;
    console.log(`${command}`);

    exec(command, (error, stdout, stderr) => {
        if (error) {
            console.error(`error:${error.message}`);
            return;
        }

        if (stderr) {
            console.error(`stderr:${stderr}`);
            return;
        }
    });
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
        const updatedData = data.replace(new RegExp(searchString, 'g'), replacement);
        fs.writeFileSync(filePath, updatedData, 'utf-8');
        console.log(`In file ${filePath}, "${searchString}" was replaced with "${replacement}"`);
    }
}

function replaceFileNameInFiles() {
    const dir = path.resolve(__dirname, "../lib-mt/");
    const extension = '.js';
    const searchString = 'libpag.js';
    const files = getFilesInDir(dir, extension);

    files.forEach(file => {
        const fileName = path.basename(file);
        replaceInFile(file, searchString, fileName);
    });
}

function getDepth(filePath, rootDir) {
    const relative = path.relative(rootDir, path.dirname(filePath));
    if (!relative) return 0;
    return relative.split(path.sep).filter(Boolean).length;
}

function buildReplacementPath(depth) {
    const prefix = '../'.repeat(depth + 2);
    return prefix + 'third_party/tgfx/web/src/';
}

function replacePathInFiles(rootDir) {
    const extension = '.d.ts';
    const searchString = '@tgfx/';

    const files = getFilesInDir(rootDir, extension);

    files.forEach(file => {
        const depth = getDepth(file, rootDir);
        const targetStr = buildReplacementPath(depth);
        replaceInFile(file, searchString, targetStr);
    });
}

if(argv.a ==="wasm-mt" || argv.a ==="wasm"){
    restoreFile(filePath);
    if(argv.a ==="wasm-mt"){
        replaceFileNameInFiles();
    }
    replacePathInFiles(path.resolve(__dirname, "../types/web/src"));
}else if(argv.a ==="wechat"){
    replacePathInFiles(path.resolve(__dirname, "../wechat/types/web/src"));
}
