import Rete from "rete";
import { CanvasControl } from "../controls/canvasControl.js";
import { GridSocket } from "../sockets/sockets";

export class OutputComponent extends Rete.Component {
  constructor() {
    super("Output");
  }

  builder(node) {
    let inp1 = new Rete.Input("result", "Result", GridSocket);
    console.log(inp1);
    node.addInput(inp1);

    let canvasControl = new CanvasControl(this.editor, "canvas", true);
    node.addControl(canvasControl);
  }

  async worker(node, inputs) {
    let result = inputs["result"].length
      ? inputs["result"][0]
      : node.data.result;
    const control = await this.editor.nodes
      .find((n) => n.id === node.id)
      .controls.get("canvas");

    if (control) {
      this.renderHeatmap(result, control.getCanvas());
    } else {
      console.error("Canvas control not found");
    }
  }

  renderHeatmap(data, canvas) {
    if (!canvas) {
      console.error("Canvas element not found");
      return;
    }

    const ctx = canvas.getContext("2d");
    const rows = data.length;
    const cols = data[0].length;

    const maxTemp = Math.max(...data.flat());
    const minTemp = Math.min(...data.flat());

    for (let i = 0; i < rows; i++) {
      for (let j = 0; j < cols; j++) {
        const temp = data[i][j];
        const normalizedTemp = (temp - minTemp) / (maxTemp - minTemp);
        const color = `rgba(255, 0, 0, ${normalizedTemp})`;

        ctx.fillStyle = color;
        ctx.fillRect(
          j * (canvas.width / cols),
          i * (canvas.height / rows),
          canvas.width / cols,
          canvas.height / rows
        );
      }
    }
  }
}
