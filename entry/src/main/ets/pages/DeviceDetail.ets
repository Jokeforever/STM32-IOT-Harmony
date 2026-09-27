import {
  DeviceDetailParam,
  DeviceProperty,
  DeviceState,
  PropertyHelper
} from '../model/DeviceModels';
import { OneNetApiService } from '../service/OneNetApiService';

@Component
struct DetailHeaderInfo {
  @Prop label: string = '';
  @Prop value: string = '--';

  build() {
    Column() {
      Text(this.label)
        .fontSize(10)
        .fontColor('#DDEBFF')

      Text(this.value)
        .fontSize(12)
        .fontColor(Color.White)
        .margin({ top: 2 })
        .maxLines(1)
        .textOverflow({ overflow: TextOverflow.Ellipsis })
    }
    .layoutWeight(1)
    .alignItems(HorizontalAlign.Start)
  }
}

@Component
struct DetailMetricCard {
  @Prop label: string = '';
  @Prop value: string = '--';
  @Prop unit: string = '';
  @Prop color: string = '#1D6FE8';

  build() {
    Column() {
      Text(this.label)
        .fontSize(12)
        .fontColor('#667085')

      Row() {
        Text(this.value === '--' ? '--' : this.value)
          .fontSize(24)
          .fontWeight(FontWeight.Bold)
          .fontColor(this.color)

        if (this.value !== '--' && this.unit !== '') {
          Text(this.unit)
            .fontSize(12)
            .fontColor('#667085')
            .margin({ left: 4, bottom: 4 })
        }
      }
      .alignItems(VerticalAlign.Bottom)
      .margin({ top: 6 })
    }
    .layoutWeight(1)
    .padding(12)
    .backgroundColor(Color.White)
    .borderRadius(14)
    .alignItems(HorizontalAlign.Start)
    .shadow({
      radius: 8,
      color: '#0F1D2939',
      offsetX: 0,
      offsetY: 2
    })
  }
}

@Component
struct DetailPropertyRow {
  @Prop label: string = '';
  @Prop value: string = '--';
  @Prop unit: string = '';

  build() {
    Row() {
      Text(this.label)
        .fontSize(14)
        .fontColor('#344054')
        .layoutWeight(1)

      Text(this.value === '--' ? '--' : this.value + this.unit)
        .fontSize(14)
        .fontWeight(FontWeight.Medium)
        .fontColor('#101828')
        .maxLines(1)
        .textOverflow({ overflow: TextOverflow.Ellipsis })
    }
    .width('100%')
    .padding({ top: 8, bottom: 8 })
  }
}

@Component
struct DetailModeButton {
  @Prop label: string = '';
  @Prop mode: number = 0;
  @Prop selected: boolean = false;
  @Prop buttonEnabled: boolean = true;
  onModeClick?: (mode: number) => void;

  build() {
    Button(this.label)
      .fontSize(13)
      .height(36)
      .layoutWeight(1)
      .backgroundColor(this.selected ? '#1D6FE8' : '#F2F4F7')
      .fontColor(this.selected ? Color.White : '#344054')
      .enabled(this.buttonEnabled)
      .onClick(() => {
        if (this.onModeClick) {
          this.onModeClick(this.mode);
        }
      })
  }
}

@Component
export struct DeviceDetail {
  navPathStack: NavPathStack = new NavPathStack();
  private oneNetApi: OneNetApiService = new OneNetApiService();
  private refreshTimer: number = -1;

  @State deviceId: number = 1;
  @State deviceName: string = '环境节点';
  @State online: boolean = false;
  @State temperature: number = 0;
  @State humidity: number = 0;
  @State properties: DeviceProperty[] = [];
  @State lastSyncTime: string = '--';
  @State lastOnlineTime: string = '--';
  @State lastReportTime: number = 0;
  @State ledMode: number = -1;
  @State bumpMode: number = -1;
  @State loading: boolean = false;
  @State errorMessage: string = '';
  @State autoRefresh: boolean = true;

