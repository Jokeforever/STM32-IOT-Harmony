// Production model, API adapter, and extracted production page methods; never contacts OneNET.
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert').strict;
const option = name => { const i = process.argv.indexOf(name); return i < 0 ? undefined : process.argv[i + 1]; };
const root = option('--root') || path.resolve(__dirname, '..');
const compiler = option('--compiler') || 'F:/DEVECO/DevEco Studio/sdk/default/openharmony/ets/build-tools/ets-loader/node_modules/typescript/lib/typescript.js';
const ts = require(compiler);
let scenarios = 0, checks = 0;
const plain = v => v === undefined ? undefined : JSON.parse(JSON.stringify(v));
const eq = (a, b) => { checks++; assert.deepEqual(plain(a), b); };
const yes = a => eq(!!a, true);
const throws = (fn, text) => { checks++; assert.throws(fn, text); };
const scenario = async (name, fn) => { try { await fn(); scenarios++; } catch (e) { e.message = name + ': ' + e.message; throw e; } };
function load(text, globals = {}, requireFn = name => { throw Error('Unexpected runtime dependency: ' + name); }) {
  const built = ts.transpileModule(text, { fileName: 'Control.ts', reportDiagnostics: true,
    compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2021, experimentalDecorators: true } });
  const errors = (built.diagnostics || []).filter(x => x.category === ts.DiagnosticCategory.Error);
  assert.equal(errors.length, 0, errors.map(x => ts.flattenDiagnosticMessageText(x.messageText, '\n')).join('\n'));
  const exported = {};
  vm.runInNewContext(built.outputText, { exports: exported, module: { exports: exported }, require: requireFn, ...globals });
  return exported;
}
const source = rel => fs.readFileSync(path.join(root, 'entry/src/main/ets', rel), 'utf8');
const model = load(source('model/ControlTracker.ets'));
const { ControlTracker: T, ControlPhase: P } = model;
const models = load(source('model/DeviceModels.ets'), { Observed: cls => cls });
const t = 1700000000000;
const props = (value = '0', time = t - 1000, key = 'led') => [{ identifier: key, value, time }];
const accepted = (ack = true) => ({ accepted: true, deviceAcknowledged: ack, message: '', requestId: '', deviceMessageId: '' });
const started = (key = 'led', target = 1) => T.begin([], 1, key, target, props('0', t - 1000, key), t);
const waiting = (key = 'led', target = 1) => T.received(started(key, target), 1, accepted(), t + 100);
const deferred = () => { let resolve, reject; const promise = new Promise((a, b) => { resolve = a; reject = b; }); return { promise, resolve, reject }; };
const step = async () => { for (let i = 0; i < 12; i++) await Promise.resolve(); };

function method(text, name) {
  const match = new RegExp('  (?:private )?(?:async )?' + name + '\\([^)]*\\)[^{]*\\{').exec(text);
  assert(match, name);
  let end = match.index + match[0].length, depth = 1;
  while (depth) { if (text[end] === '{') depth++; else if (text[end] === '}') depth--; end++; }
  return text.slice(match.index, end);
}

