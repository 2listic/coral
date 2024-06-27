import Rete from "rete";
import VueRenderPlugin from "rete-vue-render-plugin";
import ConnectionPlugin from "rete-connection-plugin";
import AreaPlugin from "rete-area-plugin";
import ContextMenuPlugin from "rete-context-menu-plugin";
import { NumComponent } from "./components/numComponent";
import { AddComponent } from "./components/addComponent";
import { InputComponent } from "./components/inputComponent";
import { SimulationComponent } from "./components/simulationComponent";
import { OutputComponent } from "./components/outputComponents";
import { RoomComponent } from "./components/roomComponent";
import { ControllerComponent } from "./components/controlComponent";
import { DeviceComponent } from "./components/deviceComponent";

export default async function (container) {
  var components = [
    new NumComponent(),
    new AddComponent(),
    new InputComponent(),
    new SimulationComponent(),
    new OutputComponent(),
    new RoomComponent(),
    new ControllerComponent(),
    new DeviceComponent(),
  ];

  var editor = new Rete.NodeEditor("hvac@0.1.0", container);
  editor.use(ConnectionPlugin);
  editor.use(VueRenderPlugin);
  editor.use(ContextMenuPlugin);
  editor.use(AreaPlugin);

  var engine = new Rete.Engine("hvac@0.1.0");

  components.map((c) => {
    editor.register(c);
    engine.register(c);
  });

  // var n1 = await components[0].createNode({ numm: 22 });
  // var n2 = await components[0].createNode({ numm: 33 });
  // var add = await components[1].createNode();

  // n1.position = [80, 200];
  // n2.position = [80, 400];
  // add.position = [500, 240];

  // editor.addNode(n1);
  // editor.addNode(n2);
  // editor.addNode(add);

  // editor.connect(n1.outputs.get("num"), add.inputs.get("num1"));
  // editor.connect(n2.outputs.get("num"), add.inputs.get("num2"));

  let inputNodeMin = await components[2].createNode({
    minTemp: 18,
    maxTemp: 25,
  });
  let inputNodeMax = await components[2].createNode({
    minTemp: 30,
    maxTemp: 45,
  });
  let simulationNode = await components[3].createNode();
  let outputNode = await components[4].createNode();
  let roomNode = await components[5].createNode({
    length: 10,
    width: 10,
    height: 10,
  });
  let controllerNode = await components[6].createNode({ threshold: 23 });
  let deviceNode = await components[7].createNode();

  inputNodeMax.position = [80, 200];
  inputNodeMin.position = [20, 30];
  simulationNode.position = [400, 200];
  outputNode.position = [720, 200];
  roomNode.position = [100, 400];
  controllerNode.position = [400, -100];
  deviceNode.position = [600, -100];

  editor.addNode(inputNodeMax);
  editor.addNode(inputNodeMin);
  editor.addNode(simulationNode);
  editor.addNode(outputNode);
  editor.addNode(roomNode);
  editor.addNode(controllerNode);
  editor.addNode(deviceNode);

  editor.connect(
    inputNodeMin.outputs.get("temp"),
    simulationNode.inputs.get("tempNearCooler")
  );
  editor.connect(
    inputNodeMax.outputs.get("temp"),
    simulationNode.inputs.get("tempFurthest")
  );
  editor.connect(
    simulationNode.outputs.get("result"),
    outputNode.inputs.get("result")
  );
  editor.connect(
    roomNode.outputs.get("shape"),
    simulationNode.inputs.get("shape")
  );
  editor.connect(
    simulationNode.outputs.get("result"),
    controllerNode.inputs.get("grid")
  );
  editor.connect(
    controllerNode.outputs.get("result"),
    deviceNode.inputs.get("bool")
  );

  editor.on(
    "process nodecreated noderemoved connectioncreated connectionremoved",
    async () => {
      await engine.abort();
      await engine.process(editor.toJSON());
    }
  );

  editor.view.resize();
  AreaPlugin.zoomAt(editor);
  editor.trigger("process");
}
