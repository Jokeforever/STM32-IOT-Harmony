import { DeviceProperty } from './DeviceModels';

export enum ControlPhase {
  SENDING = 0,
  WAITING = 1,
  MATCHED = 2,
  ACKNOWLEDGED = 3,
  TIMED_OUT = 4,
  REJECTED = 5,
  UNCERTAIN = 6
}

export interface ControlReceipt {
  accepted: boolean;
  deviceAcknowledged: boolean;
  message: string;
  requestId: string;
  deviceMessageId: string;
}

export interface ControlSetData {
  id?: string;
  code?: number | string;
  msg?: string;
}

export interface ControlSetResponse {
  code?: number | string;
  msg?: string;
  request_id?: string;
  data?: ControlSetData | null;
}

export interface ControlRequest {
  id: number;
  identifier: string;
  target: number;
  baselineTime: number;
  startedAt: number;
  finishedAt: number;
  deadline: number;
  phase: ControlPhase;
  acknowledged: boolean;
  message: string;
}

export class ControlTracker {
  static readonly WAIT_MS: number = 30000;
  static readonly POLL_MS: number = 2000;
  private static sequence: number = 0;

  static nextId(): number { ControlTracker.sequence++; return ControlTracker.sequence; }

  private static code(value: number | string | undefined): number | undefined {
    if (typeof value !== 'number' && typeof value !== 'string') { return undefined; }
    if (typeof value === 'string' && value.trim() === '') { return undefined; }
    const code = Number(value);
    return Number.isInteger(code) ? code : undefined;
  }

  static decode(response: ControlSetResponse): ControlReceipt {
    if (response === undefined || response === null || typeof response !== 'object') { throw new Error('属性设置响应格式无效'); }
    const code = ControlTracker.code(response.code);
    if (code === undefined) { throw new Error('属性设置响应缺少合法code，结果未确认'); }
    const receipt: ControlReceipt = {
      accepted: code === 0, deviceAcknowledged: false,
      message: typeof response.msg === 'string' ? response.msg : '',
      requestId: typeof response.request_id === 'string' ? response.request_id : '', deviceMessageId: ''
    };
    if (code !== 0) { receipt.message = '平台返回 ' + code.toString() + ' ' + receipt.message; return receipt; }
    const data = response.data;
    if (data === undefined || data === null) { return receipt; }
    if (typeof data !== 'object' || Array.isArray(data)) { throw new Error('设备应答格式无效，结果未确认'); }
    receipt.deviceMessageId = typeof data.id === 'string' ? data.id : '';
    if (data.code === undefined) { return receipt; }
    const deviceCode = ControlTracker.code(data.code);
    if (deviceCode === undefined) { throw new Error('设备应答缺少合法code，结果未确认'); }
    receipt.deviceAcknowledged = deviceCode === 200;
    receipt.accepted = deviceCode === 200;
    receipt.message = '设备应答 ' + deviceCode.toString() + ' ' + (typeof data.msg === 'string' ? data.msg : '');
    return receipt;
  }

  static supported(identifier: string): boolean {
    return identifier === 'led' || identifier === 'bump' || identifier === 'CO2threhold' ||
      identifier === 'droughtthrehold' || identifier === 'moistthrehold' || identifier === 'tempthrehold';
  }

  private static copy(request: ControlRequest): ControlRequest {
    return {
      id: request.id, identifier: request.identifier, target: request.target, baselineTime: request.baselineTime,
      startedAt: request.startedAt, finishedAt: request.finishedAt, deadline: request.deadline,
      phase: request.phase, acknowledged: request.acknowledged, message: request.message
    };
  }

  static find(requests: ControlRequest[], identifier: string): ControlRequest | undefined {
    for (let i: number = 0; i < requests.length; i++) { if (requests[i].identifier === identifier) { return requests[i]; } }
    return undefined;
  }

  static busy(requests: ControlRequest[], identifier: string = ''): boolean {
    for (let i: number = 0; i < requests.length; i++) {
      if ((identifier === '' || requests[i].identifier === identifier) &&
        (requests[i].phase === ControlPhase.SENDING || requests[i].phase === ControlPhase.WAITING)) { return true; }
    }
    return false;
  }

  static propertyTime(properties: DeviceProperty[], identifier: string): number {
    for (let i: number = 0; i < properties.length; i++) {
      if (properties[i].identifier === identifier) {
        const time = properties[i].time;
        return typeof time === 'number' && Number.isInteger(time) && time > 0 && time <= 8640000000000000 ? time : 0;
      }
    }
    return 0;
  }

