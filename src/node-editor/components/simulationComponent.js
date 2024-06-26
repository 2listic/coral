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
    // let shape = inputs["shape"].length ? inputs["shape"][0] : node.data.shape;
    let result = simulateHVAC(temp); //, shape); // Function to simulate HVAC behavior considering shape
    outputs["result"] = result;
  }
}

function simulateHVAC(temp) {
  //, shape) {
  const rows = 10; // fixed Number of grid rows
  const cols = 10; // fixed Number of grid columns

  let temperatureGrid = [];

  for (let i = 0; i < rows; i++) {
    let row = [];
    for (let j = 0; j < cols; j++) {
      let distanceToCenter = Math.sqrt(
        Math.pow(i - rows / 2, 2) + Math.pow(j - cols / 2, 2)
      );
      let temperature = temp - distanceToCenter;
      row.push(temperature);
    }
    temperatureGrid.push(row);
  }

  return temperatureGrid;
}
