import Rete from "rete";
import { NumSocket } from "../sockets/sockets";
import { ShapeSocket } from "../sockets/sockets";

export class SimulationComponent extends Rete.Component {
  constructor() {
    super("Simulation");
  }

  builder(node) {
    let inp1 = new Rete.Input("temp", "Temperature", NumSocket);
    let inp2 = new Rete.Input("shape", "Shape", ShapeSocket);
    let out1 = new Rete.Output("result", "Result", NumSocket);

    node.addInput(inp1);
    node.addInput(inp2);
    node.addOutput(out1);
  }

  worker(node, inputs, outputs) {
    let temp = inputs["temp"].length ? inputs["temp"][0] : node.data.temp;
    let shape = inputs["shape"].length ? inputs["shape"][0] : node.data.shape;

    let result = simulateHVAC(temp, shape); // Function to simulate HVAC behavior considering shape
    outputs["result"] = result;
  }
}

function simulateHVAC(temp, shape) {
  // Basic simulation logic for HVAC behavior considering room shape
  let volume = shape.length * shape.width * shape.height;
  let energyConsumption = (temp - 22) * volume * 0.05; // Simplified energy calculation
  return energyConsumption;
}
