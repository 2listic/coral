import Rete from "rete";
import VueRenderPlugin from "rete-vue-render-plugin";
import ConnectionPlugin from "rete-connection-plugin";
import AreaPlugin from "rete-area-plugin";
import ContextMenuPlugin from "rete-context-menu-plugin";
import { NumComponent } from "./components/numComponent";
import { StringComponent } from "./components/stringComponent";
import { AddComponent } from "./components/addComponent";
import { saveAs } from "file-saver";
import axios from 'axios';


async function getNodeTypes() {
  const response = await axios.get('http://localhost:3001/node-types');
  return response.data;
}

async function validateNetwork(network) {
  const response = await axios.post('http://localhost:3001/validate-network', network);
  return response.data.isValid;
}


export default async function (container) {
  var components = [new NumComponent(), new AddComponent(), new StringComponent()];

  var editor = new Rete.NodeEditor("demo@0.1.0", container);
  editor.use(ConnectionPlugin);
  editor.use(VueRenderPlugin);
  editor.use(ContextMenuPlugin);
  editor.use(AreaPlugin);

  var engine = new Rete.Engine("demo@0.1.0");

  components.map((c) => {
    editor.register(c);
    engine.register(c);
  });

  var n1 = await components[0].createNode({ numm: 22 });
  var n2 = await components[0].createNode({ numm: 33 });
  var str = await components[2].createNode({ str: "test" });
  var add = await components[1].createNode();

  n1.position = [80, 200];
  n2.position = [80, 400];
  add.position = [500, 240];
  str.position = [500, 400];

  editor.addNode(n1);
  editor.addNode(n2);
  editor.addNode(add);
  editor.addNode(str);

  editor.connect(n1.outputs.get("num"), add.inputs.get("num1"));
  editor.connect(n2.outputs.get("num"), add.inputs.get("num2"));

  // Now get the nodes from the backend

  const nodeTypes = await getNodeTypes();
  console.log(nodeTypes);


//   nodeTypes.forEach(type => {
//     const component = new Component(type.name);

//     component.builder = node => {
//       if (type.inputs) {
//         type.inputs.forEach(inputName => {
//           node.addInput(new Input(inputName, inputName, Node.sockets.any));
//         });
//       }

//       if (type.outputs) {
//         type.outputs.forEach(outputName => {
//           node.addOutput(new Output(outputName, outputName, Node.sockets.any));
//         });
//       }
//     };

//     component.worker = (node, inputs, outputs) => {
//       // Define node behavior
//       outputs['output'] = node.data.value;
//     };

//     editor.register(component);
// });



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


  // Add a button to export the editor state
  var export_button = document.createElement("button");
  export_button.innerText = "Export editor to JSON";
  export_button.style.position = "absolute";
  export_button.style.top = "10px";
  export_button.style.right = "10px";
  document.body.appendChild(export_button);

  // Add a button to generate C++ code
  var compile_button = document.createElement("button");
  compile_button.innerText = "Compile editor to C++ code";
  compile_button.style.position = "absolute";
  compile_button.style.top = "40px";
  compile_button.style.right = "10px";
  document.body.appendChild(compile_button);


  // Example of displaying console-like output in the editor view
  var consoleElement = document.createElement('div');
  consoleElement.style.position = 'absolute';
  consoleElement.style.bottom = '10px';
  consoleElement.style.left = '10px';
  consoleElement.style.width = '300px';
  consoleElement.style.height = '200px';
  consoleElement.style.overflow = 'auto';
  consoleElement.style.backgroundColor = '#000';
  consoleElement.style.color = '#fff';
  consoleElement.style.border = '1px solid #ccc';
  consoleElement.style.padding = '10px';
  consoleElement.style.fontFamily = 'monospace';
  consoleElement.innerText = 'Console:\n';

  editor.view.container.appendChild(consoleElement);

  function logToConsole(message) {
    consoleElement.innerText += `${message}\n`;
    consoleElement.scrollTop = consoleElement.scrollHeight; // Auto-scroll to bottom
  }

  export_button.addEventListener("click", () => {
    var json = editor.toJSON();
    var blob = new Blob([JSON.stringify(json, null, 2)], { type: "application/json" });
    saveAs(blob, "editor-state.json");
  });

  compile_button.addEventListener("click", async () => {
    // var json = editor.toJSON();
    logToConsole('Do something with editor.toJSON()');
    const network = editor.toJSON();
    const isValid = await validateNetwork(network);
    logToConsole(isValid ? 'Network is valid!' : 'Network is invalid!');
  });

  editor.logToConsole = logToConsole;
}
