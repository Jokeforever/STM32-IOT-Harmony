// Run production TrendHistory.ets with the DevEco TypeScript compiler; no network or credentials.
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert').strict;
process.env.TZ = 'Asia/Shanghai';
const option = name => { const i = process.argv.indexOf(name); return i < 0 ? undefined : process.argv[i + 1]; };
const root = path.resolve(__dirname, '..');
const source = option('--source') || path.join(root, 'entry/src/main/ets/model/TrendHistory.ets');
const compiler = option('--compiler') || 'F:/DEVECO/DevEco Studio/sdk/default/openharmony/ets/build-tools/ets-loader/node_modules/typescript/lib/typescript.js';
const ts = require(compiler);
const built = ts.transpileModule(fs.readFileSync(source, 'utf8'), {
  fileName: 'TrendHistory.ts', reportDiagnostics: true,
  compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2021 }
});
const errors = (built.diagnostics || []).filter(d => d.category === ts.DiagnosticCategory.Error);
assert.equal(errors.length, 0, errors.map(d => ts.flattenDiagnosticMessageText(d.messageText, '\n')).join('\n'));
const exported = {};
vm.runInNewContext(built.outputText, { exports: exported, module: { exports: exported } }, { filename: 'TrendHistory.js' });
const H = exported.TrendHistory;
const time = Date.UTC(2026, 10, 8, 0, 0, 0);
let scenarios = 0, checks = 0;
const plain = v => JSON.parse(JSON.stringify(v));
const eq = (actual, expected) => { checks++; assert.deepEqual(plain(actual), expected); };
const scenario = (name, action) => { try { action(); scenarios++; } catch (e) { e.message = name + ': ' + e.message; throw e; } };
const properties = (t = time, h = time, mask = '11', temp = '24', humi = '50') => {
  const p = [{ identifier: 'temp', value: temp, time: t }, { identifier: 'humi', value: humi, time: h }];
  if (mask !== undefined) p.push({ identifier: 'sensor_status', value: mask, time: Math.max(t || 0, h || 0) });
  return p;
};
const cache = (history, p = []) => JSON.stringify({ historyVersion: 2, properties: p, lastSyncTime: 'sync', lastReportTime: time, ...history });
const first = () => H.collect(undefined, properties());

