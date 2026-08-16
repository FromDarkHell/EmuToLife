// location value from JSON -> data-pad-id in the DOM
const LOCATION_TO_PAD_ID = { 1: 2, 2: 1, 3: 3 };

const MODE_SOLID = 0x00;
const MODE_FADE = 0x01;
const MODE_FLASH = 0x02;

// Dedicated <style> element for injected @keyframes
const styleEl = document.createElement("style");
document.head.appendChild(styleEl);
const sheet = styleEl.sheet;

// Track inserted rule indices by name so we can replace them
const keyframeIndex = {};

function rgb(c) {
  function rgbToPerceivedColor(color) {
    const GAMMA = 3.78;

    return {
      r: Math.round(Math.pow(color.r / 255, 1 / GAMMA) * 255),
      g: Math.round(Math.pow(color.g / 255, 1 / GAMMA) * 255),
      b: Math.round(Math.pow(color.b / 255, 1 / GAMMA) * 255),
    };
  }

  cx = rgbToPerceivedColor(c);

  return `rgb(${cx.r},${cx.g},${cx.b})`;
}

let flashSeq = 0;

// Insert (or replace) a @keyframes rule, return the name
function setKeyframes(name, body) {
  name = `${name}-${flashSeq++}`;
  if (keyframeIndex[name] !== undefined) {
    try {
      sheet.deleteRule(keyframeIndex[name]);
    } catch (_) {}
  }

  const idx = sheet.insertRule(
    `@keyframes ${name} { ${body} }`,
    sheet.cssRules.length,
  );
  keyframeIndex[name] = idx;
  return name;
}

function applyToPads(padId, applyFn) {
  document.querySelectorAll(`.pad[data-pad-id="${padId}"]`).forEach(applyFn);
}

function applyPadColor(padId, zone) {
  const { mode, color } = zone;

  // Reset first
  applyToPads(padId, (pad) => {
    pad.style.animation = "none";
    pad.style.backgroundColor = "";
  });

  if (mode === MODE_SOLID) {
    applyToPads(padId, (pad) => {
      pad.style.backgroundColor = rgb(color);
    });
  } else if (mode === MODE_FADE) {
    const speed = zone.fade.speed ?? 10;
    const count = zone.fade.count ?? 0;
    const target = zone.fade.target ?? color;

    // Each half-cycle duration in seconds
    const halfDur = (speed * 0.1).toFixed(1) + "s";
    const iters = count === 0 ? "infinite" : count;

    // One half-cycle: base → target only.
    // CSS alternate handles target → base on even iterations.
    const name = setKeyframes(
      `pad-fade-${padId}`,
      `
    0%   { background-color: ${rgb(color)}; }
    100% { background-color: ${rgb(target)}; }
    `,
    );

    applyToPads(padId, (pad) => {
      if (pad._fadeEndListener) {
        pad.removeEventListener("animationend", pad._fadeEndListener);
        pad._fadeEndListener = null;
      }

      pad.style.animation = "none";
      pad.style.backgroundColor = rgb(color);
      void pad.offsetWidth;

      // alternate: even iterations go base→target, odd go target→base
      // forwards:  hold the final frame until animationend fires
      pad.style.animation = `${name} ${halfDur} linear ${iters} alternate forwards`;

      if (count !== 0) {
        const onEnd = () => {
          pad.style.animation = "none";
          // Even count ends on base (last half went target→base)
          // Odd count ends on target (last half went base→target)
          pad.style.backgroundColor =
            count % 2 === 0 ? rgb(color) : rgb(target);
          pad._fadeEndListener = null;
        };
        pad._fadeEndListener = onEnd;
        pad.addEventListener("animationend", onEnd, { once: true });
      }
    });
  } else if (mode === MODE_FLASH) {
    const onTime = zone.flash.onTime ?? 255; // ticks (1 tick = 100ms)
    const offTime = zone.flash.offTime ?? 255;
    const count = zone.flash.count ?? 0;
    const offColor = zone.flash.offColor ?? { r: 0, g: 0, b: 0 };

    const totalTicks = onTime + offTime;
    const dur = (totalTicks * 0.1).toFixed(1) + "s"; // ticks → seconds
    const iters = count === 0 ? "infinite" : count;

    const switchPct = (onTime / totalTicks) * 100;

    const name = setKeyframes(
      `pad-flash-${padId}`,
      `
  0%              { background-color: ${rgb(color)}; }
  ${switchPct}%   { background-color: ${rgb(color)}; }
  ${switchPct + 0.0001}% { background-color: ${rgb(offColor)}; }
  100%            { background-color: ${rgb(offColor)}; }
`,
    );

    applyToPads(padId, (pad) => {
      pad.style.animation = "none";
      pad.style.backgroundColor = rgb(color);
      void pad.offsetWidth; // force reflow

      // linear: keyframe %s directly control timing, no stepping
      pad.style.animation = `${name} ${dur} linear ${iters}`;

      if (count !== 0) {
        pad.addEventListener(
          "animationend",
          () => {
            pad.style.animation = "none";
            // Cycle always ends on offColor (100% keyframe).
            // Even count  → last "on" phase was the final visible state → restore color
            // Odd count   → last "off" phase was the final visible state → keep offColor
            pad.style.backgroundColor =
              count % 2 === 0 ? rgb(color) : rgb(offColor);
          },
          { once: true },
        );
      }
    });
  }
}

