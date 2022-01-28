import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import copy from 'rollup-plugin-copy'
// https://vitejs.dev/config/
export default defineConfig({
  plugins: [
    vue(),
    copy({
      targets: [
        { src: './node_modules/libpag/lib/libpag.wasm', dest: process.env.NODE_ENV === 'production' ? 'dist/' : './' },
      ],
      hook: process.env.NODE_ENV === 'production' ? 'writeBundle' : "buildStart",
    }),
  ],
  base: './',
});
