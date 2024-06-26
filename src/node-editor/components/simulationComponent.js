import Rete from "rete";
import { NumSocket } from "../sockets/sockets";
import { ShapeSocket } from "../sockets/sockets";
import { GridSocket } from "../sockets/sockets";
export class SimulationComponent extends Rete.Component {
  constructor() {
    super("Simulation");
  }
  builder(node) {
    let inp1 = new Rete.Input("temp", "Temperature", NumSocket);
    let inp2 = new Rete.Input("shape", "Shape", ShapeSocket);
    let out1 = new Rete.Output("result", "Result", GridSocket);
    node.addInput(inp1);
    node.addInput(inp2);
    node.addOutput(out1);
  }
  worker(node, inputs, outputs) {
    let temp = inputs["temp"].length ? inputs["temp"][0] : node.data.temp;
    let shape = inputs["shape"].length ? inputs["shape"][0] : node.data.shape;
    let result = simulateHVAC(temp, shape);
    outputs["result"] = result;
  }
}
function simulateHVAC(temp, shape) {
  const rows = shape.length;
  const cols = shape.width;
  const levels = shape.height;

  let temperatureGrid = [];

  for (let x = 0; x < rows; x++) {
    let plane = [];
    for (let y = 0; y < cols; y++) {
      let column = [];
      for (let z = 0; z < levels; z++) {
        let distanceToCenter = Math.sqrt(
          Math.pow(x - rows / 2, 2) +
            Math.pow(y - cols / 2, 2) +
            Math.pow(z - levels / 2, 2)
        );
        // Add time-based variation
        let variation = Math.sin(Date.now() + x * 0.5 + y * 0.3 + z * 0.2) * 2;
        let temperature = temp - distanceToCenter * 0.5 + variation;
        column.push(temperature);
      }
      plane.push(column);
    }
    temperatureGrid.push(plane);
  }

  return temperatureGrid;
}
