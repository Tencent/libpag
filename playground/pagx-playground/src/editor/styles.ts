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
//  license is distributed on an "as IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

/** CSS rules for the editor panel, injected at runtime to avoid a separate rollup CSS plugin. */
export const EDITOR_STYLES = `
#editor-panel {
    position: fixed;
    top: 0;
    right: 0;
    bottom: 0;
    width: 50%;
    display: none;
    flex-direction: column;
    background: #1E1E1E;
    border-left: 1px solid #3C3C3C;
    z-index: 10;
}

#editor-panel.visible {
    display: flex;
}

.container.with-editor {
    width: 50%;
    margin: 0;
}

#editor-panel .editor-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    height: 40px;
    padding: 0 12px;
    background: #252526;
    border-bottom: 1px solid #3C3C3C;
    flex-shrink: 0;
}

#editor-panel .editor-title {
    color: #CCCCCC;
    font-size: 13px;
    font-weight: 500;
    user-select: none;
}

#editor-panel .editor-close-btn {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 28px;
    height: 28px;
    border: none;
    background: transparent;
    color: #CCCCCC;
    cursor: pointer;
    border-radius: 4px;
    padding: 0;
}

#editor-panel .editor-close-btn:hover {
    background: #3C3C3C;
}

#editor-panel .editor-host {
    flex: 1;
    overflow: hidden;
    position: relative;
}

#editor-panel .editor-host .cm-editor {
    height: 100%;
    font-size: 13px;
    font-family: Menlo, Monaco, 'Courier New', monospace;
}

#editor-panel .editor-host .cm-scroller {
    overflow: auto;
}

#editor-panel .editor-host .cm-gutters {
    background: #1E1E1E;
    border-right: 1px solid #3C3C3C;
    color: #6E7681;
}

#editor-panel .editor-host .cm-focused {
    outline: none;
}

.editor-toast {
    position: fixed;
    top: 16px;
    left: 50%;
    transform: translateX(-50%);
    padding: 8px 16px;
    border-radius: 18px;
    color: #FFFFFF;
    font-size: 13px;
    z-index: 1000;
    opacity: 0;
    transition: opacity 0.2s ease-in-out;
    pointer-events: none;
    white-space: nowrap;
}

.editor-toast.visible {
    opacity: 0.95;
}

.editor-toast.success {
    background: #2E7D32;
}

.editor-toast.error {
    background: #C62828;
}

#editor-panel .editor-button-bar {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 12px;
    height: 48px;
    background: #16161D;
    border-top: 1px solid #3C3C3C;
    flex-shrink: 0;
}

#editor-panel .editor-btn {
    width: 80px;
    height: 32px;
    border: 1px solid;
    border-radius: 4px;
    color: #FFFFFF;
    font-size: 12px;
    cursor: pointer;
    transition: background 0.1s, transform 0.1s;
    padding: 0;
}

#editor-panel .editor-btn:hover {
    transform: scale(1.05);
}

#editor-panel .editor-btn:active {
    transform: scale(1.0);
}

#editor-panel .editor-btn.discard {
    background: #3C3C3C;
    border-color: #4B4B5A;
}

#editor-panel .editor-btn.discard:hover {
    background: #5C5C6A;
    border-color: #8B8B9A;
}

#editor-panel .editor-btn.apply {
    background: #448EF9;
    border-color: #5BA3FF;
}

#editor-panel .editor-btn.apply:hover {
    background: #5BA3FF;
    border-color: #8BC4FF;
}

#editor-panel .editor-btn.save {
    background: #388E3C;
    border-color: #4CAF50;
}

#editor-panel .editor-btn.save:hover {
    background: #4CAF50;
    border-color: #81C784;
}

#editor-panel .editor-host .cm-scroller::-webkit-scrollbar {
    width: 8px;
    height: 8px;
}

#editor-panel .editor-host .cm-scroller::-webkit-scrollbar-thumb {
    background: rgba(75, 75, 90, 0.67);
    border-radius: 4px;
}

#editor-panel .editor-host .cm-scroller::-webkit-scrollbar-track {
    background: transparent;
}

#editor-panel .editor-host .cm-scroller::-webkit-scrollbar-thumb:hover {
    background: rgba(90, 90, 110, 0.8);
}
`;
