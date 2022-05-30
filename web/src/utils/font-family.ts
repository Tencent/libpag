const fontNames = ['Arial', 'Courier New', 'Georgia', 'Times New Roman', 'Trebuchet MS', 'Verdana'];

export const defaultFontNames = (() => {
  return ['emoji'].concat(...fontNames);
})();

export const getFontFamilies = (name: string, style = ''): string[] => {
  if (!name) return [];
  const nameChars = name.split(' ');
  let names = [];
  if (nameChars.length === 1) {
    names.push(name);
  } else {
    names.push(nameChars.join(''));
    names.push(nameChars.join(' '));
  }
  const fontFamilies = names.reduce((pre: string[], cur: string) => {
    if (!style) {
      pre.push(`"${cur}"`);
    } else {
      pre.push(`"${cur} ${style}"`);
      pre.push(`"${cur}-${style}"`);
    }
    return pre;
  }, []);
  // Fallback font when style is not found.
  if (style !== '') {
    fontFamilies.push(`"${name}"`);
  }
  return fontFamilies;
};
