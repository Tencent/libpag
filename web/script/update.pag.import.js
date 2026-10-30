const fs = require('fs');
const path = require('path');

const { wasmDir } = require('./targets').resolveTarget(process.env.ARCH, process.env.BACKEND);
const filePath = path.resolve(__dirname, '../src/pag.ts');
console.log("target:", wasmDir);
// Rollup reads its input immediately after requiring this module; finish rewriting before returning.
const data = fs.readFileSync(filePath, 'utf8');
const newImportPath = `./${wasmDir}/libpag`;
const updatedData = data.replace(/import createPAG from '.*';/, `import createPAG from '${newImportPath}';`);
if (updatedData !== data) {
    fs.writeFileSync(filePath, updatedData, 'utf8');
}
console.log(`Successfully updated import path to: ${newImportPath}`);
