import { CachedAppState, DeviceProperty, TrendHistories, TrendPoint, TrendRange, TrendViewport } from './DeviceModels';

// 记录本机实际取得的上报；time沿用接口毫秒时间，不用查询时间补造采样时间。
export class TrendHistory {
  static readonly VERSION: number = 2;
  static readonly LIMIT: number = 20;
  static readonly MAX_ZOOM: number = 10;

  private static validPoint(point: TrendPoint): boolean {
    return point !== undefined && point !== null && typeof point === 'object' &&
      typeof point.time === 'number' && Number.isFinite(point.time) && Number.isInteger(point.time) &&
      point.time > 0 && point.time <= 8640000000000000 &&
      typeof point.value === 'number' && Number.isFinite(point.value);
  }

  static normalize(points: TrendPoint[]): TrendPoint[] {
    if (points === undefined || points === null || !Array.isArray(points)) { return []; }
    const sorted: TrendPoint[] = [];
    for (let i: number = 0; i < points.length; i++) {
      if (TrendHistory.validPoint(points[i])) { sorted.push({ time: points[i].time, value: points[i].value }); }
    }
    sorted.sort((a: TrendPoint, b: TrendPoint) => a.time - b.time);
    const unique: TrendPoint[] = [];
    for (let i: number = 0; i < sorted.length; i++) {
      if (unique.length > 0 && unique[unique.length - 1].time === sorted[i].time) { unique[unique.length - 1] = sorted[i]; }
      else { unique.push(sorted[i]); }
    }
    return unique.slice(Math.max(0, unique.length - TrendHistory.LIMIT));
  }

  private static reading(properties: DeviceProperty[], identifier: string): DeviceProperty | undefined {
    if (properties === undefined || properties === null || !Array.isArray(properties)) { return undefined; }
    for (let i: number = 0; i < properties.length; i++) {
      if (properties[i] !== undefined && properties[i] !== null && properties[i].identifier === identifier) { return properties[i]; }
    }
    return undefined;
  }

  private static sensorValid(properties: DeviceProperty[], bit: number): boolean {
    const status = TrendHistory.reading(properties, 'sensor_status');
    if (status === undefined) { return true; }
    if (typeof status.value !== 'string' || status.value.trim() === '') { return false; }
    const mask = Number(status.value);
    return Number.isInteger(mask) && mask >= 0 && mask <= 2147483647 && (mask & (1 << bit)) !== 0;
  }

  static append(points: TrendPoint[], properties: DeviceProperty[], identifier: string, bit: number): TrendPoint[] {
    const history = TrendHistory.normalize(points);
    const property = TrendHistory.reading(properties, identifier);
    if (property === undefined || !TrendHistory.sensorValid(properties, bit) ||
      typeof property.value !== 'string' || property.value.trim() === '' || property.time === undefined) { return history; }
    const point: TrendPoint = { time: property.time, value: Number(property.value) };
    if (!TrendHistory.validPoint(point)) { return history; }
    const last = history.length > 0 ? history[history.length - 1] : undefined;
    if (last !== undefined && point.time < last.time) { return history; }
    if (last !== undefined && point.time === last.time) {
      // 同一时间只保留一份；若云端修正数值，替换而不增加数量。
      history[history.length - 1] = point;
    } else { history.push(point); }
    return history.slice(Math.max(0, history.length - TrendHistory.LIMIT));
  }

  static fromRaw(raw: string | undefined): TrendHistories {
    let temp: TrendPoint[] = [];
    let humi: TrendPoint[] = [];
    if (raw === undefined || raw === '') { return { tempHistory: temp, humiHistory: humi }; }
    try {
      const cached = JSON.parse(raw) as CachedAppState;
      if (cached === undefined || cached === null || typeof cached !== 'object') { return { tempHistory: temp, humiHistory: humi }; }
      if (cached.historyVersion === TrendHistory.VERSION) {
        temp = TrendHistory.normalize(cached.tempHistory);
        humi = TrendHistory.normalize(cached.humiHistory);
      }
      // 旧number[]没有可靠时间，仅用有时间的缓存属性建立一个起点。
      return {
        tempHistory: TrendHistory.append(temp, cached.properties, 'temp', 0),
        humiHistory: TrendHistory.append(humi, cached.properties, 'humi', 1)
      };
    } catch (error) { return { tempHistory: temp, humiHistory: humi }; }
  }

  static collect(raw: string | undefined, properties: DeviceProperty[]): TrendHistories {
    const cached = TrendHistory.fromRaw(raw);
    return {
      tempHistory: TrendHistory.append(cached.tempHistory, properties, 'temp', 0),
      humiHistory: TrendHistory.append(cached.humiHistory, properties, 'humi', 1)
    };
  }

  static latestReportTime(properties: DeviceProperty[]): number {
    let latest: number = 0;
    if (properties === undefined || properties === null || !Array.isArray(properties)) { return latest; }
    for (let i: number = 0; i < properties.length; i++) {
      const property = properties[i];
      if (property === undefined || property === null) { continue; }
      const time = property.time;
      if (typeof time === 'number' && Number.isInteger(time) && time > latest && time <= 8640000000000000) { latest = time; }
    }
    return latest;
  }

  static displayRange(points: TrendPoint[], defaultMin: number, defaultMax: number,
    autoFit: boolean, minimumSpan: number): TrendRange {
    const samples = TrendHistory.normalize(points);
    if (!autoFit || samples.length === 0) { return { min: defaultMin, max: defaultMax }; }
    let low = samples[0].value;
    let high = samples[0].value;
    for (let i: number = 1; i < samples.length; i++) {
      low = Math.min(low, samples[i].value);
      high = Math.max(high, samples[i].value);
    }
    // 最小跨度与留白避免恒定读数退化成零量程，也不夸大极小波动。
    const span = Math.max(high - low, minimumSpan);
    const center = low + (high - low) / 2;
    const step = span <= 10 ? 1 : (span <= 50 ? 5 : 10);
    const min = Math.max(Math.min(defaultMin, low), Math.floor((center - span * 0.65) / step) * step);
    const max = Math.min(Math.max(defaultMax, high), Math.ceil((center + span * 0.65) / step) * step);
    return { min: min, max: max };
  }

