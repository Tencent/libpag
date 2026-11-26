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

// The single transient status slot above the canvas. Owns its DOM node, timer and message tokens so
// the player only composes it; multiple asynchronous producers (editor progress, reload status) can
// show messages without each other's cleanup hiding a newer message.

import type { StatusOptions } from './types';

export class StatusPill {
    private readonly el: HTMLDivElement;
    private hideTimer: number | null = null;
    private tokenSeq = 0;
    private currentToken = 0;

    constructor(host: HTMLElement) {
        this.el = document.createElement('div');
        this.el.className = 'pagx-player-status hidden';
        host.appendChild(this.el);
    }

    /** Shows a message and returns an opaque token. `hide(token)` only clears when the token still
     *  identifies the current message, so a stale producer cannot hide a newer one. */
    show(text: string, options: StatusOptions = {}): number {
        if (this.hideTimer !== null) {
            window.clearTimeout(this.hideTimer);
            this.hideTimer = null;
        }
        const token = ++this.tokenSeq;
        this.currentToken = token;
        this.el.textContent = text;
        this.el.classList.toggle('success', options.kind === 'success');
        this.el.classList.toggle('error', options.kind === 'error');
        this.el.classList.remove('hidden');
        const autoHideMs = options.autoHideMs ?? 0;
        if (autoHideMs > 0) {
            this.hideTimer = window.setTimeout(() => {
                this.hideTimer = null;
                // Each show clears the pending timer, so this fires only for the message that set it.
                this.currentToken = 0;
                this.el.classList.add('hidden');
            }, autoHideMs);
        }
        return token;
    }

    /** Hides the pill. With a token, only clears when it still identifies the current message. */
    hide(token?: number): void {
        if (token !== undefined && token !== this.currentToken) {
            return;
        }
        if (this.hideTimer !== null) {
            window.clearTimeout(this.hideTimer);
            this.hideTimer = null;
        }
        this.currentToken = 0;
        this.el.classList.add('hidden');
    }

    destroy(): void {
        if (this.hideTimer !== null) {
            window.clearTimeout(this.hideTimer);
            this.hideTimer = null;
        }
        this.el.remove();
    }
}