function makeHost() {
  const state = { now: t, nextTimer: 1, timers: new Map(), storage: new Map() };
  class Clock extends Date { static now() { return state.now; } }
  const page = source('pages/DeviceDetail.ets');
  const names = ['aboutToDisappear', 'autoRefreshTask', 'refreshData', 'refresh', 'readControlRequests',
    'writeControlRequests', 'stopControlConfirmation', 'ensureControlConfirmation', 'controlOnline', 'controlEnabled',
    'pendingTarget', 'controlAttention', 'controlText', 'sendControl', 'setLedMode', 'setBumpMode',
    'thresholdPeer', 'thresholdRange', 'thresholdEditable', 'thresholdDraftValue', 'editThreshold', 'applyThreshold', 'formatTime', 'pad'];
  const methods = names.map(name => method(page, name)).join('\n');
  const storage = { get: key => state.storage.get(key), setOrCreate: (key, value) => state.storage.set(key, value) };
  const { Host } = load(`export class Host { ${methods} }`, {
    Date: Clock, ControlTracker: T, ControlPhase: P, PropertyHelper: models.PropertyHelper,
    TrendHistory: { latestReportTime: properties => Math.max(0, ...properties.map(p => p.time || 0)) }, AppStorage: storage,
    setInterval: (fn, ms) => { const id = state.nextTimer++; state.timers.set(id, { fn, ms }); return id; },
    clearInterval: id => state.timers.delete(id)
  });
  const host = new Host();
  Object.assign(host, { pageActive: true, pageGeneration: 1, loading: false, online: true,
    thresholdEditorId: '', thresholdDraft: '',
    properties: props(), controlRequests: [], confirmationTimer: -1, ledMode: 0, bumpMode: 0,
    temperature: 0, humidity: 0, lastReportTime: t - 1000, lastSyncTime: '--', errorMessage: '', autoRefresh: true,
    saveCount: 0, saveCachedState() { this.saveCount++; }, stopAutoRefresh() { this.autoStopped = true; } });
  const calls = { posts: [], gets: 0, query: async () => props() };
  const api = {
    setLedMode: async value => { calls.posts.push(['led', value]); return accepted(); },
    setBumpMode: async value => { calls.posts.push(['bump', value]); return accepted(); },
    setThreshold: async (key, value) => { calls.posts.push([key, value]); return accepted(); },
    queryDeviceProperties: () => { calls.gets++; return calls.query(); }
  };
  host.oneNetApi = api;
  return { host, state, calls, api, tick: async () => { for (const { fn } of [...state.timers.values()]) fn(); await step(); } };
}

