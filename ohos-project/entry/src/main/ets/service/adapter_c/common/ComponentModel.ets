import { Callback } from '@ohos.base';

export class XComponentModel {
  id: string;
  type: XComponentType = XComponentType.SURFACE;
  libraryname: string;
  focusable : boolean = true;
  onLoad : Callback<object> = (event? : object) => {
    // @todo something
  };
  onDestroy = () => {
    // @todo something
  };
  controller: XComponentController | null = null;

  constructor(id: string, type: XComponentType, libraryname: string) {
    this.id = id;
    this.type = type;
    this.libraryname = libraryname;
  }
}