scenario('first valid report', () => eq(first(), { tempHistory: [{ time, value: 24 }], humiHistory: [{ time, value: 50 }] }));
scenario('50 identical polls keep one point', () => {
  let history = first();
  for (let i = 0; i < 50; i++) history = H.collect(cache(history, properties()), properties());
  eq(history.tempHistory.length, 1); eq(history.humiHistory.length, 1);
});
scenario('same value new timestamp is a real new report', () => {
  const history = H.collect(cache(first()), properties(time + 5000, time + 5000));
  eq(history.tempHistory.length, 2); eq(history.humiHistory[1], { time: time + 5000, value: 50 });
});
scenario('temperature/humidity have independent timestamps', () => {
  const history = H.collect(cache(first()), properties(time + 5000, time));
  eq(history.tempHistory.length, 2); eq(history.humiHistory.length, 1);
});
scenario('LED property time cannot create sensor samples', () => {
  const p = properties(); p.push({ identifier: 'led', value: '1', time: time + 100000 });
  eq(H.collect(cache(first()), p), plain(first()));
});
scenario('missing property timestamp has no phone-time fallback', () => {
  const p = [{ identifier: 'temp', value: '24' }];
  eq(H.collect(undefined, p).tempHistory, []);
});
for (const bad of ['', ' ', 'NaN', 'Infinity', '24abc']) scenario('reject bad numeric reading ' + JSON.stringify(bad), () => {
  eq(H.collect(undefined, properties(time, time, '11', bad)).tempHistory, []);
});
for (const bad of [0, -1, NaN, Infinity, time + 0.5, 8640000000000001]) scenario('reject invalid time ' + bad, () => {
  eq(H.collect(undefined, properties(bad)).tempHistory, []);
});
scenario('status zero does not append retained measurements', () => eq(H.collect(undefined, properties(time, time, '0')), { tempHistory: [], humiHistory: [] }));
scenario('status bits independently gate curves', () => {
  eq(H.collect(undefined, properties(time, time, '1')).tempHistory.length, 1);
  eq(H.collect(undefined, properties(time, time, '1')).humiHistory.length, 0);
  eq(H.collect(undefined, properties(time, time, '2')).tempHistory.length, 0);
  eq(H.collect(undefined, properties(time, time, '2')).humiHistory.length, 1);
});
scenario('legacy model without a status field remains usable', () => {
  const p = properties(); p.pop(); eq(H.collect(undefined, p).tempHistory.length, 1);
});
for (const bad of ['', 'oops', '-1', '1.5', 'Infinity']) scenario('invalid status ' + bad, () => eq(H.collect(undefined, properties(time, time, bad)).tempHistory, []));
scenario('delayed older response cannot roll history back', () => {
  const current = H.collect(cache(first()), properties(time + 5000, time + 5000));
  eq(H.collect(cache(current), properties(time - 1000, time - 1000, '11', '99', '99')), plain(current));
});
scenario('same-time correction updates value without a new point', () => {
  const current = H.collect(cache(first()), properties(time, time, '11', '25', '51'));
  eq(current.tempHistory, [{ time, value: 25 }]); eq(current.humiHistory, [{ time, value: 51 }]);
});
scenario('100 reports keep the last20 for each sensor', () => {
  let history = H.collect(undefined, []);
  for (let i = 0; i < 100; i++) history = H.collect(cache(history), properties(time + i * 5000, time + i * 5000, '11', String(i), String(i)));
  eq(history.tempHistory.length, 20); eq(history.tempHistory[0], { time: time + 80 * 5000, value: 80 });
  eq(history.humiHistory[19], { time: time + 99 * 5000, value: 99 });
  eq(H.fromRaw(cache(history)), plain(history));
});
scenario('restart and same report preserve point count', () => {
  const history = H.collect(cache(first()), properties(time + 5000, time + 5000));
  eq(H.collect(cache(history, properties(time + 5000, time + 5000)), properties(time + 5000, time + 5000)), plain(history));
});
scenario('old numeric cache keeps only the reliably timed latest property', () => {
  const old = JSON.stringify({ properties: properties(), tempHistory: [10, 20, 30], humiHistory: [40, 50] });
  eq(H.fromRaw(old), plain(first())); eq(JSON.parse(old).tempHistory, [10, 20, 30]);
});
scenario('old cache without property times cannot fabricate history', () => eq(H.fromRaw(JSON.stringify({ properties: [{ identifier: 'temp', value: '24' }], tempHistory: [1, 2] })), { tempHistory: [], humiHistory: [] }));
for (const raw of [undefined, '', '{oops', 'null', '3', '[]']) scenario('malformed/empty cache ' + raw, () => eq(H.fromRaw(raw), { tempHistory: [], humiHistory: [] }));
scenario('normalize corrupted, unsorted and duplicate points', () => {
  const raw = JSON.stringify({ historyVersion: 2, properties: [], tempHistory: [3, null, { time: -1, value: 1 }, { time: time + 5, value: 2 }, { time, value: 1 }, { time: time + 5, value: 3 }, { time: 'bad', value: 4 }], humiHistory: 'bad' });
  eq(H.fromRaw(raw), { tempHistory: [{ time, value: 1 }, { time: time + 5, value: 3 }], humiHistory: [] });
});
scenario('updating never mutates the provided array/point', () => {
  const source = Object.freeze([Object.freeze({ time, value: 24 })]);
  eq(H.append(source, properties(time + 5000), 'temp', 0).length, 2); eq(source, [{ time, value: 24 }]);
});
scenario('alternating home/detail writers retain the freshest shared curve', () => {
  let raw = cache(first());
  raw = cache(H.collect(raw, properties(time + 5000, time + 5000)), properties(time + 5000, time + 5000));
  raw = cache(H.collect(raw, properties()), properties());
  const final = H.collect(raw, properties(time + 10000, time + 10000));
  eq(final.tempHistory.map(p => p.time), [time, time + 5000, time + 10000]);
});
scenario('disconnect does not grow curve; later valid report resumes', () => {
  const offline = H.collect(cache(first()), properties(time + 5000, time + 5000, '0'));
  eq(offline, plain(first()));
  eq(H.collect(cache(offline), properties(time + 10000, time + 10000)).tempHistory.length, 2);
});
scenario('time-axis positions reflect unequal report gaps', () => {
  eq(H.plotPoints([{ time, value: 0 }, { time: time + 1000, value: 25 }, { time: time + 10000, value: 50 }], 0, 50, 108), [[4, 88], [14, 48], [104, 8]]);
});
scenario('plot clipping and unmeasured widths', () => {
  eq(H.plotPoints([{ time, value: -20 }, { time: time + 1, value: 200 }], 0, 50, 108), [[4, 88], [104, 8]]);
  eq(H.plotPoints([], 0, 50, 108), []); eq(H.plotPoints([{ time, value: 25 }], 0, 50, 8), []);
  eq(H.plotPoints([{ time, value: 25 }], 0, 50, 108), [[4, 48]]);
});
scenario('point timestamp display uses local clock and handles invalid dates', () => {
  eq(H.formatPointTime(time), '11-08 08:00:00'); eq(H.formatPointTime(NaN), '--');
});
scenario('report label follows the current property snapshot including navigation restore', () => {
  eq(H.latestReportTime(properties()), time);
  const p = properties(); p.push({ identifier: 'led', value: '1', time: time + 5000 });
  eq(H.latestReportTime(p), time + 5000);
  eq(H.latestReportTime([{ identifier: 'temp', value: '24', time: NaN }]), 0);
});
scenario('zoomed plot preserves actual x spacing and scales height', () => {
  eq(H.plotPoints([{ time, value: 0 }, { time: time + 1000, value: 25 }, { time: time + 10000, value: 50 }], 0, 50, 108, 280), [[4, 272], [14, 140], [104, 8]]);
  eq(H.plotPoints([{ time, value: 23 }], 20, 26, 108, 280), [[4, 140]]);
  eq(H.plotPoints([{ time, value: 23 }], 20, 26, 108, 16), []);
});
scenario('full-range view keeps original axes', () => {
  eq(H.displayRange([{ time, value: 23 }], 0, 50, false, 4), { min: 0, max: 50 });
  eq(H.displayRange([{ time, value: 72 }], 0, 100, false, 10), { min: 0, max: 100 });
});
scenario('local temperature view makes screenshot-sized changes visible', () => {
  eq(H.displayRange([{ time, value: 21 }, { time: time + 1, value: 23 }], 0, 50, true, 4), { min: 19, max: 25 });
  eq(H.displayRange([{ time, value: 23 }], 0, 50, true, 4), { min: 20, max: 26 });
});
scenario('local humidity view fits the observed range', () => {
  eq(H.displayRange([{ time, value: 53 }, { time: time + 1, value: 72 }], 0, 100, true, 10), { min: 50, max: 75 });
});
scenario('constant readings near humidity limits have nonzero sensible range', () => {
  eq(H.displayRange([{ time, value: 0 }], 0, 100, true, 10), { min: 0, max: 7 });
  eq(H.displayRange([{ time, value: 100 }], 0, 100, true, 10), { min: 93, max: 100 });
});
scenario('empty local range uses defaults and negative temperature is retained', () => {
  eq(H.displayRange([], 0, 50, true, 4), { min: 0, max: 50 });
  const r = H.displayRange([{ time, value: -5 }, { time: time + 1, value: -3 }], 0, 50, true, 4);
  eq(r.min <= -5 && r.max >= -3 && r.max > r.min, true);
});
scenario('zoom range does not mutate samples or alter cached history', () => {
  const samples = Object.freeze([Object.freeze({ time, value: 21 }), Object.freeze({ time: time + 1, value: 23 })]);
  H.displayRange(samples, 0, 50, true, 4);
  eq(samples, [{ time, value: 21 }, { time: time + 1, value: 23 }]);
  const history = first(); const before = cache(history);
  H.displayRange(history.tempHistory, 0, 50, true, 4); eq(cache(history), before);
});
scenario('all local sample values remain inside the plotted axis', () => {
  for (const values of [[0], [1, 99], [21, 23], [-10, 60], [53, 72], [100]]) {
    const points = values.map((value, i) => ({ time: time + i, value }));
    const r = H.displayRange(points, 0, 100, true, 10);
    eq(r.max > r.min && values.every(v => v >= r.min && v <= r.max), true);
  }
});
const bounds = { timeMin: time, timeMax: time + 100000, valueMin: 0, valueMax: 100 };
scenario('viewport follows the real report window', () => {
  eq(H.viewportFor([{ time, value: 21 }, { time: time + 9000, value: 23 }], { min: 19, max: 25 }), { timeMin: time, timeMax: time + 9000, valueMin: 19, valueMax: 25 });
  const one = H.viewportFor([{ time, value: 21 }], { min: 19, max: 25 });
  eq(one.timeMax - one.timeMin, 1000); eq(one.timeMin <= time && one.timeMax >= time, true);
  const empty = H.viewportFor([], { min: 0, max: 50 }); eq(empty.timeMax > empty.timeMin, true);
});
scenario('10x viewport clamps repeated zoom-in at both axes', () => {
  let v = bounds;
  for (let i = 0; i < 30; i++) v = H.zoomViewport(v, bounds, 2, 0.5, 0.5, true, true);
  eq(v.timeMax - v.timeMin, 10000); eq(v.valueMax - v.valueMin, 10);
  eq(H.zoomViewport(v, bounds, 0.0001, 0.5, 0.5, true, true), bounds);
});
scenario('value-axis zoom leaves time range untouched', () => {
  eq(H.zoomViewport(bounds, bounds, 10, 0.5, 0.5, false, true), { timeMin: time, timeMax: time + 100000, valueMin: 45, valueMax: 55 });
});
scenario('time-axis zoom leaves value range untouched', () => {
  eq(H.zoomViewport(bounds, bounds, 10, 1, 0.5, true, false), { timeMin: time + 90000, timeMax: time + 100000, valueMin: 0, valueMax: 100 });
});
scenario('cursor-anchored zoom preserves the selected value and time pixel', () => {
  const v = H.zoomViewport(bounds, bounds, 4, 0.25, 0.75, true, true);
  eq(v.timeMin + (v.timeMax - v.timeMin) * 0.25, time + 25000);
  eq(v.valueMin + (v.valueMax - v.valueMin) * 0.75, 75);
});
scenario('panning has bounded endpoints without changing the zoom', () => {
  const v = H.zoomViewport(bounds, bounds, 10, 0.5, 0.5, true, true);
  const end = H.panViewport(v, bounds, 1000, 1000);
  eq(end, { timeMin: time + 90000, timeMax: time + 100000, valueMin: 90, valueMax: 100 });
  const start = H.panViewport(v, bounds, -1000, -1000);
  eq(start, { timeMin: time, timeMax: time + 10000, valueMin: 0, valueMax: 10 });
  eq(H.panViewport(bounds, bounds, 1000, -1000), bounds);
});
scenario('a drag translates the view by the current visible span', () => {
  const v = H.zoomViewport(bounds, bounds, 10, 0.5, 0.5, true, true);
  const p = H.panViewport(v, bounds, -0.2, 0.3);
  eq(p.timeMin, v.timeMin - 2000); eq(p.valueMin, v.valueMin + 3);
});
scenario('viewport projection never creates a clipped flat sensor reading', () => {
  const view = { timeMin: time + 1000, timeMax: time + 2000, valueMin: 20, valueMax: 30 };
  eq(H.viewportPoints([{ time, value: 10 }, { time: time + 1500, value: 25 }, { time: time + 3000, value: 40 }], view, 108, 116), [[-96, 208], [54, 58], [204, -92]]);
  eq(H.viewportPoints([], view, 108, 116), []);
  eq(H.viewportPoints([{ time, value: 25 }], view, 8, 116), []);
});
scenario('wheel direction is invertible and large deltas are bounded', () => {
  eq(H.wheelZoomFactor(-120) > 1, true); eq(H.wheelZoomFactor(120) < 1, true);
  eq(Math.abs(H.wheelZoomFactor(-30) * H.wheelZoomFactor(30) - 1) < 1e-12, true);
  eq(H.wheelZoomFactor(-12000), H.wheelZoomFactor(-120));
  eq(H.wheelZoomFactor(0), 1); eq(H.wheelZoomFactor(NaN), 1);
});
scenario('invalid zoom input preserves the viewport', () => {
  for (const factor of [0, -1, NaN, Infinity]) eq(H.zoomViewport(bounds, bounds, factor, 0.5, 0.5, true, true), bounds);
  eq(H.zoomViewport(bounds, bounds, 2, NaN, 0.5, true, true), bounds);
  eq(H.panViewport(bounds, bounds, NaN, Infinity), bounds);
});
scenario('viewport operations do not mutate bounds or samples', () => {
  const fixed = Object.freeze({ ...bounds });
  const next = H.zoomViewport(fixed, fixed, 10, 0.5, 0.5, true, true);
  H.panViewport(next, fixed, -1, 1); eq(fixed, bounds);
});
scenario('milliseconds distinguish close time labels at high zoom', () => {
  eq(H.formatPointTime(time + 123, true), '11-08 08:00:00.123');
  eq(H.formatPointTime(time + 123), '11-08 08:00:00');
});
scenario('slider-scale endpoints and intermediate values are usable', () => {
  for (const progress of [0, 1, 25, 50, 75, 100]) {
    const factor = Math.pow(H.MAX_ZOOM, progress / 100);
    const v = H.zoomViewport(bounds, bounds, factor, 0.5, 0.5, false, true);
    eq(Math.abs(100 / (v.valueMax - v.valueMin) - factor) < 1e-9, true);
  }
});
scenario('stable local data still has a usable 10x viewport', () => {
  const points = [{ time, value: 23 }];
  const base = H.viewportFor(points, H.displayRange(points, 0, 50, true, 4));
  const v = H.zoomViewport(base, base, H.MAX_ZOOM, 0.5, 0.5, true, true);
  eq(v.timeMax > v.timeMin && v.valueMax > v.valueMin, true);
  const p = H.viewportPoints(points, v, 108, 280)[0];
  eq(Number.isFinite(p[0]) && Number.isFinite(p[1]), true);
});
scenario('sweep across zoom, anchor and pan combinations stays within bounds', () => {
  let pass = true;
  for (const factor of [0.1, 1, 1.5, 10, 100, 1000]) {
    for (const anchor of [-1, 0, 0.25, 0.5, 1, 2]) {
      const v = H.zoomViewport(bounds, bounds, factor, anchor, anchor, true, true);
      for (const offset of [-100, -0.25, 0, 0.25, 100]) {
        const p = H.panViewport(v, bounds, offset, -offset);
        pass = pass && p.timeMin >= bounds.timeMin && p.timeMax <= bounds.timeMax && p.valueMin >= 0 && p.valueMax <= 100 &&
          p.timeMax > p.timeMin && p.valueMax > p.valueMin;
      }
    }
  }
  eq(pass, true);
});
console.log('PASS:', scenarios, 'trend scenarios,', checks, 'checks against production ArkTS helper. No network, credentials or UI mocks.');