async function main() {
  await scenario('API success requires separate platform and device codes', () => {
    const r = T.decode({ code: 0, data: { code: 200, id: '42', msg: 'success' }, request_id: 'request-1' });
    yes(r.accepted && r.deviceAcknowledged); eq(r.deviceMessageId, '42'); eq(r.requestId, 'request-1');
    yes(T.decode({ code: '0', data: { code: '200' } }).deviceAcknowledged);
  });
  await scenario('a device rejection cannot be hidden by platform success', () => {
    const r = T.decode({ code: 0, data: { code: 400, msg: 'invalid value' } });
    eq(r.accepted, false); eq(r.deviceAcknowledged, false); yes(r.message.includes('400'));
    eq(T.decode({ code: 0, data: { code: 0 } }).accepted, false);
  });
  await scenario('missing device receipt is retained as unacknowledged', () => {
    eq(T.decode({ code: 0 }).deviceAcknowledged, false); yes(T.decode({ code: 0 }).accepted);
    eq(T.decode({ code: 0, data: null }).deviceAcknowledged, false);
  });
  await scenario('malformed replies do not silently succeed', () => {
    for (const r of [null, {}, { code: '' }, { code: 'bad' }, { code: 0, data: [] }, { code: 0, data: { code: 'bad' } }]) throws(() => T.decode(r));
    eq(T.decode({ code: 10403, msg: 'denied' }).accepted, false);
  });
  await scenario('start snapshots property time and does not change the report', () => {
    const p = Object.freeze([Object.freeze({ identifier: 'led', value: '0', time: t - 1000 })]);
    const r = T.begin([], 1, 'led', 1, p, t)[0];
    eq(r.baselineTime, t - 1000); eq(r.phase, P.SENDING); eq(p[0].value, '0');
  });
  await scenario('same-attribute duplicate is blocked but other attributes can proceed', () => {
    const a = started(); eq(T.begin(a, 2, 'led', 2, props(), t + 1), plain(a));
    eq(T.begin(a, 3, 'bump', 2, props('0', t, 'bump'), t + 1).length, 2);
  });
  await scenario('acknowledgement is not reported-value confirmation', () => {
    const r = waiting()[0]; eq(r.phase, P.WAITING); yes(r.acknowledged); yes(T.busy([r]));
    eq(T.observe(started(), props('1', t), t + 200, t + 200)[0].phase, P.SENDING);
  });
  await scenario('pulse acknowledgement does not wait for a fleeting mode1 report', () => {
    const r = waiting('bump', 1); eq(r[0].phase, P.ACKNOWLEDGED); eq(T.busy(r), false);
    eq(T.observe(r, props('0', t, 'bump'), t + 200, t + 200)[0].phase, P.ACKNOWLEDGED);
    eq(waiting('bump', 2)[0].phase, P.WAITING);
  });
  await scenario('unacknowledged pulse remains uncertain until report or deadline', () => {
    const r = T.received(started('bump', 1), 1, accepted(false), t + 100);
    eq(r[0].phase, P.WAITING); eq(T.expire(r, t + 30100)[0].phase, P.TIMED_OUT);
  });
  await scenario('unchanged or absent property time cannot confirm an old same value', () => {
    eq(T.observe(waiting(), props('1', t - 1000), t + 200, t + 200)[0].phase, P.WAITING);
    eq(T.observe(waiting(), [{ identifier: 'led', value: '1' }], t + 200, t + 200)[0].phase, P.WAITING);
  });
  await scenario('fresh matching property confirms without using phone-cloud clock comparison', () => {
    const r = T.observe(waiting(), props('1', t - 500), t + 200, t + 200);
    eq(r[0].phase, P.MATCHED); eq(T.busy(r), false);
  });
  await scenario('query begun before reply and unrelated timestamps cannot confirm', () => {
    eq(T.observe(waiting(), props('1', t), t + 50, t + 200)[0].phase, P.WAITING);
    const p = props('1', t - 1000); p.push({ identifier: 'temp', value: '25', time: t + 100 });
    eq(T.observe(waiting(), p, t + 200, t + 200)[0].phase, P.WAITING);
  });
  await scenario('strict value parsing rejects malformed matches', () => {
    for (const value of ['', ' ', '1abc', 'NaN', 'Infinity', '2']) eq(T.observe(waiting(), props(value, t), t + 200, t + 200)[0].phase, P.WAITING);
  });
  await scenario('deadline boundaries do not turn a timeout into execution failure', () => {
    const r = waiting(); eq(T.expire(r, t + 30099)[0].phase, P.WAITING);
    eq(T.expire(r, t + 30100)[0].phase, P.TIMED_OUT);
    eq(T.expire(started(), t + 30000)[0].phase, P.UNCERTAIN);
  });
  await scenario('late report and transport-uncertain result can be matched later', () => {
    const r = T.expire(waiting(), t + 30100);
    const late = T.observe(r, props('1', t + 20000), t + 40000, t + 40000)[0];
    eq(late.phase, P.MATCHED); yes(late.message.includes('较晚'));
    const uncertain = T.uncertain(started(), 1, 'network timeout', t + 100);
    eq(T.observe(uncertain, props('1', t), t + 200, t + 200)[0].phase, P.MATCHED);
  });
  await scenario('explicit rejection never becomes a match for that request', () => {
    const r = T.received(started(), 1, { ...accepted(), accepted: false, message: 'rejected' }, t + 100);
    eq(T.observe(r, props('1', t), t + 200, t + 200)[0].phase, P.REJECTED);
  });
  await scenario('late callback cannot overwrite a superseding request', () => {
    const old = T.uncertain(started(), 1, 'timeout', t + 100);
    const next = T.begin(old, 2, 'led', 2, props(), t + 200);
    eq(T.received(next, 1, accepted(), t + 300), plain(next));
    eq(T.uncertain(next, 1, 'late error', t + 300), plain(next));
  });
  await scenario('runtime journal restoration expires and filters invalid entries', () => {
    eq(T.restore(JSON.stringify(waiting()), t + 30100)[0].phase, P.TIMED_OUT);
    for (const raw of [undefined, '', '{bad', 'null', '3', '[null, {}, 1]']) eq(T.restore(raw, t), []);
    eq(T.restore(JSON.stringify([{ ...waiting()[0], phase: 99 }]), t), []);
  });
  await scenario('all transitions leave input state unchanged', () => {
    const a = Object.freeze(started().map(Object.freeze)); const original = plain(a);
    T.received(a, 1, accepted(), t + 100); T.uncertain(a, 1, 'network', t + 100); T.expire(a, t + 40000);
    T.observe(a, props('1', t), t + 100, t + 200); eq(a, original);
    eq(T.begin([], 1, 'unknown', 1, [], t), []); eq(T.begin([], 1, 'led', NaN, [], t), []);
  });

  let response, httpCode, transportError, creates, destroys, sent;
  const fakeHttp = { ResponseCode: { OK: 200 }, RequestMethod: { POST: 'POST', GET: 'GET' }, HttpDataType: { STRING: 'string' },
    createHttp: () => { creates++; return { request: async (url, opts) => { sent.push({ url, opts }); if (transportError) throw transportError;
      return { responseCode: httpCode, result: typeof response === 'string' ? response : JSON.stringify(response) }; }, destroy: () => { destroys++; } }; } };
  const { OneNetApiService } = load(source('service/OneNetApiService.ets'), {}, name => {
    if (name === '@kit.ArkTS') return { util: {} };
    if (name === '@kit.NetworkKit') return { http: fakeHttp };
    if (name.endsWith('/ControlTracker')) return model;
    if (name.endsWith('/OneNetConfig')) return { OneNetConfig: { PRODUCT_ID: 'test-product', DEVICE_NAME: 'test-device' } };
    throw Error('Unexpected module: ' + name);
  });
  const api = new OneNetApiService(); api.createAuthorization = async () => 'test-only';
  const resetApi = r => { response = r; httpCode = 200; transportError = undefined; creates = 0; destroys = 0; sent = []; };
  for (const [key, value] of [['led', 1], ['bump', 2], ['CO2threhold', 1800], ['droughtthrehold', 30], ['moistthrehold', 75], ['tempthrehold', 30], ['temp_low_threshold', 20]]) {
    await scenario('production API preserves payload and exposes device receipt: ' + key, async () => {
      resetApi({ code: 0, data: { code: 200, id: '7' } });
      const r = key === 'led' ? await api.setLedMode(value) : key === 'bump' ? await api.setBumpMode(value) : await api.setThreshold(key, value);
      yes(r.deviceAcknowledged); eq(sent.length, 1); eq(destroys, 1);
      eq(JSON.parse(sent[0].opts.extraData), { product_id: 'test-product', device_name: 'test-device', params: { [key]: value } });
    });
  }
  await scenario('production API does not hide nested rejection', async () => {
    resetApi({ code: 0, data: { code: 500 } }); eq((await api.setLedMode(1)).accepted, false); eq(destroys, 1);
  });
  await scenario('production API cleans up transport and parsing errors without retry', async () => {
    for (const mode of ['transport', 'http', 'json', 'missingCode']) {
      resetApi({ code: 0 });
      if (mode === 'transport') transportError = Error('test network timeout');
      if (mode === 'http') httpCode = 500;
      if (mode === 'json') response = '{invalid';
      if (mode === 'missingCode') response = {};
      checks++; await assert.rejects(api.setLedMode(1)); eq(sent.length, 1); eq(destroys, 1);
    }
  });
  await scenario('unsupported property fails before HTTP creation', async () => {
    resetApi({ code: 0 }); checks++; await assert.rejects(api.setThreshold('unknown', 1)); eq(creates, 0);
  });

  await scenario('page sends once, preserves old report and shows pending target', async () => {
    const { host, calls, api } = makeHost(); const post = deferred();
    api.setLedMode = value => { calls.posts.push(['led', value]); return post.promise; };
    const first = host.setLedMode(1); await host.setLedMode(2);
    eq(calls.posts.length, 1); eq(host.ledMode, 0); eq(host.properties[0].value, '0'); yes(host.pendingTarget('led', 1));
    post.resolve(accepted()); await first;
    eq(host.ledMode, 0); eq(host.properties[0].value, '0'); eq(host.controlRequests[0].phase, P.WAITING);
    eq(host.controlEnabled('led'), false);
  });
  await scenario('page confirms only actual new report then enables control again', async () => {
    const { host, calls, tick } = makeHost(); await host.setLedMode(1);
    calls.query = async () => props('1', t);
    await tick(); eq(host.ledMode, 1); eq(host.controlRequests[0].phase, P.MATCHED);
    yes(host.controlEnabled('led')); eq(host.confirmationTimer, -1); eq(calls.posts.length, 1);
  });
  await scenario('page confirms threshold without fabricating property value', async () => {
    const { host, calls, tick } = makeHost(); host.properties = [...props('20', t - 1000, 'tempthrehold'), ...props('10', t - 1000, 'temp_low_threshold')];
    calls.query = async () => props('20', t - 1000, 'tempthrehold'); host.editThreshold('tempthrehold'); host.thresholdDraft = '21';
    await host.applyThreshold('tempthrehold');
    eq(host.properties[0].value, '20'); eq(host.controlRequests[0].target, 21);
    calls.query = async () => props('21', t, 'tempthrehold'); await tick();
    eq(host.properties[0].value, '21'); eq(host.controlRequests[0].phase, P.MATCHED);
  });
  await scenario('automatic and manual reads share the loading guard', async () => {
    const { host, calls } = makeHost(); const read = deferred(); calls.query = () => read.promise;
    const running = host.autoRefreshTask(); await host.autoRefreshTask(); await host.refresh(); await host.setLedMode(1);
    eq(calls.gets, 1); eq(calls.posts.length, 0); read.resolve(props()); await running; eq(host.loading, false);
  });
  await scenario('confirmation works even with normal auto refresh off', async () => {
    const { host, calls, tick } = makeHost(); host.autoRefresh = false;
    await host.setLedMode(1); yes(host.confirmationTimer !== -1);
    calls.query = async () => props('1', t); await tick(); eq(host.controlRequests[0].phase, P.MATCHED);
  });
  await scenario('timeout releases pending lock and never retransmits', async () => {
    const { host, calls, state, tick } = makeHost(); await host.setLedMode(1);
    state.now = t + 30000; await tick(); eq(host.controlRequests[0].phase, P.TIMED_OUT);
    eq(host.confirmationTimer, -1); eq(calls.posts.length, 1); yes(host.controlAttention('led'));
  });
  await scenario('query failure keeps command state and does not resend', async () => {
    const { host, calls, tick } = makeHost(); await host.setLedMode(1);
    calls.query = async () => { throw Error('test read failed'); }; await tick();
    eq(host.controlRequests[0].phase, P.WAITING); eq(calls.posts.length, 1); yes(host.errorMessage.includes('刷新失败'));
    calls.query = async () => props('1', t); await host.refresh(); eq(host.controlRequests[0].phase, P.MATCHED);
  });
  await scenario('leaving during a query ignores the late response and clears timers', async () => {
    const { host, calls, state } = makeHost(); const read = deferred(); calls.query = () => read.promise;
    const running = host.refresh(); host.aboutToDisappear(); read.resolve(props('1', t)); await running;
    eq(host.ledMode, 0); eq(host.saveCount, 0); eq(state.timers.size, 0); yes(host.autoStopped);
  });
  await scenario('leaving during POST preserves only the runtime journal, no detached UI updates', async () => {
    const { host, calls, api, state } = makeHost(); const post = deferred();
    api.setLedMode = () => post.promise;
    const running = host.setLedMode(1); host.aboutToDisappear(); post.resolve(accepted()); await running;
    eq(host.controlRequests[0].phase, P.SENDING); eq(calls.gets, 0); eq(state.timers.size, 0);
    const restored = T.restore(state.storage.get('monitorControlRequests'), t); eq(restored[0].phase, P.WAITING);
  });
  await scenario('pulse acknowledgement leaves the mode at the reported value', async () => {
    const { host, calls } = makeHost(); host.properties = props('0', t - 1000, 'bump');
    calls.query = async () => props('0', t, 'bump'); await host.setBumpMode(1);
    eq(host.bumpMode, 0); eq(host.controlRequests[0].phase, P.ACKNOWLEDGED); eq(host.confirmationTimer, -1);
  });
  await scenario('offline or stale state cannot trigger POST', async () => {
    const { host, calls, state } = makeHost(); state.now = t + 30000; await host.setLedMode(1);
    eq(calls.posts.length, 0); yes(host.errorMessage.includes('过期')); eq(host.controlEnabled('led'), false);
  });
  await scenario('transport error reports uncertainty and never changes mode', async () => {
    const { host, api, calls } = makeHost(); api.setLedMode = async () => { calls.posts.push(['led', 1]); throw Error('test timeout'); };
    await host.setLedMode(1); eq(host.controlRequests[0].phase, P.UNCERTAIN); eq(host.ledMode, 0); eq(calls.posts.length, 1);
  });
  await scenario('temperature pair labels and units', () => {
    eq(models.PropertyHelper.getLabel('tempthrehold'), '通风温度上限');
    eq(models.PropertyHelper.getLabel('temp_low_threshold'), '加热温度下限');
    eq(models.PropertyHelper.getUnit('temp_low_threshold'), '℃');
    yes(models.PropertyHelper.isKnownProperty('temp_low_threshold')); yes(T.supported('temp_low_threshold'));
  });
  await scenario('threshold draft can jump to an exact value and sends only on apply', async () => {
    const { host, calls } = makeHost();
    host.properties = props('2000', t - 1000, 'CO2threhold');
    host.editThreshold('CO2threhold');
    eq(host.thresholdEditorId, 'CO2threhold'); eq(host.thresholdDraft, '2000');
    eq(host.thresholdRange('CO2threhold'), { min: 350, max: 4000 });
    for (const draft of ['', '349', '4001', '1750.5', '2e3', '2000']) {
      host.thresholdDraft = draft;
      await host.applyThreshold('CO2threhold');
    }
    eq(calls.posts, []);
    host.thresholdDraft = '1777';
    eq(host.thresholdDraftValue(), 1777); eq(calls.posts, []);
    calls.query = async () => props('2000', t - 1000, 'CO2threhold');
    await Promise.all([host.applyThreshold('CO2threhold'), host.applyThreshold('CO2threhold')]);
    eq(calls.posts, [['CO2threhold', 1777]]);
    eq(host.thresholdEditorId, '');
    eq(host.properties[0].value, '2000'); eq(host.controlRequests[0].target, 1777);
  });
  await scenario('temperature range requires peer and rejects stale draft after refresh', async () => {
    const { host, calls } = makeHost();
    host.properties = props('30', t - 1000, 'tempthrehold');
    host.editThreshold('tempthrehold'); eq(host.thresholdEditorId, '');
    host.properties = [...host.properties, ...props('20', t - 1000, 'temp_low_threshold')];
    eq(host.thresholdRange('tempthrehold'), { min: 21, max: 100 });
    eq(host.thresholdRange('temp_low_threshold'), { min: 0, max: 29 });
    host.editThreshold('tempthrehold'); host.thresholdDraft = '25';
    host.properties = [host.properties[0], ...props('25', t, 'temp_low_threshold')];
    eq(host.thresholdRange('tempthrehold'), { min: 26, max: 100 });
    await host.applyThreshold('tempthrehold'); eq(calls.posts, []);
    eq(host.thresholdDraft, '25'); eq(host.thresholdEditorId, 'tempthrehold');
  });
  await scenario('soil ranges allow equality and block edits while the peer is awaiting a report', async () => {
    const { host, calls } = makeHost();
    host.properties = [...props('25', t - 1000, 'droughtthrehold'), ...props('80', t - 1000, 'moistthrehold')];
    eq(host.thresholdRange('droughtthrehold'), { min: 0, max: 80 });
    eq(host.thresholdRange('moistthrehold'), { min: 25, max: 100 });
    host.editThreshold('droughtthrehold'); host.thresholdDraft = '81';
    await host.applyThreshold('droughtthrehold'); eq(calls.posts, []);
    host.thresholdDraft = '80';
    calls.query = async () => host.properties;
    await host.applyThreshold('droughtthrehold');
    eq(calls.posts, [['droughtthrehold', 80]]); eq(host.controlRequests[0].phase, P.WAITING);
    eq(host.thresholdEditable('moistthrehold'), false);
    host.editThreshold('moistthrehold'); eq(host.thresholdEditorId, '');
  });
  await scenario('threshold editor cannot send after cancellation or while the device is offline', async () => {
    const { host, calls } = makeHost();
    host.properties = props('2000', t - 1000, 'CO2threhold');
    host.editThreshold('CO2threhold'); host.thresholdDraft = '1900';
    host.thresholdEditorId = '';
    await host.applyThreshold('CO2threhold');
    host.editThreshold('CO2threhold'); eq(host.thresholdDraft, '2000');
    host.thresholdDraft = '1900'; host.online = false;
    await host.applyThreshold('CO2threhold'); eq(calls.posts, []);
    eq(host.thresholdEditorId, 'CO2threhold');
  });
  await scenario('lower temperature uses existing confirmation and actual report', async () => {
    const { host, calls } = makeHost();
    host.properties = [...props('30', t - 1000, 'tempthrehold'), ...props('20', t - 1000, 'temp_low_threshold')];
    calls.query = async () => [...props('30', t, 'tempthrehold'), ...props('21', t, 'temp_low_threshold')];
    host.editThreshold('temp_low_threshold'); host.thresholdDraft = '21';
    await host.applyThreshold('temp_low_threshold');
    eq(calls.posts, [['temp_low_threshold', 21]]); eq(host.controlRequests[0].phase, P.MATCHED);
  });
  console.log('PASS:', scenarios, 'control scenarios,', checks, 'checks. Production tracker/API/page methods; no network, credentials or ArkUI runtime.');
}
main().catch(e => { console.error(e); process.exitCode = 1; });
