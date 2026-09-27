import { util } from '@kit.ArkTS';
import { http } from '@kit.NetworkKit';
import { OneNetConfig } from '../config/OneNetConfig';
import {
  DeviceCloudStatus,
  DeviceProperty
} from '../model/DeviceModels';

interface OneNetPropertyItem {
  identifier?: string;
  value?: string | number | boolean | null;
  data_type?: string;
  access_mode?: string;
  name?: string;
  time?: number;
}

interface OneNetQueryResponse {
  code?: number | string;
  msg?: string;
  data?: OneNetPropertyItem[] | null;
}

interface OneNetSetResponse {
  code?: number | string;
  msg?: string;
}

interface OneNetDeviceDetailData {
  status?: number;
  last_time?: string;
}

interface OneNetDeviceDetailResponse {
  code?: number | string;
  msg?: string;
  data?: OneNetDeviceDetailData | null;
}

interface OneNetSetBody {
  product_id: string;
  device_name: string;
  params: OneNetSetParams;
}

interface OneNetSetParams {
  led?: number;
  bump?: number;
}

export class OneNetApiService {
  async queryDeviceProperties(): Promise<DeviceProperty[]> {
    const httpRequest = http.createHttp();

    try {
      const authorization = await this.createAuthorization();
      const url =
        'https://iot-api.heclouds.com/thingmodel/query-device-property' +
        '?product_id=' + encodeURIComponent(OneNetConfig.PRODUCT_ID) +
        '&device_name=' + encodeURIComponent(OneNetConfig.DEVICE_NAME) +
        '&_t=' + Date.now().toString();

      const response = await httpRequest.request(url, {
        method: http.RequestMethod.GET,
        header: {
          authorization: authorization
        },
        expectDataType: http.HttpDataType.STRING,
        usingCache: false,
        connectTimeout: 10000,
        readTimeout: 10000
      });

      this.ensureHttpOk(response.responseCode);

      const result =
        JSON.parse(response.result as string) as OneNetQueryResponse;

      this.ensureApiOk(result.code, result.msg, '查询设备属性');

      const items = result.data ?? [];
      const properties: DeviceProperty[] = [];

      for (let i: number = 0; i < items.length; i++) {
        const identifier = items[i].identifier;
        if (identifier === undefined || identifier === '') {
          continue;
        }

        properties.push({
          identifier: identifier,
          value: this.formatValue(items[i].value),
          name: items[i].name,
          dataType: items[i].data_type,
          accessMode: items[i].access_mode,
          time: items[i].time
        });
      }

      return properties;
    } finally {
      httpRequest.destroy();
    }
  }

  async queryDeviceStatus(): Promise<DeviceCloudStatus> {
    const httpRequest = http.createHttp();

    try {
      const authorization = await this.createAuthorization();
      const url =
        'https://iot-api.heclouds.com/device/detail' +
        '?product_id=' + encodeURIComponent(OneNetConfig.PRODUCT_ID) +
        '&device_name=' + encodeURIComponent(OneNetConfig.DEVICE_NAME) +
        '&_t=' + Date.now().toString();

      const response = await httpRequest.request(url, {
        method: http.RequestMethod.GET,
        header: {
          authorization: authorization
        },
        expectDataType: http.HttpDataType.STRING,
        usingCache: false,
        connectTimeout: 10000,
        readTimeout: 10000
      });

      this.ensureHttpOk(response.responseCode);

      const result =
        JSON.parse(response.result as string) as OneNetDeviceDetailResponse;

      this.ensureApiOk(result.code, result.msg, '查询设备状态');

      const data = result.data;
      if (data === undefined || data === null) {
        return {
          online: false,
          lastOnlineTime: '--'
        };
      }

      return {
        online: data.status === 1,
        lastOnlineTime: data.last_time ?? '--'
      };
    } finally {
      httpRequest.destroy();
    }
  }

