/**********************************************************************************************
*
*   raylib-libretro - embed.js - Drop-in website embed loader.
*
*   Embeds the raylib-libretro WebAssembly frontend directly into a host page's DOM (no
*   iframe), pointed at a chosen game. Add one <script> tag and you get a click-to-play
*   emulator. This is a thin loader around the existing web build (index.js / index.wasm /
*   index.data) - it replicates the parts of shell.html's Emscripten Module setup that matter
*   (IndexedDB save persistence, HiDPI canvas sizing, audio resume) but scopes them to a
*   bounded container instead of the whole viewport.
*
*   Usage (any one of):
*
*     1. Self-configuring single tag - injects an embed where the script sits:
*        <script src=".../embed.js" data-game="https://example.com/mario.nes"></script>
*
*     2. Marker element - scanned on load:
*        <div data-raylib-libretro data-game="..." data-core="..."></div>
*        <script src=".../embed.js"></script>
*
*     3. Programmatic:
*        <script src=".../embed.js"></script>
*        <script>RaylibLibretro.embed('#game', { game: '...', core: 'fceumm' });</script>
*
*   Config (data-attribute / option):
*     game      - ROM URL (must be CORS-accessible). Required to play.
*     core      - bundled core name (fceumm|snes9x|gambatte|mgba|picodrive), a "<name>"
*                 resolved against the bundled /cores, or a full core URL. Omit to let the
*                 frontend autodetect the core from the ROM extension.
*     autostart - load immediately instead of showing the click-to-play poster.
*     poster    - placeholder image URL shown before play.
*     aspect    - container aspect ratio ("4:3", "3:2", "1.5", ...). Default "4:3".
*     base      - override the asset base URL (where index.js/.wasm/.data live). Defaults to
*                 the directory embed.js itself was served from, so self-hosting just works.
*
*   LIMITATION: one instance per page. The web build is not MODULARIZE'd, so the Emscripten
*   runtime uses a global Module and a hardcoded "#canvas". A second embed boot is refused.
*
*   LICENSE: zlib/libpng
*   Copyright (c) 2020 Rob Loach (@RobLoach)
*
**********************************************************************************************/

