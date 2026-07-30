const NATIVE_HOST = "com.remodownload.native_host";
const DEFAULT_OPTIONS = {
  enabled: true,
  minSizeBytes: 10 * 1024 * 1024,
  extensions: ["zip", "7z", "rar", "exe", "msi", "mp4", "mkv", "mp3", "pdf"]
};

async function getOptions() {
  const stored = await chrome.storage.sync.get(DEFAULT_OPTIONS);
  return { ...DEFAULT_OPTIONS, ...stored };
}

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

async function shouldIntercept(item) {
  const options = await getOptions();
  if (!options.enabled || !item.url) {
    return false;
  }
  if (item.fileSize && item.fileSize > 0 && item.fileSize < options.minSizeBytes) {
    return false;
  }
  const ext = extensionFromUrl(item.url);
  return !options.extensions.length || options.extensions.includes(ext);
}

function sendToNativeHost(payload) {
  return chrome.runtime.sendNativeMessage(NATIVE_HOST, {
    type: "addDownload",
    source: "chromium",
    version: chrome.runtime.getManifest().version,
    ...payload
  });
}

chrome.runtime.onInstalled.addListener(() => {
  chrome.contextMenus.create({
    id: "remo-download-link",
    title: "Download with Remo Download",
    contexts: ["link", "video", "audio"]
  });
});

chrome.contextMenus.onClicked.addListener((info) => {
  const url = info.linkUrl || info.srcUrl;
  if (url) {
    sendToNativeHost({ url, referrerUrl: info.pageUrl || "" }).catch(() => {});
  }
});

chrome.downloads.onCreated.addListener(async (item) => {
  if (await shouldIntercept(item)) {
    sendToNativeHost({
      url: item.url,
      filename: item.filename || "",
      fileSize: item.fileSize || 0,
      referrerUrl: item.referrer || ""
    }).catch(() => {});
  }
});
