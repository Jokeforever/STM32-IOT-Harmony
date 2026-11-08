import { TrendPoint, TrendRange, TrendViewport } from '../model/DeviceModels';
import { TrendHistory } from '../model/TrendHistory';
import { AppTheme } from '../theme/AppTheme';

@Component
export struct HistoryChart {
  @Prop @Watch('resetViewport') title: string = '';
  @Prop samples: TrendPoint[] = [];
  @Prop color: string = '#28634E';
  @Prop minValue: number = 0;
  @Prop maxValue: number = 100;
  @Prop unit: string = '';
  @Prop enlarged: boolean = false;
  @Prop @Watch('resetViewport') autoFit: boolean = false;
  @Prop plotHeight: number = 128;
  onExpand?: () => void;
  @State plotWidth: number = 0;
  @State zoomAxis: number = 0;
  @State timeLocked: boolean = false;
  @State valueLocked: boolean = false;
  @State viewport: TrendViewport = { timeMin: 1, timeMax: 1001, valueMin: 0, valueMax: 100 };
  @State bounds: TrendViewport = { timeMin: 1, timeMax: 1001, valueMin: 0, valueMax: 100 };
  private gestureView: TrendViewport = { timeMin: 1, timeMax: 1001, valueMin: 0, valueMax: 100 };
  private gestureBounds: TrendViewport = { timeMin: 1, timeMax: 1001, valueMin: 0, valueMax: 100 };
  private pinchScale: number = 1;
  private pinchX: number = 0.5;
  private pinchY: number = 0.5;
  private lastAxisPinch: number = 1;
  private panX: number = 0;
  private panY: number = 0;

  private range(): TrendRange {
    return TrendHistory.displayRange(this.samples, this.minValue, this.maxValue,
      this.autoFit, this.unit === '℃' ? 4 : 10);
  }

  private baseViewport(): TrendViewport { return TrendHistory.viewportFor(this.samples, this.range()); }

  private view(): TrendViewport {
    const live = this.baseViewport();
    return {
      timeMin: this.timeLocked ? this.viewport.timeMin : live.timeMin,
      timeMax: this.timeLocked ? this.viewport.timeMax : live.timeMax,
      valueMin: this.valueLocked ? this.viewport.valueMin : live.valueMin,
      valueMax: this.valueLocked ? this.viewport.valueMax : live.valueMax
    };
  }

  private limits(): TrendViewport {
    const live = this.baseViewport();
    return {
      timeMin: this.timeLocked ? this.bounds.timeMin : live.timeMin,
      timeMax: this.timeLocked ? this.bounds.timeMax : live.timeMax,
      valueMin: this.valueLocked ? this.bounds.valueMin : live.valueMin,
      valueMax: this.valueLocked ? this.bounds.valueMax : live.valueMax
    };
  }

  private resetViewport(): void { this.timeLocked = false; this.valueLocked = false; }

  private acceptView(next: TrendViewport, bounds: TrendViewport): void {
    this.bounds = bounds;
    this.viewport = next;
    this.timeLocked = (bounds.timeMax - bounds.timeMin) / (next.timeMax - next.timeMin) > 1.00001;
    this.valueLocked = (bounds.valueMax - bounds.valueMin) / (next.valueMax - next.valueMin) > 1.00001;
  }

  private scaleAt(factor: number, x: number, y: number): void {
    if (!this.enlarged || this.samples.length === 0) { return; }
    const bounds = this.limits();
    this.acceptView(TrendHistory.zoomViewport(this.view(), bounds, factor, x, y,
      this.zoomAxis !== 0, this.zoomAxis !== 1), bounds);
  }

  private zoomText(): string {
    const view = this.view();
    const bounds = this.limits();
    const x = (bounds.timeMax - bounds.timeMin) / (view.timeMax - view.timeMin);
    const y = (bounds.valueMax - bounds.valueMin) / (view.valueMax - view.valueMin);
    return '时间 ' + x.toFixed(1) + '× · 数值 ' + y.toFixed(1) + '×';
  }

