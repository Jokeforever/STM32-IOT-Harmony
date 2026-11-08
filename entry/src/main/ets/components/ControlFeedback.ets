import { AppTheme } from '../theme/AppTheme';

@Component
export struct ControlFeedback {
  @Prop text: string = '';
  @Prop attention: boolean = false;

  build() {
    if (this.text !== '') {
      Text(this.text).fontSize(12).fontColor(this.attention ? '#9B6531' : AppTheme.primary)
        .lineHeight(19).width('100%').padding(10).borderRadius(12)
        .backgroundColor(this.attention ? '#FCF1E4' : AppTheme.pale)
    }
  }
}
