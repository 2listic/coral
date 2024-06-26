import Rete from "rete";
import VueCanvasControl from "../controls/canvasControl.vue";

export class CanvasControl extends Rete.Control {
  constructor(emitter, key, readonly) {
    super(key);
    this.component = VueCanvasControl;
    this.props = { emitter, readonly };
  }

  getCanvas() {
    return this.vueContext.$refs.canvas;
  }
}
