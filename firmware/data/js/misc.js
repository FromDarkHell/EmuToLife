/** === File Uploading === */
var _pendingFile = null;
function handleFileSelected(input) {
  _pendingFile = input.files[0] ?? null;
  const btn = document.getElementById("uploadBtn");
  const label = document.getElementById("fileLabel");
  if (_pendingFile) {
    label.textContent = _pendingFile.name;
    btn.disabled = false;
    btn.style.opacity = "1";
  } else {
    label.textContent = "Choose File";
    btn.disabled = true;
    btn.style.opacity = "0.5";
  }
  document.getElementById("uploadStatus").textContent = "";
}

async function uploadFile() {
  if (!_pendingFile) return;

  const btn = document.getElementById("uploadBtn");
  const status = document.getElementById("uploadStatus");

  btn.disabled = true;
  btn.style.opacity = "0.5";
  status.textContent = "Uploading...";

  try {
    const path = "/" + _pendingFile.name;
    const res = await fetch("/upload?path=" + encodeURIComponent(path), {
      method: "POST",
      headers: {
        "Content-Type": "application/octet-stream",
      },
      body: _pendingFile,
    });

    if (res.ok) {
      const msg = await res.text();
      status.textContent = msg;
      _pendingFile = null;
      document.getElementById("fileInput").value = "";
      document.getElementById("fileLabel").textContent = "Choose File";
    } else {
      status.textContent = "Upload failed (" + res.status + ")";
      btn.disabled = false;
      btn.style.opacity = "1";
    }
  } catch (e) {
    status.textContent = "Error: " + e.message;
    btn.disabled = false;
    btn.style.opacity = "1";
  }
}

/** === Logging Functionality === */

let isFetchingLogs = false;
async function updateLogs() {
  if (isFetchingLogs) return;

  isFetchingLogs = true;
  try {
    let res = await fetch("/logs");
    if (res.ok) {
      let text = await res.text();
      document.getElementById("container").innerHTML = text.replace(
        /(?:\r\n|\r|\n)/g,
        "<br>",
      );
      let container = document.getElementById("container");
      container.scrollTop = container.scrollHeight; // auto-scroll
    }
  } catch (e) {
    console.error("Log fetch failed:", e);
  } finally {
    isFetchingLogs = false;
  }
}

function initLogs() {
  setInterval(updateLogs, 3000); // every 3s
  updateLogs(); // initial load
}

/** === Super Simple Modals lol === */

document.addEventListener("click", (e) => {
  //  Modal Behavior
  if (e.target.matches("[data-modal-open]")) {
    const modal = document.querySelector(
      e.target.getAttribute("data-modal-open"),
    );
    modal?.classList.add("active");
  }
  if (e.target.matches("[data-modal-close], .modal")) {
    e.target.closest(".modal")?.classList.remove("active");
  }
});
