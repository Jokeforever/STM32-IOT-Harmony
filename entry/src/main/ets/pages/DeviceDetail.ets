import {
  CachedAppState,
  DeviceDetailParam,
  DeviceProperty,
  DeviceState,
  PropertyHelper,
  TrendPoint
} from '../model/DeviceModels';
import { TrendHistory } from '../model/TrendHistory';
import { OneNetApiService } from '../service/OneNetApiService';

import { AppTheme } from '../theme/AppTheme';
import { HistoryChart } from '../components/HistoryChart';
import { MetricCard, OfflineNotice, SectionHeading, StatusBadge, SyncErrorCard, SyncToolbar } from '../components/MonitorComponents';

@Component
struct DetailPropertyRow {
  @Prop label: string = '';
  @Prop value: string = '--';
  @Prop unit: string = '';

  build() {
    Row({ space: 12 }) {
      Text(this.label).fontSize(13).fontColor(AppTheme.secondary).layoutWeight(1)
      Text(this.value === '--' ? '--' : this.value + this.unit)
        .fontSize(14).fontWeight(FontWeight.Medium).fontColor(AppTheme.ink)
        .textAlign(TextAlign.End).maxLines(2)
        .textOverflow({ overflow: TextOverflow.Ellipsis }).layoutWeight(2)
    }
    .width('100%').padding({ top: 9, bottom: 9 })
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
    Button({ type: ButtonType.Normal }) {
      Text(this.label).fontSize(14).fontWeight(FontWeight.Medium)
        .fontColor(this.selected ? Color.White : AppTheme.secondary)
        .maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis })
    }
    .height(44).layoutWeight(1).padding({ left: 6, right: 6 })
    .borderRadius(13)
    .backgroundColor(this.selected ? AppTheme.primary : '#F0F4EE')
    .enabled(this.buttonEnabled)
    .accessibilityText(this.label + (this.selected ? '，已选择' : ''))
    .onClick(() => { if (this.onModeClick) { this.onModeClick(this.mode); } })
  }
}

@Component
struct ThresholdStepper {
  @Prop label: string = '';
  @Prop valueText: string = '--';
  @Prop unit: string = '';
  @Prop buttonEnabled: boolean = true;
  onDecrease?: () => void;
  onIncrease?: () => void;

  build() {
    Row({ space: 12 }) {
      Column({ space: 6 }) {
        Text(this.label).fontSize(13).fontColor(AppTheme.secondary)
        Text(this.valueText === '--' ? '--' : this.valueText + ' ' + this.unit)
          .fontSize(18).fontWeight(FontWeight.Medium).fontColor(AppTheme.ink)
          .maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis })
      }
      .layoutWeight(1).alignItems(HorizontalAlign.Start)
      Row({ space: 8 }) {
        Button({ type: ButtonType.Normal }) {
          Text('−').fontSize(27).fontWeight(FontWeight.Medium).fontColor(AppTheme.primary)
        }
        .width(44).height(44).padding(0).borderRadius(14)
        .backgroundColor(AppTheme.pale).enabled(this.buttonEnabled)
        .accessibilityText(this.label + '减少')
        .onClick(() => { if (this.onDecrease) { this.onDecrease(); } })
        Button({ type: ButtonType.Normal }) {
          Text('+').fontSize(27).fontWeight(FontWeight.Medium).fontColor(Color.White)
        }
        .width(44).height(44).padding(0).borderRadius(14)
        .backgroundColor(AppTheme.primary).enabled(this.buttonEnabled)
        .accessibilityText(this.label + '增加')
        .onClick(() => { if (this.onIncrease) { this.onIncrease(); } })
      }
      .width(96)
    }
    .width('100%').padding({ top: 14, bottom: 14 })
  }
}

@Component
struct SensorStatusRow {
  @Prop label: string = '';
  @Prop valid: boolean = false;