  aboutToDisappear(): void {
    this.stopAutoRefresh();
  }

  private startAutoRefresh(): void {
    this.stopAutoRefresh();
    this.refreshTimer = setInterval(() => {
      if (this.autoRefresh && !this.loading) {
        this.autoRefreshTask();
      }
    }, 10000);
  }

  private stopAutoRefresh(): void {
    if (this.refreshTimer !== -1) {
      clearInterval(this.refreshTimer);
      this.refreshTimer = -1;
    }
  }

  private async autoRefreshTask(): Promise<void> {
    try {
      await this.refreshData();
    } catch (error) {
      const err = error as Error;
      this.errorMessage = '自动刷新失败：' + err.message;
    }
  }

  private pad(value: number): string {
    return value < 10 ? '0' + value.toString() : value.toString();
  }

  private formatTime(date: Date): string {
    return date.getFullYear().toString() +
      '-' + this.pad(date.getMonth() + 1) +
      '-' + this.pad(date.getDate()) +
      ' ' + this.pad(date.getHours()) +
      ':' + this.pad(date.getMinutes()) +
      ':' + this.pad(date.getSeconds());
  }

  private formatCloudTime(value: string): string {
    if (value === '--' || value === '') {
      return '--';
    }

    const normalized = value.replace('T', ' ');
    return normalized.length > 19
      ? normalized.substring(0, 19)
      : normalized;
  }

  private async refreshData(): Promise<void> {
    const properties = await this.oneNetApi.queryDeviceProperties();
    let hasValue: boolean = false;
    let latestTime: number = 0;

    if (properties.length > 0) {
      for (let i: number = 0; i < properties.length; i++) {
        if (properties[i].value !== '') {
          hasValue = true;
        }

        const propertyTime = properties[i].time;
        if (
          propertyTime !== undefined &&
          propertyTime !== null &&
          propertyTime > latestTime
        ) {
          latestTime = propertyTime;
        }
      }

      this.properties = properties;

      const temp = PropertyHelper.getValue(properties, 'temp');
      if (temp !== '') {
        this.temperature = parseFloat(temp);
      }

      const humi = PropertyHelper.getValue(properties, 'humi');
      if (humi !== '') {
        this.humidity = parseFloat(humi);
      }

      const led = PropertyHelper.getValue(properties, 'led');
      if (led !== '') {
        this.ledMode = parseInt(led);
      }

      const bump = PropertyHelper.getValue(properties, 'bump');
      if (bump !== '') {
        this.bumpMode = parseInt(bump);
      }

      if (latestTime > 0) {
        this.lastReportTime = latestTime;
        this.lastOnlineTime = this.formatTime(new Date(latestTime));
      }
    }

    if (this.lastReportTime > 0) {
      this.online =
        (Date.now() - this.lastReportTime) < 30000;
    } else {
      this.online = hasValue;
    }

    this.lastSyncTime = this.formatTime(new Date());
  }

  private async refresh(): Promise<void> {
    if (this.loading) {
      return;
    }

    this.loading = true;
    this.errorMessage = '';

    try {
      await this.refreshData();
    } catch (error) {
      const err = error as Error;
      this.errorMessage = '刷新失败：' + err.message;
    } finally {
      this.loading = false;
    }
  }

  private async setLedMode(mode: number): Promise<void> {
    if (this.loading) {
      return;
    }

    this.loading = true;
    this.errorMessage = '';

    try {
      await this.oneNetApi.setLedMode(mode);
      this.ledMode = mode;
      await this.refreshData();
    } catch (error) {
      const err = error as Error;
      this.errorMessage = '控制失败：' + err.message;
    } finally {
      this.loading = false;
    }
  }

  private async setBumpMode(mode: number): Promise<void> {
    if (this.loading) {
      return;
    }

    this.loading = true;
    this.errorMessage = '';

    try {
      await this.oneNetApi.setBumpMode(mode);
      this.bumpMode = mode;
      await this.refreshData();
    } catch (error) {
      const err = error as Error;
      this.errorMessage = '控制失败：' + err.message;
    } finally {
      this.loading = false;
    }
  }

