import Rete from "rete";
import { NumSocket } from "../sockets/sockets";
import { ShapeSocket } from "../sockets/sockets";
import { GridSocket } from "../sockets/sockets";

export class SimulationComponent extends Rete.Component {
  constructor() {
    super("Simulation");
  }

  builder(node) {
    let inp1 = new Rete.Input("tempNearCooler", "Temp Near Cooler", NumSocket);
    let inp2 = new Rete.Input("tempFurthest", "Temp Furthest", NumSocket);
    let inp3 = new Rete.Input("shape", "Shape", ShapeSocket);
    let out1 = new Rete.Output("result", "Result", GridSocket);

    node.addInput(inp1);
    node.addInput(inp2);
    node.addInput(inp3);
    node.addOutput(out1);
  }

  worker(node, inputs, outputs) {
    let tempNearCooler = inputs["tempNearCooler"].length
      ? inputs["tempNearCooler"][0]
      : node.data.tempNearCooler;
    let tempFurthest = inputs["tempFurthest"].length
      ? inputs["tempFurthest"][0]
      : node.data.tempFurthest;
    let shape = inputs["shape"].length ? inputs["shape"][0] : node.data.shape;

    let result = simulateHVAC(tempNearCooler, tempFurthest, shape);
    outputs["result"] = result;
  }
}

function simulateHVAC(tempNearCooler, tempFurthest, shape) {
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

        // Linear interpolation between tempNearCooler and tempFurthest
        let interpolationFactor = z / (levels - 1);
        let temperature =
          tempNearCooler * (1 - interpolationFactor) +
          tempFurthest * interpolationFactor;

        // Cooling effect stronger near the ceiling (z = levels - 1)
        let distanceToCeiling = levels - 1 - z;
        let coolingEffect = Math.max(0, temperature - distanceToCeiling * 2);

        column.push(coolingEffect - distanceToCenter * 0.1);
      }
      plane.push(column);
    }
    temperatureGrid.push(plane);
  }
  return temperatureGrid;
}
