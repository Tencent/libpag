const loadImage = (url) => {
  return new Promise((resolve) => {
    const image = new Image();
    image.onload = () => {
      resolve(image);
    };
    image.src = url;
  });
};

const compare = (array_1, array_2) => {
  for (let i = 0; i < array_1.length; i++) {
    if (array_1[i] !== array_2[i]) return false;
  }
  return true;
};

describe('My First Test', () => {
  it('finds the content "type"', async () => {
    cy.visit('/index.html');
    const window = await cy.window();
    const PAG = await window.libpag.PAGInit();
    const pagUrl = '../assets/like.pag';
    const fileBlob = await window.fetch(pagUrl).then((response) => response.blob());
    const file = new window.File([fileBlob], pagUrl.replace(/(.*\/)*([^.]+)/i, '$2'));
    const pagFile = await PAG.PAGFile.load(file);
    const canvas = window.document.getElementById('pag');
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

    const testImage = await loadImage('../../downloads/test.png');
    const testCanvas = window.document.createElement('canvas');
    testCanvas.width = 600;
    testCanvas.height = 600;
    const testCtx = testCanvas.getContext('2d');
    testCtx.drawImage(testImage, 0, 0);
    const testImageData = testCtx.getImageData(0, 0, testImage.width, testImage.height);
    const res = compare(imageData.data, testImageData.data);
    expect(res).to.be.true;
  });
});
