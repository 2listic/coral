import Rete from "rete";
import { NumSocket } from "../sockets/sockets";
import { NumControl } from "../controls/numControl.js";

export class InputComponent extends Rete.Component {
  constructor() {
    super("Input");
    this.interval = null;
  }

  builder(node) {
    let out1 = new Rete.Output("temp", "Temperature", NumSocket);

    // Adding two numerical inputs for min and max temperature
    let minTempControl = new NumControl(this.editor, "minTemp", "Min Temp");
    let maxTempControl = new NumControl(this.editor, "maxTemp", "Max Temp");

    node.addControl(minTempControl);
    node.addControl(maxTempControl);
    node.addOutput(out1);

    node.data.temp = this.generateRandomTemperature(node);
  }

  worker(node, inputs, outputs) {
    if (!this.interval) {
      this.startTemperatureSimulation(node);
    }
    outputs["temp"] = node.data.temp;
  }

  generateRandomTemperature(node) {
    let minTemp = node.data.minTemp;
    let maxTemp = node.data.maxTemp;
    return Math.random() * (maxTemp - minTemp) + minTemp;
  }

  startTemperatureSimulation(node) {
    this.interval = setInterval(() => {
      node.data.temp = this.generateRandomTemperature(node);
      this.editor.trigger("process");
    }, 1000); // Updates every second
  }
}
