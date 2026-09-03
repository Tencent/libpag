// In-process, cross-request byte cache for reusable sub-resources (CSS / JS /
// fonts) fetched while rendering a page.
//
// Why this exists: the snapshot pipeline's main cost is the page-load phase —
// many pages link the same public CDN assets (Google Fonts' CJK woff2 subsets,
// icon fonts, third-party CSS, …) and, without a cache, every request re-fetches
// them across the public network. The HTTP service (server.js) keeps one
// browser warm across requests; holding a single ResourceCache alongside it lets
// a route-level handler fulfil repeat requests straight from memory instead of
// hitting the network again. This mirrors how the backend's
// Html2PagxBrowserService owns one shared cache.
//
// Eviction is access-order LRU: when the entry count or total byte budget is
// exceeded the least-recently-used entries are dropped; a single entry larger
// than `maxEntryBytes` is never stored (so one huge asset can't evict the rest).
// The cache lives only for the process — nothing is persisted.

export interface CachedResource {
  status: number;
  contentType: string;
  body: Buffer;
}

export class ResourceCache {
  // A Map preserves insertion order, and we keep that equal to access order by
  // re-inserting on every get/set hit. The first key is therefore always the
  // least-recently-used entry, which `evict()` drops first.
  private readonly entries = new Map<string, CachedResource>();
  // In-flight fetches keyed by URL. Concurrent listeners for the same URL
  // wait on the existing promise instead of issuing a duplicate body read,
  // which matters when multiple pages of an HTTP service request the same
  // CDN bundle within the same render burst.
  private readonly inflight = new Map<string, Promise<CachedResource | undefined>>();
  private totalBytes = 0;

  // `maxEntries`     — hard cap on the number of cached items.
  // `maxTotalBytes`  — hard cap on the summed body size across all items.
  // `maxEntryBytes`  — per-item cap; a larger body is skipped entirely so one
  //                    oversized asset can't monopolise the budget.
  constructor(
    private readonly maxEntries: number,
    private readonly maxTotalBytes: number,
    private readonly maxEntryBytes: number,
  ) {}

  // Look up `url`; on a hit, move it to the most-recently-used end so it
  // survives the next eviction sweep.
  get(url: string): CachedResource | undefined {
    const entry = this.entries.get(url);
    if (entry === undefined) {
      return undefined;
    }
    this.entries.delete(url);
    this.entries.set(url, entry);
    return entry;
  }

  // Non-mutating presence check. Used by the populate listener to skip a body
  // read for a URL that is already cached (e.g. a request that was just
  // fulfilled from this very cache), without disturbing LRU order.
  has(url: string): boolean {
    return this.entries.has(url);
  }

  // Store `url` → `resource`. Bodies over `maxEntryBytes` are ignored. After a
  // write the cache is pruned back under both the entry and byte budgets.
  set(url: string, resource: CachedResource): void {
    if (resource.body.length > this.maxEntryBytes) {
      return;
    }
    const existing = this.entries.get(url);
    if (existing !== undefined) {
      this.totalBytes -= existing.body.length;
      this.entries.delete(url);
    }
    this.entries.set(url, resource);
    this.totalBytes += resource.body.length;
    this.evict();
  }

  private evict(): void {
    while (
      this.entries.size > 0
      && (this.entries.size > this.maxEntries || this.totalBytes > this.maxTotalBytes)
    ) {
      const oldestKey = this.entries.keys().next().value as string;
      const oldest = this.entries.get(oldestKey);
      if (oldest !== undefined) {
        this.totalBytes -= oldest.body.length;
      }
      this.entries.delete(oldestKey);
    }
  }

  get size(): number {
    return this.entries.size;
  }

  get bytes(): number {
    return this.totalBytes;
  }

  clear(): void {
    this.entries.clear();
    this.totalBytes = 0;
    this.inflight.clear();
  }

  // Coalesce concurrent populate calls for the same URL. The first caller
  // installs an in-flight entry; subsequent callers await the same promise
  // and skip the body read. The producer signals completion via the returned
  // settle/reject helpers so the listener can hold a stable reference even
  // if the cache is cleared mid-flight. Returns null if another caller is
  // already in-flight for `url` (the caller should skip its own body read).
  beginInflight(url: string): {
    promise: Promise<CachedResource | undefined>;
    settle: (value: CachedResource | undefined) => void;
  } | null {
    if (this.inflight.has(url)) return null;
    let settle: (value: CachedResource | undefined) => void = () => {};
    const promise = new Promise<CachedResource | undefined>((resolve) => {
      settle = resolve;
    });
    this.inflight.set(url, promise);
    const wrappedSettle = (value: CachedResource | undefined): void => {
      this.inflight.delete(url);
      settle(value);
    };
    return { promise, settle: wrappedSettle };
  }

  // Look up a pending in-flight fetch for `url`, or undefined when no peer
  // request has installed one. Allows a concurrent listener to wait on the
  // first request's body read instead of duplicating it.
  awaitInflight(url: string): Promise<CachedResource | undefined> | undefined {
    return this.inflight.get(url);
  }
}
