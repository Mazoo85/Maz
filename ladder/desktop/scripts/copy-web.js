// Copies the synthesizer page (ladder/index.html, css/, js/) into web/ for the desktop build.
const fs = require('fs');
const path = require('path');

const from = path.join(__dirname, '..', '..');
const to = path.join(__dirname, '..', 'web');
fs.rmSync(to, { recursive: true, force: true });
fs.mkdirSync(to, { recursive: true });
fs.copyFileSync(path.join(from, 'index.html'), path.join(to, 'index.html'));
for (const dir of ['css', 'js']) fs.cpSync(path.join(from, dir), path.join(to, dir), { recursive: true });
console.log('copied the page into', to);
