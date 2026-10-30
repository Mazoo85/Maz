// Macs on Apple chips refuse to run an app with no signature at all, and this
// one has no paid Apple developer certificate. An ad-hoc signature is enough
// for it to run once it has been allowed in Privacy & Security.
const { execFileSync } = require('child_process');
const path = require('path');

exports.default = async function (context) {
  if (context.electronPlatformName !== 'darwin') return;
  const app = path.join(context.appOutDir, context.packager.appInfo.productFilename + '.app');
  execFileSync('codesign', ['--force', '--deep', '--sign', '-', app], { stdio: 'inherit' });
};
