import { DeviceDetail } from './DeviceDetail';
import {
  CachedAppState,
  DeviceDetailParam,
  DeviceProperty,
  DeviceState,
  PropertyHelper
} from '../model/DeviceModels';
import { TrendHistory } from '../model/TrendHistory';
import { OneNetApiService } from '../service/OneNetApiService';

import { AppTheme } from '../theme/AppTheme';
import { MetricCard, OfflineNotice, SectionHeading, StatusBadge, SyncErrorCard, SyncToolbar } from '../components/MonitorComponents';

@Component
struct DeviceCard {
  @Prop name: string = '环境节点';
  @Prop online: boolean = false;
  @Prop known: boolean = false;
  @Prop propertyCount: number = 0;
  onOpenDetail?: () => void;

  build() {
    Column({ space: 18 }) {
      Row({ space: 12 }) {
        Image($r('app.media.monitor_leaf'))
          .width(48).height(48).padding(11)
          .backgroundColor(AppTheme.pale).borderRadius(16)
          .accessibilityLevel('no')
        Column({ space: 5 }) {
          Text(this.name)
            .fontSize(18).fontWeight(FontWeight.Bold).fontColor(AppTheme.ink)
            .maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis })
          Text('环境监测与智能控制').fontSize(12).fontColor(AppTheme.secondary)
        }
        .layoutWeight(1).alignItems(HorizontalAlign.Start)
        StatusBadge({ online: this.online, known: this.known })
      }
      .width('100%')
      Divider().color(AppTheme.border)
      Row() {
        Text(this.propertyCount > 0 ? this.propertyCount.toString() + ' 项设备属性' : '等待首次同步')
          .fontSize(13).fontColor(AppTheme.secondary).layoutWeight(1)
        Text('查看与控制').fontSize(14).fontWeight(FontWeight.Medium).fontColor(AppTheme.primary)
        Image($r('app.media.monitor_arrow')).width(18).height(18).margin({ left: 6 })
          .accessibilityLevel('no')
      }
      .width('100%')
    }
    .width('100%').padding(18).backgroundColor(AppTheme.surface)
    .border({ width: 1, color: AppTheme.border }).borderRadius(22)
    .accessibilityText('查看' + this.name + '详情与控制')
    .onClick(() => { if (this.onOpenDetail) { this.onOpenDetail(); } })
  }
}

@Component
struct HomeOverview {
  @Prop temperature: string = '--';
  @Prop humidity: string = '--';
  @Prop online: boolean = false;
  @Prop known: boolean = false;

