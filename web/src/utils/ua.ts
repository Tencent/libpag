declare const globalThis: any;

let ANDROID = false;
let MOBILE = false;
let MACOS = false;
let IPHONE = false;
let isWechatMiniProgram = globalThis.isWechatMiniProgram = !!globalThis.wx;
if (!globalThis.wx) {
    const nav = navigator.userAgent;
    ANDROID = /android|adr/i.test(nav);
    MOBILE = /(mobile)/i.test(nav) && ANDROID;
    MACOS = !(/(mobile)/i.test(nav) || MOBILE) && /Mac OS X/i.test(nav);
    IPHONE = /(iphone|ipad|ipod)/i.test(nav);
}

export {
    ANDROID,
    MOBILE,
    MACOS,
    IPHONE,
    isWechatMiniProgram
}


