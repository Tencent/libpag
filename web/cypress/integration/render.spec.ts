import { saveData, compareData } from '../utils';
import { PAGInit } from '../../src/pag';

describe('Common render', () => {
  it('Vector render', () => {
    cy.visit('/index.html');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: { PAGInit: typeof PAGInit } }) => {
      const PAG = await window.libpag.PAGInit({});
      const pagUrl = '../demo/assets/like.pag';
      const fileBlob = await window.fetch(pagUrl).then((response) => response.blob());
      const file = new window.File([fileBlob], pagUrl.replace(/(.*\/)*([^.]+)/i, '$2'));
      const pagFile = await PAG.PAGFile.load(file);
      const canvas = window.document.getElementById('pag') as HTMLCanvasElement;
      canvas.width = 300;
      canvas.height = 300;
      const pagView = await PAG.PAGView.init(pagFile, canvas);
      await pagView.setProgress(0.5);
      const currentCanvas = window.document.createElement('canvas');
      currentCanvas.width = 600;
      currentCanvas.height = 600;
      const currentCtx = currentCanvas.getContext('2d');
      currentCtx.drawImage(canvas, 0, 0);
      const imageData = currentCtx.getImageData(0, 0, currentCanvas.width, currentCanvas.height);

      // compare
      compareData('vector', imageData.data);

      // Save
      // saveData('vector', new Uint8Array(imageData.data.buffer)).then(() => {
      //   console.log('succeed');
      // });
    });
  });

  it('Video render', () => {
    cy.visit('/index.html');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: { PAGInit: typeof PAGInit } }) => {
      const PAG = await window.libpag.PAGInit({});
      const pagUrl = '../demo/assets/particle_video.pag';
      const fileBlob = await window.fetch(pagUrl).then((response) => response.blob());
      const file = new window.File([fileBlob], pagUrl.replace(/(.*\/)*([^.]+)/i, '$2'));
      const pagFile = await PAG.PAGFile.load(file);
      const canvas = window.document.getElementById('pag') as HTMLCanvasElement;
      canvas.width = 300;
      canvas.height = 300;
      const pagView = await PAG.PAGView.init(pagFile, canvas);
      await pagView.setProgress(0.5);
      const currentCanvas = window.document.createElement('canvas');
      currentCanvas.width = 600;
      currentCanvas.height = 600;
      const currentCtx = currentCanvas.getContext('2d');
      currentCtx.drawImage(canvas, 0, 0);
      const imageData = currentCtx.getImageData(0, 0, currentCanvas.width, currentCanvas.height);

      // compare
      compareData('video', imageData.data);

      // Save
      // saveData('video', new Uint8Array(imageData.data.buffer)).then(() => {
      //   console.log('succeed');
      // });
    });
  });

  it('Text render', () => {
    cy.visit('/index.html');
    cy.window().then(async (window: Cypress.AUTWindow & { libpag: { PAGInit: typeof PAGInit } }) => {
      const PAG = await window.libpag.PAGInit({});
      const pagUrl = '../demo/assets/test2.pag';
      const fileBlob = await window.fetch(pagUrl).then((response) => response.blob());
      const file = new window.File([fileBlob], pagUrl.replace(/(.*\/)*([^.]+)/i, '$2'));
      const pagFile = await PAG.PAGFile.load(file);
      const canvas = window.document.getElementById('pag') as HTMLCanvasElement;
      canvas.width = 300;
      canvas.height = 300;
      const pagView = await PAG.PAGView.init(pagFile, canvas);
      await pagView.setProgress(0.5);
      const currentCanvas = window.document.createElement('canvas');
      currentCanvas.width = 600;
      currentCanvas.height = 600;
      const currentCtx = currentCanvas.getContext('2d');
      currentCtx.drawImage(canvas, 0, 0);
      const imageData = currentCtx.getImageData(0, 0, currentCanvas.width, currentCanvas.height);

      // compare
      compareData('text', imageData.data);

      // Save
      // saveData('text', new Uint8Array(imageData.data.buffer)).then(() => {
      //   console.log('succeed');
      // });
    });
  });
});
