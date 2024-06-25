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
export default async function (container) {
  var components = [
    new NumComponent(),
    new AddComponent(),
    new InputComponent(),
    new SimulationComponent(),
    new OutputComponent(),
    new RoomComponent(),
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

  let inputNode = await components[2].createNode({ temp: 25 });
  let simulationNode = await components[3].createNode();
  let outputNode = await components[4].createNode();
  let roomNode = await components[5].createNode();

  inputNode.position = [80, 200];
  simulationNode.position = [400, 200];
  outputNode.position = [720, 200];
  roomNode.position = [60, 200];

  editor.addNode(inputNode);
  editor.addNode(simulationNode);
  editor.addNode(outputNode);

  editor.connect(
    inputNode.outputs.get("temp"),
    simulationNode.inputs.get("temp")
  );
  editor.connect(
    simulationNode.outputs.get("result"),
    outputNode.inputs.get("result")
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
