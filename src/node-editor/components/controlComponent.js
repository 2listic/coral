import Rete from "rete";
import { BoolSocket, NumSocket, GridSocket } from "../sockets/sockets";

export class ControllerComponent extends Rete.Component {
  constructor() {
    super("Controller");
  }

  builder(node) {
    let inp1 = new Rete.Input("grid", "Simulation Results", GridSocket);
    let inp2 = new Rete.Input("threshold", "Threshold", NumSocket);
    let out1 = new Rete.Output("result", "Result", BoolSocket);

    node.addInput(inp1);
    node.addInput(inp2);
    node.addOutput(out1);
  }

  worker(node, inputs, outputs) {
    let grid = inputs["grid"].length ? inputs["grid"][0] : node.data.grid;
    let threshold = inputs["threshold"].length
      ? inputs["threshold"][0]
      : node.data.threshold;

    if (!grid || !threshold) {
      outputs["result"] = false;
      return;
    }

    let middleLayer = Math.floor(grid[0][0].length / 2);
    let rows = grid.length;
    let cols = grid[0].length;

    let isAboveThreshold = false;
    for (let x = 0; x < rows; x++) {
      for (let y = 0; y < cols; y++) {
        if (grid[x][y][middleLayer] > threshold) {
          isAboveThreshold = true;
          break;
        }
      }
      if (isAboveThreshold) break;
    }
    outputs["result"] = isAboveThreshold;
  }
}
