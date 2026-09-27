import { DeviceDetail } from './DeviceDetail';
import {
  DeviceDetailParam,
  DeviceProperty,
  DeviceState,
  PropertyHelper
} from '../model/DeviceModels';
import { OneNetApiService } from '../service/OneNetApiService';

@Component
struct DeviceCard {
  @Prop name: string = '环境节点';
  @Prop online: boolean = false;
  @Prop temperature: number = 0;
  @Prop humidity: number = 0;
  @Prop propertyCount: number = 0;
  @Prop lastSyncTime: string = '--';
  onOpenDetail?: () => void;

  build() {
    Column() {
      Row() {
        Stack() {
          Circle()
            .width(44)
            .height(44)
            .fill('#E8F1FF')

          Text('🌱')
            .fontSize(22)
        }
        .width(44)
        .height(44)
        .alignContent(Alignment.Center)

        Column() {
          Text(this.name)
            .fontSize(16)
            .fontWeight(FontWeight.Medium)
            .fontColor('#101828')
            .maxLines(1)
            .textOverflow({ overflow: TextOverflow.Ellipsis })

          Text('最后同步 ' + this.lastSyncTime)
            .fontSize(11)
            .fontColor('#8A94A6')
            .margin({ top: 2 })
            .maxLines(1)
            .textOverflow({ overflow: TextOverflow.Ellipsis })
        }
        .layoutWeight(1)
        .alignItems(HorizontalAlign.Start)
        .margin({ left: 10 })

        Text(this.online ? '在线' : '离线')
          .fontSize(11)
          .fontColor(this.online ? '#0F9D58' : '#D92D20')
          .backgroundColor(this.online ? '#E9F7EF' : '#FEECEB')
          .padding({ left: 8, right: 8, top: 4, bottom: 4 })
          .borderRadius(10)
      }
      .width('100%')

      Row() {
        Column() {
          Text(this.temperature.toString() + '℃')
            .fontSize(16)
            .fontWeight(FontWeight.Bold)
            .fontColor('#1D2939')
          Text('温度')
            .fontSize(11)
            .fontColor('#8A94A6')
            .margin({ top: 2 })
        }
        .layoutWeight(1)
        .alignItems(HorizontalAlign.Center)

        Column() {
          Text(this.humidity.toString() + '%')
            .fontSize(16)
            .fontWeight(FontWeight.Bold)
            .fontColor('#1D2939')
          Text('湿度')
            .fontSize(11)
            .fontColor('#8A94A6')
            .margin({ top: 2 })
        }
        .layoutWeight(1)
        .alignItems(HorizontalAlign.Center)

        Column() {
          Text(this.propertyCount.toString() + '项')
            .fontSize(16)
            .fontWeight(FontWeight.Bold)
            .fontColor('#1D2939')
          Text('数据项')
            .fontSize(11)
            .fontColor('#8A94A6')
            .margin({ top: 2 })
        }
        .layoutWeight(1)
        .alignItems(HorizontalAlign.Center)
      }
      .width('100%')
      .margin({ top: 12 })

      Row() {
        Text('查看详情')
          .fontSize(13)
          .fontColor('#1D6FE8')

        Blank()

        Text('→')
          .fontSize(16)
          .fontColor('#1D6FE8')
      }
      .width('100%')
      .margin({ top: 10 })
      .onClick(() => {
        if (this.onOpenDetail) {
          this.onOpenDetail();
        }
      })
    }
    .width('100%')
    .padding(14)
    .backgroundColor(Color.White)
    .borderRadius(16)
    .shadow({
      radius: 10,
      color: '#0F1D2939',
      offsetX: 0,
      offsetY: 3
    })
  }
}

@Component
struct StatChip {
  @Prop label: string = '';
  @Prop value: string = '--';

