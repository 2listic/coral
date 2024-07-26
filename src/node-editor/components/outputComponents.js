import Rete from "rete";
import { CanvasControl } from "../controls/canvasControl.js";
import { GridSocket } from "../sockets/sockets";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls";

export class OutputComponent extends Rete.Component {
  constructor() {
    super("Output");
    this.scene = null;
    this.camera = null;
    this.renderer = null;
    this.controls = null;
  }

  builder(node) {
    let inp1 = new Rete.Input("result", "Result", GridSocket);
    node.addInput(inp1);

    let canvasControl = new CanvasControl(this.editor, "canvas", true);
    node.addControl(canvasControl);
  }

  async worker(node, inputs) {
    let result = inputs["result"].length
      ? inputs["result"][0]
      : node.data.result;
    const control = this.editor.nodes
      .find((n) => n.id === node.id)
      .controls.get("canvas");

    if (control) {
      this.renderHeatmap3D(result, control.getCanvas());
    } else {
      console.error("Canvas control not found");
    }
  }

  renderHeatmap3D(data, canvas) {
    if (!canvas) {
      console.error("Canvas element not found");
      return;
    }

    if (!this.scene) {
      this.initScene(canvas);
    }

    // Clear existing objects from the scene
    while (this.scene.children.length > 0) {
      this.scene.remove(this.scene.children[0]);
    }

    const rows = data.length;
    const cols = data[0].length;
    const levels = data[0][0].length;

    const maxTemp = Math.max(...data.flat(3));
    const minTemp = Math.min(...data.flat(3));

    const cubeSize = 1;
    const spacing = 0.1;

    for (let x = 0; x < rows; x++) {
      for (let y = 0; y < cols; y++) {
        for (let z = 0; z < levels; z++) {
          const temp = data[x][y][z];
          const normalizedTemp = (temp - minTemp) / (maxTemp - minTemp);
          const geometry = new THREE.BoxGeometry(cubeSize, cubeSize, cubeSize);
          const material = new THREE.MeshBasicMaterial({
            color: new THREE.Color(1 - normalizedTemp, 0, normalizedTemp),
            opacity: 0.7,
            transparent: true,
          });

          const cube = new THREE.Mesh(geometry, material);
          cube.position.set(
            x * (cubeSize + spacing),
            y * (cubeSize + spacing),
            z * (cubeSize + spacing)
          );

          this.scene.add(cube);
        }
      }
    }

    this.renderer.render(this.scene, this.camera);
  }

  initScene(canvas) {
    this.scene = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(
      75,
      canvas.width / canvas.height,
      0.1,
      1000
    );
    this.renderer = new THREE.WebGLRenderer({
      canvas: canvas,
      antialias: true,
    });
    this.renderer.setSize(canvas.width, canvas.height);

    this.controls = new OrbitControls(this.camera, this.renderer.domElement);
    this.controls.enableDamping = true;
    this.controls.dampingFactor = 0.25;
    this.controls.enableZoom = true;

    // Set up camera position
    this.camera.position.set(20, 20, 20);
    this.camera.lookAt(0, 0, 0);

    // Add ambient light
    const ambientLight = new THREE.AmbientLight(0xffffff, 0.5);
    this.scene.add(ambientLight);

    // Add directional light
    const directionalLight = new THREE.DirectionalLight(0xffffff, 0.5);
    directionalLight.position.set(10, 10, 10);
    this.scene.add(directionalLight);

    // Handle full-screen functionality
    canvas.addEventListener("dblclick", () => {
      if (!document.fullscreenElement) {
        canvas.requestFullscreen();
      } else {
        document.exitFullscreen();
      }
    });

    // Handle Escape key to exit full-screen mode
    document.addEventListener("keydown", (event) => {
      if (event.key === "Escape" && document.fullscreenElement) {
        document.exitFullscreen();
      }
    });

    // Animation loop
    const animate = () => {
      requestAnimationFrame(animate);
      this.controls.update();
      this.renderer.render(this.scene, this.camera);
    };
    animate();
  }
}
