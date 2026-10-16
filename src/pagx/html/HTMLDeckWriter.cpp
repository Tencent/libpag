/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include "pagx/html/HTMLDeckWriter.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

namespace {

// Deck shell stylesheet. Pages are stacked absolutely; the visible one gets `.on`. Each page's
// root div (the fragment's outermost <div data-pagx-version>) is centered via
// translate(-50%,-50%) and scaled by --s (fit-to-window factor, computed in JS) times --z (the
// user's zoom factor, 1 = fit). The #deck>section>div selector outranks the fragment's own
// class-based position rule (the root class sets position:relative and width/height, which stay
// in effect), so the fit transform wins without touching the fragment itself.
constexpr const char* DECK_STYLE =
    R"DECK_CSS(html,body{margin:0;padding:0;width:100%;height:100%;overflow:hidden;background:#101014;font:13px/1.4 -apple-system,"Segoe UI",system-ui,sans-serif;color:#9aa0a6}
#deck{position:absolute;top:0;right:0;bottom:0;left:0;touch-action:none;user-select:none;-webkit-user-select:none}
#deck.zoomed{cursor:grab}
#deck.zoomed:active{cursor:grabbing}
#deck>section{position:absolute;top:0;right:0;bottom:0;left:0;display:none;isolation:isolate}
#deck>section.on{display:block;animation:deckfade .18s ease}
@keyframes deckfade{from{opacity:0}to{opacity:1}}
#deck>section>div{position:absolute;left:50%;top:50%;transform-origin:center;will-change:transform;transform:translate(calc(-50% + var(--tx,0px)),calc(-50% + var(--ty,0px))) scale(calc(var(--s,1) * var(--z,1)))}
#deck-page,#deck-zoom{position:fixed;bottom:10px;padding:2px 8px;border-radius:9px;background:rgba(0,0,0,.45);font-variant-numeric:tabular-nums;user-select:none;pointer-events:none;z-index:9}
#deck-page{right:14px}
#deck-zoom{left:14px;display:none}
#deck-zoom.on{display:block}
)DECK_CSS";

// Deck interaction script. Kept dependency-free and ES5-flavored for broad browser support.
// Navigation is deferred by 250ms after a plain click/tap so that a double-click can cancel the
// pending page turn and zoom instead of paging once and then zooming.
constexpr const char* DECK_SCRIPT = R"DECK_JS((function () {
var S = [].slice.call(document.querySelectorAll('#deck>section'));
var DECK = document.getElementById('deck');
var PAGE = document.getElementById('deck-page');
var ZOOM = document.getElementById('deck-zoom');
var MINZ = 0.25, MAXZ = 8, DRAG = 5;
var c = 0, z = 1, tx = 0, ty = 0;
var tapTimer = null, dragging = false, dragMoved = false, dragX = 0, dragY = 0, dragTX = 0, dragTY = 0;
var tStart = null, pinch = null;

function cur() { return S[c]; }
function rootOf(s) {
  for (var e = s.firstElementChild; e; e = e.nextElementSibling) {
    if (e.tagName === 'DIV') return e;
  }
  return null;
}
function fit() {
  S.forEach(function (s) {
    var w = +s.dataset.w, h = +s.dataset.h;
    if (w > 0 && h > 0) s.style.setProperty('--s', Math.min(innerWidth / w, innerHeight / h).toFixed(6));
  });
}
function apply() {
  var s = cur();
  if (s) {
    s.style.setProperty('--z', String(z));
    s.style.setProperty('--tx', tx + 'px');
    s.style.setProperty('--ty', ty + 'px');
  }
  DECK.classList.toggle('zoomed', z > 1);
  ZOOM.classList.toggle('on', z > 1);
  ZOOM.textContent = Math.round(z * 100) + '%';
}
function reset() { z = 1; tx = 0; ty = 0; apply(); }
function go(n) {
  c = Math.max(0, Math.min(S.length - 1, n));
  S.forEach(function (s, i) { s.classList.toggle('on', i === c); });
  PAGE.textContent = (c + 1) + ' / ' + S.length;
  history.replaceState(null, '', '#p' + (c + 1));
  reset();
}
function zoomAt(f, mx, my) {
  var nz = Math.max(MINZ, Math.min(MAXZ, z * f));
  if (nz === z) return;
  var d = rootOf(cur());
  var cx = innerWidth / 2, cy = innerHeight / 2;
  if (d) {
    var r = d.getBoundingClientRect();
    cx = r.left + r.width / 2;
    cy = r.top + r.height / 2;
  }
  tx += (mx - cx) * (1 - nz / z);
  ty += (my - cy) * (1 - nz / z);
  z = nz;
  apply();
}
function toggleZoom(x, y) { if (z === 1) zoomAt(2, x, y); else reset(); }
function tap(x, y) { go(x < innerWidth / 3 ? c - 1 : c + 1); }
function scheduleTap(x, y) {
  cancelTap();
  tapTimer = setTimeout(function () { tapTimer = null; tap(x, y); }, 250);
}
function cancelTap() { if (tapTimer) { clearTimeout(tapTimer); tapTimer = null; } }

