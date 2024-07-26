import Rete from "rete";
import { BoolSocket } from "../sockets/sockets";
import { ConsumptionControl } from "../controls/consumptionControl.js";

export class ConsumptionComponent extends Rete.Component {
  constructor() {
    super("Consumption");
    this.statusHistory = [];
    this.consumptionHistory = Array(24).fill(0); // Initialize with zeros for the chart
  }

  builder(node) {
    let inp1 = new Rete.Input("status", "Device Status", BoolSocket);
    node.addInput(inp1);

    let consumptionControl = new ConsumptionControl(
      this.editor,
      "consumption",
      true
    );
    node.addControl(consumptionControl);
  }

  worker(node, inputs) {
    let status = inputs["status"].length
      ? inputs["status"][0]
      : node.data.status;

    if (status !== undefined) {
      this.statusHistory.push(status);
    }

    if (this.statusHistory.length >= 24) {
      let actualConsumption = this.calculateActualConsumption(
        this.statusHistory
      );
      this.consumptionHistory.push(actualConsumption);
      if (this.consumptionHistory.length > 24) {
        this.consumptionHistory.shift(); // Maintain only the last 24 consumption entries
      }
      this.statusHistory = []; // Reset status history
    }

    let consumptionAlwaysOn = this.calculateAlwaysOnConsumption();

    node.data.consumption = {
      alwaysOn: Array(24).fill(consumptionAlwaysOn),
      actual: this.consumptionHistory,
    };

    this.updateChart(node);
  }

  calculateAlwaysOnConsumption() {
    const powerPerHour = 1;
    const hoursInDay = 24;
    return powerPerHour * hoursInDay;
  }

  calculateActualConsumption(statusArray) {
    const powerPerHour = 1;
    let actualConsumption = 0;

    statusArray.forEach((status) => {
      if (status) {
        actualConsumption += powerPerHour;
      }
    });

    return actualConsumption;
  }

  updateChart(node) {
    const control = this.editor.nodes
      .find((n) => n.id === node.id)
      .controls.get("consumption");

    if (control) {
      control.setValue({
        alwaysOn: Array(24).fill(24),
        actual: this.consumptionHistory,
      });
    }
  }
}
