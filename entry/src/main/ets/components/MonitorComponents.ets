import { AppTheme } from '../theme/AppTheme';

@Component
export struct StatusBadge {
  @Prop online: boolean = false;
  @Prop known: boolean = true;
  @Prop dark: boolean = false;

  build() {
    Row({ space: 6 }) {
      Circle()
        .width(6)
        .height(6)
        .fill(!this.known ? '#C49A59' : (this.online ? '#5CA67A' : '#C87968'))
      Text(!this.known ? '未确认' : (this.online ? '在线' : '离线'))
        .fontSize(12)
        .fontWeight(FontWeight.Medium)
        .fontColor(this.dark ? '#E4F1E6' : AppTheme.secondary)
    }
    .padding({ left: 10, right: 10, top: 7, bottom: 7 })
    .backgroundColor(this.dark ? '#24543F' : '#F1F5F0')
    .borderRadius(20)
    .accessibilityText(!this.known ? '设备状态尚未确认' : (this.online ? '设备在线' : '设备离线'))
  }
}

@Component
export struct SectionHeading {
  @Prop title: string = '';
  @Prop caption: string = '';

  build() {
    Row() {
      Text(this.title)
        .fontSize(18)
        .fontWeight(FontWeight.Bold)
        .fontColor(AppTheme.ink)
        .layoutWeight(1)
      Text(this.caption)
        .fontSize(12)
        .fontColor(AppTheme.secondary)
        .maxLines(1)
        .textOverflow({ overflow: TextOverflow.Ellipsis })
    }
    .width('100%')
    .margin({ top: 26, bottom: 12 })
  }
}

@Component
export struct MetricCard {
  @Prop label: string = '';
  @Prop value: string = '--';
  @Prop unit: string = '';
  @Prop icon: Resource = $r('app.media.monitor_leaf');
  @Prop accent: string = '#28634E';
  @Prop tint: string = '#ECF4EB';
  @Prop compact: boolean = false;

  build() {
    Column({ space: 10 }) {
      Image(this.icon)
        .width(this.compact ? 26 : 30)
        .height(this.compact ? 26 : 30)
        .objectFit(ImageFit.Contain)
        .padding(5)
        .backgroundColor(this.tint)
        .borderRadius(9)
        .accessibilityLevel('no')
      Text(this.label)
        .fontSize(this.compact ? 12 : 13)
        .fontColor(AppTheme.secondary)
        .maxLines(1)
        .textOverflow({ overflow: TextOverflow.Ellipsis })
      Row({ space: 3 }) {
        Text(this.value)
          .fontSize(this.compact ? 22 : 30)
          .minFontSize(this.compact ? 16 : 20)
          .maxFontSize(this.compact ? 22 : 30)
          .height(36)
          .fontWeight(FontWeight.Bold)
          .fontColor(this.accent)
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
          .layoutWeight(1)
        if (!this.compact && this.value !== '--' && this.unit !== '') {
          Text(this.unit)
            .fontSize(12)
            .fontColor(AppTheme.secondary)
            .margin({ bottom: 3 })
        }
      }
      .width('100%')
      .alignItems(VerticalAlign.Bottom)
      if (this.compact) {
        Text(this.unit).fontSize(11).fontColor(AppTheme.secondary).height(14)
      }
    }
    .alignItems(HorizontalAlign.Start)
    .layoutWeight(1)
    .padding(this.compact ? 13 : 17)
    .backgroundColor(AppTheme.surface)
    .border({ width: 1, color: AppTheme.border })
    .borderRadius(20)
    .accessibilityText(this.label + ' ' + this.value + (this.value === '--' ? '' : this.unit))
  }
}

@Component
export struct SyncToolbar {
  @Prop autoRefresh: boolean = true;
  @Prop loading: boolean = false;
  onToggle?: (enabled: boolean) => void;
  onRefresh?: () => void;