  async setLedMode(mode: number): Promise<void> {
    await this.setDeviceProperty('led', mode);
  }

  async setBumpMode(mode: number): Promise<void> {
    await this.setDeviceProperty('bump', mode);
  }

  private async setDeviceProperty(
    identifier: string,
    value: number
  ): Promise<void> {
    const httpRequest = http.createHttp();

    try {
      const authorization = await this.createAuthorization();
      const params: OneNetSetParams = {};
      if (identifier === 'led') {
        params.led = value;
      } else if (identifier === 'bump') {
        params.bump = value;
      }

      const body: OneNetSetBody = {
        product_id: OneNetConfig.PRODUCT_ID,
        device_name: OneNetConfig.DEVICE_NAME,
        params: params
      };

      const response = await httpRequest.request(
        'https://iot-api.heclouds.com/thingmodel/set-device-property',
        {
          method: http.RequestMethod.POST,
          header: {
            authorization: authorization,
            'Content-Type': 'application/json'
          },
          extraData: JSON.stringify(body),
          expectDataType: http.HttpDataType.STRING,
          usingCache: false,
          connectTimeout: 10000,
          readTimeout: 10000
        }
      );

      this.ensureHttpOk(response.responseCode);

      const result =
        JSON.parse(response.result as string) as OneNetSetResponse;

      this.ensureApiOk(result.code, result.msg, '下发设备属性');
    } finally {
      httpRequest.destroy();
    }
  }

  private ensureHttpOk(code: http.ResponseCode | number): void {
    if (code !== http.ResponseCode.OK) {
      throw new Error('HTTP 错误码：' + code.toString());
    }
  }

  private ensureApiOk(
    code: number | string | undefined,
    msg: string | undefined,
    action: string
  ): void {
    if (code === undefined || code === 0 || code === '0') {
      return;
    }

    throw new Error(action + '失败：' + code.toString() + ' ' + (msg ?? ''));
  }

  private formatValue(
    value: string | number | boolean | null | undefined
  ): string {
    if (value === undefined || value === null) {
      return '';
    }

    return value.toString();
  }

  private async createAuthorization(): Promise<string> {
    const version = '2022-05-01';
    const method = 'sha1';
    const resource = 'userid/' + OneNetConfig.USER_ID;
    const et = Math.ceil(
      (Date.now() + 365 * 24 * 3600 * 1000) / 1000
    );
    const stringToSign =
      et.toString() + '\n' +
      method + '\n' +
      resource + '\n' +
      version;

    const base64Helper = new util.Base64Helper();
    const keyData: Uint8Array =
      base64Helper.decodeSync(OneNetConfig.ACCESS_KEY);
    const textEncoder = new util.TextEncoder();
    const messageData: Uint8Array =
      textEncoder.encodeInto(stringToSign);
    const sign = base64Helper.encodeToStringSync(
      this.hmacSha1(keyData, messageData)
    );

    return (
      'version=' + version +
      '&res=' + encodeURIComponent(resource) +
      '&et=' + et.toString() +
      '&method=' + method +
      '&sign=' + encodeURIComponent(sign)
    );
  }

  private hmacSha1(key: Uint8Array, message: Uint8Array): Uint8Array {
    const blockSize: number = 64;
    const keyBlock: Uint8Array = new Uint8Array(blockSize);

    if (key.length > blockSize) {
      keyBlock.set(this.sha1(key));
    } else {
      keyBlock.set(key);
    }

    const innerPad: Uint8Array = new Uint8Array(blockSize);
    const outerPad: Uint8Array = new Uint8Array(blockSize);

    for (let i: number = 0; i < blockSize; i++) {
      innerPad[i] = keyBlock[i] ^ 0x36;
      outerPad[i] = keyBlock[i] ^ 0x5C;
    }

    const innerData: Uint8Array =
      new Uint8Array(blockSize + message.length);
    innerData.set(innerPad);
    innerData.set(message, blockSize);
    const innerHash: Uint8Array = this.sha1(innerData);

    const outerData: Uint8Array =
      new Uint8Array(blockSize + innerHash.length);
    outerData.set(outerPad);
    outerData.set(innerHash, blockSize);

    return this.sha1(outerData);
  }