  private selectedZoom(): number {
    const view = this.view();
    const bounds = this.limits();
    return this.zoomAxis === 1 ? (bounds.timeMax - bounds.timeMin) / (view.timeMax - view.timeMin) :
      (bounds.valueMax - bounds.valueMin) / (view.valueMax - view.valueMin);
  }

  private sliderPosition(): number { return Math.log(this.selectedZoom()) / Math.log(TrendHistory.MAX_ZOOM) * 100; }

  private slideZoom(value: number): void {
    const view = this.view();
    const last = this.samples.length > 0 ? this.samples[this.samples.length - 1] : undefined;
    const x = last !== undefined && last.time >= view.timeMin && last.time <= view.timeMax ?
      (last.time - view.timeMin) / (view.timeMax - view.timeMin) : 0.5;
    const y = last !== undefined && last.value >= view.valueMin && last.value <= view.valueMax ?
      (last.value - view.valueMin) / (view.valueMax - view.valueMin) : 0.5;
    this.scaleAt(Math.pow(TrendHistory.MAX_ZOOM, value / 100) / this.selectedZoom(), x, y);
  }

  private onWheel(event: AxisEvent): void {
    if (!this.enlarged || this.samples.length === 0) { event.propagation(); return; }
    if (event.action === AxisAction.END || event.action === AxisAction.CANCEL) { return; }
    if (event.action === AxisAction.BEGIN) { this.lastAxisPinch = 1; }
    const pinch = event.getPinchAxisScaleValue();
    if (Number.isFinite(pinch) && pinch > 0) {
      this.scaleAt(pinch / this.lastAxisPinch, (event.x - 4) / (this.plotWidth - 8),
        1 - (event.y - 8) / (this.plotHeight - 16));
      this.lastAxisPinch = pinch;
      return;
    }
    const delta = event.getVerticalAxisValue();
    if (!Number.isFinite(delta) || delta === 0) { event.propagation(); return; }
    this.scaleAt(TrendHistory.wheelZoomFactor(delta), (event.x - 4) / (this.plotWidth - 8),
      1 - (event.y - 8) / (this.plotHeight - 16));
  }

  private beginPan(event: GestureEvent): void {
    this.gestureView = this.view(); this.gestureBounds = this.limits();
    this.panX = event.offsetX; this.panY = event.offsetY;
  }

  private movePan(event: GestureEvent): void {
    if (this.plotWidth <= 8) { return; }
    this.acceptView(TrendHistory.panViewport(this.gestureView, this.gestureBounds,
      -(event.offsetX - this.panX) / (this.plotWidth - 8),
      (event.offsetY - this.panY) / (this.plotHeight - 16)), this.gestureBounds);
  }

  private beginPinch(event: GestureEvent): void {
    this.gestureView = this.view(); this.gestureBounds = this.limits();
    this.pinchScale = event.scale > 0 ? event.scale : 1;
    this.pinchX = (event.pinchCenterX - 4) / (this.plotWidth - 8);
    this.pinchY = 1 - (event.pinchCenterY - 8) / (this.plotHeight - 16);
  }

  private movePinch(event: GestureEvent): void {
    this.acceptView(TrendHistory.zoomViewport(this.gestureView, this.gestureBounds, event.scale / this.pinchScale,
      this.pinchX, this.pinchY, this.zoomAxis !== 0, this.zoomAxis !== 1), this.gestureBounds);
  }

  private buildPoints(): number[][] {
    return TrendHistory.viewportPoints(this.samples, this.view(), this.plotWidth, this.plotHeight);
  }

  private tick(index: number): string {
    const view = this.view();
    const step = (view.valueMax - view.valueMin) / 4;
    const value = view.valueMax - step * index;
    return value.toFixed(step < 0.1 ? 3 : (step < 1 ? 2 : (Number.isInteger(value) ? 0 : 1)));
  }