  build() {
    Column({ space: 24 }) {
      Row({ space: 10 }) {
        Image($r('app.media.monitor_leaf_light')).width(24).height(24)
          .accessibilityLevel('no')
        Text('环境总览').fontSize(16).fontWeight(FontWeight.Medium)
          .fontColor('#E5F0E5').layoutWeight(1)
        StatusBadge({ online: this.online, known: this.known, dark: true })
      }
      .width('100%')
      Row({ space: 16 }) {
        Column({ space: 6 }) {
          Text('空气温度').fontSize(13).fontColor('#B9D1BE')
          Row({ space: 4 }) {
            Text(this.temperature).fontSize(42).fontWeight(FontWeight.Bold)
              .fontColor('#F8FCF4').maxLines(1)
              .textOverflow({ overflow: TextOverflow.Ellipsis })
            if (this.temperature !== '--') {
              Text('℃').fontSize(16).fontColor('#BED9C5').margin({ bottom: 7 })
            }
          }
          .alignItems(VerticalAlign.Bottom)
        }
        .layoutWeight(1).alignItems(HorizontalAlign.Start)
        Divider().vertical(true).height(48).color('#39624B')
        Column({ space: 6 }) {
          Text('空气湿度').fontSize(13).fontColor('#B9D1BE')
          Row({ space: 4 }) {
            Text(this.humidity).fontSize(42).fontWeight(FontWeight.Bold)
              .fontColor('#F8FCF4').maxLines(1)
              .textOverflow({ overflow: TextOverflow.Ellipsis })
            if (this.humidity !== '--') {
              Text('%').fontSize(16).fontColor('#BED9C5').margin({ bottom: 7 })
            }
          }
          .alignItems(VerticalAlign.Bottom)
        }
        .layoutWeight(1).alignItems(HorizontalAlign.Start)
      }
      .width('100%')
      Text(this.known ? '最近一次上报的数据快照' : '连接设备后，在这里查看环境变化')
        .fontSize(12).fontColor('#B9D1BE').width('100%')
    }
    .width('100%').padding(22).borderRadius(26)
    .backgroundColor(AppTheme.forest)
    .linearGradient({ angle: 125, colors: [['#173F32', 0], ['#2F6347', 1]] })
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
    if (AppStorage.get<string>('cachedAppState') === undefined) {
      PersistentStorage.persistProp('cachedAppState', '');
    }
    this.restoreCachedState();
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

  private restoreCachedState(): void {
    const raw = AppStorage.get<string>('cachedAppState');
    if (raw === undefined || raw === '') {
      return;
    }

    try {
      const cached = JSON.parse(raw) as CachedAppState;
      if (cached.properties.length === 0) {
        return;
      }

      this.properties = cached.properties;
      this.lastSyncTime = cached.lastSyncTime;
      this.lastReportTime = cached.lastReportTime;

      if (cached.lastReportTime > 0) {
        this.lastOnlineTime = this.formatTime(new Date(cached.lastReportTime));
        this.deviceOnline = (Date.now() - cached.lastReportTime) < 30000;
      }

      const temp = PropertyHelper.getValue(cached.properties, 'temp');
      if (temp !== '') {
        this.temperature = parseFloat(temp);
      }

      const humi = PropertyHelper.getValue(cached.properties, 'humi');
      if (humi !== '') {
        this.humidity = parseFloat(humi);
      }

      const led = PropertyHelper.getValue(cached.properties, 'led');
      if (led !== '') {
        this.ledMode = parseInt(led);
      }
    } catch (error) {
    }
  }

  private reportTimeText(): string {
    const time = TrendHistory.latestReportTime(this.properties);
    return time > 0 ? this.formatTime(new Date(time)) : '--';
  }

  private saveCachedState(): void {
    const history = TrendHistory.collect(AppStorage.get<string>('cachedAppState'), this.properties);
    const cached: CachedAppState = {
      properties: this.properties,
      lastSyncTime: this.lastSyncTime,
      lastReportTime: this.lastReportTime,
      historyVersion: TrendHistory.VERSION,
      tempHistory: history.tempHistory,
      humiHistory: history.humiHistory
    };
    AppStorage.setOrCreate('cachedAppState', JSON.stringify(cached));
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
    this.saveCachedState();
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

  private displayValue(identifier: string): string {
    return PropertyHelper.formatValue(identifier, PropertyHelper.getValue(this.properties, identifier));
  }

  build() {
    Navigation(this.pageStack) {
      Scroll() {
        Column() {
          Row() {
            Text('SMART GROW').fontSize(11).letterSpacing(2)
              .fontWeight(FontWeight.Bold).fontColor(AppTheme.primary).layoutWeight(1)
            Text(this.appVersion).fontSize(12).fontColor(AppTheme.secondary)
              .padding({ left: 10, right: 10, top: 5, bottom: 5 })
              .backgroundColor('#E8EEE5').borderRadius(12)
          }
          .width('100%')
          Text('智慧农业工作台').fontSize(28).fontWeight(FontWeight.Bold)
            .fontColor(AppTheme.ink).width('100%').margin({ top: 10 })
          Text('感知环境，让每一次照料更从容。').fontSize(14)
            .fontColor(AppTheme.secondary).width('100%').margin({ top: 7, bottom: 22 })

          HomeOverview({
            temperature: this.displayValue('temp'),
            humidity: this.displayValue('humi'),
            online: this.deviceOnline,
            known: this.lastReportTime > 0
          })

          Row({ space: 7 }) {
            Image($r('app.media.monitor_clock')).width(15).height(15).accessibilityLevel('no')
            Text('最后同步').fontSize(12).fontColor(AppTheme.secondary)
            Text(this.lastSyncTime).fontSize(12).fontColor(AppTheme.ink)
              .layoutWeight(1).textAlign(TextAlign.End).maxLines(1)
              .textOverflow({ overflow: TextOverflow.Ellipsis })
          }
          .width('100%').padding({ top: 14, bottom: 14 })
          Row({ space: 7 }) {
            Text('设备上报').fontSize(12).fontColor(AppTheme.secondary)
            Text(this.reportTimeText())
              .fontSize(12).fontColor(AppTheme.ink).layoutWeight(1).textAlign(TextAlign.End)
              .maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis })
          }.width('100%').padding({ bottom: 14 })

          if (this.errorMessage !== '') {
            SyncErrorCard({ message: this.errorMessage }).margin({ bottom: 12 })
          }
          if (!this.deviceOnline && this.lastReportTime > 0) {
            OfflineNotice({ lastOnline: this.lastOnlineTime }).margin({ bottom: 12 })
          }

          SyncToolbar({
            autoRefresh: this.autoRefresh,
            loading: this.loading,
            onToggle: (enabled: boolean) => {
              this.autoRefresh = enabled;
              if (enabled) {
                this.startAutoRefresh();
                this.syncOneNet();
              } else {
                this.stopAutoRefresh();
              }
            },
            onRefresh: () => { this.syncOneNet(); }
          })

          SectionHeading({ title: '环境速览', caption: '最近一次上报' })
          Row({ space: 8 }) {
            MetricCard({ label: '光照', value: this.displayValue('light'), unit: 'lx',
              icon: $r('app.media.monitor_sun'), accent: '#AA7C31', tint: '#FBF2DD', compact: true })
            MetricCard({ label: 'CO₂', value: this.displayValue('ppm'), unit: 'ppm',
              icon: $r('app.media.monitor_air'), accent: '#677BA0', tint: '#EEF1F8', compact: true })
            MetricCard({ label: '土壤湿度', value: this.displayValue('moist'), unit: '%',
              icon: $r('app.media.monitor_soil'), accent: '#467C59', tint: '#ECF4EB', compact: true })
          }
          .width('100%')

          SectionHeading({ title: '我的设备', caption: '1 个环境节点' })
          DeviceCard({
            name: this.deviceName,
            online: this.deviceOnline,
            known: this.lastReportTime > 0,
            propertyCount: this.properties.length,
            onOpenDetail: () => { this.openDetail(); }
          })

          Row({ space: 14 }) {
            Column({ space: 7 }) {
              Text('照明模式').fontSize(12).fontColor(AppTheme.secondary)
              Text(this.displayValue('led')).fontSize(16).fontWeight(FontWeight.Medium)
                .fontColor(AppTheme.ink)
            }
            .layoutWeight(1).alignItems(HorizontalAlign.Start)
            Divider().vertical(true).height(32).color(AppTheme.border)
            Column({ space: 7 }) {
              Text('灌溉模式').fontSize(12).fontColor(AppTheme.secondary)
              Text(this.displayValue('bump')).fontSize(16).fontWeight(FontWeight.Medium)
                .fontColor(AppTheme.ink)
            }
            .layoutWeight(1).alignItems(HorizontalAlign.Start)
          }
          .width('100%').padding(18).margin({ top: 12 })
          .backgroundColor('#ECF1E9').borderRadius(20)

          Text('首页展示最近一次成功同步的数据。')
            .fontSize(12).fontColor(AppTheme.muted).margin({ top: 20 })
        }
        .width('100%').alignItems(HorizontalAlign.Start)
        .padding({ left: 20, right: 20, top: 24, bottom: 30 })
      }
      .width('100%').height('100%').scrollBar(BarState.Off)
      .backgroundColor(AppTheme.background)
    }
    .mode(NavigationMode.Stack)
    .navDestination(this.deviceDetailPage)
    .hideTitleBar(true)
    .width('100%')
    .height('100%')
  }
}
