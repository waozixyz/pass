var currentScript = document.currentScript;
var appBase = currentScript && currentScript.dataset && currentScript.dataset.appBase
  ? currentScript.dataset.appBase
  : '';
if (appBase && appBase.charAt(appBase.length - 1) !== '/') appBase += '/';
var statusElement = document.getElementById('status');
var loadingScreen = document.getElementById('loading-screen');

if ('serviceWorker' in navigator) {
  navigator.serviceWorker.register(appBase + 'sw.js').catch(function() {});
}

function setStatus(text) {
  if (statusElement) statusElement.textContent = text || '';
}

function hideLoadingScreen() {
  if (!loadingScreen) return;
  window.requestAnimationFrame(function() {
    loadingScreen.classList.add('is-hidden');
  });
}

function hideWhenCanvasPaints() {
  var checks = 0;
  var timer = setInterval(function() {
    checks += 1;
    if (window.__kryCanvas && window.__kryCanvas.frames > 1) {
      hideLoadingScreen();
      clearInterval(timer);
    } else if (checks > 300) {
      clearInterval(timer);
    }
  }, 50);
}

function scheduleStorageSync(delay, logSuccess) {
  if (typeof FS === 'undefined' || typeof FS.syncfs !== 'function') return;
  Module.__kryonStorageSyncPending = true;
  Module.__kryonStorageSyncLogSuccess = Module.__kryonStorageSyncLogSuccess || !!logSuccess;
  if (Module.__kryonStorageSyncTimer) clearTimeout(Module.__kryonStorageSyncTimer);
  Module.__kryonStorageSyncTimer = setTimeout(function() {
    Module.__kryonStorageSyncTimer = 0;
    Module.__kryonStorageSyncing = true;
    Module.__kryonStorageSyncPending = false;
    FS.syncfs(false, function(err) {
      Module.__kryonStorageSyncing = false;
      if (err) console.error('Pass storage sync failed:', err);
      else if (Module.__kryonStorageSyncLogSuccess) console.log('Pass storage synced');
      Module.__kryonStorageSyncLogSuccess = false;
    });
  }, Math.max(0, delay || 0));
}

var Module = {
  canvas: document.getElementById('canvas'),
  preRun: [function() {
    if (typeof FS === 'undefined' || typeof IDBFS === 'undefined') return;
    FS.mkdir('/pass-data');
    FS.mount(IDBFS, {}, '/pass-data');
    FS.chdir('/pass-data');
    Module.addRunDependency('pass-idbfs');
    FS.syncfs(true, function(err) {
      if (err) console.error('Pass storage load failed:', err);
      Module.removeRunDependency('pass-idbfs');
    });
  }],
  setStatus: setStatus,
  print: function(text) {
    if (/PASS: app ready/.test(text || '')) hideLoadingScreen();
    console.log(text);
  },
  printErr: function(text) {
    console.error(text);
  }
};

Module.__kryonScheduleStorageSync = scheduleStorageSync;
Module.__kryonFlushStorageSync = function(logSuccess) {
  scheduleStorageSync(0, logSuccess);
};

window.addEventListener('error', function(event) {
  setStatus('App failed to load');
  console.error(event.error || event.message);
});

var script = document.createElement('script');
script.async = true;
script.src = appBase + 'index.js';
script.onload = function() { setStatus('Starting app...'); hideWhenCanvasPaints(); };
script.onerror = function() { setStatus('App failed to load'); };
document.body.appendChild(script);
