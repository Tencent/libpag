const express = require('express');
const app = express();
const path = require('path');

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

const port = 8081;
const args = process.argv.slice(2);
const fileName = args.includes('wasm-mt') ? 'index': 'index-st';
app.listen(port, () => {
  console.log(`Server is running on port ${port}`);

  const url = `http://localhost:${port}/demo/${fileName}.html`;
  const start = (process.platform === 'darwin'? 'open': 'start');
  require('child_process').exec(start + ' ' + url);
});
