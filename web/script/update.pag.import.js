const fs = require('fs');
const path = require('path');

const arch = process.env.ARCH;
const target = (arch === 'wasm-mt'? 'wasm-mt': 'wasm');
const filePath = path.resolve(__dirname, '../src/pag.ts');
console.log("target:",target);
fs.readFile(filePath, 'utf8', (err, data) => {
    if (err) {
        console.error('Error reading file:', err);
        return;
    }

    let newImportPath = `./${target}/libpag`;
    const updatedData = data.replace(/import createPAG from '.*';/, `import createPAG from '${newImportPath}';`);
    fs.writeFile(filePath, updatedData, 'utf8', (err) => {
        if (err) {
            console.error('Error writing file:', err);
            return;
        }
        console.log(`Successfully updated import path to: ${newImportPath}`);
    });
});