  static begin(requests: ControlRequest[], id: number, identifier: string, target: number,
    properties: DeviceProperty[], now: number): ControlRequest[] {
    if (!ControlTracker.supported(identifier) || !Number.isInteger(target) || ControlTracker.busy(requests, identifier)) {
      return requests.slice();
    }
    const next: ControlRequest[] = [];
    for (let i: number = 0; i < requests.length; i++) {
      if (requests[i].identifier !== identifier) { next.push(ControlTracker.copy(requests[i])); }
    }
    next.push({
      id: id, identifier: identifier, target: target, baselineTime: ControlTracker.propertyTime(properties, identifier),
      startedAt: now, finishedAt: 0, deadline: now + ControlTracker.WAIT_MS,
      phase: ControlPhase.SENDING, acknowledged: false, message: ''
    });
    return next;
  }

  static received(requests: ControlRequest[], id: number, receipt: ControlReceipt, now: number): ControlRequest[] {
    const next: ControlRequest[] = [];
    for (let i: number = 0; i < requests.length; i++) {
      const request = ControlTracker.copy(requests[i]);
      if (request.id === id) {
        request.finishedAt = now; request.deadline = now + ControlTracker.WAIT_MS;
        request.acknowledged = receipt.deviceAcknowledged; request.message = receipt.message;
        request.phase = !receipt.accepted ? ControlPhase.REJECTED :
          (request.identifier === 'bump' && request.target === 1 && receipt.deviceAcknowledged ?
            ControlPhase.ACKNOWLEDGED : ControlPhase.WAITING);
      }
      next.push(request);
    }
    return next;
  }

  static uncertain(requests: ControlRequest[], id: number, message: string, now: number): ControlRequest[] {
    const next: ControlRequest[] = [];
    for (let i: number = 0; i < requests.length; i++) {
      const request = ControlTracker.copy(requests[i]);
      if (request.id === id) { request.phase = ControlPhase.UNCERTAIN; request.finishedAt = now; request.message = message; }
      next.push(request);
    }
    return next;
  }

  static expire(requests: ControlRequest[], now: number): ControlRequest[] {
    const next: ControlRequest[] = [];
    for (let i: number = 0; i < requests.length; i++) {
      const request = ControlTracker.copy(requests[i]);
      if (now >= request.deadline) {
        if (request.phase === ControlPhase.WAITING) { request.phase = ControlPhase.TIMED_OUT; }
        else if (request.phase === ControlPhase.SENDING) { request.phase = ControlPhase.UNCERTAIN; request.message = '请求尚无明确结果'; }
      }
      next.push(request);
    }
    return next;
  }

  static observe(requests: ControlRequest[], properties: DeviceProperty[], queryStartedAt: number, now: number): ControlRequest[] {
    const next: ControlRequest[] = [];
    for (let i: number = 0; i < requests.length; i++) {
      const request = ControlTracker.copy(requests[i]);
      if ((request.phase === ControlPhase.WAITING || request.phase === ControlPhase.TIMED_OUT ||
        request.phase === ControlPhase.UNCERTAIN || request.phase === ControlPhase.ACKNOWLEDGED) &&
        request.finishedAt > 0 && queryStartedAt >= request.finishedAt) {
        for (let j: number = 0; j < properties.length; j++) {
          const property = properties[j];
          if (property.identifier === request.identifier && typeof property.value === 'string' && property.value.trim() !== '' &&
            Number(property.value) === request.target && ControlTracker.propertyTime(properties, property.identifier) > request.baselineTime) {
            request.phase = ControlPhase.MATCHED;
            request.message = now >= request.deadline ? '确认较晚，上报值已匹配' : '';
            break;
          }
        }
      }
      next.push(request);
    }
    return ControlTracker.expire(next, now);
  }

  static restore(raw: string | undefined, now: number): ControlRequest[] {
    if (raw === undefined || raw === '') { return []; }
    try {
      const parsed = JSON.parse(raw) as ControlRequest[];
      if (!Array.isArray(parsed)) { return []; }
      const requests: ControlRequest[] = [];
      for (let i: number = 0; i < parsed.length; i++) {
        const r = parsed[i];
        if (r === undefined || r === null || !ControlTracker.supported(r.identifier) || !Number.isInteger(r.id) || r.id <= 0 ||
          !Number.isInteger(r.target) || !Number.isFinite(r.baselineTime) || r.baselineTime < 0 ||
          !Number.isFinite(r.startedAt) || !Number.isFinite(r.finishedAt) || !Number.isFinite(r.deadline) ||
          !Number.isInteger(r.phase) || r.phase < 0 || r.phase > ControlPhase.UNCERTAIN ||
          typeof r.acknowledged !== 'boolean' || typeof r.message !== 'string') { continue; }
        if (ControlTracker.find(requests, r.identifier) === undefined) { requests.push(ControlTracker.copy(r)); }
      }
      return ControlTracker.expire(requests, now);
    } catch (error) { return []; }
  }
}
