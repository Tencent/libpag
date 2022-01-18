import './App.css';
import { useEffect } from 'react';
import { PAGInit } from 'libpag';

function App() {
  useEffect(() => {
    PAGInit({
      locateFile: (file) => 'https://unpkg.com/libpag@latest/lib/' + file,
    }).then((PAG) => {
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
  });
  return (
    <div className="App">
      <header className="App-header">
        <canvas id="pag"></canvas>
      </header>
    </div>
  );
}

export default App;
