export const calculateDisplaySize = (canvas: HTMLCanvasElement) => {
  const styleDeclaration = globalThis.getComputedStyle(canvas, null);
  const computedSize = {
    width: Number(styleDeclaration.width.replace('px', '')),
    height: Number(styleDeclaration.height.replace('px', '')),
  };
  if (computedSize.width > 0 && computedSize.height > 0) {
    return computedSize;
  } else {
    const styleSize = {
      width: Number(canvas.style.width.replace('px', '')),
      height: Number(canvas.style.height.replace('px', '')),
    };
    if (styleSize.width > 0 && styleSize.height > 0) {
      return styleSize;
    } else {
      return {
        width: canvas.width,
        height: canvas.height,
      };
    }
  }
};
