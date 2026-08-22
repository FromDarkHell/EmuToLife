async function changePlatform(value, button) {
  const statusEl = document.getElementById("platform-status");
  const buttons = button.parentElement.querySelectorAll("button");

  buttons.forEach((b) => (b.disabled = true));
  statusEl.style.display = "block";
  statusEl.textContent = "Switching platform...";

  // The device reboots as part of handling this request, so it may never get to
  // send a response back - that's expected, not a failure. Once rebooted, it's
  // the websocket reconnecting (see websockets.js) that tells us it's back.
  window.__awaitingReboot = true;
  try {
    await fetch("/platform", {
      method: "POST",
      headers: {
        "Content-Type": "application/x-www-form-urlencoded",
      },
      body: `platform=${value}`,
    });
  } catch (e) {
    console.debug("[platform] fetch didn't complete", e);
  }
}