  build() {
    Column() {
      Text(this.value)
        .fontSize(15)
        .fontWeight(FontWeight.Bold)
        .fontColor(Color.White)
        .maxLines(1)
        .textOverflow({ overflow: TextOverflow.Ellipsis })

      Text(this.label)
        .fontSize(10)
        .fontColor('#DDEBFF')
        .margin({ top: 2 })
    }
    .layoutWeight(1)
    .alignItems(HorizontalAlign.Center)
  }
}

@Entry
@Component
struct Index {
  @Builder
  deviceDetailPage(name: string) {
    if (name === 'deviceDetail') {
      DeviceDetail()
    }
  }

  pageStack: NavPathStack = new NavPathStack();
  private oneNetApi: OneNetApiService = new OneNetApiService();
  private refreshTimer: number = -1;
  private appVersion: string = 'v13';

  @State deviceName: string = '环境节点';
  @State deviceOnline: boolean = false;
  @State temperature: number = 0;
  @State humidity: number = 0;
  @State properties: DeviceProperty[] = [];
  @State ledMode: number = -1;
  @State lastSyncTime: string = '--';
  @State lastOnlineTime: string = '--';
  @State lastReportTime: number = 0;
  @State loading: boolean = false;
  @State errorMessage: string = '';
  @State autoRefresh: boolean = true;

  aboutToAppear(): void {
    this.syncOneNet();
    this.startAutoRefresh();
  }

  aboutToDisappear(): void {
    this.stopAutoRefresh();
  }

  private startAutoRefresh(): void {
    this.stopAutoRefresh();
    this.refreshTimer = setInterval(() => {
      if (this.autoRefresh && !this.loading) {
        this.syncOneNet();
      }
    }, 10000);
  }

