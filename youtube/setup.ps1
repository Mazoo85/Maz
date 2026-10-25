# YouTube for Claude - one-step setup for Windows.
#
# Puts the YouTube extension on your free Cloudflare account and gives you the
# line to paste into claude.ai. You only click "Allow" once, in the browser.
#
# Run it by pasting this into PowerShell (Start menu -> type PowerShell -> Enter),
# (it asks for your YouTube key; or put it in place of AIza... to skip the question):
#
#   irm https://raw.githubusercontent.com/Mazoo85/Maz/claude/youtube-extension/youtube/setup.ps1 | iex
#   $env:YT_KEY='AIza...'; irm https://raw.githubusercontent.com/Mazoo85/Maz/claude/youtube-extension/youtube/setup.ps1 | iex
#
# What it does, in order:
#   1. installs Node.js if it is missing (Windows asks you to allow it once);
#   2. downloads the server (worker.js) and its Cloudflare settings;
#   3. opens Cloudflare in your browser so you can press "Allow";
#   4. deploys the server as "youtube-for-claude";
#   5. stores your YouTube key and a new private password on Cloudflare;
#   6. checks the server answers, copies the connector line to your clipboard
#      and opens the claude.ai Connectors page.
# Nothing secret is written to disk except Cloudflare's own sign-in file.

# 'Continue', not 'Stop': Windows PowerShell 5 turns a tool's progress messages
# on stderr into fatal errors under 'Stop'. Every step checks its own result.
$ErrorActionPreference = 'Continue'
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
$env:WRANGLER_SEND_METRICS = 'false'

$Branch = 'claude/youtube-extension'
$Raw = "https://raw.githubusercontent.com/Mazoo85/Maz/$Branch/youtube"
$Wrangler = 'wrangler@4'

function Say($text) { Write-Host ''; Write-Host "==> $text" -ForegroundColor Cyan }
function Stop-Setup($text) {
  Write-Host ''
  Write-Host "Setup stopped: $text" -ForegroundColor Red
  Write-Host 'Copy everything above into your Claude chat and it will tell you what to do.'
  throw 'setup stopped'
}

function Update-PathFromSystem {
  $machine = [Environment]::GetEnvironmentVariable('Path', 'Machine')
  $user = [Environment]::GetEnvironmentVariable('Path', 'User')
  $env:Path = "$machine;$user"
}

function Invoke-Wrangler {
  # npx.cmd, not npx: PowerShell often blocks npx.ps1 ("running scripts is disabled").
  & npx.cmd --yes $Wrangler @args
}

function Set-WranglerSecret($name, $value) {
  # Piped, so the value never shows on screen or lands in a file.
  $value | & npx.cmd --yes $Wrangler secret put $name
}

function Get-NodeMajor {
  if (-not (Get-Command node -ErrorAction SilentlyContinue)) { return 0 }
  $v = "$(& node --version)"
  if ($v -match '^v(\d+)') { return [int]$Matches[1] }
  return 0
}

# ---------------------------------------------------------------- update?
# A finished setup leaves "YouTube connector address.txt" on the Desktop. If it
# is there, this run only updates the server: the key and password already on
# Cloudflare stay, so the connector in claude.ai keeps working unchanged.
$desktop = [Environment]::GetFolderPath('Desktop')
if (-not $desktop -or -not (Test-Path $desktop)) { $desktop = $HOME }
$note = Join-Path $desktop 'YouTube connector address.txt'
$updating = $false
if (Test-Path $note) {
  $saved = [regex]::Match((Get-Content -Raw -Path $note), 'https://[A-Za-z0-9.-]+\.workers\.dev/mcp/(mazyt[a-z0-9]+)')
  if ($saved.Success) {
    $updating = $true
    $password = $saved.Groups[1].Value
    Write-Host 'Found your earlier setup on the Desktop, so this run just updates the server.' -ForegroundColor Green
    Write-Host 'Your key, password and claude.ai connector stay exactly as they are.'
  }
}

