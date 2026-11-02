const crypto = require('node:crypto');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const { ArtifactNames, Backends, Modes, SnapshotBuild, HashText, RemoveBackup, ValidateBuildInfo,
  ValidateCache, ValidateResult } = require('./artifacts');
const { SourceMetadata, ValidateProvenance, CheckUpdateSource } = require('./cache-source');

const ProjectRoot = path.resolve(__dirname, '../..');
const TimeoutMS = 30 * 60 * 1000;

function ParseOptions(args) {
  const options = { backend: 'webgl', mode: 'compare', filter: '', output: '', noSandbox: false };
  for (let i = 0; i < args.length; i++) {
    if (args[i] === '--no-sandbox') {
      options.noSandbox = true;
      continue;
    }
    const match = /^--(backend|mode|filter|output)(?:=(.*))?$/.exec(args[i]);
    if (!match) throw new Error(`Unknown option: ${args[i]}`);
    const value = match[2] === undefined ? args[++i] : match[2];
    if (!value || value.startsWith('--')) throw new Error(`Missing value: ${match[1]}`);
    options[match[1]] = value;
  }
  if (!Backends.includes(options.backend) || !Modes.includes(options.mode)) {
    throw new Error('Invalid backend or mode');
  }
  if (options.mode === 'update' && options.filter) throw new Error('Filtered baseline updates are forbidden');
  return options;
}

function ReadCache(directory) {
  const cache = {
    md5: fs.readFileSync(path.join(directory, 'md5.json'), 'utf8'),
    version: fs.readFileSync(path.join(directory, 'version.json'), 'utf8'),
    head: fs.readFileSync(path.join(directory, 'HEAD'), 'utf8'),
  };
  ValidateCache(cache);
  return cache;
}

function ReadExpectedCache(directory, backend) {
  const cache = ReadCache(directory);
  const provenance = JSON.parse(fs.readFileSync(path.join(directory, 'provenance.json'), 'utf8'));
  const metadata = SourceMetadata(provenance.buildInfo?.sourceRevision);
  ValidateProvenance(cache, provenance, backend, metadata);
  return { ...cache, provenance };
}

function StartServer(directory) {
  const allowed = new Set([...ArtifactNames, 'build-info.json']);
  function handleRequest(request, response) {
    const name = request.url?.slice(1);
    if (!['GET', 'HEAD'].includes(request.method) || !allowed.has(name)) {
      response.writeHead(404).end();
      return;
    }
    const file = path.join(directory, name);
    // The allowlist above already rejects traversal sequences; keep this containment check
    // explicit so the request URL can never reach the file system unsanitized.
    if (path.dirname(file) !== directory) {
      response.writeHead(404).end();
      return;
    }
    const types = { '.js': 'text/javascript', '.html': 'text/html', '.json': 'application/json',
      '.wasm': 'application/wasm' };
    response.setHeader('Content-Type', types[path.extname(file)] || 'application/octet-stream');
    response.setHeader('Cache-Control', 'no-store');
    response.setHeader('X-Content-Type-Options', 'nosniff');
    if (request.method === 'HEAD') {
      response.end();
      return;
    }
    const stream = fs.createReadStream(file);
    stream.on('error', function onError() { response.destroy(); });
    stream.pipe(response);
  }
  return new Promise(function start(resolve, reject) {
    const server = http.createServer(handleRequest);
    server.once('error', reject);
    server.listen(0, '127.0.0.1', function onListening() { resolve(server); });
  });
}

async function WithDeadline(promise, timeout, message) {
  let timer = null;
  try {
    const deadline = new Promise(function limit(resolve, reject) {
      timer = setTimeout(function expired() { reject(new Error(message)); }, timeout);
    });
    return await Promise.race([promise, deadline]);
  } finally {
    clearTimeout(timer);
  }
}