  private stopAutoRefresh(): void {
    if (this.refreshTimer !== -1) {
      clearInterval(this.refreshTimer);
      this.refreshTimer = -1;
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

  private async refreshOneNetData(): Promise<void> {
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

      if (latestTime > 0) {
        this.lastReportTime = latestTime;
        this.lastOnlineTime = this.formatTime(new Date(latestTime));
      }
    }

    if (this.lastReportTime > 0) {
      this.deviceOnline =
        (Date.now() - this.lastReportTime) < 30000;
    } else {
      this.deviceOnline = hasValue;
    }

    this.lastSyncTime = this.formatTime(new Date());
  }

  private async syncOneNet(): Promise<void> {
    if (this.loading) {
      return;
    }

    this.loading = true;
    this.errorMessage = '';

    try {
      await this.refreshOneNetData();
    } catch (error) {
      const err = error as Error;
      this.errorMessage = 'OneNET 同步失败：' + err.message;
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
      await this.refreshOneNetData();
    } catch (error) {
      const err = error as Error;
      this.errorMessage = '设备控制失败：' + err.message;
    } finally {
      this.loading = false;
    }
  }

  private buildDetailResult(): DeviceDetailParam {
    return {
      id: 1,
      name: this.deviceName,
      state: this.deviceOnline
        ? DeviceState.ONLINE
        : DeviceState.OFFLINE,
      temperature: this.temperature,
      humidity: this.humidity,
      properties: this.properties,
      isRealDevice: true,
      lastSyncTime: this.lastSyncTime,
      lastOnlineTime: this.lastOnlineTime
    };
  }

  private openDetail(): void {
    this.stopAutoRefresh();

    this.pageStack.pushPathByName(
      'deviceDetail',
      this.buildDetailResult(),
      (popInfo) => {
        const result = popInfo.result as DeviceDetailParam;
        if (result) {
          this.deviceName = result.name;
          this.deviceOnline = result.state === DeviceState.ONLINE;
          this.temperature = result.temperature;
          this.humidity = result.humidity;
          this.properties = result.properties;
          this.lastSyncTime = result.lastSyncTime;
          this.lastOnlineTime = result.lastOnlineTime;

          const led = PropertyHelper.getValue(result.properties, 'led');
          if (led !== '') {
            this.ledMode = parseInt(led);
          }
        }

        this.startAutoRefresh();
        this.syncOneNet();
      }
    );
  }

  @Builder
  modeButton(label: string, mode: number) {
    Button(label)
      .fontSize(13)
      .height(40)
      .layoutWeight(1)
      .backgroundColor(this.ledMode === mode ? '#1D6FE8' : '#F2F4F7')
      .fontColor(this.ledMode === mode ? Color.White : '#344054')
      .enabled(!this.loading)
      .onClick(() => {
        this.setLedMode(mode);
      })
  }

  build() {
    Navigation(this.pageStack) {
      Column() {
        // ===== 顶部仪表盘 =====
        Column() {
          Row() {
            Column() {
              Text('智慧农业监控')
                .fontSize(24)
                .fontWeight(FontWeight.Bold)
                .fontColor(Color.White)

              Text('OneNET 物联网设备监控台')
                .fontSize(13)
                .fontColor('#DDEBFF')
                .margin({ top: 4 })
            }
            .layoutWeight(1)
            .alignItems(HorizontalAlign.Start)

            Button(this.loading ? '同步中' : '同步')
              .fontSize(13)
              .height(36)
              .backgroundColor(Color.White)
              .fontColor('#1D6FE8')
              .enabled(!this.loading)
              .onClick(() => {
                this.syncOneNet();
              })
          }
          .width('100%')

          Row() {
            StatChip({
              label: '在线设备',
              value: this.deviceOnline ? '1/1' : '0/1'
            })
            StatChip({
              label: '版本号',
              value: this.appVersion
            })
            StatChip({
              label: '最后同步',
              value: this.lastSyncTime
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
          Row() {
            Text('!')
              .fontSize(13)
              .fontColor(Color.White)
              .textAlign(TextAlign.Center)
              .width(20)
              .height(20)
              .backgroundColor('#D92D20')
              .borderRadius(10)

            Text(this.errorMessage)
              .fontSize(12)
              .fontColor('#B42318')
              .layoutWeight(1)
              .margin({ left: 8 })
              .maxLines(2)
              .textOverflow({ overflow: TextOverflow.Ellipsis })

            Button('关闭')
              .fontSize(11)
              .height(28)
              .backgroundColor(Color.Transparent)
              .fontColor('#B42318')
              .onClick(() => {
                this.errorMessage = '';
              })
          }
          .width('92%')
          .padding(10)
          .margin({ top: 10 })
          .backgroundColor('#FEF3F2')
          .borderRadius(10)
        }

        // ===== 刷新控制 =====
        Row() {
          Button(this.autoRefresh ? '自动刷新：开' : '自动刷新：关')
            .fontSize(12)
            .height(32)
            .layoutWeight(1)
            .backgroundColor(this.autoRefresh ? '#E8F1FF' : '#F2F4F7')
            .fontColor(this.autoRefresh ? '#1D6FE8' : '#667085')
            .onClick(() => {
              this.autoRefresh = !this.autoRefresh;
              if (this.autoRefresh) {
                this.startAutoRefresh();
                this.syncOneNet();
              } else {
                this.stopAutoRefresh();
              }
            })

          Button('立即刷新')
            .fontSize(12)
            .height(32)
            .layoutWeight(1)
            .margin({ left: 8 })
            .enabled(!this.loading)
            .onClick(() => {
              this.syncOneNet();
            })
        }
        .width('92%')
        .margin({ top: 10 })

        // ===== 设备卡片 =====
        DeviceCard({
          name: this.deviceName,
          online: this.deviceOnline,
          temperature: this.temperature,
          humidity: this.humidity,
          propertyCount: this.properties.length,
          lastSyncTime: this.lastSyncTime,
          onOpenDetail: () => {
            this.openDetail();
          }
        })
          .width('92%')
          .margin({ top: 12 })

        Blank()

      }
      .width('100%')
      .height('100%')
      .backgroundColor('#F4F7FB')
    }
    .mode(NavigationMode.Stack)
    .navDestination(this.deviceDetailPage)
    .hideTitleBar(true)
    .width('100%')
    .height('100%')
  }
}
