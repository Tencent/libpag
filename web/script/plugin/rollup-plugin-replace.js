import MagicString from 'magic-string';
import { replaceFunctionConfig } from './replace-config';

export default function replaceFunc() {
  return {
    name: 'replaceFunc',
    transform(code, id) {
      let codeStr = `${code}`;
      const magic = new MagicString(codeStr);
      replaceFunctionConfig.forEach((item) => {
        if (item.type === 'string') {
          const startOffset = codeStr.indexOf(item.start);
          if (startOffset > -1) {
            magic.overwrite(startOffset, startOffset + item.start.length, item.replaceStr);
          }
        } else {
          const startOffset = codeStr.indexOf(item.start);
          const endOffset = codeStr.indexOf(item.end);
          if (startOffset > -1 && endOffset > startOffset && item.replaceStr) {
            magic.overwrite(startOffset, endOffset, item.replaceStr);
          }
        }
      });
      return {
        code: magic.toString(),
        map: magic.generateMap({ hires: true }),
      };
    },
  };
}
