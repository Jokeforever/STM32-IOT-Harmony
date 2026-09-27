export enum DeviceState {
  OFFLINE = 0,
  ONLINE = 1
}

export enum LedMode {
  OFF = 0,
  ON = 1,
  AUTO = 2
}

@Observed
export class DeviceInfo {
  public id: number;
  public name: string;
  public state: DeviceState;
  public temperature: number;
  public humidity: number;
  public properties: DeviceProperty[] = [];
  public isRealDevice: boolean;
  public lastSyncTime: string;
  public lastOnlineTime: string;

  constructor(
    id: number,
    name: string,
    state: DeviceState,
    temperature: number,
    humidity: number,
    isRealDevice: boolean = false
  ) {
    this.id = id;
    this.name = name;
    this.state = state;
    this.temperature = temperature;
    this.humidity = humidity;
    this.isRealDevice = isRealDevice;
    this.lastSyncTime = '--';
    this.lastOnlineTime = '--';
  }

  public toggleState(): void {
    this.state =
      this.state === DeviceState.ONLINE
        ? DeviceState.OFFLINE
        : DeviceState.ONLINE;
  }
}

export interface DeviceProperty {
  identifier: string;
  value: string;
  name?: string;
  dataType?: string;
  accessMode?: string;
  time?: number;
}

export interface DeviceCloudStatus {
  online: boolean;
  lastOnlineTime: string;
}

export interface DeviceDetailParam {
  id: number;
  name: string;
  state: DeviceState;
  temperature: number;
  humidity: number;
  properties: DeviceProperty[];
  isRealDevice: boolean;
  lastSyncTime: string;
  lastOnlineTime: string;
}

export class PropertyHelper {
  static getValue(properties: DeviceProperty[], identifier: string): string {
    for (let i: number = 0; i < properties.length; i++) {
      if (properties[i].identifier === identifier) {
        return properties[i].value;
      }
    }
    return '';
  }

  static getLabel(identifier: string): string {
    if (identifier === 'temp') {
      return '空气温度';
    } else if (identifier === 'humi') {
      return '空气湿度';
    } else if (identifier === 'light') {
      return '光照强度';
    } else if (identifier === 'ppm') {
      return 'CO2 浓度';
    } else if (identifier === 'moist') {
      return '土壤湿度';
    } else if (identifier === 'water_vol') {
      return '水量';
    } else if (identifier === 'led') {
      return 'LED 模式';
    } else if (identifier === 'bump') {
      return '水泵模式';
    } else if (identifier === 'CO2threhold') {
      return 'CO2 阈值';
    } else if (identifier === 'droughtthrehold') {
      return '干旱阈值';
    } else if (identifier === 'moistthrehold') {
      return '湿润阈值';
    } else if (identifier === 'tempthrehold') {
      return '温度阈值';
    }
    return identifier;
  }

  static getUnit(identifier: string): string {
    if (identifier === 'temp' || identifier === 'tempthrehold') {
      return '℃';
    } else if (
      identifier === 'humi' ||
      identifier === 'moist' ||
      identifier === 'moistthrehold' ||
      identifier === 'droughtthrehold'
    ) {
      return '%';
    } else if (identifier === 'light') {
      return 'lx';
    } else if (
      identifier === 'ppm' ||
      identifier === 'CO2threhold'
    ) {
      return 'ppm';
    }
    return '';
  }

  static formatValue(identifier: string, value: string): string {
    if (value === undefined || value === null || value === '') {
      return '--';
    }

    if (identifier === 'led') {
      return PropertyHelper.getLedModeText(value);
    } else if (identifier === 'bump') {
      return PropertyHelper.getBumpModeText(value);
    }

    return value;
  }

  static getLedModeText(value: string): string {
    if (value === '0') {
      return '关闭';
    } else if (value === '1') {
      return '常亮';
    } else if (value === '2') {
      return '自动';
    }
    return '--';
  }

  static getBumpModeText(value: string): string {
    if (value === '0') {
      return '关闭';
    } else if (value === '1') {
      return '开启';
    } else if (value === '2') {
      return '自动';
    }
    return '--';
  }

  static isKnownProperty(identifier: string): boolean {
    return identifier === 'temp' ||
      identifier === 'humi' ||
      identifier === 'light' ||
      identifier === 'ppm' ||
      identifier === 'moist' ||
      identifier === 'water_vol' ||
      identifier === 'led' ||
      identifier === 'bump' ||
      identifier === 'CO2threhold' ||
      identifier === 'droughtthrehold' ||
      identifier === 'moistthrehold' ||
      identifier === 'tempthrehold';
  }
}

