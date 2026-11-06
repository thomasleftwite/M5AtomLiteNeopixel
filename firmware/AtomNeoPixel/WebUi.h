#pragma once

#include <pgmspace.h>

// This same document is served by the Atom and extracted by tools/serve-ui.mjs.
const char UI_PAGE[] PROGMEM = R"ATOMHTML(<!doctype html>
<html lang="ja">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Atom NeoPixel</title>
<style>
  :root { color-scheme: light dark; font: 1rem system-ui, sans-serif; }
  body { max-width: 36rem; margin: 2rem auto; padding: 0 1rem; }
  fieldset { display: grid; gap: 1rem; padding: 1rem; margin: 1rem 0; }
  label { display: grid; gap: .3rem; }
  select, button { font: inherit; padding: .45rem .8rem; }
  input[type=color] { min-width: 5rem; min-height: 2.5rem; }
  #status { min-height: 2.5em; }
</style>
<h1>Atom NeoPixel</h1>
<p id="mode"></p>
<button id="connect" type="button" hidden>BLE 接続</button>
<button id="refresh" type="button">状態を取得</button>
<p id="status" role="status" aria-live="polite"></p>
<form id="controls">
  <fieldset id="settings" disabled>
    <legend>外付け LED</legend>
    <label>LED 個数
      <select id="ledCount"></select>
    </label>
    <label>パターン
      <select id="pattern">
        <option value="0">消灯</option>
        <option value="1">固定点灯</option>
        <option value="2">点滅</option>
        <option value="3">フェード</option>
        <option value="4">レインボー</option>
      </select>
    </label>
    <label>色 <input id="color" type="color" value="#ffffff"></label>
    <label>明るさ（機器の上限に対する割合）<input id="brightness" type="range" min="0" max="100" value="50"></label>
    <label>速度 <input id="speed" type="range" min="0" max="100" value="50">
      <output id="speedValue" for="speed">50%</output>
    </label>
    <button type="submit">反映する</button>
  </fieldset>
</form>
<script>
'use strict';
const SERVICE = '251c86a0-3990-4b1e-bb96-5ed47d413b60';
const COMMAND = '251c86a1-3990-4b1e-bb96-5ed47d413b60';
const STATE = '251c86a2-3990-4b1e-bb96-5ed47d413b60';
const RAINBOW = 4;
const MAX_LED_COUNT = 15;
const localUi = ['localhost', '127.0.0.1'].includes(location.hostname)
  || ['file:', 'content:'].includes(location.protocol);
const byId = (id) => document.getElementById(id);
const connect = byId('connect');
const refresh = byId('refresh');
const settings = byId('settings');
const status = byId('status');
const pattern = byId('pattern');
const color = byId('color');
const speed = byId('speed');
const ledCount = byId('ledCount');
for (let count = 1; count <= MAX_LED_COUNT; count++) {
  ledCount.add(new Option(count, count));
}
ledCount.value = String(MAX_LED_COUNT);
let device;
let commandCharacteristic;
let stateCharacteristic;
let ready = false;
let busy = false;
let edited = false;
let rainbowSpeed = false;

function updateControls() {
  settings.disabled = busy || !ready;
  refresh.disabled = busy || (localUi && !ready);
  connect.disabled = busy;
  connect.textContent = ready ? 'BLE 切断' : 'BLE 接続';
}

function toHex(bytes) {
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, '0')).join('');
}

function fromHex(hex) {
  if (!/^(?:[0-9a-f]{2}){6,7}$/i.test(hex)) throw new Error('機器の応答が不正です');
  return Uint8Array.from(hex.match(/../g), (pair) => Number.parseInt(pair, 16));
}

function fromView(view) {
  return new Uint8Array(view.buffer, view.byteOffset, view.byteLength);
}

function updatePatternControls(restoredSpeed) {
  const isRainbow = Number(pattern.value) === RAINBOW;
  const previousSpeed = Number(speed.value);
  color.disabled = isRainbow;
  if (isRainbow !== rainbowSpeed) {
    speed.min = isRainbow ? '1' : '0';
    speed.max = isRainbow ? '60' : '100';
    speed.value = isRainbow ? 1 + Math.round(previousSpeed * 59 / 100)
      : Math.round((previousSpeed - 1) * 100 / 59);
    rainbowSpeed = isRainbow;
  }
  if (restoredSpeed !== undefined) speed.value = restoredSpeed;
  byId('speedValue').textContent = isRainbow
    ? (speed.value === '60' ? '1' : speed.value + '/60') + ' cycle/sec'
    : speed.value + '%';
}

