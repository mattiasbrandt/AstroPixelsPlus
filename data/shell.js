/* AstroPixels — Shared shell: topbar, nav, status bar */
(function () {
  var page = (document.body && document.body.dataset && document.body.dataset.page) || 'home';

  var NAV = [
    { key: 'home',      href: '/',              label: '🏠 Home' },
    { key: 'panels',    href: '/panels.html',   label: '🧩 Panels' },
    { key: 'holos',     href: '/holos.html',    label: '💡 Holos' },
    { key: 'logics',    href: '/logics.html',   label: '🔵 Logics' },
    { key: 'sequences', href: '/sequences.html', label: '🎬 Sequences' },
    { key: 'sound',     href: '/sound.html',    label: '🔊 Sound' },
    { key: 'setup',     href: '/setup.html',    label: '⚙️ Setup' },
  ];

  var PAGE_TITLES = {
    home:      'AstroPixels',
    panels:    'Panels — AstroPixels',
    holos:     'Holos — AstroPixels',
    logics:    'Logics — AstroPixels',
    sequences: 'Sequences — AstroPixels',
    sound:     'Sound — AstroPixels',
    setup:     'Setup — AstroPixels',
    wifi:      'WiFi — AstroPixels',
    serial:    'Serial — AstroPixels',
    remote:    'Remote — AstroPixels',
    firmware:  'Firmware — AstroPixels',
  };

  document.title = PAGE_TITLES[page] || 'AstroPixels';

  var shellTop = document.getElementById('shell-top');
  if (shellTop) {
    var tmpl = document.getElementById('topbar-actions-template');
    var actionsHtml = tmpl ? tmpl.innerHTML.trim() : '';

    var navHtml = NAV.map(function (item) {
      var isActive = page === item.key;
      return '<a href="' + item.href + '"' +
        (isActive ? ' class="active" aria-current="page"' : '') +
        '>' + item.label + '</a>';
    }).join('');

    shellTop.innerHTML =
      '<div class="topbar">' +
        '<a href="/" class="topbar-brand">' +
          '<img src="/r2d2dome.svg" alt="R2-D2" class="topbar-logo">' +
          '<span data-droid-name-target>AstroPixels</span>' +
        '</a>' +
        (actionsHtml ? '<div class="topbar-actions">' + actionsHtml + '</div>' : '') +
      '</div>' +
      '<nav>' + navHtml + '</nav>';
  }

  var shellStatus = document.getElementById('shell-status');
  if (shellStatus) {
    shellStatus.innerHTML =
      '<div class="status-bar" id="conn-status">' +
        '<span class="dot yellow"></span>Connecting...' +
      '</div>';
  }
})();