# ---------------------------------------------------------------- the key
if (-not $updating) {
# Some apps show a copied key as dots (AIzaSy•••), so a key that is not real
# letters and digits is asked for again rather than stopping the setup.
function Test-YouTubeKey($k) { return "$k" -match '^AIza[0-9A-Za-z_-]{30,}$' }
$key = "$env:YT_KEY".Trim()
$env:YT_KEY = $null
if ($key -and -not (Test-YouTubeKey $key)) {
  Write-Host 'The key on that line was not the real key (it may have been copied as dots).' -ForegroundColor Yellow
}
for ($try = 1; -not (Test-YouTubeKey $key); $try++) {
  if ($try -gt 3) { Stop-Setup 'that still does not look like a YouTube API key. It starts with AIza and has about 39 letters and digits.' }
  Write-Host 'Copy your key from https://console.cloud.google.com/apis/credentials (click Show key), then'
  $key = "$(Read-Host 'right-click here to paste it, and press Enter')".Trim()
}
}

# ---------------------------------------------------------------- Node.js
Say 'Checking for Node.js (Cloudflare''s setup tool needs it)'
$major = Get-NodeMajor
if ($major -gt 0 -and $major -lt 20) {
  Start-Process 'https://nodejs.org/en/download'
  Stop-Setup "this PC has an old Node.js ($(& node --version)). Install the LTS version from the page that just opened, then run the same line again."
}
if ($major -eq 0) {
  if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Start-Process 'https://nodejs.org/en/download'
    Stop-Setup 'Node.js is missing and this PC has no winget to install it. Install Node.js (LTS) from the page that just opened, then run the same line again.'
  }
  Write-Host 'Installing Node.js. If Windows asks whether to allow changes, click Yes.'
  winget install -e --id OpenJS.NodeJS.LTS --accept-source-agreements --accept-package-agreements --silent
  Update-PathFromSystem
  if ((Get-NodeMajor) -eq 0) {
    Stop-Setup 'Node.js was installed but this window cannot see it yet. Close PowerShell, open a new one, and run the same line again.'
  }
}
Write-Host ("Node.js " + (& node --version))

# ---------------------------------------------------------------- the files
Say 'Downloading the YouTube server'
$dir = Join-Path $env:TEMP 'youtube-for-claude'
New-Item -ItemType Directory -Force -Path $dir | Out-Null
foreach ($f in 'worker.js', 'wrangler.jsonc') {
  try {
    Invoke-WebRequest -UseBasicParsing -Uri "$Raw/$f" -OutFile (Join-Path $dir $f) -ErrorAction Stop
  } catch {
    Stop-Setup "could not download $f from GitHub: $($_.Exception.Message)"
  }
}
Set-Location $dir

# ---------------------------------------------------------------- sign in
Say 'Signing in to Cloudflare'
$who = (Invoke-Wrangler whoami 2>&1 | ForEach-Object { "$_" }) -join "`n"
if ($who -match 'You are logged in') {
  Write-Host 'Already signed in to Cloudflare. Skipping this step.'
} else {
  Write-Host ''
  Write-Host '  >>> A Cloudflare tab is opening in your browser.' -ForegroundColor Yellow
  Write-Host '  >>> Sign in if asked, click ALLOW, then come back to this window.' -ForegroundColor Yellow
  Write-Host ''
  Invoke-Wrangler login
  if ($LASTEXITCODE -ne 0) { Stop-Setup 'the Cloudflare sign-in did not finish.' }
}

# ---------------------------------------------------------------- deploy
Say 'Putting the server on Cloudflare'
$out = (Invoke-Wrangler deploy 2>&1 | ForEach-Object { "$_" }) -join "`n"
Write-Host $out
if ($LASTEXITCODE -ne 0) { Stop-Setup 'Cloudflare did not accept the server.' }
$m = [regex]::Match($out, 'https://[A-Za-z0-9.-]+\.workers\.dev')
if (-not $m.Success) { Stop-Setup 'the server deployed but Cloudflare did not say its address.' }
$url = $m.Value

# ---------------------------------------------------------------- secrets
if ($updating) {
  Say 'Keeping your YouTube key and password as they are'
} else {
Say 'Storing your YouTube key and a private password on Cloudflare'
$chars = [char[]]'abcdefghijkmnopqrstuvwxyz23456789'
$rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
$bytes = New-Object byte[] 24
$rng.GetBytes($bytes)
$password = 'mazyt' + (-join ($bytes | ForEach-Object { $chars[$_ % $chars.Length] }))

Set-WranglerSecret 'YOUTUBE_API_KEY' $key
if ($LASTEXITCODE -ne 0) { Stop-Setup 'Cloudflare did not store the YouTube key.' }
Set-WranglerSecret 'CONNECTOR_PASSWORD' $password
if ($LASTEXITCODE -ne 0) { Stop-Setup 'Cloudflare did not store the password.' }
}