  private sha1(data: Uint8Array): Uint8Array {
    const hash: number[] = [
      0x67452301,
      0xEFCDAB89,
      0x98BADCFE,
      0x10325476,
      0xC3D2E1F0
    ];
    const messageBits: number = data.length * 8;
    const bytes: number[] = [];

    for (let i: number = 0; i < data.length; i++) {
      bytes.push(data[i]);
    }

    bytes.push(0x80);
    while ((bytes.length % 64) !== 56) {
      bytes.push(0);
    }

    const highBits: number = Math.floor(messageBits / 0x100000000);
    const lowBits: number = messageBits >>> 0;
    bytes.push((highBits >>> 24) & 0xFF);
    bytes.push((highBits >>> 16) & 0xFF);
    bytes.push((highBits >>> 8) & 0xFF);
    bytes.push(highBits & 0xFF);
    bytes.push((lowBits >>> 24) & 0xFF);
    bytes.push((lowBits >>> 16) & 0xFF);
    bytes.push((lowBits >>> 8) & 0xFF);
    bytes.push(lowBits & 0xFF);

    for (let offset: number = 0; offset < bytes.length; offset += 64) {
      const words: number[] = [];

      for (let i: number = 0; i < 80; i++) {
        words.push(0);
      }

      for (let i: number = 0; i < 16; i++) {
        const index: number = offset + i * 4;
        words[i] = (
          (bytes[index] << 24) |
          (bytes[index + 1] << 16) |
          (bytes[index + 2] << 8) |
          bytes[index + 3]
        ) >>> 0;
      }

      for (let i: number = 16; i < 80; i++) {
        words[i] = this.rotateLeft(
          words[i - 3] ^
            words[i - 8] ^
            words[i - 14] ^
            words[i - 16],
          1
        );
      }

      let a: number = hash[0];
      let b: number = hash[1];
      let c: number = hash[2];
      let d: number = hash[3];
      let e: number = hash[4];

      for (let i: number = 0; i < 80; i++) {
        let f: number = 0;
        let k: number = 0;

        if (i < 20) {
          f = (b & c) | ((~b) & d);
          k = 0x5A827999;
        } else if (i < 40) {
          f = b ^ c ^ d;
          k = 0x6ED9EBA1;
        } else if (i < 60) {
          f = (b & c) | (b & d) | (c & d);
          k = 0x8F1BBCDC;
        } else {
          f = b ^ c ^ d;
          k = 0xCA62C1D6;
        }

        const temp: number = (
          this.rotateLeft(a, 5) +
          f +
          e +
          k +
          words[i]
        ) >>> 0;
        e = d;
        d = c;
        c = this.rotateLeft(b, 30);
        b = a;
        a = temp;
      }

      hash[0] = (hash[0] + a) >>> 0;
      hash[1] = (hash[1] + b) >>> 0;
      hash[2] = (hash[2] + c) >>> 0;
      hash[3] = (hash[3] + d) >>> 0;
      hash[4] = (hash[4] + e) >>> 0;
    }

    const result: Uint8Array = new Uint8Array(20);

    for (let i: number = 0; i < 5; i++) {
      result[i * 4] = (hash[i] >>> 24) & 0xFF;
      result[i * 4 + 1] = (hash[i] >>> 16) & 0xFF;
      result[i * 4 + 2] = (hash[i] >>> 8) & 0xFF;
      result[i * 4 + 3] = hash[i] & 0xFF;
    }

    return result;
  }

  private rotateLeft(value: number, bits: number): number {
    return ((value << bits) | (value >>> (32 - bits))) >>> 0;
  }
}
