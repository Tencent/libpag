/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

// Monaco is loaded from CDN at runtime via the AMD loader (same as the monaco-test.html page).
// The npm package is kept in devDependencies for TypeScript types only — `import type` is erased
// by esbuild at build time, so rollup never tries to bundle Monaco's non-standard ESM internals
// (which use bare `vs/...` specifiers that nodeResolve cannot map).
import type * as MonacoNS from 'monaco-editor';

export type Monaco = typeof MonacoNS;

// Module-level Monaco singleton. Loaded on demand when the first editor is created — nothing is
// fetched at import time, so hosts that never open the editor panel never touch the CDN. All
// editors share the same instance. The AMD loader is injected as a <script> tag, then
// require(['vs/editor/editor.main']) pulls in the editor + all basic languages (including XML).
const MONACO_CDN = 'https://cdn.jsdelivr.net/npm/monaco-editor@0.47.0/min/vs';

let monacoInstance: Monaco | null = null;
let monacoLoadPromise: Promise<Monaco> | null = null;

export function getMonaco(): Monaco | null {
    return monacoInstance;
}

export function loadMonaco(): Promise<Monaco> {
    if (monacoInstance !== null) {
        return Promise.resolve(monacoInstance);
    }
    if (monacoLoadPromise !== null) {
        return monacoLoadPromise;
    }
    monacoLoadPromise = new Promise<Monaco>((resolve, reject) => {
        const script = document.createElement('script');
        script.src = `${MONACO_CDN}/loader.js`;
        // Reset the cached promise on failure so a later open() retries the CDN load; without
        // this, one transient network failure would leave every future loadMonaco() call stuck
        // on the rejected promise (editor dead until a full page reload).
        const failLoad = (message: string): void => {
            monacoLoadPromise = null;
            script.remove();
            reject(new Error(message));
        };
        script.onload = () => {
            const win = window as unknown as {
                require?: {
                    config: (opts: { paths: { vs: string } }) => void;
                    (deps: string[], cb: (...modules: unknown[]) => void,
                     errback?: (err: unknown) => void): void;
                };
                monaco?: Monaco;
            };
            const amdRequire = win.require;
            if (amdRequire === undefined) {
                failLoad('Monaco AMD loader failed to initialize');
                return;
            }
            amdRequire.config({ paths: { vs: MONACO_CDN } });
            amdRequire(['vs/editor/editor.main'], () => {
                const m = win.monaco;
                if (m === undefined) {
                    failLoad('Monaco failed to load from CDN');
                    return;
                }
                pruneBuiltInContextMenuItems(amdRequire);
                definePagxTheme(m);
                installShadowMenuStyles();
                monacoInstance = m;
                resolve(m);
            }, () => failLoad('Monaco modules failed to load from CDN'));
        };
        script.onerror = () => failLoad('Failed to load Monaco loader script from CDN');
        document.head.appendChild(script);
    });
    return monacoLoadPromise;
}

// Defines a custom theme matching the VS Code Dark+ palette used by the previous CodeMirror
// highlighter, so syntax colors stay consistent across the migration.
function definePagxTheme(m: Monaco): void {
    m.editor.defineTheme('pagx-dark', {
        base: 'vs-dark',
        inherit: true,
        rules: [
            { token: 'tag', foreground: '569CD6' },
            { token: 'attribute.name', foreground: '9CDCFE' },
            { token: 'attribute.value', foreground: 'CE9178' },
            { token: 'string', foreground: 'CE9178' },
            { token: 'comment', foreground: '6A9955' },
            { token: 'delimiter', foreground: '569CD6' },
            { token: '', foreground: 'D4D4D4' },
        ],
        colors: {
            'editor.background': '#1E1E1E',
            'editor.foreground': '#D4D4D4',
            'editorLineNumber.foreground': '#6E7681',
            'editorLineNumber.activeForeground': '#CCCCCC',
            // Keep active and inactive selections identical so the double-click transition from
            // read-only browsing into edit mode does not flash between two colors.
            'editor.selectionBackground': '#A8C7FA',
            'editor.inactiveSelectionBackground': '#A8C7FA',
            'editor.lineHighlightBackground': '#2D2D2D',
            'editorCursor.foreground': '#FFFFFF',
            'editorGutter.background': '#1E1E1E',
            // Opaque scrollbar slider colors (DevTools style). vs-dark's defaults are
            // semi-transparent (#79797966 etc.); setting opaque hex values here makes Monaco's
            // theme system emit --vscode-scrollbarSlider-background etc. as opaque colors, which
            // the built-in scrollbars.css applies to .slider via var(). This is more reliable
            // than overriding .slider background in EDITOR_STYLES because the variable is set on
            // .monaco-editor as an inline style by the theme application, and inline style + var()
            // would need !important on a very specific selector to override — setting the token
            // here updates the variable at the source.
            'scrollbarSlider.background': '#797979',
            'scrollbarSlider.hoverBackground': '#646464',
            'scrollbarSlider.activeBackground': '#505050',
            'scrollbar.shadow': '#00000000',
        },
    });
}

