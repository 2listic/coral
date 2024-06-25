import Rete from "rete";
import { NumSocket } from "../sockets/sockets";

export class OutputComponent extends Rete.Component {
  constructor() {
    super("Output");
  }

  builder(node) {
    let inp1 = new Rete.Input("result", "Result", NumSocket);

    node.addInput(inp1);
  }

  worker(node, inputs) {
    let result = inputs["result"].length
      ? inputs["result"][0]
      : node.data.result;
    displayResult(result);
  }
}

function displayResult(result) {
  console.log("HVAC Energy Consumption:", result);
}
