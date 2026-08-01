const NATIVE_HOST = "com.remoodownload.native_host";
const DEFAULT_OPTIONS = {
  enabled: true,
  minSizeBytes: 10 * 1024 * 1024,
  extensions: ["zip", "7z", "rar", "exe", "msi", "mp4", "mkv", "mp3", "pdf"],
  autoLaunchGui: false
};

// Persistent connection management
let nativePort = null;
let connectionAttempts = 0;
const MAX_RECONNECT_ATTEMPTS = 3;

// Badge and status tracking
let activeDownloads = 0;
let lastDownloadInfo = null;
let statusPollingInterval = null;

// Update badge with active download count
function updateBadge(count) {
  activeDownloads = count;
  if (count > 0) {
    chrome.action.setBadgeText({ text: count.toString() });
    chrome.action.setBadgeBackgroundColor({ color: "#2196F3" }); // Blue
  } else {
    chrome.action.setBadgeText({ text: "" });
  }
}

// Poll service for download status
async function pollDownloadStatus() {
  const port = connectToNativeHost();
  if (!port) {
    console.log("[Remoo] Cannot poll status: not connected to native host");
    return;
  }

  try {
    // Request status from service
    const message = {
      type: "getStatus",
      source: "chromium"
    };

    port.postMessage(message);

    // Note: Response will be handled in onMessage listener
    // We'll update badge and lastDownloadInfo there
  } catch (error) {
    console.error("[Remoo] Error polling status:", error);
  }
}

// Start status polling (every 2 seconds)
function startStatusPolling() {
  if (statusPollingInterval) {
    return; // Already polling
  }

  console.log("[Remoo] Starting status polling");
  statusPollingInterval = setInterval(pollDownloadStatus, 2000);
  pollDownloadStatus(); // Poll immediately
}

// Stop status polling
function stopStatusPolling() {
  if (statusPollingInterval) {
    console.log("[Remoo] Stopping status polling");
    clearInterval(statusPollingInterval);
    statusPollingInterval = null;
  }
}

// Get current options from storage
async function getOptions() {
  const stored = await chrome.storage.sync.get(DEFAULT_OPTIONS);
  return { ...DEFAULT_OPTIONS, ...stored };
}

// Extract file extension from URL
function extensionFromUrl(url) {
  try {
    const pathname = new URL(url).pathname;
    const filename = pathname.split("/").pop() || "";
    const dot = filename.lastIndexOf(".");
    return dot >= 0 ? filename.slice(dot + 1).toLowerCase() : "";
  } catch {
    return "";
  }
}

