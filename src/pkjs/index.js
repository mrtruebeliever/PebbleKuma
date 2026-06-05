var Clay = require('pebble-clay');
var i18n = require('./i18n');
var keys = require('message_keys');

// The settings page is rebuilt in the chosen language each time it opens, so we
// drive Clay manually (autoHandleEvents:false).
function getLangIdx() { return parseInt(localStorage.getItem('LANG') || '0', 10) || 0; }
var clay = new Clay(i18n.buildConfig(getLangIdx()), null, { autoHandleEvents: false });

// Load states (mirror data.h).
var LOAD_LOADING = 0;
var LOAD_OK = 1;
var LOAD_ERROR = 2;
var LOAD_UNCONFIGURED = 3;

var MAX_MONITORS = 32;
var MAX_PAGES = 8;
var MAX_HB = 30;
var UPTIME_UNKNOWN = 0xFFFF;

// --- Persisted state ---------------------------------------------------------

function getState() {
  var pages;
  try { pages = JSON.parse(localStorage.getItem('PAGES') || '[]'); }
  catch (e) { pages = []; }
  return {
    baseUrl: (localStorage.getItem('BASE_URL') || '').replace(/\/+$/, ''),
    pages: pages,
    activeIndex: parseInt(localStorage.getItem('ACTIVE') || '0', 10) || 0
  };
}

// --- AppMessage senders ------------------------------------------------------

function sendLoad(state) {
  var msg = {};
  msg[keys.LOAD_STATE] = state;
  Pebble.sendAppMessage(msg);
}

function sendPageList(state, cb, sortBy) {
  var msg = {};
  msg[keys.PAGE_NAMES] = state.pages.join('\n');  // watch derives the count from this
  msg[keys.ACTIVE_PAGE] = state.activeIndex;
  msg[keys.LANGUAGE] = getLangIdx();
  if (sortBy !== undefined) { msg[keys.SORT_BY] = sortBy; }  // only from Clay
  Pebble.sendAppMessage(msg, cb, function () { console.log('pagelist send failed'); });
}

// Streams the monitors one message at a time, advancing on each ACK so the
// inbox buffer never overflows and the watch list fills progressively.
function streamMonitors(monitors) {
  var count = Math.min(monitors.length, MAX_MONITORS);
  var head = {};
  head[keys.MON_COUNT] = count;     // resets the list on the watch
  head[keys.LOAD_STATE] = LOAD_OK;  // OK so rows render as they arrive
  Pebble.sendAppMessage(head, function () {
    var i = 0;
    function sendNext() {
      if (i >= count) { return; }
      var m = monitors[i];
      var msg = {};
      msg[keys.MON_INDEX] = i;
      msg[keys.MON_NAME] = String(m.name).substring(0, 39);
      msg[keys.MON_STATUS] = m.status;
      msg[keys.MON_UPTIME] = m.uptime;
      msg[keys.MON_PING] = m.ping;
      if (m.pstats) { msg[keys.MON_PSTATS] = m.pstats; }
      if (m.time) { msg[keys.MON_TIME] = m.time; }
      if (m.type) { msg[keys.MON_TYPE] = m.type; }
      if (m.msg) { msg[keys.MON_MSG] = String(m.msg).substring(0, 63); }
      if (m.hb && m.hb.length) { msg[keys.MON_HB] = m.hb; }
      Pebble.sendAppMessage(msg, function () { i++; sendNext(); },
        function () { console.log('monitor ' + i + ' send failed'); i++; sendNext(); });
    }
    sendNext();
  }, function () { console.log('head send failed'); });
}

// --- Fetch + aggregate -------------------------------------------------------

function getJSON(url, cb) {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', url);
  xhr.timeout = 15000;
  xhr.onload = function () {
    try { cb(JSON.parse(xhr.responseText)); }
    catch (e) { console.log('parse error ' + url + ': ' + e); cb(null); }
  };
  xhr.onerror = function () { console.log('request failed ' + url); cb(null); };
  xhr.ontimeout = function () { console.log('request timeout ' + url); cb(null); };
  xhr.send();
}