(function () {
  'use strict';

  // document.currentScript is only valid during this script's synchronous execution, so
  // capture it now (it's null inside later async callbacks).
  var thisScript = document.currentScript;
  var SCRIPT_BASE = thisScript ? new URL('.', thisScript.src).href : './';

  // Match shell.html: cap the backing-store scale so a HiDPI phone doesn't render a 3x
  // framebuffer (9x the fill rate) and tank the framerate. 2x is still crisp.
  var MAX_PIXEL_RATIO = 2;

  // Cores bundled into index.data by the web build (see bin/CMakeLists.txt). A bare name
  // resolves to /cores/<name>_libretro.so; anything with a slash or dot is treated as a URL.
  var BUNDLED_CORES = ['fceumm', 'snes9x', 'gambatte', 'mgba', 'picodrive'];

  var PLAY_SVG =
    '<svg viewBox="0 0 100 100" width="100%" height="100%" aria-hidden="true">' +
    '<circle cx="50" cy="50" r="47" fill="rgba(0,0,0,0.55)" stroke="#fff" stroke-width="3"/>' +
    '<polygon points="40,30 40,70 72,50" fill="#fff"/></svg>';

  // The runtime uses a global Module + a hardcoded canvas id, so only one emulator can run
  // per page. Refuse a second boot rather than corrupt the first.
  var booted = false;
  // navigator.wakeLock handle; re-acquired when the tab becomes visible again.
  var wakeLock = null;
  // Guard so we only wrap the AudioContext constructor once.
  var audioProxyInstalled = false;

  // --------------------------------------------------------------------------------------
  // Small DOM/style helpers (inline styles only - never inject global CSS into the host).
  // --------------------------------------------------------------------------------------

  function setStyles(el, styles) {
    for (var k in styles) {
      if (Object.prototype.hasOwnProperty.call(styles, k) && styles[k] != null) {
        el.style[k] = styles[k];
      }
    }
  }

  function truthy(v) {
    return v === true || v === '' || v === 'true' || v === '1' || v === 'yes';
  }

  function parseAspect(a) {
    if (!a) return '4 / 3';
    if (a.indexOf(':') !== -1) {
      var p = a.split(':');
      return p[0].trim() + ' / ' + p[1].trim();
    }
    return String(a);
  }

  function resolveBase(base) {
    if (!base) return SCRIPT_BASE;
    // Resolve relative to the page, ensure a single trailing slash.
    return new URL(base, window.location.href).href.replace(/\/?$/, '/');
  }

  function normalizeConfig(o) {
    o = o || {};
    return {
      game: o.game || null,
      core: o.core || null,
      poster: o.poster || null,
      aspect: o.aspect || '4:3',
      base: o.base || null,
      autostart: truthy(o.autostart)
    };
  }

  function showMessage(container, text) {
    var msg = document.createElement('div');
    setStyles(msg, {
      position: 'absolute', top: '0', right: '0', bottom: '0', left: '0',
      display: 'flex', alignItems: 'center', justifyContent: 'center',
      padding: '1em', boxSizing: 'border-box', textAlign: 'center',
      color: '#fff', background: 'rgba(0,0,0,0.8)',
      font: '14px/1.4 system-ui, sans-serif'
    });
    msg.textContent = text;
    container.appendChild(msg);
    return msg;
  }

  // --------------------------------------------------------------------------------------
  // Audio resume + screen wake lock (ported from shell.html, scoped to a real gesture).
  // --------------------------------------------------------------------------------------

  function requestWakeLock() {
    if (!('wakeLock' in navigator) || wakeLock) return;
    navigator.wakeLock.request('screen').then(function (lock) {
      wakeLock = lock;
      lock.addEventListener('release', function () { wakeLock = null; });
    }).catch(function () { /* denied or not visible; ignore */ });
  }

  // Browsers start AudioContexts suspended until a user gesture. Wrap the constructor so we
  // can track every context the runtime creates and resume them on the next gesture. We only
  // install this at boot (after the user opted in), so it never touches a host page that
  // didn't ask for an emulator.
  function installAudioResume() {
    if (audioProxyInstalled) return;
    audioProxyInstalled = true;
    var contexts = [];
    ['AudioContext', 'webkitAudioContext'].forEach(function (name) {
      var Orig = window[name];
      if (!Orig) return;
      window[name] = new Proxy(Orig, {
        construct: function (target, args) {
          var ctx = Reflect.construct(target, args, target);
          contexts.push(ctx);
          return ctx;
        }
      });
    });
    function onGesture() {
      contexts.forEach(function (ctx) {
        if (ctx && ctx.state === 'suspended' && ctx.resume) ctx.resume().catch(function () {});
      });
      requestWakeLock();
    }
    ['pointerdown', 'touchend', 'keydown'].forEach(function (t) {
      window.addEventListener(t, onGesture, { passive: true });
    });
    document.addEventListener('visibilitychange', function () {
      if (document.visibilityState === 'visible') requestWakeLock();
    });
  }

  // --------------------------------------------------------------------------------------
  // Container + click-to-play poster.
  // --------------------------------------------------------------------------------------

  function styleContainer(el, cfg) {
    var s = el.style;
    if (!s.position) s.position = 'relative';
    if (!s.display) s.display = 'block';
    if (!s.width && !s.maxWidth) s.maxWidth = '512px';
    if (!s.width) s.width = '100%';
    if (!s.margin) s.margin = '0 auto';
    s.aspectRatio = parseAspect(cfg.aspect);
    if (!s.background && !s.backgroundColor) s.backgroundColor = '#000';
    s.overflow = 'hidden';
    return el;
  }

  function showPoster(container, cfg, onPlay) {
    var poster = document.createElement('div');
    setStyles(poster, {
      position: 'absolute', top: '0', right: '0', bottom: '0', left: '0',
      display: 'flex', alignItems: 'center', justifyContent: 'center',
      cursor: 'pointer', backgroundColor: '#000',
      backgroundImage: cfg.poster ? 'url("' + cfg.poster + '")' : 'none',
      backgroundSize: 'contain', backgroundRepeat: 'no-repeat', backgroundPosition: 'center'
    });
    poster.setAttribute('role', 'button');
    poster.setAttribute('aria-label', 'Play game');
    poster.setAttribute('tabindex', '0');

    var btn = document.createElement('div');
    btn.innerHTML = PLAY_SVG;
    setStyles(btn, {
      width: '22%', maxWidth: '88px', minWidth: '48px', aspectRatio: '1 / 1',
      pointerEvents: 'none', filter: 'drop-shadow(0 2px 6px rgba(0,0,0,0.6))'
    });
    poster.appendChild(btn);

    function play() { poster.removeEventListener('click', play); onPlay(); }
    poster.addEventListener('click', play);
    poster.addEventListener('keydown', function (e) {
      if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); play(); }
    });
    container.appendChild(poster);
  }

  // --------------------------------------------------------------------------------------
  // Boot: create the canvas, build the global Module, inject index.js.
  // --------------------------------------------------------------------------------------

  function boot(container, cfg) {
    if (booted) {
      console.error('[raylib-libretro] An emulator is already running on this page. Only one ' +
        'inline embed is supported per page (the WASM runtime uses a global Module).');
      showMessage(container, 'Only one game can run per page.');
      return;
    }
    booted = true;

    var base = resolveBase(cfg.base);
    installAudioResume();

    // Clear the poster and lay down the canvas. The id must stay "canvas": the menu's
    // fullscreen path calls emscripten_request_fullscreen("#canvas", ...).
    container.textContent = '';
    var canvas = document.createElement('canvas');
    canvas.id = 'canvas';
    canvas.className = 'emscripten';
    canvas.setAttribute('tabindex', '-1');
    canvas.oncontextmenu = function (e) { e.preventDefault(); };
    setStyles(canvas, {
      position: 'absolute', top: '0', left: '0', width: '100%', height: '100%',
      display: 'block', border: '0', outline: 'none', backgroundColor: '#000',
      touchAction: 'none', webkitTapHighlightColor: 'transparent',
      webkitUserSelect: 'none', userSelect: 'none'
    });
    // Allow drag-and-drop loading onto the canvas (GLFW listens here) without hijacking
    // drag-and-drop for the rest of the host page.
    ['dragenter', 'dragover', 'drop'].forEach(function (t) {
      canvas.addEventListener(t, function (e) { e.preventDefault(); }, false);
    });
    container.appendChild(canvas);

    // HiDPI sizing. raylib's web backend sizes the canvas backing store to the browser
    // window in CSS pixels and re-applies on every 'resize'; for an embedded canvas that is
    // both the wrong element and the wrong size. We display the canvas at the container's CSS
    // size and push the (capped) physical-pixel size into the backing store via
    // ResizeCanvasFromJS (-> SetWindowSize). Re-applying inside requestAnimationFrame makes
    // ours run last in the frame, overriding raylib's own resize handler - the same trick
    // shell.html uses, just fed container dimensions instead of window dimensions.
    function applyCanvasResize() {
      var dpr = Math.min(window.devicePixelRatio || 1, MAX_PIXEL_RATIO);
      var rect = container.getBoundingClientRect();
      var cssW = Math.max(1, Math.round(rect.width));
      var cssH = Math.max(1, Math.round(rect.height));
      canvas.style.width = cssW + 'px';
      canvas.style.height = cssH + 'px';
      if (window.Module && window.Module.calledRun && window.Module.ccall) {
        window.Module.ccall('ResizeCanvasFromJS', null, ['number', 'number'],
          [Math.round(cssW * dpr), Math.round(cssH * dpr)]);
      }
    }
    var resizeQueued = false;
    function queueCanvasResize() {
      if (resizeQueued) return;
      resizeQueued = true;
      requestAnimationFrame(function () { resizeQueued = false; applyCanvasResize(); });
    }
    if (window.ResizeObserver) new ResizeObserver(queueCanvasResize).observe(container);
    window.addEventListener('resize', queueCanvasResize);
    window.addEventListener('orientationchange', queueCanvasResize);

    var pendingGameFetch = null;

    var Module = {
      canvas: canvas,
      arguments: [],
      // Resolve index.wasm / index.data (and the preload package) against the asset base,
      // since the host page lives at a different path/origin than the build output.
      locateFile: function (path) { return base + path; },
      print: function () { console.log.apply(console, arguments); },
      printErr: function () { console.error.apply(console, arguments); },

      preRun: [
        // Persist /userdata to IndexedDB so raylib-libretro.cfg, battery saves and BIOS files
        // survive reloads. main() is deferred via the run-dependency until syncfs(true) lands.
        // Storage is scoped to the host page's origin, which is correct for an inline embed.
        function () {
          if (navigator.storage && navigator.storage.persist) {
            navigator.storage.persist().catch(function () {});
          }
          addRunDependency('idbfs-load');
          try { FS.mkdir('/userdata'); } catch (e) {}
          FS.mount(IDBFS, {}, '/userdata');
          FS.syncfs(true, function (err) {
            if (err) console.error('[raylib-libretro] IDBFS load failed:', err);
            try { FS.mkdir('/userdata/saves'); } catch (e) {}
            try { FS.mkdir('/userdata/system'); } catch (e) {}
            removeRunDependency('idbfs-load');
          });
        },
        // Wire up the core (-L argument) and start the game download. A bundled/named core is
        // passed straight as an argument; a core URL is fetched to the FS first (blocking
        // main via a run-dependency). The game is fetched in the background and handed off
        // once main() is running, so the UI comes up while a large ROM is still downloading.
        function () {
          function fetchToFS(url, label) {
            var filename = url.split('/').pop().split('?')[0] || 'download';
            var destPath = '/downloads/' + filename;
            try { FS.mkdir('/downloads'); } catch (e) {}
            return fetch(url).then(function (r) {
              if (!r.ok) throw new Error('HTTP ' + r.status + ' fetching ' + url);
              return r.arrayBuffer();
            }).then(function (buf) {
              FS.writeFile(destPath, new Uint8Array(buf));
              return destPath;
            }).catch(function (err) {
              console.error('[raylib-libretro] Failed to load ' + label + ': ' + err +
                ' (if this is a TypeError, the server must send Access-Control-Allow-Origin ' +
                'for cross-origin ' + label + ' files)');
              return null;
            });
          }

          if (cfg.core) {
            var isUrl = cfg.core.indexOf('/') !== -1 || cfg.core.indexOf('.') !== -1;
            if (!isUrl) {
              // Bundled or bare core name -> resolve against the preloaded /cores.
              Module.arguments = Module.arguments.concat(['-L', '/cores/' + cfg.core + '_libretro.so']);
            } else {
              var coreFilename = cfg.core.split('/').pop().split('?')[0] || 'url-core';
              Module.arguments = Module.arguments.concat(['-L', '/downloads/' + coreFilename]);
              addRunDependency('url-core');
              fetchToFS(cfg.core, 'core').finally(function () { removeRunDependency('url-core'); });
            }
          }

          if (cfg.game) {
            pendingGameFetch = fetchToFS(cfg.game, 'game');
          }
        }
      ],

      postRun: [
        // Size the backing store to physical pixels now that ResizeCanvasFromJS exists.
        function () { queueCanvasResize(); },
        // Hand off the background game download. LoadLibretroGameFromJS loads into the core
        // chosen via -L, or autodetects from the ROM extension (same as drag-and-drop).
        function () {
          if (!pendingGameFetch) return;
          pendingGameFetch.then(function (destPath) {
            if (!destPath) return;
            var ok = Module.ccall('LoadLibretroGameFromJS', 'number', ['string'], [destPath]);
            if (!ok) console.error('[raylib-libretro] LoadLibretroGameFromJS failed for ' + destPath);
          });
          pendingGameFetch = null;
        },
        // Safety-net persistence flush: periodically and on tab close / app switch, commit
        // any uncommitted cfg / SRAM to IndexedDB. Mirrors shell.html.
        function () {
          var flushInFlight = false;
          function flushPersistence() {
            if (flushInFlight) return;
            flushInFlight = true;
            try {
              if (Module._SaveLibretroAllSettings) Module._SaveLibretroAllSettings();
            } catch (e) { console.error('[raylib-libretro] flush save failed:', e); }
            FS.syncfs(false, function (err) {
              if (err) console.error('[raylib-libretro] IDBFS sync failed:', err);
              flushInFlight = false;
            });
          }
          setInterval(flushPersistence, 60000);
          window.addEventListener('pagehide', flushPersistence);
          document.addEventListener('visibilitychange', function () {
            if (document.visibilityState === 'hidden') flushPersistence();
          });
        }
      ]
    };

    window.Module = Module;

    // Inject the Emscripten glue. It picks up the global Module we just defined (the build is
    // not MODULARIZE'd) and auto-runs main().
    var script = document.createElement('script');
    script.async = true;
    script.src = base + 'index.js';
    script.onerror = function () {
      console.error('[raylib-libretro] Failed to load ' + script.src +
        ' (check the asset base URL and that the host sends Access-Control-Allow-Origin).');
      showMessage(container, 'Failed to load the emulator.');
    };
    document.body.appendChild(script);
  }

  // --------------------------------------------------------------------------------------
  // Public entry point + auto-init.
  // --------------------------------------------------------------------------------------

  function embed(target, opts) {
    var el = typeof target === 'string' ? document.querySelector(target) : target;
    if (!el) {
      console.error('[raylib-libretro] embed target not found:', target);
      return null;
    }
    var cfg = normalizeConfig(opts);
    if (!cfg.game) {
      console.warn('[raylib-libretro] No "game" specified; nothing to load.');
    }
    styleContainer(el, cfg);
    if (cfg.autostart) {
      boot(el, cfg);
    } else {
      showPoster(el, cfg, function () { boot(el, cfg); });
    }
    return el;
  }

  // Self-configuring single tag: if embed.js itself carries data-game/data-core, inject an
  // embed container right after the script element. (Place the tag where you want the game.)
  function initFromScriptTag() {
    if (!thisScript) return;
    var ds = thisScript.dataset;
    if (ds.game == null && ds.core == null) return;
    var holder = document.createElement('div');
    thisScript.insertAdjacentElement('afterend', holder);
    embed(holder, ds);
  }

  // Marker elements: <div data-raylib-libretro data-game="..."></div>
  function initMarkers() {
    var nodes = document.querySelectorAll('[data-raylib-libretro]');
    Array.prototype.forEach.call(nodes, function (node) {
      if (node.__rlRetroInit) return;
      node.__rlRetroInit = true;
      embed(node, node.dataset);
    });
  }

  window.RaylibLibretro = window.RaylibLibretro || { embed: embed, version: '0.0.1' };

  initFromScriptTag();
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initMarkers);
  } else {
    initMarkers();
  }
})();
