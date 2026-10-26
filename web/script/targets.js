// Single source of truth for the backend x arch artifact mapping. Build scripts must derive
// lib/wasm directory names and the demo html file name from this table instead of re-deriving
// them locally, so adding or renaming a variant touches exactly one file.
const TARGETS = [
  { arch: 'wasm', backend: 'gl', wasmDir: 'wasm', libDir: 'lib', demoHtml: 'index-st.html' },
  { arch: 'wasm-mt', backend: 'gl', wasmDir: 'wasm-mt', libDir: 'lib-mt', demoHtml: 'index.html' },
  { arch: 'wasm', backend: 'webgpu', wasmDir: 'wasm-webgpu', libDir: 'lib-webgpu', demoHtml: 'index-webgpu.html' },
  { arch: 'wasm-mt', backend: 'webgpu', wasmDir: 'wasm-webgpu-mt', libDir: 'lib-webgpu-mt', demoHtml: 'index-webgpu-mt.html' },
];

// Unknown values fall back to the single-threaded GL variant, matching the historical behavior
// of the ternary chains this table replaces.
const resolveTarget = (arch, backend) => {
  const normalizedArch = arch === 'wasm-mt' ? 'wasm-mt' : 'wasm';
  const normalizedBackend = backend === 'webgpu' ? 'webgpu' : 'gl';
  return TARGETS.find((target) => target.arch === normalizedArch && target.backend === normalizedBackend);
};

const resolveByWasmDir = (wasmDir) => TARGETS.find((target) => target.wasmDir === wasmDir);

module.exports = { TARGETS, resolveTarget, resolveByWasmDir };
