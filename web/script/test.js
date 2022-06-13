const { exec, spawn } = require('child_process');

const controller = new AbortController();
const { signal } = controller;
const grep = spawn('npm', ['run', 'server'], { signal });
grep.on('error', (err) => {
  // 如果控制器中止，则这将在 err 为 AbortError 的情况下被调用
});

const ls = spawn('npm', ['run', 'cypress']);
ls.stdout.on('data', (data) => {
  console.log(`${data}`);
});
ls.stderr.on('data', (data) => {
  console.error(`${data}`);
});
ls.on('close', (code) => {
  controller.abort(); // stop server process
  console.log(`child process exited with code ${code}`);
});
