const express = require('express');
const app = express();
const path = require('path');
const { execFile } = require('child_process');

// Enable SharedArrayBuffer
app.use((req, res, next) => {
  res.set('Cross-Origin-Opener-Policy', 'same-origin');
  res.set('Cross-Origin-Embedder-Policy', 'require-corp');
  next();
});

// 配置静态文件服务，显式设置.wasm文件的MIME类型
app.use('', express.static(path.join('./'), {
  setHeaders: (res, filePath) => {
    if (filePath.endsWith('.wasm')) {
      res.set('Content-Type', 'application/wasm');
    }
  }
}));

app.get('/', (req, res) => {
  res.send('Hello, libpag demo!');
});

function OpenBrowser(url) {
  if (process.platform === 'darwin') {
    execFile('open', [url]);
    return;
  }
  // `start` is a cmd.exe builtin, so it can only be reached through the shell interpreter while
  // the URL itself stays a separate argument that cmd never parses.
  execFile('cmd', ['/c', 'start', '', url]);
}

const port = 8081;
const args = process.argv.slice(2);
const target = require('./script/targets').resolveTarget(
  args.includes('wasm-mt') ? 'wasm-mt' : 'wasm',
  args.includes('webgpu') ? 'webgpu' : 'gl'
);
app.listen(port, () => {
  console.log(`Server is running on port ${port}`);
  OpenBrowser(`http://localhost:${port}/demo/${target.demoHtml}`);
});
