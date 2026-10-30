// Ladder 24 as a desktop program: one window running the same synthesizer as
// the web page (../index.html), copied into web/ when the app is built.
const { app, BrowserWindow, protocol, net, session, shell } = require('electron');
const path = require('path');
const { pathToFileURL } = require('url');

// The page is served from ladder://app/ rather than file://, so it has a real
// origin: the sound engine's audio thread (AudioWorklet) only starts from one,
// and saved patches stay put between runs.
protocol.registerSchemesAsPrivileged([
  { scheme: 'ladder', privileges: { standard: true, secure: true, supportFetchAPI: true } },
]);

const WEB = path.join(__dirname, 'web');

function createWindow() {
  const win = new BrowserWindow({
    width: 1440,
    height: 960,
    minWidth: 380,
    minHeight: 560,
    title: 'Ladder 24',
    backgroundColor: '#120e0b',
    autoHideMenuBar: true,
    webPreferences: { contextIsolation: true, sandbox: true, backgroundThrottling: false },
  });
  win.loadURL('ladder://app/index.html');
  // Links to websites open in the normal browser, never inside the synth.
  win.webContents.setWindowOpenHandler(({ url }) => {
    if (url.startsWith('https://')) shell.openExternal(url);
    return { action: 'deny' };
  });
}

// One copy at a time: opening it again brings the existing window forward.
if (!app.requestSingleInstanceLock()) {
  app.quit();
} else {
  app.on('second-instance', () => {
    const [win] = BrowserWindow.getAllWindows();
    if (win) {
      if (win.isMinimized()) win.restore();
      win.focus();
    }
  });

  app.whenReady().then(() => {
    protocol.handle('ladder', (request) => {
      const file = path.normalize(path.join(WEB, decodeURIComponent(new URL(request.url).pathname)));
      if (!file.startsWith(WEB + path.sep)) return new Response('Not found', { status: 404 });
      return net.fetch(pathToFileURL(file).toString());
    });
    // MIDI keyboards: allowed without asking, since that is what the program is for.
    const midi = (permission) => permission === 'midi' || permission === 'midiSysex';
    session.defaultSession.setPermissionRequestHandler((wc, permission, done) => done(midi(permission)));
    session.defaultSession.setPermissionCheckHandler((wc, permission) => midi(permission));
    createWindow();
    app.on('activate', () => {
      if (BrowserWindow.getAllWindows().length === 0) createWindow();
    });
  });

  app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') app.quit();
  });
}