  private getDisplayValue(identifier: string): string {
    return PropertyHelper.formatValue(
      identifier,
      PropertyHelper.getValue(this.properties, identifier)
    );
  }

  private getUnit(identifier: string): string {
    return PropertyHelper.getUnit(identifier);
  }

  private buildResult(): DeviceDetailParam {
    return {
      id: this.deviceId,
      name: this.deviceName,
      state: this.online ? DeviceState.ONLINE : DeviceState.OFFLINE,
      temperature: this.temperature,
      humidity: this.humidity,
      properties: this.properties,
      isRealDevice: true,
      lastSyncTime: this.lastSyncTime,
      lastOnlineTime: this.lastOnlineTime
    };
  }

  build() {
    NavDestination() {
      Scroll() {
        Column() {
          // ===== 设备头部 =====
          Column() {
            Row() {
              Column() {
                Text(this.deviceName)
                  .fontSize(22)
                  .fontWeight(FontWeight.Bold)
                  .fontColor(Color.White)

                Text('OneNET 实机')
                  .fontSize(12)
                  .fontColor('#DDEBFF')
                  .margin({ top: 4 })
              }
              .layoutWeight(1)
              .alignItems(HorizontalAlign.Start)

              Text(this.online ? '在线' : '离线')
                .fontSize(12)
                .fontColor(Color.White)
                .backgroundColor(this.online ? '#0F9D58' : '#D92D20')
                .padding({ left: 10, right: 10, top: 5, bottom: 5 })
                .borderRadius(12)
            }
            .width('100%')

            Row() {
              DetailHeaderInfo({
                label: '设备 ID',
                value: this.deviceId.toString()
              })
              DetailHeaderInfo({
                label: '最后同步',
                value: this.lastSyncTime
              })
              DetailHeaderInfo({
                label: '最后在线',
                value: this.formatCloudTime(this.lastOnlineTime)
              })
            }
            .width('100%')
            .margin({ top: 16 })
          }
          .width('92%')
          .padding(16)
          .margin({ top: 12 })
          .backgroundColor('#1D6FE8')
          .borderRadius(18)
          .shadow({
            radius: 16,
            color: '#331D6FE8',
            offsetX: 0,
            offsetY: 6
          })

          // ===== 错误提示 =====
          if (this.errorMessage !== '') {
            Text(this.errorMessage)
              .fontSize(12)
              .fontColor('#B42318')
              .width('92%')
              .padding(10)
              .margin({ top: 10 })
              .backgroundColor('#FEF3F2')
              .borderRadius(10)
              .maxLines(2)
              .textOverflow({ overflow: TextOverflow.Ellipsis })
          }

          // ===== 刷新控制 =====
          Row() {
            Button(this.autoRefresh ? '自动刷新：开' : '自动刷新：关')
              .fontSize(12)
              .height(34)
              .layoutWeight(1)
              .backgroundColor(this.autoRefresh ? '#E8F1FF' : '#F2F4F7')
              .fontColor(this.autoRefresh ? '#1D6FE8' : '#667085')
              .onClick(() => {
                this.autoRefresh = !this.autoRefresh;
                if (this.autoRefresh) {
                  this.startAutoRefresh();
                  this.refresh();
                } else {
                  this.stopAutoRefresh();
                }
              })

            Button(this.loading ? '刷新中...' : '刷新数据')
              .fontSize(12)
              .height(34)
              .layoutWeight(1)
              .margin({ left: 8 })
              .enabled(!this.loading)
              .onClick(() => {
                this.refresh();
              })

            Button('返回')
              .fontSize(12)
              .height(34)
              .layoutWeight(1)
              .margin({ left: 8 })
              .backgroundColor('#F2F4F7')
              .fontColor('#344054')
              .onClick(() => {
                this.navPathStack.pop(this.buildResult());
              })
          }
          .width('92%')
          .margin({ top: 12 })

          // ===== 环境数据 =====
          Text('环境数据')
            .fontSize(16)
            .fontWeight(FontWeight.Medium)
            .fontColor('#101828')
            .width('92%')
            .margin({ top: 18 })

          Row({ space: 10 }) {
            DetailMetricCard({
              label: '空气温度',
              value: this.getDisplayValue('temp'),
              unit: this.getUnit('temp'),
              color: '#1D6FE8'
            })
            DetailMetricCard({
              label: '空气湿度',
              value: this.getDisplayValue('humi'),
              unit: this.getUnit('humi'),
              color: '#0F9D58'
            })
          }
          .width('92%')
          .margin({ top: 10 })

          Row({ space: 10 }) {
            DetailMetricCard({
              label: '光照强度',
              value: this.getDisplayValue('light'),
              unit: this.getUnit('light'),
              color: '#F79009'
            })
            DetailMetricCard({
              label: 'CO2 浓度',
              value: this.getDisplayValue('ppm'),
              unit: this.getUnit('ppm'),
              color: '#7A5AF8'
            })
          }
          .width('92%')
          .margin({ top: 10 })

          Row({ space: 10 }) {
            DetailMetricCard({
              label: '土壤湿度',
              value: this.getDisplayValue('moist'),
              unit: this.getUnit('moist'),
              color: '#06AED4'
            })
            DetailMetricCard({
              label: '水量',
              value: this.getDisplayValue('water_vol'),
              unit: this.getUnit('water_vol'),
              color: '#2E90FA'
            })
          }
          .width('92%')
          .margin({ top: 10 })

          // ===== 执行器 =====
          Text('执行器')
            .fontSize(16)
            .fontWeight(FontWeight.Medium)
            .fontColor('#101828')
            .width('92%')
            .margin({ top: 18 })

          // LED 模式控制
          Column() {
            Text('LED 模式')
              .fontSize(14)
              .fontWeight(FontWeight.Medium)
              .fontColor('#101828')
              .width('100%')

            DetailPropertyRow({
              label: '当前模式',
              value: PropertyHelper.getLedModeText(this.ledMode.toString()),
              unit: ''
            })

            Row({ space: 8 }) {
              DetailModeButton({
                label: '关闭',
                mode: 0,
                selected: this.ledMode === 0,
                buttonEnabled: !this.loading,
                onModeClick: (mode: number) => {
                  this.setLedMode(mode);
                }
              })
              DetailModeButton({
                label: '常亮',
                mode: 1,
                selected: this.ledMode === 1,
                buttonEnabled: !this.loading,
                onModeClick: (mode: number) => {
                  this.setLedMode(mode);
                }
              })
              DetailModeButton({
                label: '自动',
                mode: 2,
                selected: this.ledMode === 2,
                buttonEnabled: !this.loading,
                onModeClick: (mode: number) => {
                  this.setLedMode(mode);
                }
              })
            }
            .width('100%')
            .margin({ top: 6 })
          }
          .width('92%')
          .padding(14)
          .margin({ top: 10 })
          .backgroundColor(Color.White)
          .borderRadius(16)
          .shadow({
            radius: 10,
            color: '#0F1D2939',
            offsetX: 0,
            offsetY: 3
          })

          // 水泵模式控制
          Column() {
            Text('水泵模式')
              .fontSize(14)
              .fontWeight(FontWeight.Medium)
              .fontColor('#101828')
              .width('100%')

            DetailPropertyRow({
              label: '当前模式',
              value: PropertyHelper.formatValue(
                'bump',
                PropertyHelper.getValue(this.properties, 'bump')
              ),
              unit: ''
            })

            Row({ space: 8 }) {
              DetailModeButton({
                label: '关闭',
                mode: 0,
                selected: this.bumpMode === 0,
                buttonEnabled: !this.loading,
                onModeClick: (mode: number) => {
                  this.setBumpMode(mode);
                }
              })
              DetailModeButton({
                label: '开启',
                mode: 1,
                selected: this.bumpMode === 1,
                buttonEnabled: !this.loading,
                onModeClick: (mode: number) => {
                  this.setBumpMode(mode);
                }
              })
              DetailModeButton({
                label: '自动',
                mode: 2,
                selected: this.bumpMode === 2,
                buttonEnabled: !this.loading,
                onModeClick: (mode: number) => {
                  this.setBumpMode(mode);
                }
              })
            }
            .width('100%')
            .margin({ top: 6 })
          }
          .width('92%')
          .padding(14)
          .margin({ top: 10 })
          .backgroundColor(Color.White)
          .borderRadius(16)
          .shadow({
            radius: 10,
            color: '#0F1D2939',
            offsetX: 0,
            offsetY: 3
          })

          // ===== 阈值设置 =====
          Text('阈值设置')
            .fontSize(16)
            .fontWeight(FontWeight.Medium)
            .fontColor('#101828')
            .width('92%')
            .margin({ top: 18 })

          Column() {
            DetailPropertyRow({
              label: 'CO2 阈值',
              value: this.getDisplayValue('CO2threhold'),
              unit: this.getUnit('CO2threhold')
            })
            DetailPropertyRow({
              label: '干旱阈值',
              value: this.getDisplayValue('droughtthrehold'),
              unit: this.getUnit('droughtthrehold')
            })
            DetailPropertyRow({
              label: '湿润阈值',
              value: this.getDisplayValue('moistthrehold'),
              unit: this.getUnit('moistthrehold')
            })
            DetailPropertyRow({
              label: '温度阈值',
              value: this.getDisplayValue('tempthrehold'),
              unit: this.getUnit('tempthrehold')
            })
          }
          .width('92%')
          .padding(14)
          .margin({ top: 10 })
          .backgroundColor(Color.White)
          .borderRadius(16)
          .shadow({
            radius: 10,
            color: '#0F1D2939',
            offsetX: 0,
            offsetY: 3
          })

          // ===== 全部属性 =====
          Text('全部属性')
            .fontSize(16)
            .fontWeight(FontWeight.Medium)
            .fontColor('#101828')
            .width('92%')
            .margin({ top: 18 })

          Column() {
            ForEach(
              this.properties,
              (property: DeviceProperty) => {
                DetailPropertyRow({
                  label: PropertyHelper.getLabel(property.identifier),
                  value: PropertyHelper.formatValue(
                    property.identifier,
                    property.value
                  ),
                  unit: PropertyHelper.getUnit(property.identifier)
                })
              },
              (property: DeviceProperty) =>
                property.identifier + '_' +
                property.value + '_' +
                (property.time === undefined
                  ? '0'
                  : property.time.toString())
            )
          }
          .width('92%')
          .padding(14)
          .margin({ top: 10, bottom: 20 })
          .backgroundColor(Color.White)
          .borderRadius(16)
          .shadow({
            radius: 10,
            color: '#0F1D2939',
            offsetX: 0,
            offsetY: 3
          })
        }
        .width('100%')
        .padding({ bottom: 20 })
      }
      .width('100%')
      .height('100%')
      .scrollBar(BarState.Auto)
    }
    .title('设备详情')
    .onBackPressed(() => {
      this.navPathStack.pop(this.buildResult());
      return true;
    })
    .onReady((context: NavDestinationContext) => {
      this.navPathStack = context.pathStack;

      const param = context.pathInfo.param as DeviceDetailParam;
      if (param) {
        this.deviceId = param.id;
        this.deviceName = param.name;
        this.online = param.state === DeviceState.ONLINE;
        this.temperature = param.temperature;
        this.humidity = param.humidity;
        this.properties = param.properties;
        this.lastSyncTime = param.lastSyncTime;
        this.lastOnlineTime = param.lastOnlineTime;

        const led = PropertyHelper.getValue(param.properties, 'led');
        if (led !== '') {
          this.ledMode = parseInt(led);
        }

        this.refresh();
        this.startAutoRefresh();
      }
    })
  }
}
