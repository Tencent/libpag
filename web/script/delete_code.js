const fs = require('fs');
const filePath = '../../third_party/tgfx/src/platform/web/TGFXWasmBindings.cpp';

const data = fs.readFileSync(filePath, 'utf8').split('\n');

const filteredData = data.filter(line => !line.includes("register_vector<std::string>(\"VectorString\");"));

fs.writeFileSync(filePath, filteredData.join('\n'), 'utf8');
