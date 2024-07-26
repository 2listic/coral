import Rete from "rete";
import VueConsumptionControl from "./consumptionControl.vue";

export class ConsumptionControl extends Rete.Control {
  constructor(emitter, key, readonly) {
    super(key);
    this.component = VueConsumptionControl;
    this.props = { emitter, ikey: key, readonly };
  }

  setValue(value) {
    this.vueContext.updateChart(value.alwaysOn, value.actual);
  }
}
