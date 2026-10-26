// Generates the four demo html variants from demo/index.template.html. The generated files are
// deterministic, so committed copies never drift from the template.
const fs = require('fs');
const path = require('path');

const demoDir = path.resolve(__dirname, '../demo');
const template = fs.readFileSync(path.join(demoDir, 'index.template.html'), 'utf8');

const variants = [
  ['index.html', 'wasm-mt'],
  ['index-st.html', 'wasm'],
  ['index-webgpu.html', 'wasm-webgpu'],
  ['index-webgpu-mt.html', 'wasm-webgpu-mt'],
];

for (const [fileName, outDir] of variants) {
  fs.writeFileSync(path.join(demoDir, fileName), template.replaceAll('__DEMO_OUT_DIR__', outDir));
}
console.log('Demo html variants generated.');
