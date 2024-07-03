import Rete from "rete";
import { DeviceControl } from "../controls/deviceControl";
import { BoolSocket } from "../sockets/sockets";

export class DeviceComponent extends Rete.Component {
  constructor() {
    super("Device");
  }

  builder(node) {
    let inp = new Rete.Input("bool", "Status", BoolSocket);

    node.addInput(inp);
    node.addControl(new DeviceControl(this.editor, "image"));
  }

  worker(node, inputs) {
    let status = inputs["bool"].length ? inputs["bool"][0] : false;
    node.data.status = status;

    // Update the control status
    let control = this.editor.nodes
      .find((n) => n.id === node.id)
      .controls.get("image");
    control.setStatus(status);
  }
}