addEventListener('keydown', function (e) {
  if (e.metaKey || e.ctrlKey || e.altKey) return;
  var k = e.key;
  if (k === 'ArrowRight' || k === 'PageDown' || k === ' ') go(c + 1);
  else if (k === 'ArrowLeft' || k === 'PageUp') go(c - 1);
  else if (k === 'Home') go(0);
  else if (k === 'End') go(S.length - 1);
  else if (k === '+' || k === '=') zoomAt(1.25, innerWidth / 2, innerHeight / 2);
  else if (k === '-' || k === '_') zoomAt(0.8, innerWidth / 2, innerHeight / 2);
  else if (k === '0') reset();
  else return;
  e.preventDefault();
});
addEventListener('wheel', function (e) {
  if (!e.ctrlKey) return;
  e.preventDefault();
  zoomAt(e.deltaY < 0 ? 1.1 : 1 / 1.1, e.clientX, e.clientY);
}, { passive: false });
addEventListener('dblclick', function (e) { cancelTap(); toggleZoom(e.clientX, e.clientY); });

DECK.addEventListener('mousedown', function (e) {
  if (e.button !== 0) return;
  dragging = true; dragMoved = false;
  dragX = e.clientX; dragY = e.clientY; dragTX = tx; dragTY = ty;
});
addEventListener('mousemove', function (e) {
  if (!dragging) return;
  var dx = e.clientX - dragX, dy = e.clientY - dragY;
  if (!dragMoved && dx * dx + dy * dy > DRAG * DRAG) dragMoved = true;
  if (dragMoved && z > 1) { tx = dragTX + dx; ty = dragTY + dy; apply(); }
});
addEventListener('mouseup', function (e) {
  if (!dragging) return;
  dragging = false;
  if (!dragMoved) scheduleTap(e.clientX, e.clientY);
});

DECK.addEventListener('touchstart', function (e) {
  if (e.touches.length === 1) {
    if (tapTimer) {
      cancelTap();
      toggleZoom(e.touches[0].clientX, e.touches[0].clientY);
      tStart = null;
      e.preventDefault();
      return;
    }
    tStart = { x: e.touches[0].clientX, y: e.touches[0].clientY, tx: tx, ty: ty, moved: false };
  } else if (e.touches.length === 2) {
    tStart = null;
    var a = e.touches[0], b = e.touches[1];
    pinch = { d: Math.hypot(a.clientX - b.clientX, a.clientY - b.clientY), z: z };
  }
}, { passive: false });
DECK.addEventListener('touchmove', function (e) {
  if (pinch && e.touches.length >= 2) {
    e.preventDefault();
    var a = e.touches[0], b = e.touches[1];
    var d = Math.hypot(a.clientX - b.clientX, a.clientY - b.clientY);
    if (pinch.d > 0) {
      var nz = Math.max(MINZ, Math.min(MAXZ, pinch.z * d / pinch.d));
      zoomAt(nz / z, (a.clientX + b.clientX) / 2, (a.clientY + b.clientY) / 2);
    }
    return;
  }
  if (tStart && e.touches.length === 1) {
    var dx = e.touches[0].clientX - tStart.x, dy = e.touches[0].clientY - tStart.y;
    if (!tStart.moved && dx * dx + dy * dy > DRAG * DRAG) tStart.moved = true;
    if (tStart.moved) {
      e.preventDefault();
      if (z > 1) { tx = tStart.tx + dx; ty = tStart.ty + dy; apply(); }
    }
  }
}, { passive: false });
DECK.addEventListener('touchend', function (e) {
  if (pinch) { if (e.touches.length === 0) pinch = null; return; }
  if (!tStart) return;
  var t = e.changedTouches[0];
  if (!tStart.moved) {
    scheduleTap(t.clientX, t.clientY);
  } else if (z === 1) {
    var ax = t.clientX - tStart.x, ay = t.clientY - tStart.y;
    if (Math.abs(ax) > Math.abs(ay) && Math.abs(ax) > 40) go(ax < 0 ? c + 1 : c - 1);
  }
  tStart = null;
});

addEventListener('resize', function () { fit(); tx = 0; ty = 0; apply(); });
fit();
go((parseInt((location.hash || '').slice(2), 10) || 1) - 1);
})();)DECK_JS";

}  // namespace

std::string BuildDeckHTML(const std::vector<std::string>& pageFragments,
                          const std::vector<std::pair<float, float>>& pageSizes) {
  std::string html;
  size_t totalSize = 0;
  for (const auto& fragment : pageFragments) {
    totalSize += fragment.size();
  }
  html.reserve(totalSize + pageFragments.size() * 64 + 8192);
  html += "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n";
  html += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n";
  html += "<title>PAGX Deck</title>\n<style>\n";
  html += DECK_STYLE;
  html += "</style>\n</head>\n<body>\n<div id=\"deck\">\n";
  for (size_t i = 0; i < pageFragments.size(); i++) {
    auto width = i < pageSizes.size() ? pageSizes[i].first : 0.0f;
    auto height = i < pageSizes.size() ? pageSizes[i].second : 0.0f;
    html += "<section data-w=\"" + CssFloatToString(width) + "\" data-h=\"" +
            CssFloatToString(height) + "\">\n";
    html += pageFragments[i];
    html += "\n</section>\n";
  }
  html += "</div>\n<div id=\"deck-page\"></div>\n<div id=\"deck-zoom\"></div>\n<script>\n";
  html += DECK_SCRIPT;
  html += "</script>\n</body>\n</html>\n";
  return html;
}

}  // namespace pagx