  static viewportFor(points: TrendPoint[], range: TrendRange): TrendViewport {
    const samples = TrendHistory.normalize(points);
    let start: number = samples.length > 0 ? samples[0].time : 1;
    let end: number = samples.length > 0 ? samples[samples.length - 1].time : 1001;
    if (start === end) { start = Math.max(1, start - 500); end = start + 1000; }
    return { timeMin: start, timeMax: end, valueMin: range.min, valueMax: range.max };
  }

  private static boundedRange(min: number, max: number, lower: number, upper: number): TrendRange {
    const span = Math.min(upper - lower, Math.max((upper - lower) / TrendHistory.MAX_ZOOM, max - min));
    const start = Math.max(lower, Math.min(upper - span, min));
    return { min: start, max: start + span };
  }

  private static zoomRange(min: number, max: number, lower: number, upper: number,
    factor: number, anchor: number): TrendRange {
    const pivot = Math.max(0, Math.min(1, anchor));
    const size = Math.max((upper - lower) / TrendHistory.MAX_ZOOM, Math.min(upper - lower, (max - min) / factor));
    const start = min + (max - min) * pivot - size * pivot;
    return TrendHistory.boundedRange(start, start + size, lower, upper);
  }

  static zoomViewport(view: TrendViewport, bounds: TrendViewport, factor: number,
    anchorX: number, anchorY: number, timeAxis: boolean, valueAxis: boolean): TrendViewport {
    if (!Number.isFinite(factor) || factor <= 0 || !Number.isFinite(anchorX) || !Number.isFinite(anchorY)) {
      return { timeMin: view.timeMin, timeMax: view.timeMax, valueMin: view.valueMin, valueMax: view.valueMax };
    }
    const time: TrendRange = timeAxis ? TrendHistory.zoomRange(view.timeMin, view.timeMax, bounds.timeMin, bounds.timeMax, factor, anchorX) :
      { min: view.timeMin, max: view.timeMax };
    const value: TrendRange = valueAxis ? TrendHistory.zoomRange(view.valueMin, view.valueMax, bounds.valueMin, bounds.valueMax, factor, anchorY) :
      { min: view.valueMin, max: view.valueMax };
    return { timeMin: time.min, timeMax: time.max, valueMin: value.min, valueMax: value.max };
  }

  static panViewport(view: TrendViewport, bounds: TrendViewport, offsetX: number, offsetY: number): TrendViewport {
    const dx = Number.isFinite(offsetX) ? offsetX * (view.timeMax - view.timeMin) : 0;
    const dy = Number.isFinite(offsetY) ? offsetY * (view.valueMax - view.valueMin) : 0;
    const time = TrendHistory.boundedRange(view.timeMin + dx, view.timeMax + dx, bounds.timeMin, bounds.timeMax);
    const value = TrendHistory.boundedRange(view.valueMin + dy, view.valueMax + dy, bounds.valueMin, bounds.valueMax);
    return { timeMin: time.min, timeMax: time.max, valueMin: value.min, valueMax: value.max };
  }

  static viewportPoints(points: TrendPoint[], view: TrendViewport, width: number, height: number): number[][] {
    if (!Number.isFinite(width) || !Number.isFinite(height) || width <= 8 || height <= 16 ||
      view.timeMax <= view.timeMin || view.valueMax <= view.valueMin) { return []; }
    const result: number[][] = [];
    for (let i: number = 0; i < points.length; i++) {
      // 不将超出视窗的数值硬压到边线上；保留线段坐标，由图表容器裁剪。
      result.push([
        4 + (points[i].time - view.timeMin) / (view.timeMax - view.timeMin) * (width - 8),
        height - 8 - (points[i].value - view.valueMin) / (view.valueMax - view.valueMin) * (height - 16)
      ]);
    }
    return result;
  }

  static wheelZoomFactor(delta: number): number {
    if (!Number.isFinite(delta) || delta === 0) { return 1; }
    return Math.exp(-Math.max(-120, Math.min(120, delta)) * 0.006);
  }

  static plotPoints(points: TrendPoint[], min: number, max: number, width: number, height: number = 96): number[][] {
    if (points.length === 0 || !Number.isFinite(width) || width <= 8 ||
      !Number.isFinite(height) || height <= 16) { return []; }
    const result: number[][] = [];
    const start = points[0].time;
    const end = points[points.length - 1].time;
    for (let i: number = 0; i < points.length; i++) {
      const x = 4 + (end === start ? 0 : (width - 8) * (points[i].time - start) / (end - start));
      const value = Math.max(min, Math.min(max, points[i].value));
      const y = max === min ? height - 8 : height - 8 - (value - min) / (max - min) * (height - 16);
      result.push([x, y]);
    }
    return result;
  }

  static formatPointTime(time: number, milliseconds: boolean = false): string {
    const date = new Date(time);
    if (!Number.isFinite(date.getTime())) { return '--'; }
    const pad = (value: number): string => value < 10 ? '0' + value.toString() : value.toString();
    return pad(date.getMonth() + 1) + '-' + pad(date.getDate()) + ' ' +
      pad(date.getHours()) + ':' + pad(date.getMinutes()) + ':' + pad(date.getSeconds()) +
      (milliseconds ? '.' + date.getMilliseconds().toString().padStart(3, '0') : '');
  }
}