function showState(bytes) {
  if (bytes.length === 6) throw new Error('機器のファームウェアを更新してください。');
  const isRainbow = bytes[0] === RAINBOW;
  if (bytes.length !== 7 || bytes[0] > RAINBOW || bytes[4] > 100
      || (isRainbow ? bytes[5] < 1 || bytes[5] > 60 : bytes[5] > 100)
      || bytes[6] < 1 || bytes[6] > MAX_LED_COUNT) {
    throw new Error('機器の状態が不正です');
  }
  pattern.value = String(bytes[0]);
  color.value = '#' + toHex(bytes.subarray(1, 4));
  byId('brightness').value = String(bytes[4]);
  ledCount.value = String(bytes[6]);
  updatePatternControls(bytes[5]);
  edited = false;
}

function selectedState() {
  const rgb = Number.parseInt(color.value.slice(1), 16);
  return Uint8Array.of(Number(pattern.value), rgb >> 16, (rgb >> 8) & 255,
    rgb & 255, Number(byId('brightness').value), Number(speed.value), Number(ledCount.value));
}

async function requestState(bytes) {
  const options = {cache: 'no-store', signal: AbortSignal.timeout(5000)};
  if (bytes) {
    options.method = 'PUT';
    options.headers = {'Content-Type': 'text/plain', 'X-Atom-Control': '1'};
    options.body = toHex(bytes);
  }
  const response = await fetch('/api/state', options);
  if (!response.ok) throw new Error('機器へのアクセスに失敗しました（HTTP ' + response.status + '）');
  return fromHex(await response.text());
}

function onDisconnect(event) {
  if (event.target !== device) return;
  clearConnection();
  status.textContent = 'BLE 切断。再接続するには本体ボタンを長押ししてください。';
}

function clearConnection() {
  const previousDevice = device;
  if (previousDevice) {
    previousDevice.removeEventListener('gattserverdisconnected', onDisconnect);
    device = undefined;
    if (previousDevice.gatt.connected) previousDevice.gatt.disconnect();
  }
  ready = false;
  commandCharacteristic = undefined;
  stateCharacteristic = undefined;
  updateControls();
}

async function withControls(action) {
  if (busy) return;
  busy = true;
  updateControls();
  try {
    await action();
  } catch (error) {
    if (!localUi || !ready) clearConnection();
    status.textContent = error.message;
  } finally {
    busy = false;
    updateControls();
  }
}

connect.addEventListener('click', () => withControls(async () => {
  if (ready) {
    clearConnection();
    status.textContent = 'BLE を切断しました。';
    return;
  }
  if (!window.isSecureContext || !navigator.bluetooth) {
    throw new Error('この開き方では Web Bluetooth を利用できません。Chrome で 127.0.0.1 の GUI を開いてください。');
  }
  device = await navigator.bluetooth.requestDevice({filters: [{services: [SERVICE]}]});
  const selectedDevice = device;
  device.addEventListener('gattserverdisconnected', onDisconnect);
  const gatt = await device.gatt.connect();
  const service = await gatt.getPrimaryService(SERVICE);
  commandCharacteristic = await service.getCharacteristic(COMMAND);
  stateCharacteristic = await service.getCharacteristic(STATE);
  showState(fromView(await stateCharacteristic.readValue()));
  stateCharacteristic.addEventListener('characteristicvaluechanged', (event) => {
    if (device === selectedDevice && !edited && !busy) showState(fromView(event.target.value));
  });
  await stateCharacteristic.startNotifications();
  if (device !== selectedDevice || !selectedDevice.gatt.connected) throw new Error('BLE 接続が切れました。');
  ready = true;
  status.textContent = '接続済み · ' + (device.name || 'Atom NeoPixel');
}));

async function refreshState() {
  showState(localUi ? fromView(await stateCharacteristic.readValue()) : await requestState());
  ready = true;
  status.textContent = '機器の状態を取得しました。';
}

refresh.addEventListener('click', () => withControls(refreshState));
byId('controls').addEventListener('input', (event) => {
  edited = true;
  if (event.target === pattern || event.target === speed) updatePatternControls();
});
byId('controls').addEventListener('submit', (event) => {
  event.preventDefault();
  withControls(async () => {
    if (!ready) return;
    const bytes = selectedState();
    if (localUi) {
      await commandCharacteristic.writeValueWithResponse(bytes);
      edited = false;
      status.textContent = 'BLE で設定を送信しました。';
    } else {
      showState(await requestState(bytes));
      status.textContent = '機器に反映しました。';
    }
  });
});

if (localUi) {
  byId('mode').textContent = 'BLE · 本体ボタンを約 1.5 秒押してから接続してください。';
  connect.hidden = false;
  updateControls();
} else {
  byId('mode').textContent = 'Wi-Fi · Atom Lite の操作画面';
  withControls(refreshState);
}
</script>
</html>
)ATOMHTML";
