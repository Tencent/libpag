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

// In-memory cache for PAGX data extracted from pages before redirect.
// Keyed by tabId. Entries are consumed (deleted) once the viewer retrieves them.
const pendingPagxData = {};

// Timeout for cached PAGX data (10 minutes). If not retrieved by then, clean up.
const CACHE_TIMEOUT_MS = 10 * 60 * 1000;

// Track tabs already being redirected to avoid duplicate handling.
const redirectingTabs = new Set();

// Helper: Check if a URL points to a .pagx file.
function isPagxUrl(url) {
  if (!url) return false;
  try {
    const pathname = new URL(url).pathname;
    return pathname.endsWith(".pagx");
  } catch (_) {
    return false;
  }
}

// Helper: Cache PAGX data for a tab and set up auto-cleanup.
function cachePagxData(tabId, text, sourceUrl) {
  const cacheEntry = {
    text: text,
    sourceUrl: sourceUrl,
    timestamp: Date.now(),
  };
  pendingPagxData[tabId] = cacheEntry;

  setTimeout(() => {
    if (pendingPagxData[tabId] === cacheEntry) {
      delete pendingPagxData[tabId];
      console.warn(`PAGX Viewer: Cache entry for tab ${tabId} expired`);
    }
  }, CACHE_TIMEOUT_MS);
}

// Helper: Redirect a tab to the viewer page.
function redirectToViewer(tabId) {
  const viewerUrl =
    chrome.runtime.getURL("viewer/viewer.html") + "?tabId=" + tabId;
  chrome.tabs.update(tabId, { url: viewerUrl });
}

// Clean up cached data when tab closes.
chrome.tabs.onRemoved.addListener((tabId) => {
  delete pendingPagxData[tabId];
  redirectingTabs.delete(tabId);
});

// ---------------------------------------------------------------------------
// Strategy 1: webNavigation.onCompleted
// Handles the case where Chrome navigates to a .pagx file URL and renders it
// as text/XML. We inject a content script to grab the text, then redirect.
// ---------------------------------------------------------------------------
chrome.webNavigation.onCompleted.addListener(async (details) => {
  if (details.frameId !== 0) return;
  if (!isPagxUrl(details.url)) return;
  if (redirectingTabs.has(details.tabId)) return;
  redirectingTabs.add(details.tabId);

  try {
    const results = await chrome.scripting.executeScript({
      target: { tabId: details.tabId },
      func: () => {
        return document.body.innerText || document.documentElement.textContent || "";
      },
    });
    if (results && results[0] && results[0].result) {
      cachePagxData(details.tabId, results[0].result, details.url);
    }
  } catch (error) {
    console.warn("PAGX Viewer: failed to extract content from page:", error);
    // Try fetching the file directly as fallback
    try {
      const response = await fetch(details.url);
      if (response.ok) {
        const text = await response.text();
        if (text) {
          cachePagxData(details.tabId, text, details.url);
        }
      }
    } catch (fetchError) {
      console.warn("PAGX Viewer: fallback fetch also failed:", fetchError);
    }
  }

  redirectToViewer(details.tabId);
  // Allow re-redirect after a short delay (in case the tab is reused)
  setTimeout(() => redirectingTabs.delete(details.tabId), 2000);
}, {
  url: [{ pathSuffix: ".pagx" }],
});

// ---------------------------------------------------------------------------
// Strategy 2: chrome.downloads interception
// When Chrome decides to download a .pagx file instead of displaying it
// (e.g., dragging file to Dock/Taskbar icon, or server sends attachment
// Content-Disposition), we cancel the download and open it in the viewer.
// ---------------------------------------------------------------------------
chrome.downloads.onCreated.addListener(async (downloadItem) => {
  // Check filename or URL for .pagx
  const filename = downloadItem.filename || downloadItem.url || "";
  const url = downloadItem.url || "";
  if (!filename.endsWith(".pagx") && !isPagxUrl(url)) return;

  // Cancel the download immediately
  try {
    await chrome.downloads.cancel(downloadItem.id);
  } catch (_) {
    // Download may have already completed
  }

  // Clean up the cancelled download entry
  try {
    await chrome.downloads.erase({ id: downloadItem.id });
  } catch (_) {
    // Ignore cleanup errors
  }

  // Fetch the file content directly from the URL
  let text = "";
  try {
    const response = await fetch(url);
    if (response.ok) {
      text = await response.text();
    }
  } catch (error) {
    console.warn("PAGX Viewer: failed to fetch downloaded file:", error);
  }

  // Open the viewer in a new tab with the file content cached
  const tab = await chrome.tabs.create({
    url: chrome.runtime.getURL("viewer/viewer.html"),
  });

  if (text) {
    cachePagxData(tab.id, text, url);
    // Update the URL to include tabId so viewer can retrieve the cached data
    const viewerUrl =
      chrome.runtime.getURL("viewer/viewer.html") + "?tabId=" + tab.id;
    chrome.tabs.update(tab.id, { url: viewerUrl });
  }
});

// ---------------------------------------------------------------------------
// Strategy 3: tabs.onUpdated fallback
// Catches cases where webNavigation events don't fire (e.g., certain file://
// URL handling differences across platforms).
// ---------------------------------------------------------------------------
chrome.tabs.onUpdated.addListener(async (tabId, changeInfo, tab) => {
  if (changeInfo.status !== "complete") return;
  if (!tab.url || !isPagxUrl(tab.url)) return;
  // Skip if already handled by webNavigation or if already redirecting
  if (redirectingTabs.has(tabId)) return;
  // Skip extension pages (avoid redirect loops)
  if (tab.url.startsWith("chrome-extension://")) return;

  redirectingTabs.add(tabId);

  // Try to extract content via script injection first
  try {
    const results = await chrome.scripting.executeScript({
      target: { tabId: tabId },
      func: () => {
        return document.body.innerText || document.documentElement.textContent || "";
      },
    });
    if (results && results[0] && results[0].result) {
      cachePagxData(tabId, results[0].result, tab.url);
    }
  } catch (error) {
    // Script injection failed (e.g., file:// without permission), try fetch
    try {
      const response = await fetch(tab.url);
      if (response.ok) {
        const text = await response.text();
        if (text) {
          cachePagxData(tabId, text, tab.url);
        }
      }
    } catch (fetchError) {
      console.warn("PAGX Viewer: failed to read file:", fetchError);
    }
  }

  redirectToViewer(tabId);
  setTimeout(() => redirectingTabs.delete(tabId), 2000);
});

// ---------------------------------------------------------------------------
// Message handler for communication with the viewer page.
// ---------------------------------------------------------------------------
chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  if (message.type === "getPagxData") {
    const tabId = message.tabId;
    const data = pendingPagxData[tabId] || null;
    delete pendingPagxData[tabId];
    sendResponse(data);
    return false;
  }

  if (message.type === "fetchUrl") {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 30000);

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
