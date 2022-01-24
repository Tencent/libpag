import MagicString from 'magic-string';
import replaceConfig from './replace-config';

export default function replaceFunc() {
  return {
    name: 'replaceFunc',
    transform (code, id) {
      let codeStr = `${code}`;
      const magic = new MagicString(codeStr);
      replaceConfig.forEach(item => {
        if (item.type === 'string') {
          codeStr = codeStr.replace(item.start, (match, offset) => {
            magic.overwrite(offset, offset + match.length, item.replaceStr)
          });
        } else {
          const startOffset = codeStr.indexOf(item.start);
          const endOffset = codeStr.indexOf(item.end);
          if (startOffset > 0 && endOffset > startOffset && item.replaceStr) {
            magic.overwrite(startOffset, endOffset, item.replaceStr)
          }
        }
      })
      return {
        code: magic.toString(),
        map: magic.generateMap({ hires: true })
      }
    }
  }
}
