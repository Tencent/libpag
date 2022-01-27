<script setup>
import { PAGInit } from 'libpag';

PAGInit().then((PAG) => {
  const url = 'https://pag.io/file/like.pag';
  fetch(url)
    .then((response) => response.blob())
    .then(async (blob) => {
      const file = new window.File([blob], url.replace(/(.*\/)*([^.]+)/i, '$2'));
      const pagFile = await PAG.PAGFile.load(file);
      document.getElementById('pag').width = await pagFile.width();
      document.getElementById('pag').height = await pagFile.height();
      const pagView = await PAG.PAGView.init(pagFile, '#pag');
      pagView.setRepeatCount(0);
      await pagView.play();
    });
});
</script>

<template>
  <div>
    <canvas class="canvas" id="pag"></canvas>
  </div>
</template>

<style>
#app {
  font-family: Avenir, Helvetica, Arial, sans-serif;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
  text-align: center;
  color: #2c3e50;
  margin-top: 60px;
}
</style>