# ---------------------------------------------------------------- check
Say "Checking $url/check (a brand-new address can take a minute to appear)"
$check = $null
for ($i = 0; $i -lt 18 -and -not ($check -and $check.youtube.ok -and $check.password_set); $i++) {
  Start-Sleep -Seconds 5
  try { $check = Invoke-RestMethod -UseBasicParsing -Uri "$url/check" -TimeoutSec 20 } catch { $check = $null }
}
if (-not $check) { Stop-Setup "the server did not answer at $url/check after 90 seconds. Open that address in your browser in a few minutes; if it answers, the setup worked." }
$check | ConvertTo-Json -Depth 4 | Write-Host
if (-not $check.youtube.ok) { Stop-Setup ('YouTube refused the key: ' + $check.youtube.error) }
if (-not $check.password_set -or -not $check.api_key_set) {
  if ($updating) { Stop-Setup "the server on Cloudflare has lost its key or password. Delete '$note' from your Desktop and run the same line again for a fresh setup." }
  Stop-Setup 'the password is not showing on the server yet. Run the same line again.'
}
if ($updating) {
  $ping = '{"jsonrpc":"2.0","id":1,"method":"ping"}'
  try {
    Invoke-WebRequest -UseBasicParsing -Method Post -Uri "$url/mcp/$password" -Body $ping -ContentType 'application/json' -TimeoutSec 20 -ErrorAction Stop | Out-Null
  } catch {
    Stop-Setup "the password saved on your Desktop no longer opens the server. Delete '$note' from your Desktop and run the same line again for a fresh setup."
  }
  Write-Host ''
  Write-Host 'Updated. YouTube for Claude is running the newest version.' -ForegroundColor Green
  Write-Host 'Nothing to change in claude.ai: your connector address is the same.'
  Write-Host 'Start a NEW chat to get the new tools.'
  try {
    Add-Type -AssemblyName PresentationFramework
    [void][System.Windows.MessageBox]::Show("YouTube for Claude is updated.`n`nNothing to change in claude.ai. Start a new chat to use the new tools.", 'YouTube for Claude - updated')
  } catch { }
  return
}

# ---------------------------------------------------------------- done
$connector = "$url/mcp/$password"
try { Set-Clipboard -Value $connector } catch { }

# Saved to the Desktop too ($note), so the address cannot get lost in this window.
@(
  'YouTube for Claude - your connector address',
  '',
  $connector,
  '',
  'To finish, in claude.ai:',
  '  1. Open https://claude.ai/settings/connectors',
  '  2. Click "Add custom connector" (it may be behind a + or "Browse connectors" button).',
  '  3. Name: YouTube',
  '  4. Remote MCP server URL: the address above (it is also on your clipboard: Ctrl+V).',
  '  5. Leave the other fields empty and click Add.',
  '',
  'Then start a NEW chat, switch YouTube on in the connectors menu by the message box, and ask:',
  '  "What''s trending on YouTube in the UK right now?"',
  '',
  'This address contains your private password. Do not post it anywhere.'
) | Set-Content -Path $note -Encoding UTF8

Write-Host ''
Write-Host 'All done. YouTube for Claude is running.' -ForegroundColor Green
Write-Host ''
Write-Host 'Your connector address (copied to your clipboard, and saved on your Desktop'
Write-Host 'as "YouTube connector address.txt"):'
Write-Host "  $connector" -ForegroundColor Yellow
Write-Host ''
Write-Host 'Last step, in the claude.ai page that is opening now:'
Write-Host '  1. Click "Add custom connector".'
Write-Host '  2. Name: YouTube'
Write-Host '  3. Remote MCP server URL: paste (Ctrl+V). Leave the other fields empty.'
Write-Host '  4. Click Add.'
Write-Host ''
Write-Host 'That address contains your private password. Do not post it anywhere.'
try { Start-Process 'https://claude.ai/settings/connectors' } catch { Write-Host 'Open https://claude.ai/settings/connectors in your browser.' }
try { Start-Process notepad.exe -ArgumentList "`"$note`"" } catch { }
try {
  Add-Type -AssemblyName PresentationFramework
  [void][System.Windows.MessageBox]::Show(
    "YouTube for Claude is running.`n`nYour connector address is on your clipboard and in the Notepad window (also saved on your Desktop).`n`nIn claude.ai: Add custom connector > Name: YouTube > URL: press Ctrl+V > Add.",
    'YouTube for Claude - done')
} catch { }
