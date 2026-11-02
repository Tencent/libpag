// Generates the four demo html variants from demo/index.template.html. The generated files are
// deterministic, so committed copies never drift from the template.
const fs = require('fs');
const path = require('path');

const { TARGETS } = require('./targets');

const demoDir = path.resolve(__dirname, '../demo');
const template = fs.readFileSync(path.join(demoDir, 'index.template.html'), 'utf8');

for (const { demoHtml, wasmDir } of TARGETS) {
  fs.writeFileSync(path.join(demoDir, demoHtml), template.replaceAll('__DEMO_OUT_DIR__', wasmDir));
}
console.log('Demo html variants generated.');
