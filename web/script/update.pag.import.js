const fs = require('fs');
const path = require('path');

const { wasmDir } = require('./targets').resolveTarget(process.env.ARCH, process.env.BACKEND);
const filePath = path.resolve(__dirname, '../src/pag.ts');
console.log("target:", wasmDir);
fs.readFile(filePath, 'utf8', (err, data) => {
    if (err) {
        console.error('Error reading file:', err);
        return;
    }

    let newImportPath = `./${wasmDir}/libpag`;
    const updatedData = data.replace(/import createPAG from '.*';/, `import createPAG from '${newImportPath}';`);
    fs.writeFile(filePath, updatedData, 'utf8', (err) => {
        if (err) {
            console.error('Error writing file:', err);
            return;
        }
        console.log(`Successfully updated import path to: ${newImportPath}`);
    });
});
