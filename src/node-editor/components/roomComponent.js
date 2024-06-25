import Rete from "rete";
import { ShapeSocket } from "../sockets/sockets";
import { NumControl } from "../controls/numControl.js";

export class RoomComponent extends Rete.Component {
  constructor() {
    super("Room Shape");
  }

  builder(node) {
    let out1 = new Rete.Output("shape", "Shape", ShapeSocket);

    node.addControl(new NumControl(this.editor, "length", "Length"));
    node.addControl(new NumControl(this.editor, "width", "Width"));
    node.addControl(new NumControl(this.editor, "height", "Height"));

    node.addOutput(out1);
  }

  worker(node, inputs, outputs) {
    const length = node.data.length || 1;
    const width = node.data.width || 1;
    const height = node.data.height || 1;

    outputs["shape"] = { length, width, height };
  }
}