  build() {
    Row({ space: 10 }) {
      Button({ type: ButtonType.Normal }) {
        Row({ space: 7 }) {
          Circle().width(6).height(6)
            .fill(this.autoRefresh ? AppTheme.primary : AppTheme.muted)
          Text(this.autoRefresh ? '10 秒自动刷新' : '自动刷新已暂停')
            .fontSize(13)
            .fontColor(this.autoRefresh ? AppTheme.primary : AppTheme.secondary)
        }
      }
      .height(44)
      .layoutWeight(1)
      .borderRadius(14)
      .backgroundColor(this.autoRefresh ? AppTheme.pale : '#EBEEE9')
      .onClick(() => {
        if (this.onToggle) {
          this.onToggle(!this.autoRefresh);
        }
      })

      Button({ type: ButtonType.Normal }) {
        Row({ space: 7 }) {
          Image($r('app.media.monitor_refresh')).width(16).height(16)
            .accessibilityLevel('no')
          Text(this.loading ? '正在同步' : '立即刷新')
            .fontSize(13)
            .fontWeight(FontWeight.Medium)
            .fontColor(Color.White)
        }
      }
      .height(44)
      .layoutWeight(1)
      .borderRadius(14)
      .backgroundColor(AppTheme.primary)
      .enabled(!this.loading)
      .onClick(() => {
        if (this.onRefresh) {
          this.onRefresh();
        }
      })
    }
    .width('100%')
  }
}

@Component
export struct SyncErrorCard {
  @Prop message: string = '';
  @State expanded: boolean = false;

  private isAuthorizationError(): boolean {
    return this.message.includes('10403') || this.message.includes('invalid authorization');
  }

  build() {
    Column({ space: 8 }) {
      Row({ space: 10 }) {
        Text('!')
          .fontSize(16)
          .fontWeight(FontWeight.Bold)
          .fontColor('#AB623C')
          .textAlign(TextAlign.Center)
          .width(28)
          .height(28)
          .backgroundColor('#F6E8D9')
          .borderRadius(10)
        Column({ space: 4 }) {
          Text(this.isAuthorizationError() ? '云端授权需要检查' : '操作暂未完成')
            .fontSize(14)
            .fontWeight(FontWeight.Medium)
            .fontColor('#8D5536')
          Text(this.isAuthorizationError()
            ? '暂时无法读取设备，请核对 OneNET 访问凭据。'
            : '请重试操作，或展开查看具体原因。')
            .fontSize(12)
            .fontColor('#9D7659')
        }
        .alignItems(HorizontalAlign.Start)
        .layoutWeight(1)
        Button(this.expanded ? '收起' : '详情', { type: ButtonType.Normal })
          .fontSize(12)
          .fontColor('#8D5536')
          .height(36)
          .padding({ left: 8, right: 8 })
          .backgroundColor(Color.Transparent)
          .onClick(() => { this.expanded = !this.expanded; })
      }
      .width('100%')
      if (this.expanded) {
        Text(this.message)
          .fontSize(12)
          .fontColor('#8D5536')
          .width('100%')
          .copyOption(CopyOptions.InApp)
      }
    }
    .width('100%')
    .alignItems(HorizontalAlign.Start)
    .padding(14)
    .backgroundColor('#FBF2E7')
    .border({ width: 1, color: '#EEDBC4' })
    .borderRadius(18)
  }
}

@Component
export struct OfflineNotice {
  @Prop lastOnline: string = '--';

  build() {
    Column({ space: 4 }) {
      Text('设备已离线，当前保留最近一次读数')
        .fontSize(13)
        .fontWeight(FontWeight.Medium)
        .fontColor('#9E5547')
      Text('最后在线  ' + this.lastOnline)
        .fontSize(12)
        .fontColor('#A87568')
    }
    .alignItems(HorizontalAlign.Start)
    .width('100%')
    .padding(14)
    .backgroundColor('#FBEEEA')
    .borderRadius(18)
  }
}
