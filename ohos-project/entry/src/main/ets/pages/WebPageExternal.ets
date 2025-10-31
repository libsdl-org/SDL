import webview from "@ohos.web.webview";

let pageURL = ""
export function updateURL(url: string)
{
  pageURL = url;
}
@CustomDialog
export default struct WebPageExternal {
  controller: CustomDialogController
  webcontroller: webview.WebviewController = new webview.WebviewController();
  onClose: () => void = () => {}

  build() {
    Column() {
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
      Web({
        src: pageURL,
        controller: this.webcontroller
      })
        .height('90%')
    }
    .justifyContent(FlexAlign.Center)
    .alignItems(HorizontalAlign.Start)
  }
}