async function CloseResources(browser, server, timeout = 10000) {
  const errors = [];
  try {
    if (browser) await WithDeadline(browser.close(), timeout, 'Browser cleanup timed out');
  } catch (error) {
    errors.push(error.message);
    try {
      browser.process()?.kill('SIGKILL');
    } catch (killError) {
      errors.push(killError.message);
    }
  } finally {
    if (server) {
      try {
        const stopped = new Promise(function stop(resolve, reject) {
          server.close(function closed(error) { if (error) reject(error); else resolve(); });
          server.closeAllConnections();
        });
        await WithDeadline(stopped, timeout, 'HTTP server cleanup timed out');
      } catch (error) {
        errors.push(error.message);
      }
    }
  }
  return errors;
}

function InstallConfig(config) {
  window.__pagTestConfig = config;
}

function IsFinished() {
  return window.__pagTest?.finished === true;
}

function ReadState() {
  return window.__pagTest;
}

function ListFiles(roots) {
  const fs = window.__pagTestFS;
  const files = [];
  if (!fs) return files;
  function visit(file) {
    const stat = fs.lstat(file);
    if (fs.isDir(stat.mode)) {
      for (const name of fs.readdir(file)) {
        if (name !== '.' && name !== '..') visit(`${file}/${name}`);
      }
    } else if (fs.isFile(stat.mode)) {
      files.push({ path: file, size: stat.size });
    } else {
      throw new Error(`Unsupported output entry: ${file}`);
    }
  }
  for (const root of roots) {
    if (fs.analyzePath(root).exists) visit(root);
  }
  return files;
}

function ReadChunk(file, offset, length) {
  const fs = window.__pagTestFS;
  const stream = fs.open(file, 'r');
  try {
    const bytes = new Uint8Array(length);
    const count = fs.read(stream, bytes, 0, length, offset);
    if (count !== length) throw new Error(`Short read: ${file}`);
    let binary = '';
    for (let i = 0; i < bytes.length; i += 8192) {
      binary += String.fromCharCode(...bytes.subarray(i, i + 8192));
    }
    return btoa(binary);
  } finally {
    fs.close(stream);
  }
}

function OutputPath(directory, file, backend, mode) {
  const allowed = file === '/test/report.json' || file.startsWith('/test/out/') ||
    file.startsWith('/test/baseline-out/') ||
    (mode === 'update' && file.startsWith(`/test/baseline/.cache/${backend}/`));
  if (!allowed || file.includes('\\') || file.includes('\0') || file.split('/').includes('..')) {
    throw new Error(`Invalid output path: ${file}`);
  }
  const destination = path.resolve(directory, file.slice('/test/'.length));
  if (!destination.startsWith(path.resolve(directory) + path.sep)) throw new Error('Output path escapes run');
  return destination;
}

async function CollectFiles(page, directory, options) {
  const roots = ['/test/report.json', '/test/out', '/test/baseline-out'];
  if (options.mode === 'update') roots.push(`/test/baseline/.cache/${options.backend}`);
  const files = await page.evaluate(ListFiles, roots);
  const seen = new Set();
  let total = 0;
  for (const file of files) {
    if (!Number.isSafeInteger(file.size) || file.size < 0 || file.size > 512 * 1024 * 1024 ||
        seen.has(file.path) || files.length > 200000) throw new Error('Invalid artifact manifest');
    seen.add(file.path);
    total += file.size;
    if (total > 2 * 1024 * 1024 * 1024) throw new Error('Artifact size budget exceeded');
    const destination = OutputPath(directory, file.path, options.backend, options.mode);
    fs.mkdirSync(path.dirname(destination), { recursive: true });
    const fd = fs.openSync(destination, 'wx');
    try {
      for (let offset = 0; offset < file.size; offset += 256 * 1024) {
        const length = Math.min(256 * 1024, file.size - offset);
        const data = Buffer.from(await page.evaluate(ReadChunk, file.path, offset, length), 'base64');
        if (data.length !== length) throw new Error(`Incomplete artifact: ${file.path}`);
        fs.writeFileSync(fd, data);
      }
    } finally {
      fs.closeSync(fd);
    }
  }
  return files;
}