let lastZones = {};

function zonesEqual(a, b) {
  return JSON.stringify(a) === JSON.stringify(b);
}

function handlePadState(data) {
  for (const zone of Object.values(data)) {
    const padId = LOCATION_TO_PAD_ID[zone.location];
    if (padId === undefined) continue;

    if (!zonesEqual(lastZones[padId], zone)) {
      applyPadColor(padId, zone);
      lastZones[padId] = zone;
    }
  }
}

let _ws = null;
function connectWS() {
  const statusEl = document.getElementById("ws-status");
  const statusTextEl = document.getElementById("ws-status-text");

  const PING_INTERVAL_MS = 3000; // how often the client pings
  const PONG_TIMEOUT_MS = 6000; // how long to wait for a pong reply

  let _pingInterval = null;
  let _pongTimeout = null;
  let _awaitingPong = false;

  function setStatus(state, text) {
    statusEl.className = state;
    statusTextEl.textContent = text;
  }

  function sendPing() {
    if (_ws.readyState !== WebSocket.OPEN) return;

    if (_awaitingPong) {
      console.warn("[WS] Pong not received in time");
      setStatus("disconnected", "Disconnected");
      stopHeartbeat();
      _ws.close();
      return;
    }

    _awaitingPong = true;
    _ws.send(JSON.stringify({ type: "ping" }));
    console.debug("[WS] Ping sent");

    // If pong doesn't arrive within the timeout, next sendPing will catch it
    _pongTimeout = setTimeout(() => {
      if (_awaitingPong) {
        console.warn("[WS] Pong timeout");
        setStatus("disconnected", "No Response");
      }
    }, PONG_TIMEOUT_MS);
  }

  function startHeartbeat() {
    stopHeartbeat();
    _awaitingPong = false;
    _pingInterval = setInterval(sendPing, PING_INTERVAL_MS);
  }

  function stopHeartbeat() {
    if (_pingInterval !== null) {
      clearInterval(_pingInterval);
      _pingInterval = null;
    }
    if (_pongTimeout !== null) {
      clearTimeout(_pongTimeout);
      _pongTimeout = null;
    }
    _awaitingPong = false;
  }

  setStatus("connecting", "Connecting");

  const url = `ws://${location.host}/ws`;
  _ws = new WebSocket(url);

  _ws.onopen = () => {
    console.log("[WS] Connected");

    // If something (e.g. a platform switch) rebooted the device and we're waiting for it to
    // come back, reconnecting here IS that signal - reload instead of resuming normally so the
    // page picks up the post-reboot state.
    if (window.__awaitingReboot) {
      window.__awaitingReboot = false;
      setStatus("connected", "Restarted - reloading...");
      location.reload();
      return;
    }

    setStatus("connected", "Connected");
    startHeartbeat();
  };

  _ws.onmessage = (evt) => {
    try {
      const data = JSON.parse(evt.data);

      if (data.type === "pong") {
        console.debug("[WS] Pong received");
        clearTimeout(_pongTimeout);
        _pongTimeout = null;
        _awaitingPong = false;
        setStatus("connected", "Connected");
        return;
      }

      if (data.type === "toybox") {
        toybox = data.toybox;
        renderToybox(true);
      }

      handlePadState(data);
    } catch (e) {
      console.error("[WS] Bad message:", e);
    }
  };

  _ws.onclose = () => {
    console.warn("[WS] Closed");
    setStatus("disconnected", "Disconnected");
    stopHeartbeat();
    setTimeout(connectWS, 2000);
  };

  _ws.onerror = (e) => {
    console.error("[WS] Error:", e);
    setStatus("disconnected", "Error");
    stopHeartbeat();
    _ws.close();
  };
}
