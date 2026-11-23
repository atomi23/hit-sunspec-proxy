// Run from the repo root: node tools/ble_refresh_test.js [--report-only] [bluetooth.html]
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const { setImmediate: flushPromises } = require('node:timers/promises');

const args = process.argv.slice(2);
const html = fs.readFileSync(args.find(arg => arg !== '--report-only') || 'web/bluetooth.html', 'utf8');
const script = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].at(-1)[1];

async function replay(periodic) {
  let now = 0, recording = false, inFlight = 0, maxInFlight = 0;
  let desired = {};
  const jobs = [], elements = new Map();
  const counts = { post: 0, status: 0, events: 0 };
  function after(callback, delay) {
    jobs.push({ at: now + delay, callback });
  }
  async function advance(end) {
    while (true) {
      jobs.sort((a, b) => a.at - b.at);
      if (!jobs.length || jobs[0].at > end) break;
      const job = jobs.shift();
      now = job.at;
      job.callback();
      await flushPromises();
    }
    now = end;
  }
  function fetch(url) {
    const kind = url.startsWith('/api/bleled/set?') ? 'post'
      : url === '/api/bleled' ? 'status' : url === '/api/events' ? 'events' : null;
    const poll = recording && (kind === 'status' || kind === 'events');
    if (recording && kind) ++counts[kind];
    if (poll) maxInFlight = Math.max(maxInFlight, ++inFlight);
    return new Promise(resolve => after(() => {
      if (kind === 'post') {
        for (const [key, value] of new URL(url, 'http://mock').searchParams) {
          desired[key] = key === 'on' ? value === '1' : key === 'bri' ? Number(value) : '#' + value;
        }
      }
      resolve({ ok: true, json: () => new Promise(resolveBody => after(() => {
        if (poll) --inFlight;
        const applied = now >= (periodic ? 20500 : 1500) && desired.rgb;
        const data = kind === 'status' ? {
          enabled: true, connected: !!applied, state: applied ? 'verbunden' : 'bereit',
          on: false, bri: 25, rgb: '#ffffff', ...(applied ? desired : {}),
        } : kind === 'events' ? {
          events: applied ? [{ src: 'LED', lv: 'I', t: now, msg: 'Farbe gesendet' }] : [], uptimeMs: now,
        } : {};
        resolveBody(data);
      }, 30)) });
    }, 90));
  }

  const getElementById = id => {
    if (!elements.has(id)) elements.set(id, {});
    return elements.get(id);
  };
  getElementById('bF').bleLedEnabled = {};
  for (const field of ['Name', 'Addr', 'AddrType', 'Variant']) getElementById('bF')['bleLed' + field] = {};
  const context = vm.createContext({
    fetch, LANG: 'de', uiText: de => de, applyThemeLang() {},
    document: { hidden: false, getElementById, querySelectorAll: () => [] },
    Date: class extends Date { static now() { return now; } },
    setTimeout: after,
    setInterval(callback, delay) {
      if (!periodic) return;
      const tick = () => { callback(); after(tick, delay); };
      after(tick, delay);
    },
  });
  vm.runInContext(script, context);
  await advance(500);
  recording = true;
  for (const [delay, command] of [[0, 'on=1'], [30, 'bri=72'], [60, 'rgb=00ffff']]) {
    after(() => vm.runInContext(`bSet('${command}')`, context), delay);
  }
  await advance(periodic ? 24750 : 8000);

  const last = getElementById('bLast').textContent;
  const log = getElementById('bLog').innerHTML.includes('Farbe gesendet');
  const total = counts.post + counts.status + counts.events;
  console.log(`${periodic ? 'Retry + periodic' : 'Rapid clicks only'}: POST=${counts.post} status GET=${counts.status} events GET=${counts.events} total=${total} max concurrent polls=${maxInFlight}`);
  console.log(`  Last sent: ${last}; LED log updated: ${log ? 'yes' : 'no'}`);
  if (!args.includes('--report-only')) {
    assert.equal(counts.post, 3);
    assert.equal(maxInFlight, 1);
    assert.equal(last, 'ein, Helligkeit 72 %, Farbe #00ffff');
    assert.equal(getElementById('bBri').value, 72);
    assert.equal(getElementById('bCol').value, '#00ffff');
    if (periodic) assert(log, 'the LED log should show the delayed result');
    else {
      assert(counts.status > 0 && counts.status <= 8, 'one burst should cover all three commands');
      assert.equal(counts.events, 0, 'command polling must not reload the event log');
    }
  }
}

(async () => {
  await replay(false);
  await replay(true);
})().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
