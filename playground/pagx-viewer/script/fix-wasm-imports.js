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

function replaceInFile(filePath, searchString, replacement) {
    const data = fs.readFileSync(filePath, 'utf-8');
    if (data.includes(searchString)) {
        const updatedData = data.replaceAll(searchString, replacement);
        fs.writeFileSync(filePath, updatedData, 'utf-8');
        console.log(`In file ${filePath}, "${searchString}" was replaced with "${replacement}"`);
    }
}

// Multi-threaded build keeps canonical filenames (pagx-viewer.umd.js, pagx-viewer.wasm).
// Single-threaded build adds a `.st` infix; the wasm reference baked into the emcc glue
// must be rewritten so each bundle loads its matching wasm.
const libDir = path.resolve(__dirname, '../lib');
const isSt = argv.a === 'wasm';
const nameInfix = isSt ? '.st' : '';
const bundleNames = ['umd', 'esm', 'cjs', 'min'].map(
    (kind) => `pagx-viewer${nameInfix}.${kind}.js`,
);

for (const bundleName of bundleNames) {
    const filePath = path.join(libDir, bundleName);
    if (!fs.existsSync(filePath)) {
        continue;
    }
    if (isSt) {
        // The emcc glue resolves `new URL("pagx-viewer.wasm", import.meta.url)`; redirect
        // it to the renamed single-threaded wasm.
        replaceInFile(filePath, 'pagx-viewer.wasm', 'pagx-viewer.st.wasm');
    } else {
        // The multi-threaded glue spawns pthread workers via
        // `new URL("pagx-viewer.js", import.meta.url)`; redirect that to the actual bundle
        // filename so the worker re-loads the same module.
        replaceInFile(filePath, 'pagx-viewer.js', bundleName);
    }
}
