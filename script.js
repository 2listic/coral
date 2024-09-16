import { canvas2D } from "./floor2d.js"; // Import from floor2d.js
import { canvas3D, init3D, scene } from "./scene3d.js";  

let walls = []
function convertPathsTo3D() {
  if (walls.length > 0) {
    walls.forEach((wall) => scene.remove(wall));
    walls = [];
  }

  paper.project.activeLayer.children.forEach((item) => {
    if (item instanceof paper.Path && item.segments.length >= 2) {
      // Ensure item is a Path with at least 2 segments
      const startX =
        (item.segments[0].point.x / paper.view.bounds.width) * 10 - 5;
      const startY =
        -(item.segments[0].point.y / paper.view.bounds.height) * 10 + 5;
      const endX =
        (item.segments[1].point.x / paper.view.bounds.width) * 10 - 5;
      const endY =
        -(item.segments[1].point.y / paper.view.bounds.height) * 10 + 5;

      const wallLength = Math.sqrt(
        Math.pow(endX - startX, 2) + Math.pow(endY - startY, 2)
      );
      const wallHeight = 2.5; // Height of the walls
      const wallThickness = 0.1; // Thickness of the walls

      const wallGeometry = new THREE.BoxGeometry(
        wallLength,
        wallHeight,
        wallThickness
      );
      const wallMaterial = new THREE.MeshStandardMaterial({
        color: 0x888888,
        opacity: 0.5,
        transparent: true,
      });
      const wall = new THREE.Mesh(wallGeometry, wallMaterial);

      wall.position.x = (startX + endX) / 2;
      wall.position.y = wallHeight / 2;
      wall.position.z = (startY + endY) / 2;

      const angle = Math.atan2(endY - startY, endX - startX);
      wall.rotation.y = -angle;

      scene.add(wall);
      walls.push(wall);
    }
  });
}

// Switch between 2D and 3D modes
const switchButton = document.getElementById("switchMode");
switchButton.addEventListener("click", () => {
  if (canvas3D.style.display === "none") {
    convertPathsTo3D();
    canvas2D.style.display = "none";
    canvas3D.style.display = "block";
    switchButton.textContent = "Switch to 2D";
  } else {
    canvas2D.style.display = "block";
    canvas3D.style.display = "none";
    switchButton.textContent = "Switch to 3D";
  }
});

// Initialize the 3D scene
init3D();

// Handle window resizing
window.addEventListener("resize", () => {
  paper.view.viewSize = new paper.Size(window.innerWidth, window.innerHeight);
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});
