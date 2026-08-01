const NATIVE_HOST = "com.remoodownload.native_host";

const DEFAULT_OPTIONS = {
  enabled: true,
  minSizeBytes: 10 * 1024 * 1024,
  extensions: ["zip", "7z", "rar", "exe", "msi", "mp4", "mkv", "mp3", "pdf"],
  autoLaunchGui: false
};

// Update last download section
function updateLastDownload(downloadInfo, activeCount) {
  const section = document.querySelector("#lastDownloadSection");
  const nameEl = document.querySelector("#lastDownloadName");
  const statusEl = document.querySelector("#lastDownloadStatus");
  const progressEl = document.querySelector("#lastDownloadProgress");
  const progressBar = document.querySelector("#lastDownloadProgressBar");

  if (!downloadInfo || activeCount === 0) {
    section.classList.add("hidden");
    return;
  }

  section.classList.remove("hidden");

  // Update filename
  nameEl.textContent = downloadInfo.filename || downloadInfo.url?.split("/").pop() || "Unknown";
  nameEl.title = downloadInfo.filename || downloadInfo.url || "";

  // Update status
  const status = downloadInfo.status || downloadInfo.state || "unknown";
  statusEl.textContent = status.charAt(0).toUpperCase() + status.slice(1);
  statusEl.className = `status-text ${status.toLowerCase()}`;

  // Update progress
  const progress = downloadInfo.progress || 0;
  progressEl.textContent = `${progress}%`;
  progressBar.style.width = `${progress}%`;

  // Store path for "Open Folder" button
  if (downloadInfo.savePath) {
    document.querySelector("#openFolder").dataset.path = downloadInfo.savePath;
  }
}

// Load last download info
async function loadLastDownload() {
  try {
    const response = await chrome.runtime.sendMessage({ action: "getLastDownload" });
    if (response) {
      updateLastDownload(response.lastDownload, response.activeDownloads || 0);
    }
  } catch (error) {
    console.error("Failed to load last download:", error);
  }
}

// Update connection status UI
function updateConnectionStatus(connected, message = "") {
  const statusBadge = document.querySelector("#connectionStatus");
  const statusText = document.querySelector("#statusText");

  statusBadge.classList.remove("status-connected", "status-disconnected", "status-unknown");

  if (connected === true) {
    statusBadge.classList.add("status-connected");
    statusText.textContent = "Connected";
  } else if (connected === false) {
    statusBadge.classList.add("status-disconnected");
    statusText.textContent = message || "Disconnected";
  } else {
    statusBadge.classList.add("status-unknown");
    statusText.textContent = message || "Unknown";
  }
}

// Show message box
function showMessage(text, type = "info") {
  const messageBox = document.querySelector("#messageBox");
  messageBox.textContent = text;
  messageBox.className = `message-box ${type}`;

  setTimeout(() => {
    messageBox.classList.add("hidden");
  }, 4000);
}

// Test connection to native host
async function testConnection() {
  const testButton = document.querySelector("#testConnection");
  testButton.disabled = true;
  testButton.textContent = "Testing...";

  updateConnectionStatus(null, "Testing...");

  try {
    const port = chrome.runtime.connectNative(NATIVE_HOST);

    let responseReceived = false;
    const timeout = setTimeout(() => {
      if (!responseReceived) {
        port.disconnect();
        updateConnectionStatus(false, "Timeout");
        showMessage("Connection timeout. Make sure remo_service is running.", "error");
        testButton.disabled = false;
        testButton.textContent = "Test Connection";
      }
    }, 5000);

    port.onMessage.addListener((response) => {
      responseReceived = true;
      clearTimeout(timeout);
      port.disconnect();

      if (response && response.ok) {
        updateConnectionStatus(true);
        showMessage("Successfully connected to Remoo Download service!", "success");
      } else {
        updateConnectionStatus(false, "Service Error");
        const error = response?.error || "Unknown error";
        showMessage(`Service returned error: ${error}`, "error");
      }

      testButton.disabled = false;
      testButton.textContent = "Test Connection";
    });

    port.onDisconnect.addListener(() => {
      if (!responseReceived) {
        clearTimeout(timeout);
        const error = chrome.runtime.lastError;

        if (error) {
          updateConnectionStatus(false, "Not Found");

          if (error.message.includes("Specified native messaging host not found")) {
            showMessage("Native host not registered. Please run register_native_host.ps1", "error");
          } else if (error.message.includes("Native host has exited")) {
            showMessage("Native host exited. Check that remo_service is running.", "error");
          } else {
            showMessage(`Connection failed: ${error.message}`, "error");
          }
        }

        testButton.disabled = false;
        testButton.textContent = "Test Connection";
      }
    });

    // Send a test ping message
    port.postMessage({
      type: "ping",
      url: "https://example.com/test.zip"
    });

  } catch (error) {
    updateConnectionStatus(false, "Error");
    showMessage(`Unexpected error: ${error.message}`, "error");
    testButton.disabled = false;
    testButton.textContent = "Test Connection";
  }
}

// Load options from storage
async function loadOptions() {
  const options = { ...DEFAULT_OPTIONS, ...(await chrome.storage.sync.get(DEFAULT_OPTIONS)) };
  document.querySelector("#enabled").checked = options.enabled;
  document.querySelector("#autoLaunchGui").checked = options.autoLaunchGui || false;
  document.querySelector("#minSizeMb").value = Math.round(options.minSizeBytes / 1024 / 1024);
  document.querySelector("#extensions").value = options.extensions.join(", ");
}

// Save options to storage
async function saveOptions() {
  const saveButton = document.querySelector("#save");
  saveButton.disabled = true;
  saveButton.textContent = "Saving...";

  const extensionsInput = document.querySelector("#extensions").value;
  const extensions = extensionsInput
    .split(",")
    .map((value) => value.trim().replace(/^\./, "").toLowerCase())
    .filter(Boolean);

  const options = {
    enabled: document.querySelector("#enabled").checked,
    autoLaunchGui: document.querySelector("#autoLaunchGui").checked,
    minSizeBytes: Number(document.querySelector("#minSizeMb").value || 0) * 1024 * 1024,
    extensions
  };

  await chrome.storage.sync.set(options);

  showMessage("Settings saved successfully!", "success");

  saveButton.disabled = false;
  saveButton.textContent = "Save Settings";

  // Update the display to show cleaned extensions
  document.querySelector("#extensions").value = extensions.join(", ");
}

// Open folder containing download
async function openFolder() {
  const path = document.querySelector("#openFolder").dataset.path;
  if (!path) {
    showMessage("No download path available", "error");
    return;
  }

  try {
    await chrome.runtime.sendMessage({ action: "openFolder", path });
    showMessage("Opening folder...", "info");
  } catch (error) {
    showMessage("Failed to open folder", "error");
    console.error(error);
  }
}

// Event listeners
document.querySelector("#save").addEventListener("click", saveOptions);
document.querySelector("#testConnection").addEventListener("click", testConnection);
document.querySelector("#openFolder").addEventListener("click", openFolder);

// Initialize
loadOptions();
checkInitialConnection();
loadLastDownload();

// Refresh last download info every 3 seconds
setInterval(loadLastDownload, 3000);