function CommitCache(source, destination, baselineVersion, runInfo) {
  ValidateResult(runInfo.result);
  if (runInfo.result.exitCode !== 0 || runInfo.errors.length || runInfo.buildInfo.mode !== 'update') {
    throw new Error('Cannot publish a failed baseline run');
  }
  const cache = ReadCache(source);
  if (cache.version !== baselineVersion) throw new Error('Generated cache version mismatch');
  runInfo.cacheHashes = {};
  for (const name of ['md5', 'version', 'head']) runInfo.cacheHashes[name] = HashText(cache[name]);
  ValidateProvenance(cache, runInfo, runInfo.buildInfo.backend, runInfo.source);
  const parent = path.dirname(destination);
  fs.mkdirSync(parent, { recursive: true });
  const stage = fs.mkdtempSync(path.join(parent, '.web-cache-'));
  const backup = `${stage}.previous`;
  try {
    fs.writeFileSync(path.join(stage, 'md5.json'), cache.md5);
    fs.writeFileSync(path.join(stage, 'version.json'), cache.version);
    fs.writeFileSync(path.join(stage, 'HEAD'), cache.head);
    fs.writeFileSync(path.join(stage, 'provenance.json'), JSON.stringify(runInfo, null, 2) + '\n');
    if (fs.existsSync(destination)) fs.renameSync(destination, backup);
    try {
      fs.renameSync(stage, destination);
    } catch (error) {
      if (fs.existsSync(backup)) fs.renameSync(backup, destination);
      throw error;
    }
    RemoveBackup(backup);
  } finally {
    fs.rmSync(stage, { recursive: true, force: true });
  }
}

