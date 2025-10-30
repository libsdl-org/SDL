import webview from "@ohos.web.webview";

@CustomDialog
export default struct WebPageExternal {
  controller: CustomDialogController
  webcontroller: webview.WebviewController = new webview.WebviewController();
  onClose: () => void = () => {}
  url: string = ""

  build() {
    Column() {
      Web({
        src: this.url,
        controller: this.webcontroller
      })
        .height('90%')
      Flex({ direction: FlexDirection.Row }) {
        Button("Close").onClick((event: ClickEvent) => {
          this.onClose()
        })
          .width(100)
          .height(20)
        Button("Refresh").onClick((event: ClickEvent) => {
          this.webcontroller.refresh()
        })
          .width(100)
          .height(20)
        Button("Stop").onClick((event: ClickEvent) => {
          this.webcontroller.stop()
        })
          .width(100)
          .height(20)
      }
    }
    .justifyContent(FlexAlign.Center)
    .alignItems(HorizontalAlign.Start)
  }
}