import { exec } from 'child_process';
import express from 'express';
const app = express();
import path from 'path';

// Enable SharedArrayBuffer
app.use((req, res, next) => {
  res.set('Cross-Origin-Opener-Policy', 'same-origin');
  res.set('Cross-Origin-Embedder-Policy', 'require-corp');
  next();
});

app.use('', express.static(path.join('./')));
app.use('', express.static(path.join('demo/')));
// 配置静态文件服务，显式设置.wasm文件的MIME类型
app.use('', express.static(path.join('./'), {
  setHeaders: (res, filePath) => {
    if (filePath.endsWith('.wasm')) {
      res.set('Content-Type', 'application/wasm');
    }
  }
}));
app.use('', express.static(path.join('demo/'), {
  setHeaders: (res, filePath) => {
    if (filePath.endsWith('.wasm')) {
      res.set('Content-Type', 'application/wasm');
    }
  }
}));

app.get('/', (req, res) => {
  res.send('Hello, tgfx-benchmark!');
});

const port = 8081;
const args = process.argv.slice(2);
const fileName = args.includes('wasm-mt') ? 'index': 'index-st';
app.listen(port, () => {
  console.log(`Server is running on port ${port}`);

  const url = `http://localhost:${port}/${fileName}.html`;
  const start = (process.platform === 'darwin'? 'open': 'start');
  exec(start + ' ' + url);
});