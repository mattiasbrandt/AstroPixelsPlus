(function() {
  'use strict';

  function byId(id) {
    return document.getElementById(id);
  }

  function toastOrAlert(message, kind) {
    if (window.uiToast) window.uiToast(message, kind || 'error');
    else alert(message);
  }

  function setDisplay(el, value) {
    if (el) el.style.display = value;
  }

  function defaultConflictName(slot, idx) {
    return slot.label || ('Slot ' + idx);
  }

  window.initWiringConfig = function(config) {
    var details = byId(config.detailsId);
    var tbody = byId(config.tbodyId);
    var tableWrap = byId(config.tableWrapId);
    var loading = byId(config.loadingId);
    var errorEl = byId(config.errorId);
    var conflictEl = byId(config.conflictId);
    var saveBtn = byId(config.saveBtnId);
    var saveResult = byId(config.saveResultId);
    var slots = [];
    var activeTestIdx = -1;

    if (!details || !tbody || !tableWrap || !loading || !errorEl ||
        !conflictEl || !saveBtn || !saveResult) {
      return;
    }

    function rowByIdx(idx) {
      return tbody.querySelector('tr[data-idx="' + idx + '"]');
    }

    function resetTestButton(btn) {
      if (!btn) return;
      btn.dataset.testing = '0';
      btn.textContent = config.testLabel || 'Test';
      btn.classList.remove('accent');
    }

    function stopActiveTest() {
      return window.apiPost('/api/servo/stop', {
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ board: config.board })
      }).then(function(r) {
        if (!r.ok) throw new Error('HTTP ' + r.status);
        activeTestIdx = -1;
        return r;
      });
    }

    function updateRowEnabled(tr, slot) {
      var sel = tr.querySelector('select');
      var testBtn = tr.querySelector('button');
      if (sel) sel.disabled = !slot.active;
      if (testBtn) testBtn.disabled = !slot.active;
      tr.style.opacity = slot.active ? '1' : '0.55';
    }

    function revalidate() {
      var byCh = {};
      slots.forEach(function(slot, idx) {
        if (!slot.active) return;
        (byCh[slot.channel] = byCh[slot.channel] || []).push(idx);
      });

      Array.from(tbody.querySelectorAll('tr[data-idx]')).forEach(function(tr) {
        tr.style.background = '';
      });

      var msg = '';
      var conflict = false;
      Object.keys(byCh).forEach(function(ch) {
        if (byCh[ch].length <= 1) return;
        conflict = true;
        var names = byCh[ch].map(function(i) {
          return (config.conflictName || defaultConflictName)(slots[i], i);
        }).join(', ');
        msg += 'Channel ' + ch + ' assigned to: ' + names + '. ';
        byCh[ch].forEach(function(i) {
          var row = rowByIdx(i);
          if (row) row.style.background = 'rgba(220,180,0,.18)';
        });
      });

      if (conflict) {
        conflictEl.textContent = msg;
        setDisplay(conflictEl, '');
        saveBtn.disabled = true;
      } else {
        setDisplay(conflictEl, 'none');
        saveBtn.disabled = false;
      }
    }

    function handleActiveChange(slot, idx, tr, checkbox) {
      if (!checkbox.checked && activeTestIdx === idx) {
        stopActiveTest().then(function() {
          slot.active = false;
          updateRowEnabled(tr, slot);
          resetTestButton(tr.querySelector('button'));
          revalidate();
        }).catch(function(err) {
          checkbox.checked = true;
          slot.active = true;
          toastOrAlert((config.deactivateStopFailedMessage || 'Could not deactivate') +
            ': stop failed (' + err.message + '). ' +
            (config.activeTestStillRunningText || 'Test is still running.') , 'error');
        });
        return;
      }
      slot.active = checkbox.checked;
      updateRowEnabled(tr, slot);
      revalidate();
    }

    function handleTestClick(idx, btn) {
      if (btn.dataset.testing === '1') {
        stopActiveTest().then(function() {
          resetTestButton(btn);
        }).catch(function(err) {
          toastOrAlert('Stop failed: ' + err.message + ' - ' +
            (config.activeTestMayStillRunText || 'test may still be running. Retry.'), 'error');
        });
        return;
      }

      if (activeTestIdx >= 0 && activeTestIdx !== idx) {
        var prevRow = rowByIdx(activeTestIdx);
        if (prevRow) resetTestButton(prevRow.querySelector('button'));
      }

      window.apiPost('/api/servo/test', {
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ board: config.board, channel: slots[idx].channel })
      }).then(function(r) {
        if (!r.ok) throw new Error('HTTP ' + r.status);
        btn.dataset.testing = '1';
        btn.textContent = config.stopLabel || 'Stop';
        btn.classList.add('accent');
        activeTestIdx = idx;
      }).catch(function(err) {
        toastOrAlert((config.testFailedPrefix || 'Test failed') + ': ' + err.message, 'error');
      });
    }

    function makeCell(className, text) {
      var td = document.createElement('td');
      td.className = className || 'panel-cell';
      td.textContent = text;
      return td;
    }

    function buildRow(slot, idx) {
      var tr = document.createElement('tr');
      tr.dataset.idx = idx;

      tr.appendChild(makeCell('panel-cell',
        (config.firstCellText || defaultConflictName)(slot, idx)));

      var actTd = makeCell('panel-cell-center', '');
      var actCb = document.createElement('input');
      actCb.type = 'checkbox';
      actCb.checked = !!slot.active;
      actCb.addEventListener('change', function() {
        handleActiveChange(slot, idx, tr, actCb);
      });
      actTd.appendChild(actCb);
      tr.appendChild(actTd);

      var chTd = makeCell('panel-cell-center', '');
      var chSel = document.createElement('select');
      for (var c = 0; c <= 15; c++) {
        var opt = document.createElement('option');
        opt.value = c;
        opt.textContent = c;
        if (c === slot.channel) opt.selected = true;
        chSel.appendChild(opt);
      }
      chSel.addEventListener('change', function() {
        slot.channel = parseInt(chSel.value, 10);
        revalidate();
      });
      chTd.appendChild(chSel);
      tr.appendChild(chTd);

      var extraTd = makeCell('panel-cell-center',
        config.extraCellText ? config.extraCellText(slot, idx) : '');
      if (config.decorateExtraCell) config.decorateExtraCell(extraTd, slot, idx);
      tr.appendChild(extraTd);

      var testTd = makeCell('panel-cell-center', '');
      var testBtn = document.createElement('button');
      testBtn.className = 'btn';
      testBtn.textContent = config.testLabel || 'Test';
      testBtn.dataset.testing = '0';
      testBtn.addEventListener('click', function() {
        handleTestClick(idx, testBtn);
      });
      testTd.appendChild(testBtn);
      tr.appendChild(testTd);

      updateRowEnabled(tr, slot);
      return tr;
    }

    function renderTable() {
      tbody.innerHTML = '';
      if (config.prepareSlots) config.prepareSlots(slots);

      if (config.groupOrder && config.groupKey) {
        var groups = {};
        slots.forEach(function(slot, idx) {
          var key = config.groupKey(slot, idx);
          (groups[key] = groups[key] || []).push({ slot: slot, idx: idx });
        });
        config.groupOrder.forEach(function(key) {
          if (!groups[key]) return;
          var hdr = document.createElement('tr');
          var hdrTd = document.createElement('td');
          hdrTd.colSpan = 5;
          hdrTd.style.color = 'var(--accent)';
          hdrTd.style.fontWeight = 'bold';
          hdrTd.style.paddingTop = '0.5em';
          hdrTd.textContent = config.groupHeading ? config.groupHeading(key) : key;
          hdr.appendChild(hdrTd);
          tbody.appendChild(hdr);
          groups[key].forEach(function(item) {
            tbody.appendChild(buildRow(item.slot, item.idx));
          });
        });
      } else {
        slots.forEach(function(slot, idx) {
          tbody.appendChild(buildRow(slot, idx));
        });
      }

      revalidate();
    }

    function fetchConfig() {
      setDisplay(loading, '');
      setDisplay(errorEl, 'none');
      setDisplay(tableWrap, 'none');
      saveResult.textContent = '';

      fetch(config.configUrl).then(function(r) {
        if (!r.ok) throw new Error('HTTP ' + r.status);
        return r.json();
      }).then(function(j) {
        slots = j.slots || [];
        renderTable();
        setDisplay(loading, 'none');
        setDisplay(tableWrap, '');
      }).catch(function(err) {
        setDisplay(loading, 'none');
        errorEl.textContent = 'Failed to load config: ' + err.message;
        setDisplay(errorEl, '');
      });
    }

    function load() {
      if (activeTestIdx >= 0) {
        stopActiveTest().then(fetchConfig).catch(function(err) {
          toastOrAlert('Reload aborted: stop failed (' + err.message + ') - ' +
            (config.activeTestMayStillRunText || 'test may still be running.') , 'error');
        });
      } else {
        fetchConfig();
      }
    }

    function save() {
      var body = {
        slots: slots.map(function(slot, i) {
          return { index: i, channel: slot.channel, active: !!slot.active };
        })
      };
      saveResult.textContent = 'Saving...';
      window.apiPost(config.configUrl, {
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
      }).then(function(r) {
        return r.json().then(function(j) { return { ok: r.ok, body: j }; });
      }).then(function(res) {
        if (!res.ok) {
          saveResult.innerHTML = '<span style="color:var(--danger)">Save failed: ' +
            (res.body && res.body.error ? res.body.error : 'unknown error') + '</span>';
          return;
        }
        saveResult.innerHTML = config.saveSuccessHtml ||
          '<span style="color:var(--success)">Saved and applied.</span>';
        activeTestIdx = -1;
        renderTable();
      }).catch(function(err) {
        saveResult.innerHTML = '<span style="color:var(--danger)">Save failed: ' +
          err.message + '</span>';
      });
    }

    window[config.loadName] = load;
    window[config.saveName] = save;

    try {
      if (localStorage.getItem(config.localKey) === '1') details.open = true;
    } catch (e) {}

    details.addEventListener('toggle', function() {
      try { localStorage.setItem(config.localKey, details.open ? '1' : '0'); } catch (e) {}
      if (details.open && slots.length === 0) load();
    });

    if (details.open) load();
  };
})();
