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

const DB_NAME = 'pagx-viewer-cache';
const DB_VERSION = 1;
const STORE_NAME = 'resources';

interface CacheEntry {
    key: string;
    version: string;
    data: ArrayBuffer;
}

function getExtensionVersion(): string {
    return chrome.runtime.getManifest().version;
}

function openDatabase(): Promise<IDBDatabase> {
    return new Promise((resolve, reject) => {
        const request = indexedDB.open(DB_NAME, DB_VERSION);

        request.onupgradeneeded = () => {
            const db = request.result;
            if (!db.objectStoreNames.contains(STORE_NAME)) {
                db.createObjectStore(STORE_NAME, { keyPath: 'key' });
            }
        };

        request.onsuccess = () => resolve(request.result);
        request.onerror = () => reject(request.error);
    });
}

/**
 * Retrieves a cached resource by key. Returns null if not cached or version mismatch.
 */
export async function getCached(key: string): Promise<ArrayBuffer | null> {
    try {
        const db = await openDatabase();
        const version = getExtensionVersion();

        return new Promise((resolve) => {
            const transaction = db.transaction(STORE_NAME, 'readonly');
            const store = transaction.objectStore(STORE_NAME);
            const request = store.get(key);

            request.onsuccess = () => {
                const entry = request.result as CacheEntry | undefined;
                if (entry && entry.version === version && entry.data) {
                    resolve(entry.data);
                } else {
                    resolve(null);
                }
                db.close();
            };

            request.onerror = () => {
                resolve(null);
                db.close();
            };
        });
    } catch (error) {
        console.warn('[PAGX Cache] Failed to read cache:', error);
        return null;
    }
}

/**
 * Stores a resource in the cache. Silently fails on error.
 */
export async function putCache(key: string, data: ArrayBuffer): Promise<void> {
    try {
        const db = await openDatabase();
        const version = getExtensionVersion();
        const entry: CacheEntry = { key, version, data };

        return new Promise((resolve) => {
            const transaction = db.transaction(STORE_NAME, 'readwrite');
            const store = transaction.objectStore(STORE_NAME);
            const request = store.put(entry);

            request.onsuccess = () => {
                db.close();
                resolve();
            };

            request.onerror = () => {
                console.warn('[PAGX Cache] Failed to write cache for', key);
                db.close();
                resolve();
            };
        });
    } catch (error) {
        console.warn('[PAGX Cache] Failed to write cache:', error);
    }
}

/**
 * Removes all cache entries whose version does not match the current extension version.
 */
export async function clearStaleCache(): Promise<void> {
    try {
        const db = await openDatabase();
        const version = getExtensionVersion();

        const transaction = db.transaction(STORE_NAME, 'readwrite');
        const store = transaction.objectStore(STORE_NAME);
        const request = store.openCursor();

        request.onsuccess = () => {
            const cursor = request.result;
            if (cursor) {
                const entry = cursor.value as CacheEntry;
                if (entry.version !== version) {
                    cursor.delete();
                }
                cursor.continue();
            }
        };

        transaction.oncomplete = () => db.close();
        transaction.onerror = () => db.close();
    } catch (error) {
        console.warn('[PAGX Cache] Failed to clear stale cache:', error);
    }
}
