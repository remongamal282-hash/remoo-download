const DEFAULT_OPTIONS = {
  enabled: true,
  minSizeBytes: 10 * 1024 * 1024,
  extensions: ["zip", "7z", "rar", "exe", "msi", "mp4", "mkv", "mp3", "pdf"]
};

async function loadOptions() {
  const options = { ...DEFAULT_OPTIONS, ...(await chrome.storage.sync.get(DEFAULT_OPTIONS)) };
  document.querySelector("#enabled").checked = options.enabled;
  document.querySelector("#minSizeMb").value = Math.round(options.minSizeBytes / 1024 / 1024);
  document.querySelector("#extensions").value = options.extensions.join(",");
}

async function saveOptions() {
  const extensions = document.querySelector("#extensions").value
    .split(",")
    .map((value) => value.trim().replace(/^\./, "").toLowerCase())
    .filter(Boolean);
  await chrome.storage.sync.set({
    enabled: document.querySelector("#enabled").checked,
    minSizeBytes: Number(document.querySelector("#minSizeMb").value || 0) * 1024 * 1024,
    extensions
  });
  window.close();
}

document.querySelector("#save").addEventListener("click", saveOptions);
loadOptions();