  build() {
    Row({ space: 8 }) {
      Text(this.label).fontSize(14).fontColor(AppTheme.ink).layoutWeight(1)
      Circle().width(6).height(6).fill(this.valid ? '#5A966B' : '#C87968')
      Text(this.valid ? '数据有效' : '数据无效').fontSize(13)
        .fontColor(this.valid ? AppTheme.primary : '#AB6254')
    }
    .width('100%').padding({ top: 12, bottom: 12 })
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
  @State propertiesExpanded: boolean = false;
  @State tempHistory: TrendPoint[] = [];
  @State humiHistory: TrendPoint[] = [];
  @State trendExpanded: boolean = false;
  @State trendMetric: number = 0;
  @State trendAutoFit: boolean = true;

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

  private restoreCachedHistory(): void {
    const history = TrendHistory.fromRaw(AppStorage.get<string>('cachedAppState'));
    this.tempHistory = history.tempHistory;
    this.humiHistory = history.humiHistory;
  }

  private reportTimeText(): string {
    const time = TrendHistory.latestReportTime(this.properties);
    return time > 0 ? this.formatTime(new Date(time)) : '--';
  }

  private saveCachedState(): void {
    // 查询结束后再读取共享缓存，避免较旧页面的历史覆盖新点。
    const history = TrendHistory.collect(AppStorage.get<string>('cachedAppState'), this.properties);
    this.tempHistory = history.tempHistory;
    this.humiHistory = history.humiHistory;
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

  private getSensorStatusMask(): number {
    const raw = PropertyHelper.getValue(this.properties, 'sensor_status');
    const parsed = parseInt(raw);
    return Number.isNaN(parsed) ? 0 : parsed;
  }

  private hasSensorStatus(): boolean {
    return PropertyHelper.getValue(this.properties, 'sensor_status') !== '';
  }

  private isSensorValid(bit: number): boolean {
    return (this.getSensorStatusMask() & (1 << bit)) !== 0;
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
    this.saveCachedState();
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
      // 先保持用户选择的模式，等待 STM32 下一次上报后再由自动刷新确认。
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
      // 先保持用户选择的模式，等待 STM32 下一次上报后再由自动刷新确认。
    } catch (error) {
      const err = error as Error;
      this.errorMessage = '控制失败：' + err.message;
    } finally {
      this.loading = false;
    }
  }

  private getDisplayValue(identifier: string): string {
    const value = PropertyHelper.formatValue(
      identifier,
      PropertyHelper.getValue(this.properties, identifier)
    );
    if (identifier === 'water_vol' && value !== '--') {
      const volume = Number(value);
      if (Number.isFinite(volume)) {
        return volume.toFixed(2);
      }
    }
    return value;
  }

  private getUnit(identifier: string): string {
    return PropertyHelper.getUnit(identifier);
  }

  private parseIntValue(value: string, fallback: number): number {
    const parsed = parseInt(value);
    return Number.isNaN(parsed) ? fallback : parsed;
  }

  private getThresholdMin(identifier: string): number {
    return identifier === 'CO2threhold' ? 350 : 0;
  }

  private getThresholdMax(identifier: string): number {
    return identifier === 'CO2threhold' ? 4000 : 100;
  }

  private updateLocalProperty(identifier: string, value: string): void {
    const updated: DeviceProperty[] = [];
    let found = false;

    for (let i: number = 0; i < this.properties.length; i++) {
      if (this.properties[i].identifier === identifier) {
        updated.push({
          identifier: this.properties[i].identifier,
          value: value,
          name: this.properties[i].name,
          dataType: this.properties[i].dataType,
          accessMode: this.properties[i].accessMode,
          time: this.properties[i].time
        });
        found = true;
      } else {
        updated.push(this.properties[i]);
      }
    }

    if (!found) {
      updated.push({ identifier: identifier, value: value });
    }
    this.properties = updated;
  }

  private async adjustThreshold(identifier: string, delta: number): Promise<void> {
    if (this.loading) {
      return;
    }

    this.loading = true;
    this.errorMessage = '';

    try {
      const current = this.parseIntValue(
        PropertyHelper.getValue(this.properties, identifier),
        0
      );
      let next = current + delta;
      const min = this.getThresholdMin(identifier);
      const max = this.getThresholdMax(identifier);

      if (next < min) {
        next = min;
      }
      if (next > max) {
        next = max;
      }

      await this.oneNetApi.setThreshold(identifier, next);
      this.updateLocalProperty(identifier, next.toString());
    } catch (error) {
      const err = error as Error;
      this.errorMessage = '阈值下发失败：' + err.message;
    } finally {
      this.loading = false;
    }
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

  private openTrend(metric: number): void {
    this.trendMetric = metric;
    this.trendAutoFit = true;
    this.trendExpanded = true;
  }

  @Builder
  expandedTrendView() {
    Column({ space: 16 }) {
      Row({ space: 12 }) {
        Column({ space: 4 }) {
          Text('温湿度趋势').fontSize(23).fontWeight(FontWeight.Bold).fontColor(AppTheme.ink)
          Text('按实际上报时间展示').fontSize(12).fontColor(AppTheme.secondary)
        }.layoutWeight(1).alignItems(HorizontalAlign.Start)
        Button({ type: ButtonType.Normal }) {
          Text('关闭').fontSize(14).fontColor(AppTheme.primary)
        }.width(64).height(44).padding(0).backgroundColor(AppTheme.pale).borderRadius(14)
        .accessibilityText('关闭放大趋势')
        .onClick(() => { this.trendExpanded = false; })
      }.width('100%')
      Scroll() {
        Column({ space: 16 }) {
          Row({ space: 8 }) {
            DetailModeButton({ label: '空气温度', mode: 0, selected: this.trendMetric === 0,
              onModeClick: (mode: number) => { this.trendMetric = mode; } })
            DetailModeButton({ label: '空气湿度', mode: 1, selected: this.trendMetric === 1,
              onModeClick: (mode: number) => { this.trendMetric = mode; } })
          }.width('100%')
          Row({ space: 8 }) {
            DetailModeButton({ label: '局部放大', mode: 1, selected: this.trendAutoFit,
              onModeClick: (_mode: number) => { this.trendAutoFit = true; } })
            DetailModeButton({ label: '完整量程', mode: 0, selected: !this.trendAutoFit,
              onModeClick: (_mode: number) => { this.trendAutoFit = false; } })
          }.width('100%')
          Column() {
            if (this.trendMetric === 0) {
              HistoryChart({ title: '空气温度', samples: this.tempHistory, color: '#B77B45',
                minValue: 0, maxValue: 50, unit: '℃', enlarged: true, autoFit: this.trendAutoFit, plotHeight: 280 })
            } else {
              HistoryChart({ title: '空气湿度', samples: this.humiHistory, color: '#3D8D9C',
                minValue: 0, maxValue: 100, unit: '%', enlarged: true, autoFit: this.trendAutoFit, plotHeight: 280 })
            }
          }.width('100%').padding(18).backgroundColor(AppTheme.surface)
          .border({ width: 1, color: AppTheme.border }).borderRadius(22)
          Row({ space: 12 }) {
            Text(this.autoRefresh ? '跟随详情页自动刷新' : '自动刷新已关闭')
              .fontSize(12).fontColor(AppTheme.secondary).layoutWeight(1)
            Button({ type: ButtonType.Normal }) {
              Text(this.loading ? '刷新中' : '立即刷新').fontSize(14).fontColor(AppTheme.primary)
            }.height(44).padding({ left: 18, right: 18 }).borderRadius(14)
            .backgroundColor(AppTheme.pale).enabled(!this.loading)
            .onClick(() => { this.refresh(); })
          }.width('100%')
          Text('仅显示本机获取的最近 20 条有效上报。放大只改变图表显示，不改变传感器读数。')
            .fontSize(12).fontColor(AppTheme.secondary).width('100%').lineHeight(20)
          if (this.errorMessage !== '') {
            SyncErrorCard({ message: this.errorMessage })
          }
        }.width('100%').padding({ bottom: 24 })
      }.width('100%').layoutWeight(1).scrollBar(BarState.Off)
    }.width('100%').height('100%').padding({ left: 20, right: 20, top: 16, bottom: 16 })
    .backgroundColor(AppTheme.background)
  }

  build() {
    NavDestination() {
      Scroll() {
        Column() {
          Column({ space: 12 }) {
            Row({ space: 12 }) {
              Image($r('app.media.monitor_leaf')).width(46).height(46).padding(10)
                .backgroundColor(AppTheme.pale).borderRadius(16).accessibilityLevel('no')
              Column({ space: 5 }) {
                Text(this.deviceName).fontSize(22).fontWeight(FontWeight.Bold).fontColor(AppTheme.ink)
                  .maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis })
                Text('环境监测 · 远程控制').fontSize(12).fontColor(AppTheme.secondary)
              }
              .layoutWeight(1).alignItems(HorizontalAlign.Start)
              StatusBadge({ online: this.online, known: this.lastReportTime > 0 })
            }
            .width('100%')
            Divider().color(AppTheme.border)
            Column() {
              DetailPropertyRow({ label: '最后同步', value: this.lastSyncTime })
              DetailPropertyRow({ label: '设备上报', value: this.reportTimeText() })
              DetailPropertyRow({ label: '最后在线', value: this.formatCloudTime(this.lastOnlineTime) })
            }
            .width('100%')
          }
          .width('100%').padding(18).backgroundColor(AppTheme.surface)
          .border({ width: 1, color: AppTheme.border }).borderRadius(22)

          if (this.errorMessage !== '') {
            SyncErrorCard({ message: this.errorMessage }).margin({ top: 12 })
          }
          if (!this.online && this.lastReportTime > 0) {
            OfflineNotice({ lastOnline: this.formatCloudTime(this.lastOnlineTime) }).margin({ top: 12 })
          }
          SyncToolbar({
            autoRefresh: this.autoRefresh,
            loading: this.loading,
            onToggle: (enabled: boolean) => {
              this.autoRefresh = enabled;
              if (enabled) {
                this.startAutoRefresh();
                this.refresh();
              } else {
                this.stopAutoRefresh();
              }
            },
            onRefresh: () => { this.refresh(); }
          }).margin({ top: 16 })

          SectionHeading({ title: '环境读数', caption: '最近一次上报' })
          Row({ space: 10 }) {
            MetricCard({ label: '空气温度', value: this.getDisplayValue('temp'),
              unit: this.getUnit('temp'), icon: $r('app.media.monitor_temperature'),
              accent: '#B77B45', tint: '#FCF1E7' })
            MetricCard({ label: '空气湿度', value: this.getDisplayValue('humi'),
              unit: this.getUnit('humi'), icon: $r('app.media.monitor_humidity'),
              accent: '#3D8D9C', tint: '#EBF5F6' })
          }.width('100%').margin({ bottom: 10 })
          Row({ space: 10 }) {
            MetricCard({ label: '光照强度', value: this.getDisplayValue('light'),
              unit: this.getUnit('light'), icon: $r('app.media.monitor_sun'),
              accent: '#AA7C31', tint: '#FBF2DD' })
            MetricCard({ label: 'CO₂ 浓度', value: this.getDisplayValue('ppm'),
              unit: this.getUnit('ppm'), icon: $r('app.media.monitor_air'),
              accent: '#677BA0', tint: '#EEF1F8' })
          }.width('100%').margin({ bottom: 10 })
          Row({ space: 10 }) {
            MetricCard({ label: '土壤湿度', value: this.getDisplayValue('moist'),
              unit: this.getUnit('moist'), icon: $r('app.media.monitor_soil'),
              accent: '#467C59', tint: '#ECF4EB' })
            MetricCard({ label: '估算补水量', value: this.getDisplayValue('water_vol'),
              unit: this.getUnit('water_vol'), icon: $r('app.media.monitor_water'),
              accent: '#44879B', tint: '#ECF4F7' })
          }.width('100%').margin({ bottom: 10 })
          SectionHeading({ title: '设备控制', caption: '选择工作模式' })
          Column({ space: 16 }) {
            Row({ space: 10 }) {
              Image($r('app.media.monitor_sun')).width(24).height(24).accessibilityLevel('no')
              Text('照明模式').fontSize(16).fontWeight(FontWeight.Bold)
                .fontColor(AppTheme.ink).layoutWeight(1)
              Text(PropertyHelper.getLedModeText(this.ledMode.toString()))
                .fontSize(13).fontColor(AppTheme.primary)
            }.width('100%')
            Row({ space: 8 }) {
              DetailModeButton({ label: '关闭', mode: 0, selected: this.ledMode === 0,
                buttonEnabled: !this.loading, onModeClick: (mode: number) => { this.setLedMode(mode); } })
              DetailModeButton({ label: '常亮', mode: 1, selected: this.ledMode === 1,
                buttonEnabled: !this.loading, onModeClick: (mode: number) => { this.setLedMode(mode); } })
              DetailModeButton({ label: '自动', mode: 2, selected: this.ledMode === 2,
                buttonEnabled: !this.loading, onModeClick: (mode: number) => { this.setLedMode(mode); } })
            }.width('100%')
          }
          .width('100%').padding(18).margin({ bottom: 12 })
          .backgroundColor(AppTheme.surface).border({ width: 1, color: AppTheme.border }).borderRadius(22)
          Column({ space: 16 }) {
            Row({ space: 10 }) {
              Image($r('app.media.monitor_water')).width(24).height(24).accessibilityLevel('no')
              Text('灌溉模式').fontSize(16).fontWeight(FontWeight.Bold)
                .fontColor(AppTheme.ink).layoutWeight(1)
              Text(PropertyHelper.getBumpModeText(this.bumpMode.toString()))
                .fontSize(13).fontColor(AppTheme.primary)
            }.width('100%')
            Row({ space: 8 }) {
              DetailModeButton({ label: '关闭', mode: 0, selected: this.bumpMode === 0,
                buttonEnabled: !this.loading, onModeClick: (mode: number) => { this.setBumpMode(mode); } })
              DetailModeButton({ label: '脉冲灌溉', mode: 1, selected: this.bumpMode === 1,
                buttonEnabled: !this.loading, onModeClick: (mode: number) => { this.setBumpMode(mode); } })
              DetailModeButton({ label: '自动', mode: 2, selected: this.bumpMode === 2,
                buttonEnabled: !this.loading, onModeClick: (mode: number) => { this.setBumpMode(mode); } })
            }.width('100%')
            Text('脉冲灌溉按估算补水量间歇运行，结束后自动关闭。')
              .fontSize(12).fontColor(AppTheme.secondary).width('100%')
          }
          .width('100%').padding(18).margin({ bottom: 12 })
          .backgroundColor(AppTheme.surface).border({ width: 1, color: AppTheme.border }).borderRadius(22)
          SectionHeading({ title: '阈值设置', caption: '修改后下发设备' })
          Column() {
            ThresholdStepper({ label: 'CO₂ 阈值', valueText: this.getDisplayValue('CO2threhold'),
              unit: this.getUnit('CO2threhold'), buttonEnabled: !this.loading,
              onDecrease: () => { this.adjustThreshold('CO2threhold', -100); },
              onIncrease: () => { this.adjustThreshold('CO2threhold', 100); } })
            Divider().color(AppTheme.border)
            ThresholdStepper({ label: '干旱阈值', valueText: this.getDisplayValue('droughtthrehold'),
              unit: this.getUnit('droughtthrehold'), buttonEnabled: !this.loading,
              onDecrease: () => { this.adjustThreshold('droughtthrehold', -1); },
              onIncrease: () => { this.adjustThreshold('droughtthrehold', 1); } })
            Divider().color(AppTheme.border)
            ThresholdStepper({ label: '湿润阈值', valueText: this.getDisplayValue('moistthrehold'),
              unit: this.getUnit('moistthrehold'), buttonEnabled: !this.loading,
              onDecrease: () => { this.adjustThreshold('moistthrehold', -1); },
              onIncrease: () => { this.adjustThreshold('moistthrehold', 1); } })
            Divider().color(AppTheme.border)
            ThresholdStepper({ label: '温度阈值', valueText: this.getDisplayValue('tempthrehold'),
              unit: this.getUnit('tempthrehold'), buttonEnabled: !this.loading,
              onDecrease: () => { this.adjustThreshold('tempthrehold', -1); },
              onIncrease: () => { this.adjustThreshold('tempthrehold', 1); } })
          }
          .width('100%').padding({ left: 18, right: 18, top: 2, bottom: 2 })
          .backgroundColor(AppTheme.surface).border({ width: 1, color: AppTheme.border }).borderRadius(22)

          SectionHeading({ title: '温湿度趋势', caption: '最多保留 20 条上报' })
          Column({ space: 18 }) {
            HistoryChart({ title: '空气温度', samples: this.tempHistory, color: '#B77B45', minValue: 0, maxValue: 50, unit: '℃', onExpand: () => { this.openTrend(0); } })
            Divider().color(AppTheme.border)
            HistoryChart({ title: '空气湿度', samples: this.humiHistory, color: '#3D8D9C', minValue: 0, maxValue: 100, unit: '%', onExpand: () => { this.openTrend(1); } })
            Text('仅记录本机已获取的有效上报，横轴为上报时间。')
              .fontSize(11).fontColor(AppTheme.muted).width('100%')
          }
          .width('100%').padding(18).backgroundColor(AppTheme.surface)
          .border({ width: 1, color: AppTheme.border }).borderRadius(22)

          if (this.hasSensorStatus()) {
            SectionHeading({ title: '采集状态', caption: '传感器数据校验' })
            Column() {
              SensorStatusRow({ label: '空气温度', valid: this.isSensorValid(0) })
              Divider().color(AppTheme.border)
              SensorStatusRow({ label: '空气湿度', valid: this.isSensorValid(1) })
              Divider().color(AppTheme.border)
              SensorStatusRow({ label: 'CO₂ 浓度', valid: this.isSensorValid(3) })
            }
            .width('100%').padding({ left: 18, right: 18, top: 4, bottom: 4 })
            .backgroundColor(AppTheme.surface).border({ width: 1, color: AppTheme.border }).borderRadius(22)
          }

          Column() {
            Button({ type: ButtonType.Normal }) {
              Row() {
                Text('全部属性').fontSize(15).fontWeight(FontWeight.Medium).fontColor(AppTheme.ink)
                  .layoutWeight(1).textAlign(TextAlign.Start)
                Text(this.properties.length.toString() + ' 项').fontSize(12).fontColor(AppTheme.secondary)
                Text(this.propertiesExpanded ? '收起' : '展开').fontSize(12).fontColor(AppTheme.primary)
                  .margin({ left: 12 })
              }.width('100%')
            }
            .width('100%').height(54).padding({ left: 18, right: 18 })
            .backgroundColor(Color.Transparent)
            .onClick(() => { this.propertiesExpanded = !this.propertiesExpanded; })
            if (this.propertiesExpanded) {
              Divider().color(AppTheme.border)
              Column() {
                if (this.properties.length === 0) {
                  Text('同步成功后显示设备属性').fontSize(13).fontColor(AppTheme.muted)
                    .width('100%').padding({ top: 12, bottom: 12 })
                }
                ForEach(this.properties, (property: DeviceProperty) => {
                  DetailPropertyRow({ label: PropertyHelper.getLabel(property.identifier),
                    value: PropertyHelper.formatValue(property.identifier, property.value),
                    unit: PropertyHelper.getUnit(property.identifier) })
                }, (property: DeviceProperty) => property.identifier + '_' + property.value + '_' +
                  (property.time === undefined ? '0' : property.time.toString()))
              }.width('100%').padding({ left: 18, right: 18, bottom: 10 })
            }
          }
          .width('100%').margin({ top: 24 }).backgroundColor(AppTheme.surface)
          .border({ width: 1, color: AppTheme.border }).borderRadius(20)
        }
        .width('100%').alignItems(HorizontalAlign.Start)
        .padding({ left: 20, right: 20, top: 12, bottom: 32 })
      }
      .width('100%').height('100%').scrollBar(BarState.Off)
      .backgroundColor(AppTheme.background)
    }
    .backgroundColor(AppTheme.background)
    .bindContentCover($$this.trendExpanded, this.expandedTrendView(), { modalTransition: ModalTransition.DEFAULT })
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
        this.lastReportTime = 0;
        for (let i: number = 0; i < param.properties.length; i++) {
          const reportTime = param.properties[i].time;
          if (reportTime !== undefined && Number.isFinite(reportTime) && reportTime > this.lastReportTime) { this.lastReportTime = reportTime; }
        }

        const led = PropertyHelper.getValue(param.properties, 'led');
        if (led !== '') {
          this.ledMode = parseInt(led);
        }

        this.restoreCachedHistory();
        this.refresh();
        this.startAutoRefresh();
      }
    })
  }
}
