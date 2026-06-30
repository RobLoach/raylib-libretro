/**********************************************************************************************
*
*   raylib-libretro - Service worker for the web (PWA) build.
*
*   Makes the app installable ("Add to Home Screen") and usable offline by caching the
*   Emscripten app shell. Cross-origin requests (e.g. ?game=/?core= ROM and core
*   downloads) are passed straight through and never cached.
*
**********************************************************************************************/

// CACHE_VERSION is replaced at release time with the tag (e.g. v1.2.3)
// so each deployment evicts the previous cache on mobile devices.
// CACHE_VERSION is replaced at release time with the tag (e.g. v1.2.3).
// Bump the suffix here whenever the app shell changes significantly so
// existing browsers pick up the new cache on next activate.
const CACHE_VERSION = 'raylib-libretro-v0.0.32';

// Best-effort precache of the app shell. Big binaries (index.wasm / index.data)
// are cached lazily on first fetch so a single 404 can't break installation.
const APP_SHELL = [
  '.',
  'index.html',
  'index.js',
  'manifest.webmanifest',
  'icon-192.png',
  'icon-512.png',
  'apple-touch-icon.png',
];

self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(CACHE_VERSION).then((cache) =>
      // Add entries individually so one missing file doesn't reject the whole install.
      Promise.all(APP_SHELL.map((url) =>
        cache.add(url).catch((err) => console.warn('sw: precache skipped', url, err))
      ))
    ).then(() => self.skipWaiting())
  );
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(
        keys.filter((k) => k !== CACHE_VERSION).map((k) => caches.delete(k))
      ))
      .then(() => self.clients.claim())
  );
});

self.addEventListener('fetch', (event) => {
  const req = event.request;

  // Only handle same-origin GETs. Let the browser deal with cross-origin
  // (ROM/core downloads) and non-GET requests normally.
  if (req.method !== 'GET' || new URL(req.url).origin !== self.location.origin) {
    return;
  }

  // Never cache build artifacts — they change on every rebuild and are too large.
  const url = new URL(req.url);
  const path = url.pathname;
  if (path.endsWith('.data') || path.endsWith('.wasm')) {
    return;
  }

  event.respondWith(
    caches.match(req).then((cached) => {
      if (cached) return cached;
      return fetch(req).then((res) => {
        // Cache successful, complete (non-partial) responses for next time.
        if (res && res.ok && res.status === 200) {
          const copy = res.clone();
          caches.open(CACHE_VERSION).then((cache) => cache.put(req, copy));
        }
        return res;
      }).catch(() => {
        // Offline: fall back to the cached shell for navigations.
        if (req.mode === 'navigate') return caches.match('index.html');
        return Response.error();
      });
    })
  );
});
