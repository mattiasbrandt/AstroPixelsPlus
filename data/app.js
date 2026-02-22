/* AstroPixels Web UI — Shared JavaScript */
/* WebSocket client, command sender, state poller */

(function() {
  'use strict';

  // --- WebSocket ---
  var ws = null;
  var wsRetry = 1000;

  function wsConnect() {
    var loc = window.location;
    var uri = (loc.protocol === 'https:' ? 'wss:' : 'ws:') + '//' + loc.host + '/ws';
    ws = new WebSocket(uri);

    ws.onopen = function() {
      wsRetry = 1000;
      setConnStatus(true);
    };

    ws.onclose = function() {
      setConnStatus(false);
      setTimeout(wsConnect, wsRetry);
      wsRetry = Math.min(wsRetry * 1.5, 10000);
    };

    ws.onmessage = function(evt) {
      try {
        var msg = JSON.parse(evt.data);
        if (msg.type === 'state' && typeof window.onStateUpdate === 'function') {
          window.onStateUpdate(msg.data);
        }
        if (msg.type === 'log' && typeof window.onLogLine === 'function') {
          window.onLogLine(msg.line);
        }
        if (msg.type === 'health' && typeof window.onHealthUpdate === 'function') {
          window.onHealthUpdate(msg.data);
        }
      } catch(e) { /* ignore parse errors */ }
    };
  }

  function setConnStatus(ok) {
    var el = document.getElementById('conn-status');
    if (el) {
      el.innerHTML = '<span class="dot ' + (ok ? 'green' : 'red') + '"></span>' +
                     (ok ? 'Connected' : 'Disconnected');
    }
  }

  // --- Command sender ---
  window.sendCmd = function(cmd) {
    // Try WebSocket first for lower latency, fall back to REST
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(cmd);
    } else {
      fetch('/api/cmd', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'cmd=' + encodeURIComponent(cmd)
      }).catch(function(){});
    }
    // Brief visual feedback on the clicked button
    if (event && event.target) {
      var btn = event.target;
      btn.style.background = '#0088aa';
      setTimeout(function() { btn.style.background = ''; }, 150);
    }
  };

  // --- State poller (fallback if WS not available) ---
  window.fetchState = function(cb) {
    fetch('/api/state').then(function(r) { return r.json(); })
      .then(cb)
      .catch(function(){});
  };

  // --- Init ---
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', wsConnect);
  } else {
    wsConnect();
  }
})();
