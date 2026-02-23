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
        if (msg.type === 'ota' && typeof window.onOtaProgress === 'function') {
          window.onOtaProgress(msg.progress);
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

  function getApiToken() {
    try {
      return localStorage.getItem('apitoken') || '';
    } catch (e) {
      return '';
    }
  }

  window.getApiToken = getApiToken;

  window.setApiToken = function(token) {
    try {
      if (token) localStorage.setItem('apitoken', token);
      else localStorage.removeItem('apitoken');
    } catch (e) {}
  };

  function withWriteAuthHeaders(headers) {
    var merged = headers || {};
    var token = getApiToken();
    if (token) {
      merged['X-AP-Token'] = token;
    }
    return merged;
  }

  window.apiPostForm = function(path, body) {
    return fetch(path, {
      method: 'POST',
      headers: withWriteAuthHeaders({'Content-Type': 'application/x-www-form-urlencoded'}),
      body: body
    });
  };

  window.apiPost = function(path, options) {
    var opts = options || {};
    opts.method = 'POST';
    opts.headers = withWriteAuthHeaders(opts.headers || {});
    return fetch(path, opts);
  };

  function holoName(id) {
    if (id === '1') return 'front holo';
    if (id === '2') return 'rear holo';
    if (id === '3') return 'top holo';
    return 'holo';
  }

  function logicTarget(id) {
    if (id === '0') return 'all logic displays';
    if (id === '1') return 'front logic display';
    if (id === '2') return 'rear logic display';
    return 'logic display';
  }

  function psiTarget(id) {
    if (id === '0') return 'both PSI indicators';
    if (id === '1') return 'front PSI indicator';
    if (id === '2') return 'rear PSI indicator';
    return 'PSI indicator';
  }

  function describeCommand(cmd) {
    var m;

    if (cmd === ':OP00') return 'Open all dome panels';
    if (cmd === ':CL00') return 'Close all dome panels';
    if (cmd === ':OF00') return 'Flutter all dome panels';

    m = cmd.match(/^:OP(\d{2})$/);
    if (m) return 'Open panel/group ' + m[1];
    m = cmd.match(/^:CL(\d{2})$/);
    if (m) return 'Close panel/group ' + m[1];
    m = cmd.match(/^:OF(\d{2})$/);
    if (m) return 'Flutter panel/group ' + m[1];

    m = cmd.match(/^:SE(\d{2})$/);
    if (m) return 'Run sequence ' + m[1];

    if (cmd === '*ON00') return 'Turn all holo lights on';
    if (cmd === '*OF00') return 'Turn all holo lights off';
    if (cmd === '*ST00') return 'Reset all holos to default state';

    m = cmd.match(/^\*ON0([1-3])$/);
    if (m) return 'Turn ' + holoName(m[1]) + ' light on';
    m = cmd.match(/^\*OF0([1-3])$/);
    if (m) return 'Turn ' + holoName(m[1]) + ' light off';
    if (cmd === '*OF04') return 'Turn radar eye light off';
    m = cmd.match(/^\*RD0([1-3])$/);
    if (m) return 'Random servo movement for ' + holoName(m[1]);
    m = cmd.match(/^\*HW0([1-3])$/);
    if (m) return 'Wag movement for ' + holoName(m[1]);
    m = cmd.match(/^\*HN0([1-3])$/);
    if (m) return 'Nod movement for ' + holoName(m[1]);

    m = cmd.match(/^\*HPS[36]0([1-3])$/);
    if (m) return 'LED effect for ' + holoName(m[1]);
    m = cmd.match(/^\*HRS([346R])$/);
    if (m) {
      if (m[1] === '3') return 'Radar eye pulse effect';
      if (m[1] === 'R') return 'Radar eye red pulse effect';
      if (m[1] === '6') return 'Radar eye rainbow effect';
      if (m[1] === '4') return 'Radar eye color-cycle effect';
    }
    m = cmd.match(/^\*HP([0-8])0([1-3])$/);
    if (m) return 'Set ' + holoName(m[2]) + ' position preset ' + m[1];

    m = cmd.match(/^@([012])T(\d{1,2})$/);
    if (m) return 'Set sequence ' + m[2] + ' on ' + logicTarget(m[1]);
    m = cmd.match(/^@([012])P(11|[1-6])$/);
    if (m) return 'Set PSI mode ' + m[2] + ' on ' + psiTarget(m[1]);
    m = cmd.match(/^@[123]M/);
    if (m) return 'Send scroll text to logic display';

    if (cmd === '$R') return 'Play random sound';
    if (cmd === '$S') return 'Play scream sound';
    if (cmd === '$L') return 'Play Leia sound';
    if (cmd === '$C') return 'Play cantina sound';
    if (cmd === '$c') return 'Play beep-cantina sound';
    if (cmd === '$W') return 'Play Star Wars sound';
    if (cmd === '$M') return 'Play march sound';
    if (cmd === '$D') return 'Play disco sound';
    if (cmd === '$F') return 'Play faint sound';
    if (cmd === '$s') return 'Stop currently playing sound';
    if (cmd === '$O') return 'Mute audio output';
    if (cmd === '$-') return 'Decrease volume';
    if (cmd === '$m') return 'Set volume to medium';
    if (cmd === '$+') return 'Increase volume';
    if (cmd === '$f') return 'Set volume to maximum';
    if (cmd === '$p') return 'Set volume to minimum';

    return '';
  }

  function extractCommandFromButton(btn) {
    var direct = btn.getAttribute('data-cmd');
    if (direct) return direct;

    var onClick = btn.getAttribute('onclick') || '';
    var match = onClick.match(/sendCmd\((['"])(.*?)\1\)/);
    if (!match) return '';
    return match[2] || '';
  }

  function isValidManualCommand(cmd) {
    if (!cmd || cmd.length > 63) return false;
    for (var i = 0; i < cmd.length; i++) {
      var cc = cmd.charCodeAt(i);
      if (cc < 32 || cc > 126) return false;
    }
    return true;
  }

  function enhanceTooltips() {
    var buttons = document.querySelectorAll('button[onclick]');
    for (var i = 0; i < buttons.length; i++) {
      var btn = buttons[i];
      if (btn.getAttribute('title')) continue;
      var cmd = extractCommandFromButton(btn);
      if (!cmd) continue;
      var tip = describeCommand(cmd);
      if (tip) btn.setAttribute('title', tip + ' (' + cmd + ')');
    }

    var linkTips = {
      '/wifi.html': 'Configure WiFi credentials and AP mode',
      '/serial.html': 'Configure Marcduino serial settings and passthrough',
      '/sound.html': 'Configure sound player type, startup track, and defaults',
      '/remote.html': 'Configure ESPNOW droid remote pairing and secret',
      '/firmware.html': 'Upload firmware and run maintenance actions'
    };

    var links = document.querySelectorAll('a[href]');
    for (var j = 0; j < links.length; j++) {
      var a = links[j];
      if (a.getAttribute('title')) continue;
      var href = a.getAttribute('href');
      if (linkTips[href]) a.setAttribute('title', linkTips[href]);
    }
  }

  // --- Command sender ---
  window.sendCmd = function(cmd) {
    if (!isValidManualCommand(cmd)) {
      return;
    }
    // Try WebSocket first for lower latency, fall back to REST
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(cmd);
    } else {
      apiPostForm('/api/cmd', 'cmd=' + encodeURIComponent(cmd)).catch(function(){});
    }
    // Brief visual feedback on the clicked button
    var ev = (typeof event !== 'undefined') ? event : null;
    if (ev && ev.target) {
      var btn = ev.target;
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
    document.addEventListener('DOMContentLoaded', function() {
      enhanceTooltips();
      wsConnect();
    });
  } else {
    enhanceTooltips();
    wsConnect();
  }
})();
