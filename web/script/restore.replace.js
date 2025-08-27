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

function replaceStringInFiles() {
    const dir = path.resolve(__dirname, "../lib-mt/");
    const extension = '.js';
    const searchString = 'libpag.js';
    const files = getFilesInDir(dir, extension);

    files.forEach(file => {
        const fileName = path.basename(file);
        replaceInFile(file, searchString, fileName);
    });
}

restoreFile(filePath);
if(argv.a !==undefined && argv.a ==="wasm-mt"){
    replaceStringInFiles();
}