// Extract filename from URL or Content-Disposition header
function extractFilename(url, contentDisposition = "") {
  // Try Content-Disposition header first
  if (contentDisposition) {
    const filenameMatch = contentDisposition.match(/filename[^;=\n]*=((['"]).*?\2|[^;\n]*)/);
    if (filenameMatch && filenameMatch[1]) {
      let filename = filenameMatch[1].replace(/['"]/g, '');
      // Decode if URL-encoded
      try {
        filename = decodeURIComponent(filename);
      } catch (e) {
        // Keep original if decode fails
      }
      return filename;
    }
  }

  // Fallback to URL pathname
  try {
    const pathname = new URL(url).pathname;
    return pathname.split("/").pop() || "";
  } catch {
    return "";
  }
}

// Check if a download should be intercepted based on filters
async function shouldIntercept(item) {
  const options = await getOptions();

  // Debug logging
  console.log("[Remoo Debug] shouldIntercept called:", {
    url: item.url,
    filename: item.filename,
    fileSize: item.fileSize,
    mime: item.mime,
    enabled: options.enabled,
    minSizeBytes: options.minSizeBytes,
    extensionFilter: options.extensions
  });

  // Check if interception is enabled
  if (!options.enabled) {
    console.log("[Remoo Debug] Interception disabled in settings");
    return false;
  }

  // Must have a valid URL
  if (!item.url || item.url.startsWith("data:") || item.url.startsWith("blob:")) {
    console.log("[Remoo Debug] Invalid URL type:", item.url);
    return false;
  }

  // Check file size if available AND greater than 0
  // Note: fileSize is often 0 or -1 in onCreated, so we skip size check if unknown
  if (item.fileSize && item.fileSize > 0) {
    if (item.fileSize < options.minSizeBytes) {
      console.log("[Remoo Debug] File too small:", item.fileSize, "< minimum:", options.minSizeBytes);
      return false;
    } else {
      console.log("[Remoo Debug] File size OK:", item.fileSize, ">=", options.minSizeBytes);
    }
  } else {
    console.log("[Remoo Debug] File size unknown (0 or -1), will check extension only");
  }

  // Check file extension
  const ext = extensionFromUrl(item.url);
  console.log("[Remoo Debug] File extension:", ext);

  // If no extensions filter configured, intercept all (except very small files)
  if (!options.extensions || options.extensions.length === 0) {
    console.log("[Remoo Debug] No extension filter, intercepting");
    return true;
  }

  // If extension is in the filter list, intercept
  const shouldInterceptByExt = options.extensions.includes(ext);
  console.log("[Remoo Debug] Extension check result:", shouldInterceptByExt, "- extension list:", options.extensions);

  return shouldInterceptByExt;
}

// Connect to native host with reconnection logic
function connectToNativeHost() {
  if (nativePort && nativePort.disconnect) {
    // Already connected or connecting
    return nativePort;
  }

  try {
    nativePort = chrome.runtime.connectNative(NATIVE_HOST);
    connectionAttempts = 0;

    nativePort.onMessage.addListener((response) => {
      console.log("[Remoo] Native host response:", response);

      // Handle status response
      if (response && response.type === "status") {
        const active = response.activeDownloads || 0;
        updateBadge(active);

        if (response.downloads && response.downloads.length > 0) {
          lastDownloadInfo = response.downloads[0]; // Store most recent download

          // Store in chrome.storage for popup access
          chrome.storage.local.set({
            lastDownload: lastDownloadInfo,
            activeDownloads: active
          });
        }
        return;
      }

      if (response && response.ok) {
        // Show success notification
        chrome.notifications.create({
          type: "basic",
          iconUrl: "icon48.png",
          title: "Download Started",
          message: "Remoo Download is handling this file."
        });

        // Start polling to update badge
        startStatusPolling();
      } else if (response && response.error) {
        console.error("[Remoo] Native host error:", response.error);

        // Show error notification for service errors
        if (response.error === "service_not_running") {
          chrome.notifications.create({
            type: "basic",
            iconUrl: "icon48.png",
            title: "Service Not Running",
            message: "Please start remo_service to handle downloads."
          });
        }
      }
    });

    nativePort.onDisconnect.addListener(() => {
      const error = chrome.runtime.lastError;
      if (error) {
        console.error("[Remoo] Native host disconnected:", error.message);

        // Handle specific errors
        if (error.message.includes("Specified native messaging host not found")) {
          console.error("[Remoo] Native host not registered. Run register_native_host.ps1");
        } else if (error.message.includes("Native host has exited")) {
          console.error("[Remoo] Native host exited unexpectedly");
        }

        // Attempt reconnection with exponential backoff
        if (connectionAttempts < MAX_RECONNECT_ATTEMPTS) {
          connectionAttempts++;
          const delay = Math.pow(2, connectionAttempts) * 1000; // 2s, 4s, 8s
          console.log(`[Remoo] Attempting reconnection in ${delay}ms (attempt ${connectionAttempts}/${MAX_RECONNECT_ATTEMPTS})`);

          setTimeout(() => {
            nativePort = null;
            connectToNativeHost();
          }, delay);
        }
      }

      nativePort = null;
    });

    console.log("[Remoo] Connected to native host");
    return nativePort;

  } catch (error) {
    console.error("[Remoo] Failed to connect to native host:", error);
    nativePort = null;
    return null;
  }
}

// Send download to native host
function sendToNativeHost(payload) {
  const port = connectToNativeHost();

  if (!port) {
    console.error("[Remoo] Cannot send to native host: not connected");
    return Promise.reject(new Error("Not connected to native host"));
  }

  try {
    const message = {
      type: "addDownload",
      source: "chromium",
      version: chrome.runtime.getManifest().version,
      timestamp: new Date().toISOString(),
      ...payload
    };

    console.log("[Remoo] Sending to native host:", message);
    port.postMessage(message);
    return Promise.resolve();
  } catch (error) {
    console.error("[Remoo] Error sending to native host:", error);
    return Promise.reject(error);
  }
}

// Handle context menu click
chrome.contextMenus.onClicked.addListener((info, tab) => {
  if (info.menuItemId === "remo-download-link") {
    const url = info.linkUrl || info.srcUrl;
    if (url) {
      sendToNativeHost({
        url,
        referrerUrl: info.pageUrl || "",
        filename: extractFilename(url)
      }).catch((error) => {
        console.error("[Remoo] Failed to send from context menu:", error);
      });
    }
  }
});

// Handle downloads created by the browser
chrome.downloads.onCreated.addListener(async (item) => {
  console.log("[Remoo Debug] downloads.onCreated fired:", {
    id: item.id,
    url: item.url,
    filename: item.filename,
    fileSize: item.fileSize,
    state: item.state,
    mime: item.mime
  });

  if (await shouldIntercept(item)) {
    console.log("[Remoo] ✓ INTERCEPTING download:", item.url);

    // Cancel the browser's download
    try {
      await chrome.downloads.cancel(item.id);
      await chrome.downloads.erase({ id: item.id });
      console.log("[Remoo] ✓ Browser download cancelled and erased");
    } catch (error) {
      console.warn("[Remoo] Could not cancel browser download:", error);
    }

    // Send to Remoo Download service
    sendToNativeHost({
      url: item.url,
      filename: item.filename || extractFilename(item.url),
      fileSize: item.fileSize || 0,
      referrerUrl: item.referrer || "",
      mime: item.mime || ""
    }).catch((error) => {
      console.error("[Remoo] Failed to send intercepted download:", error);

      // If sending fails, show notification
      chrome.notifications.create({
        type: "basic",
        iconUrl: "icon48.png",
        title: "Download Failed",
        message: "Could not send download to Remoo Download service."
      });
    });
  } else {
    console.log("[Remoo] ✗ NOT intercepting download (failed filter check):", item.url);
  }
});

// Create context menu on installation
chrome.runtime.onInstalled.addListener(() => {
  chrome.contextMenus.create({
    id: "remo-download-link",
    title: "Download with Remoo Download",
    contexts: ["link", "video", "audio"]
  });

  console.log("[Remoo] Extension installed and context menu created");

  // Start polling on installation
  startStatusPolling();
});

// Initialize connection on startup
chrome.runtime.onStartup.addListener(() => {
  console.log("[Remoo] Extension started, connecting to native host");
  connectToNativeHost();
  startStatusPolling();
});

// Handle messages from popup
chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  if (request.action === "testConnection") {
    const port = connectToNativeHost();
    if (port) {
      sendResponse({ success: true });
    } else {
      sendResponse({ success: false, error: "Could not connect" });
    }
    return true;
  }

  if (request.action === "getLastDownload") {
    chrome.storage.local.get(["lastDownload", "activeDownloads"], (result) => {
      sendResponse({
        lastDownload: result.lastDownload || null,
        activeDownloads: result.activeDownloads || 0
      });
    });
    return true;
  }

  if (request.action === "openFolder") {
    const port = connectToNativeHost();
    if (port && request.path) {
      port.postMessage({
        type: "openFolder",
        path: request.path
      });
      sendResponse({ success: true });
    } else {
      sendResponse({ success: false, error: "Invalid path or not connected" });
    }
    return true;
  }
});

// Try to connect on load
connectToNativeHost();
startStatusPolling();