// Combines the status-page + heartbeat responses into a flat monitor list.
function buildMonitors(sp, hb) {
  var list = [];
  var groups = (sp && sp.publicGroupList) || [];
  var hbList = (hb && hb.heartbeatList) || {};
  var upList = (hb && hb.uptimeList) || {};
  groups.forEach(function (g) {
    (g.monitorList || []).forEach(function (mon) {
      var id = mon.id;
      var beats = hbList[id] || [];
      var last = beats.length ? beats[beats.length - 1] : null;
      var status = last ? last.status : 255;
      var up = upList[id + '_24'];
      var uptime = (typeof up === 'number') ? Math.round(up * 10000) : UPTIME_UNKNOWN;
      var bytes = beats.slice(-MAX_HB).map(function (b) { return b.status & 0xff; });

      // Ping: current + min/avg/max over the heartbeats that carry one.
      var pings = beats.map(function (b) { return b.ping; })
        .filter(function (p) { return typeof p === 'number'; });
      var ping = (last && typeof last.ping === 'number') ? last.ping : -1;
      var pstats = '';
      if (pings.length) {
        var mn = Math.min.apply(null, pings);
        var mx = Math.max.apply(null, pings);
        var avg = Math.round(pings.reduce(function (a, b) { return a + b; }, 0) / pings.length);
        pstats = mn + '/' + avg + '/' + mx;
      }
      // Last-checked time-of-day "HH:MM" from "YYYY-MM-DD HH:MM:SS..." (server local).
      var time = '';
      if (last && typeof last.time === 'string' && last.time.length >= 16) {
        time = last.time.substring(11, 16);
      }
      var msg = (last && last.msg) ? String(last.msg) : '';

      list.push({
        name: mon.name || ('#' + id), status: status, uptime: uptime, hb: bytes,
        ping: ping, pstats: pstats, time: time, type: mon.type || '', msg: msg
      });
    });
  });
  return list;
}

function loadPage(state, index) {
  var slug = state.pages[index];
  if (!state.baseUrl || !slug) { sendLoad(LOAD_UNCONFIGURED); return; }
  state.activeIndex = index;
  localStorage.setItem('ACTIVE', String(index));
  sendLoad(LOAD_LOADING);
  var base = state.baseUrl;
  getJSON(base + '/api/status-page/' + slug, function (sp) {
    if (!sp) { sendLoad(LOAD_ERROR); return; }
    getJSON(base + '/api/status-page/heartbeat/' + slug, function (hb) {
      streamMonitors(buildMonitors(sp, hb));
    });
  });
}

// --- Lifecycle ---------------------------------------------------------------

Pebble.addEventListener('ready', function () {
  var state = getState();
  if (!state.baseUrl || state.pages.length === 0) {
    sendPageList(state, function () { sendLoad(LOAD_UNCONFIGURED); });
    return;
  }
  sendPageList(state, function () { loadPage(state, state.activeIndex); });
});

// Rebuild the settings page in the currently-selected language, then open it.
Pebble.addEventListener('showConfiguration', function () {
  clay = new Clay(i18n.buildConfig(getLangIdx()), null, { autoHandleEvents: false });
  Pebble.openURL(clay.generateUrl());
});

// Watch -> phone: page switch or manual refresh.
Pebble.addEventListener('appmessage', function (e) {
  var d = e.payload || {};
  var state = getState();
  if (d.REQUEST_PAGE !== undefined && d.REQUEST_PAGE !== null) {
    loadPage(state, parseInt(d.REQUEST_PAGE, 10) || 0);
  } else if (d.REFRESH !== undefined && d.REFRESH !== null) {
    loadPage(state, state.activeIndex);
  }
});

// Clay settings saved: persist them, push the page list, load the default page.
Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) { return; }
  var s = clay.getSettings(e.response);
  var baseUrl = (s[keys.BASE_URL] || '').toString().trim();
  var pagesRaw = (s[keys.STATUS_PAGES] || '').toString();
  var def = (s[keys.DEFAULT_PAGE] || '').toString().trim();
  var sortBy = parseInt(s[keys.SORT_BY], 10) || 0;
  var langIdx = parseInt(s[keys.LANGUAGE], 10) || 0;

  var pages = pagesRaw.split(',').map(function (x) { return x.trim(); })
    .filter(function (x) { return x.length; }).slice(0, MAX_PAGES);
  var activeIndex = 0;
  var di = pages.indexOf(def);
  if (di >= 0) { activeIndex = di; }

  localStorage.setItem('BASE_URL', baseUrl);
  localStorage.setItem('PAGES', JSON.stringify(pages));
  localStorage.setItem('ACTIVE', String(activeIndex));
  localStorage.setItem('LANG', String(langIdx));

  var state = getState();
  sendPageList(state, function () { loadPage(state, state.activeIndex); }, sortBy);
});
