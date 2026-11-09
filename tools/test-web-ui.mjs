// Node built-ins only. Execute the actual UI script with DOM/BLE/HTTP doubles.
import { readFile } from 'node:fs/promises';
import { strict as assert } from 'node:assert';
import vm from 'node:vm';

const source = await readFile(new URL('../firmware/AtomNeoPixel/WebUi.h', import.meta.url), 'utf8');
const html = source.match(/R"ATOMHTML\(([\s\S]+?)\)ATOMHTML";/)[1];
const script = html.match(/<script>([\s\S]+?)<\/script>/)[1];
assert.equal((html.match(/<option value=/g) || []).length, 5, 'No new pattern menu');
assert(html.indexOf('id="hueSpanControl"') > html.indexOf('id="speedValue"'));
assert(html.indexOf('id="hueSpanControl"') < html.indexOf('type="submit"'));
assert.match(html, /#hueSpanControl\.inactive\s*\{[^}]*GrayText/);

class Element {
  get value() { return this._value; }
  set value(value) { this._value = String(value); }
  constructor(tag) {
    this.value = tag.match(/\bvalue="([^"]*)"/)?.[1] || '';
    this.min = tag.match(/\bmin="([^"]*)"/)?.[1];
    this.max = tag.match(/\bmax="([^"]*)"/)?.[1];
    this.step = tag.match(/\bstep="([^"]*)"/)?.[1];
    this.disabled = /\bdisabled\b/.test(tag);
    this.listeners = {};
    this.classes = new Set((tag.match(/\bclass="([^"]*)"/)?.[1] || '').split(' '));
    this.classList = { toggle: (name, enabled) => enabled ? this.classes.add(name) : this.classes.delete(name) };
  }
  addEventListener(name, callback) { (this.listeners[name] ||= []).push(callback); }
  add() {}
  async fire(name, event = {}) {
    for (const callback of this.listeners[name] || []) await callback({target: this, preventDefault() {}, ...event});
  }
}

const settle = () => new Promise(resolve => setImmediate(resolve));
function fixture(local) {
  const elements = Object.fromEntries([...html.matchAll(/<[^>]*\bid="([^"]+)"[^>]*>/g)]
    .map(([tag, id]) => [id, new Element(tag)]));
  elements.pattern.value = '0';
  let wire = Uint8Array.of(0, 255, 255, 255, 50, 8, 15, 104, 1);
  const writes = [];
  const notification = {};
  const stateCharacteristic = {
    async readValue() { return new DataView(wire.slice().buffer); },
    addEventListener(name, callback) { notification[name] = callback; },
    async startNotifications() {},
  };
  const commandCharacteristic = {
    async writeValueWithResponse(bytes) { wire = Uint8Array.from(bytes); writes.push([...wire]); },
  };
  const device = {
    name: 'AtomPixel-test', addEventListener() {}, removeEventListener() {},
    gatt: {
      connected: false,
      async connect() { this.connected = true; return this; },
      disconnect() { this.connected = false; },
      async getPrimaryService() {
        return {getCharacteristic: async uuid => uuid.includes('86a1') ? commandCharacteristic : stateCharacteristic};
      },
    },
  };
  const context = vm.createContext({
    document: {getElementById: id => elements[id]},
    location: {hostname: local ? '127.0.0.1' : '192.0.2.1', protocol: 'http:'},
    window: {isSecureContext: true},
    navigator: {bluetooth: {requestDevice: async () => device}},
    Option: class {}, Uint8Array, AbortSignal,
    fetch: async (url, options) => {
      assert.equal(url, '/api/state');
      if (options.method === 'PUT') {
        assert.equal(options.headers['X-Atom-Control'], '1');
        assert.equal(options.headers['Content-Type'], 'text/plain');
        assert.match(options.body, /^[0-9a-f]{18}$/);
        wire = Uint8Array.from(Buffer.from(options.body, 'hex'));
        writes.push([...wire]);
      }
      return {ok: true, text: async () => Buffer.from(wire).toString('hex')};
    },
  });
  vm.runInContext(script, context, {filename: 'WebUi.h'});
  return {elements, context, writes, notification, setWire: bytes => { wire = Uint8Array.from(bytes); }};
}

for (const local of [false, true]) {
  const f = fixture(local);
  const e = f.elements;
  const run = code => vm.runInContext(code, f.context);
  assert.equal(e.hueSpan.min, '0');
  assert.equal(e.hueSpan.max, '360');
  assert.equal(e.hueSpan.step, '10');
  assert.equal(e.hueSpan.value, '360');
  if (local) {
    assert(e.settings.disabled);
    await e.connect.fire('click');
  }
  await settle();
  assert(!e.settings.disabled);
  assert(e.hueSpan.disabled && e.hueSpanControl.classes.has('inactive'));
  for (let pattern = 0; pattern <= 4; ++pattern) {
    e.pattern.value = String(pattern);
    await e.controls.fire('input', {target: e.pattern});
    assert.equal(e.hueSpan.disabled, pattern !== 4);
    assert.equal(e.hueSpanControl.classes.has('inactive'), pattern !== 4);
    assert.equal(e.color.disabled, pattern === 4);
  }
  assert.equal(e.speed.min, '1');
  assert.equal(e.speed.max, '60');
  assert.equal(e.speed.step, '1');
  for (const count of [1, 4, 15]) {
    e.ledCount.value = String(count);
    for (let angle = 0; angle <= 360; angle += 10) {
      e.hueSpan.value = String(angle);
      await e.controls.fire('input', {target: e.hueSpan});
      assert.equal(e.hueSpanValue.textContent, `${angle} deg`);
      for (const speed of [1, 60]) {
        e.speed.value = String(speed);
        await e.controls.fire('input', {target: e.speed});
        await e.controls.fire('submit');
        await settle();
        assert.deepEqual(f.writes.at(-1), [4, 255, 255, 255, 50, speed, count, angle & 255, angle >> 8]);
        assert(!e.settings.disabled);
      }
    }
  }
  e.hueSpan.value = '180';
  e.pattern.value = '1';
  await e.controls.fire('input', {target: e.pattern});
  assert(e.hueSpan.disabled);
  e.pattern.value = '4';
  await e.controls.fire('input', {target: e.pattern});
  assert.equal(e.hueSpan.value, '180', 'Keep angle across pattern switches');
  f.setWire([4, 255, 0, 0, 80, 30, 4, 180, 0]);
  await e.refresh.fire('click');
  assert.equal(e.hueSpan.value, '180');
  assert.equal(e.hueSpanValue.textContent, '180 deg');
  assert.equal(e.speed.value, '30');
  assert.equal(e.ledCount.value, '4');
  for (const bytes of [
    [0, 255, 255, 255, 50, 8], [0, 255, 255, 255, 50, 8, 15],
    [4, 255, 255, 255, 50, 30, 15, 1, 0], [4, 255, 255, 255, 50, 30, 15, 114, 1],
    [5, 255, 255, 255, 50, 30, 15, 104, 1], [4, 255, 255, 255, 50, 61, 15, 104, 1],
  ]) {
    assert.throws(() => run(`showState(Uint8Array.from(${JSON.stringify(bytes)}))`));
  }
  assert.throws(() => run('fromHex("00".repeat(8))'));
  if (local) {
    await e.connect.fire('click');
    assert(e.settings.disabled, 'Disconnected controls must be unavailable');
  }
  console.log(`PASS: ${local ? 'BLE' : 'Wi-Fi'} UI, angle matrix, sending, readback, disabled controls, invalid/old state`);
}
