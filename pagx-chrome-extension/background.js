/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  libpag is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 libpag. All rights reserved.
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

// In-memory cache for PAGX data extracted from pages before redirect.
// Keyed by tabId. Entries are consumed (deleted) once the viewer retrieves them.
const pendingPagxData = {};

// Timeout for cached PAGX data (10 minutes). If not retrieved by then, clean up.
const CACHE_TIMEOUT_MS = 10 * 60 * 1000;

// Clean up cached data when tab closes
chrome.tabs.onRemoved.addListener((tabId) => {
    if (pendingPagxData[tabId]) {
        delete pendingPagxData[tabId];
    }
});

// Detect navigation to .pagx files and redirect to the built-in viewer page.
chrome.webNavigation.onCompleted.addListener(async (details) => {
  if (details.frameId !== 0) {
    return;
  }
  const url = details.url;
  // Parse the URL to check the pathname suffix. data: and chrome-extension: URLs are skipped.
  let pathname = "";
  try {
    pathname = new URL(url).pathname;
  } catch (_) {
    return;
  }
  if (!pathname.endsWith(".pagx")) {
    return;
  }

  // Inject a script into the page to grab the raw text content that the browser already loaded.
  try {
    const results = await chrome.scripting.executeScript({
      target: { tabId: details.tabId },
      func: () => {
        // When the browser opens an XML/text file directly, the content is inside the body.
        return document.body.innerText || document.documentElement.textContent || "";
      },
    });
    if (results && results[0] && results[0].result) {
      const cacheEntry = {
        text: results[0].result,
        sourceUrl: url,
        timestamp: Date.now(),
      };
      pendingPagxData[details.tabId] = cacheEntry;
      
      // Set auto-cleanup timeout for this cache entry
      setTimeout(() => {
        if (pendingPagxData[details.tabId] === cacheEntry) {
          delete pendingPagxData[details.tabId];
          console.warn(`PAGX Viewer: Cache entry for tab ${details.tabId} expired`);
        }
      }, CACHE_TIMEOUT_MS);
    }
  } catch (error) {
    // Injection may fail on restricted pages. The viewer will show the drop-zone instead.
    console.warn("PAGX Viewer: failed to extract content from page:", error);
  }

  // Redirect to the viewer page.
  const viewerUrl =
    chrome.runtime.getURL("viewer/viewer.html") +
    "?tabId=" +
    details.tabId;
  chrome.tabs.update(details.tabId, { url: viewerUrl });
}, {
  url: [{ pathSuffix: ".pagx" }],
});

// Message handler for communication with the viewer page.
chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  if (message.type === "getPagxData") {
    // Return cached PAGX content for a given tab, then delete the cache entry.
    const tabId = message.tabId;
    const data = pendingPagxData[tabId] || null;
    delete pendingPagxData[tabId];
    sendResponse(data);
    return false;
  }

  if (message.type === "fetchUrl") {
    // Proxy fetch for the viewer page. The Service Worker is not subject to COEP restrictions,
    // so it can fetch file:// and cross-origin http(s) URLs on behalf of the viewer.
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 30000); // 30 second timeout
    
    fetch(message.url, { signal: controller.signal })
      .then((response) => {
        clearTimeout(timeout);
        if (!response.ok) {
          throw new Error("HTTP " + response.status + " " + response.statusText);
        }
        return response.arrayBuffer();
      })
      .then((buffer) => {
        sendResponse({ data: Array.from(new Uint8Array(buffer)) });
      })
      .catch((error) => {
        clearTimeout(timeout);
        const errorMsg = error.name === 'AbortError' ? 'Request timeout' : error.message;
        sendResponse({ error: errorMsg });
      });
    return true; // async sendResponse
  }

  return false;
});
