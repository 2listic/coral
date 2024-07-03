import Rete from "rete";
import DeviceControlComponent from "../controls/deviceControl.vue";

export class DeviceControl extends Rete.Control {
  constructor(emitter, key) {
    super(key);
    this.component = DeviceControlComponent;
    this.props = { emitter, ikey: key, readonly: false, status: false };
  }

  setImageSrc(src) {
    this.vueContext.setImageSrc(src);
  }

  setStatus(status) {
    this.vueContext.currentStatus = status; // Update to use data property
  }
}
