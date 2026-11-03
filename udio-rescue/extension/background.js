/*
 * Udio Rescue — the saver. Chrome runs this in the background.
 *
 * It saves files with Chrome's own downloader into Downloads/Udio Rescue/,
 * waits for each one to finish, and reports back whether it worked.
 */
"use strict";

const FOLDER = "Udio Rescue";
const WAIT_LIMIT_MS = 10 * 60 * 1000;

// A file name may only go inside our folder: no "..", no absolute paths.
function cleanRelative(name) {
  return String(name || "file")
    .split(/[\\/]+/)
    .filter((part) => part && part !== "." && part !== "..")
    .join("/") || "file";
}

function waitForDownload(id) {
  return new Promise((resolve) => {
    let done = false;
    // Chrome may adjust a name (e.g. to match the file type): report the real one.
    const finishComplete = () => {
      chrome.downloads.search({ id }).then((items) => {
        const full = (items[0] && items[0].filename) || "";
        finish({ ok: true, file: full.split(/[\\/]/).pop() });
      }).catch(() => finish({ ok: true }));
    };
    const finish = (result) => {
      if (done) return;
      done = true;
      chrome.downloads.onChanged.removeListener(onChanged);
      clearTimeout(timer);
      resolve(result);
    };
    const onChanged = (delta) => {
      if (delta.id !== id || !delta.state) return;
      if (delta.state.current === "complete") finishComplete();
      else if (delta.state.current === "interrupted") {
        chrome.downloads.search({ id }).then((items) => {
          finish({ ok: false, error: (items[0] && items[0].error) || "interrupted" });
        }).catch(() => finish({ ok: false, error: "interrupted" }));
      }
    };
    chrome.downloads.onChanged.addListener(onChanged);
    const timer = setTimeout(() => finish({ ok: false, error: "took too long" }), WAIT_LIMIT_MS);
    // It may already have finished before we started listening.
    chrome.downloads.search({ id }).then((items) => {
      const item = items[0];
      if (!item) return;
      if (item.state === "complete") finishComplete();
      else if (item.state === "interrupted") finish({ ok: false, error: item.error || "interrupted" });
    }).catch(() => {});
  });
}

async function save(url, filename, overwrite) {
  try {
    const id = await chrome.downloads.download({
      url,
      filename: FOLDER + "/" + cleanRelative(filename),
      conflictAction: overwrite ? "overwrite" : "uniquify",
      saveAs: false,
    });
    if (typeof id !== "number") return { ok: false, error: "Chrome refused the download" };
    return await waitForDownload(id);
  } catch (e) {
    return { ok: false, error: String((e && e.message) || e) };
  }
}

chrome.runtime.onMessage.addListener((msg, _sender, sendResponse) => {
  if (!msg || msg.to !== "udio-rescue-bg") return false;
  if (msg.type === "save") {
    save(msg.url, msg.filename, !!msg.overwrite).then(sendResponse);
    return true; // answer arrives later
  }
  if (msg.type === "open-folder") {
    chrome.downloads.showDefaultFolder();
    sendResponse({ ok: true });
    return false;
  }
  return false;
});

// Clicking the toolbar button: show the panel on Udio, or open Udio.
chrome.action.onClicked.addListener((tab) => {
  const onUdio = tab && tab.url && /^https:\/\/([a-z0-9-]+\.)*udio\.com\//i.test(tab.url);
  if (onUdio) {
    chrome.tabs.sendMessage(tab.id, { to: "udio-rescue-panel", type: "show" }).catch(() => {});
  } else {
    chrome.tabs.create({ url: "https://www.udio.com/" });
  }
});
