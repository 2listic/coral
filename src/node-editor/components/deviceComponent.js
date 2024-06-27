import Rete from "rete";
import { BoolSocket } from "../sockets/sockets";

export class DeviceComponent extends Rete.Component {
  constructor() {
    super("Device");
    this.data.component = DeviceNode;
  }

  builder(node) {
    var input = new Rete.Input("state", "State", BoolSocket);

    node.addInput(input);
  }

  worker(node, inputs) {
    const state = inputs["state"].length ? inputs["state"][0] : false;
    node.data.state = state;
  }
}

// Vue component for rendering the node
const DeviceNode = {
  props: ["node", "editor"],
  data() {
    return {
      state: false,
    };
  },
  methods: {
    update() {
      if (this.state !== this.node.data.state) {
        this.state = this.node.data.state;
        this.$forceUpdate();
      }
    },
  },
  render(h) {
    this.update();
    const color = this.state ? "green" : "red";
    const text = this.state ? "ON" : "OFF";

    return h(
      "div",
      {
        style: {
          background: color,
          padding: "20px",
          borderRadius: "5px",
          display: "flex",
          flexDirection: "column",
          alignItems: "center",
        },
      },
      [
        h("img", {
          attrs: {
            src: "cooler.jpeg",
            width: "50",
            height: "50",
          },
        }),
        h(
          "span",
          {
            style: {
              color: "white",
              marginTop: "10px",
              fontWeight: "bold",
            },
          },
          text
        ),
      ]
    );
  },
};