// The standalone editor offers no public API to remove built-in context menu entries (the
// clipboard trio, Change All Occurrences, Command Palette). The pagx editor replaces the whole
// menu with its own actions, so right after the AMD bundle loads - before any editor instance
// appends its own items - every entry registered under MenuId.EditorContext is cleared. This
// relies on the private MenuRegistry._menuItems structure of the pinned monaco-editor 0.47.0
// build (see MONACO_CDN); each step is shape-checked so a future Monaco upgrade degrades to
// showing the default menu instead of throwing.
function pruneBuiltInContextMenuItems(
    amdRequire: (deps: string[], cb: (...modules: unknown[]) => void) => void,
): void {
    amdRequire(['vs/platform/actions/common/actions'], (actionsModule: unknown) => {
        const actions = actionsModule as {
            MenuId?: { EditorContext?: unknown };
            MenuRegistry?: { _menuItems?: unknown };
        } | null;
        const menuId = actions?.MenuId?.EditorContext;
        const menuItems = actions?.MenuRegistry?._menuItems;
        if (menuId === undefined || !(menuItems instanceof Map)) {
            return;
        }
        const list = menuItems.get(menuId) as { clear?: () => void } | undefined;
        list?.clear?.();
    });
}

// Monaco 0.47.0 renders its context menu inside a Shadow DOM (div.shadow-root-host attached to
// <body>). Styles injected into document.head cannot cross that shadow boundary, so the plain
// EDITOR_STYLES sheet has no effect on menu items like span.keybinding. To style the shortcut
// column we watch for the shadow host to appear and adopt an extra sheet into its shadowRoot;
// the observer stops itself once the sheet is installed. Idempotent — subsequent Monaco loads
// (there is only ever one) short-circuit via the module-level flag.
let shadowMenuStylesInstalled = false;

function installShadowMenuStyles(): void {
    if (shadowMenuStylesInstalled || typeof document === 'undefined') {
        return;
    }
    const css = '.monaco-menu-container span.keybinding { color: #8A8A8A !important; }';
    const tryInstall = (host: Element): boolean => {
        const root = (host as Element & { shadowRoot?: ShadowRoot | null }).shadowRoot;
        if (root === null || root === undefined) {
            return false;
        }
        // Skip if we've already added the sheet to this root (Monaco reuses one shadow host).
        for (const child of Array.from(root.children)) {
            if (child instanceof HTMLStyleElement && child.dataset.pagxMenuStyles === 'true') {
                return true;
            }
        }
        const style = document.createElement('style');
        style.dataset.pagxMenuStyles = 'true';
        style.textContent = css;
        root.appendChild(style);
        return true;
    };
    // The host may already exist by the time Monaco resolves. Try synchronously first.
    for (const host of Array.from(document.querySelectorAll('.shadow-root-host'))) {
        if (tryInstall(host)) {
            shadowMenuStylesInstalled = true;
            return;
        }
    }
    // Otherwise wait for it to be appended to <body>. Monaco creates the host lazily the first
    // time the context menu opens; the observer disconnects as soon as we succeed.
    const observer = new MutationObserver(() => {
        for (const host of Array.from(document.querySelectorAll('.shadow-root-host'))) {
            if (tryInstall(host)) {
                shadowMenuStylesInstalled = true;
                observer.disconnect();
                return;
            }
        }
    });
    observer.observe(document.body, { childList: true, subtree: true });
}

// Clipboard write for the tag-block and whole-file copy actions. navigator.clipboard is only
// exposed in secure contexts, so plain-http hosts fall back to a temporary textarea and
// execCommand('copy') (deprecated, but the only mechanism available there).
export function copyTextToClipboard(text: string): void {
    if (navigator.clipboard !== undefined) {
        navigator.clipboard.writeText(text).then(undefined, () => execCommandCopy(text));
        return;
    }
    execCommandCopy(text);
}

function execCommandCopy(text: string): void {
    const textarea = document.createElement('textarea');
    textarea.value = text;
    textarea.style.position = 'fixed';
    textarea.style.opacity = '0';
    document.body.appendChild(textarea);
    textarea.select();
    document.execCommand('copy');
    textarea.remove();
}