  private latestVisible(): boolean {
    if (this.samples.length === 0) { return false; }
    const point = this.samples[this.samples.length - 1];
    const view = this.view();
    return point.time >= view.timeMin && point.time <= view.timeMax && point.value >= view.valueMin && point.value <= view.valueMax;
  }

  private latestCoordinate(axis: number): number {
    const points = this.buildPoints();
    return points.length > 0 ? points[points.length - 1][axis] - 4 : 0;
  }

  private timeLabel(end: boolean): string {
    if (this.samples.length === 0) { return '--'; }
    const view = this.view();
    return TrendHistory.formatPointTime(Math.round(end ? view.timeMax : view.timeMin), view.timeMax - view.timeMin < 10000);
  }

  @Builder
  plotContent() {
    Stack({ alignContent: Alignment.TopStart }) {
      Column() {
        ForEach([0, 1, 2, 3, 4], (index: number) => {
          Divider().color('#E8EEE7').strokeWidth(0.5)
        }, (index: number) => index.toString())
      }.width('100%').height(this.plotHeight - 16).margin({ top: 8 }).justifyContent(FlexAlign.SpaceBetween)
      if (this.samples.length === 0) {
        Text('等待有效上报').fontSize(12).fontColor(AppTheme.muted)
          .width('100%').height(this.plotHeight).textAlign(TextAlign.Center)
      } else if (this.plotWidth > 8) {
        if (this.samples.length > 1) {
          Polyline({ width: this.plotWidth, height: this.plotHeight })
            .points(this.buildPoints()).stroke(this.color).strokeWidth(this.enlarged ? 3 : 2.5).fill(Color.Transparent)
        }
        if (this.latestVisible()) {
          Circle().width(8).height(8).fill(this.color).stroke(Color.White).strokeWidth(2)
            .position({ x: this.latestCoordinate(0), y: this.latestCoordinate(1) })
        }
        if (this.samples.length === 1) {
          Text('等待下一次上报').fontSize(12).fontColor(AppTheme.muted)
            .width('100%').height(this.plotHeight).textAlign(TextAlign.Center)
        }
      }
    }.width('100%').height(this.plotHeight).clip(true)
    .onAreaChange((_oldArea: Area, newArea: Area) => {
      const width = parseFloat(newArea.width.toString());
      if (Number.isFinite(width) && width > 8 && this.plotWidth !== width) { this.plotWidth = width; }
    })
  }