async function Run(options) {
  if (options.mode === 'update' && process.env.PAG_WEB_BASELINE_INVOKED !== '1') {
    throw new Error('Baseline generation requires the explicit baseline entry point');
  }
  const published = path.join(__dirname, 'dist', `${options.backend}-${options.mode}`);
  const cacheDirectory = path.join(ProjectRoot, 'test/baseline/.cache', options.backend);
  let buildInfo = null;
  let config = null;
  const output = options.output ? path.resolve(options.output) :
    path.join(__dirname, 'results', `${options.backend}-${options.mode}-${crypto.randomUUID()}`);
  if (!output.startsWith(ProjectRoot + path.sep) || fs.existsSync(output)) {
    throw new Error('Output must be a new directory inside the repository');
  }
  fs.mkdirSync(output, { recursive: true });
  const runInfo = { buildInfo, filter: options.filter, result: null, errors: [], artifacts: [] };
  let server = null;
  let browser = null;
  let page = null;
  let timer = null;
  let closing = false;
  let failed = false;
  let collecting = false;
  try {
    const directory = path.join(output, 'artifacts');
    buildInfo = SnapshotBuild(published, directory, options.backend, options.mode);
    runInfo.buildInfo = buildInfo;
    config = { buildInfo, filter: options.filter,
      baselineVersion: fs.readFileSync(path.join(ProjectRoot, 'test/baseline/version.json'), 'utf8'),
      cache: options.mode === 'compare' ? ReadExpectedCache(cacheDirectory, options.backend) : null };
    if (options.mode === 'update') {
      runInfo.source = { ...CheckUpdateSource(buildInfo), ref: 'origin/main', clean: true };
    } else {
      runInfo.cacheProvenance = config.cache.provenance;
    }
    server = await StartServer(directory);
    const args = ['--enable-unsafe-webgpu'];
    if (options.noSandbox) args.push('--no-sandbox', '--disable-setuid-sandbox');
    if (process.platform === 'darwin') args.push('--use-gl=angle', '--use-angle=metal');
    if (process.platform === 'linux') {
      args.push('--disable-dev-shm-usage');
      if (options.backend === 'webgpu') args.push('--use-webgpu-adapter=swiftshader');
    }
    browser = await require('puppeteer').launch({ headless: true, args });
    runInfo.browser = await browser.version();
    page = await browser.newPage();
    page.on('console', function onConsole(message) { console.log(message.text()); });
    await page.evaluateOnNewDocument(InstallConfig, config);
    const fatal = new Promise(function watch(resolve, reject) {
      timer = setTimeout(function onTimeout() { reject(new Error('Test or artifact transfer timed out')); }, TimeoutMS);
      page.on('pageerror', function onPageError(error) { reject(error); });
      page.on('error', function onCrash(error) { reject(error); });
      browser.on('disconnected', function onDisconnected() {
        if (!closing) reject(new Error('Browser disconnected'));
      });
    });
    async function execute() {
      await page.goto(`http://127.0.0.1:${server.address().port}/index.html`,
        { waitUntil: 'domcontentloaded', timeout: TimeoutMS });
      await page.waitForFunction(IsFinished, { timeout: TimeoutMS });
      const state = await page.evaluate(ReadState);
      runInfo.result = state.result;
      runInfo.errors.push(...state.errors);
      collecting = true;
      runInfo.artifacts = await CollectFiles(page, output, options);
      ValidateBuildInfo(state.buildInfo, options.backend, options.mode);
      if (state.buildInfo.buildId !== buildInfo.buildId) throw new Error('WASM build identity mismatch');
      ValidateResult(state.result);
      const report = JSON.parse(fs.readFileSync(path.join(output, 'report.json'), 'utf8'));
      if (!Number.isSafeInteger(report.disabled) || report.tests - report.disabled !== state.result.tests ||
          report.failures !== state.result.failed) {
        throw new Error('GoogleTest report does not match the execution result');
      }
      const finalState = await page.evaluate(ReadState);
      runInfo.errors = finalState.errors;
      if (state.result.exitCode !== 0 || runInfo.errors.length) throw new Error('Tests or browser validation failed');
    }
    await Promise.race([execute(), fatal]);
  } catch (error) {
    failed = true;
    runInfo.errors.push(error.message);
    console.error(error.message);
    if (page && !page.isClosed() && !collecting) {
      let partialTimer = null;
      try {
        collecting = true;
        const deadline = new Promise(function limit(resolve, reject) {
          partialTimer = setTimeout(function timeout() { reject(new Error('Partial transfer timed out')); }, 10000);
        });
        runInfo.artifacts = await Promise.race([CollectFiles(page, output, options), deadline]);
      } catch (artifactError) {
        runInfo.errors.push(`Partial artifacts unavailable: ${artifactError.message}`);
      } finally {
        clearTimeout(partialTimer);
      }
    }
  } finally {
    clearTimeout(timer);
    closing = true;
    const cleanupErrors = await CloseResources(browser, server);
    if (cleanupErrors.length) {
      failed = true;
      runInfo.errors.push(...cleanupErrors);
    }
  }
  if (!failed && options.mode === 'update') {
    try {
      CheckUpdateSource(buildInfo);
      CommitCache(path.join(output, 'baseline/.cache', options.backend), cacheDirectory,
        config.baselineVersion, runInfo);
    } catch (error) {
      failed = true;
      runInfo.errors.push(error.message);
    }
  }
  runInfo.exitCode = failed ? 1 : 0;
  fs.writeFileSync(path.join(output, 'run-result.json'), JSON.stringify(runInfo, null, 2) + '\n');
  console.log(`Results: ${output}`);
  if (runInfo.result) console.log(JSON.stringify(runInfo.result));
  return failed ? 1 : 0;
}

module.exports = { ParseOptions, ReadCache, ReadExpectedCache, StartServer, CloseResources, OutputPath,
  CollectFiles, CommitCache, Run };

if (require.main === module) {
  Run(ParseOptions(process.argv.slice(2))).then(function finish(code) {
    process.exitCode = code;
  }).catch(function fail(error) {
    console.error(error.message);
    process.exitCode = 1;
  });
}
