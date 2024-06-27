import Rete from "rete";
import VueDeviceControl from "../controls/deviceControl.vue";

export class DeviceControl extends Rete.Control {
  constructor(emitter, key, readonly) {
    super(key);
    this.component = VueDeviceControl;
    this.props = { emitter, ikey: key, readonly };
  }

  setValue(val) {
    this.vueContext.value = val;
  }
}