  build() {
    Column({ space: 16 }) {
      Row({ space: 12 }) {
        Column({ space: 5 }) {
          Text(this.title).fontSize(15).fontWeight(FontWeight.Medium).fontColor(AppTheme.ink)
          Text('最近 ' + this.samples.length.toString() + ' 次有效上报').fontSize(11).fontColor(AppTheme.muted)
        }.alignItems(HorizontalAlign.Start).layoutWeight(1)
        Text(this.samples.length > 0 ? this.samples[this.samples.length - 1].value.toFixed(1) + this.unit : '--')
          .fontSize(this.enlarged ? 30 : 23).fontWeight(FontWeight.Bold).fontColor(this.color)
          .maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis })
        if (!this.enlarged) {
          Button({ type: ButtonType.Normal }) {
            Text('放大').fontSize(12).fontWeight(FontWeight.Medium).fontColor(AppTheme.primary)
          }.width(56).height(44).padding(0).borderRadius(13).backgroundColor(AppTheme.pale)
          .accessibilityText('放大查看' + this.title + '趋势')
          .onClick(() => { if (this.onExpand) { this.onExpand(); } })
        }
      }.width('100%')
      if (this.enlarged) {
        Row({ space: 6 }) {
          ForEach(['数值', '时间', '双轴'], (label: string, index: number) => {
            Button({ type: ButtonType.Normal }) {
              Text(label).fontSize(12).fontColor(this.zoomAxis === index ? Color.White : AppTheme.primary)
            }.height(40).layoutWeight(1).padding(0).borderRadius(12)
            .backgroundColor(this.zoomAxis === index ? AppTheme.primary : AppTheme.pale)
            .accessibilityText('缩放' + label + '轴').onClick(() => { this.zoomAxis = index; })
          }, (label: string) => label)
          Button({ type: ButtonType.Normal }) {
            Text('重置').fontSize(12).fontColor(AppTheme.primary)
          }.height(40).width(56).padding(0).borderRadius(12).backgroundColor(AppTheme.pale)
          .onClick(() => { this.resetViewport(); })
        }.width('100%')
        Row({ space: 8 }) {
          Text('1×').fontSize(11).fontColor(AppTheme.muted)
          Slider({ value: this.sliderPosition(), min: 0, max: 100, step: 1 })
            .layoutWeight(1).selectedColor(AppTheme.primary).trackColor(AppTheme.pale)
            .enabled(this.samples.length > 0).onChange((value: number) => { this.slideZoom(value); })
          Text(TrendHistory.MAX_ZOOM.toString() + '×').fontSize(11).fontColor(AppTheme.muted)
        }.width('100%')
        Text(this.zoomText()).fontSize(12).fontColor(AppTheme.primary).width('100%')
      }
      Row({ space: 8 }) {
        Column() {
          ForEach([0, 1, 2, 3, 4], (index: number) => {
            Text(this.tick(index)).fontSize(11).fontColor(AppTheme.muted).width('100%').textAlign(TextAlign.End)
          }, (index: number) => index.toString())
        }.width(this.enlarged ? 48 : 36).height(this.plotHeight).justifyContent(FlexAlign.SpaceBetween)
        if (this.enlarged) {
          Stack() { this.plotContent() }.layoutWeight(1).height(this.plotHeight)
            .onAxisEvent((event: AxisEvent) => { this.onWheel(event); })
            .onGestureJudgeBegin((_info: GestureInfo, event: BaseGestureEvent) => {
              // 轴事件由onAxisEvent处理，避免滚轮同时触发Pan而平移或滚动页面。
              if ((event.axisVertical ?? 0) !== 0 || (event.axisHorizontal ?? 0) !== 0 || (event.axisPinch ?? 0) !== 0) {
                return GestureJudgeResult.REJECT;
              }
              return GestureJudgeResult.CONTINUE;
            })
            .priorityGesture(GestureGroup(GestureMode.Exclusive,
              PinchGesture({ fingers: 2 }).onActionStart((event: GestureEvent) => { this.beginPinch(event); })
                .onActionUpdate((event: GestureEvent) => { this.movePinch(event); }),
              PanGesture({ fingers: 1, direction: PanDirection.All }).onActionStart((event: GestureEvent) => { this.beginPan(event); })
                .onActionUpdate((event: GestureEvent) => { this.movePan(event); }),
              TapGesture({ count: 2 }).onAction(() => { this.resetViewport(); })
            ))
        } else {
          Stack() { this.plotContent() }.layoutWeight(1).height(this.plotHeight)
        }
      }.width('100%').alignItems(VerticalAlign.Top)
      Row({ space: 8 }) {
        Text(this.unit).fontSize(11).fontColor(AppTheme.muted).width(this.enlarged ? 48 : 36).textAlign(TextAlign.End)
        Row() {
          Text(this.timeLabel(false)).fontSize(10).fontColor(AppTheme.muted).maxLines(1)
          Blank()
          Text(this.samples.length > 1 ? this.timeLabel(true) : '').fontSize(10).fontColor(AppTheme.muted).maxLines(1)
        }.layoutWeight(1)
      }.width('100%')
      if (this.enlarged) {
        Text('图内滚轮 / 双指缩放 · 拖动平移 · 双击或重置恢复')
          .fontSize(12).fontColor(AppTheme.secondary).width('100%').lineHeight(20)
        if (this.timeLocked || this.valueLocked) {
          Text('已锁定缩放轴的视窗；新上报仍接收，重置可回到最新全图。')
            .fontSize(11).fontColor(AppTheme.muted).width('100%')
        }
      }
    }.width('100%').padding({ top: 4, bottom: 4 })
  }
}
