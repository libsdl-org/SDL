const DOMAIN = 0x0000;

@Entry
@Component
struct Index {
  build() {
    Row() {
      Column() {
        XComponent({ id: 'SDL_XComponent', type: XComponentType.SURFACE, libraryname: 'SDL3' }).id('SDL_XComponent')
      }
      .width('100%')
    }
    .height('100%')
  }
}
