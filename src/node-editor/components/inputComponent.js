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
    return Math.random() * 10 + 20; // Generates a number between 20 and 30
  }

  startTemperatureSimulation(node) {
    this.interval = setInterval(() => {
      node.data.temp = this.generateRandomTemperature();
      this.editor.trigger("process");
    }, 10000); // Updates every 10 seconds
  }
}
