import Rete from "rete";
import { NumSocket } from "../sockets/sockets";

export class InputComponent extends Rete.Component {
  constructor() {
    super("Input");
    this.interval = null;
  }

  builder(node) {
    let out1 = new Rete.Output("temp", "Temperature", NumSocket);

    node.addOutput(out1);

    // Initialize temperature data
    node.data.temp = this.generateRandomTemperature();
  }

  worker(node, inputs, outputs) {
    if (!this.interval) {
      this.startTemperatureSimulation(node);
    }
    outputs["temp"] = node.data.temp;
  }

  generateRandomTemperature() {
    let res = 0;
    let n = Math.random();
    if (n < 0.5) {
      res = n * 10 + 20;
    } else {
      res = n * 5 + 30;
    }
    return res;
  }

  startTemperatureSimulation(node) {
    this.interval = setInterval(() => {
      node.data.temp = this.generateRandomTemperature();
      this.editor.trigger("process");
    }, 1000); // Updates every second
  }
}
