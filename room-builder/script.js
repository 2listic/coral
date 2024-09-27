import { canvas2D } from "./floor2d.js"; // Import from floor2d.js
import { canvas3D, init3D, scene, camera, renderer } from "./scene3d.js";  

const container2D = document.getElementById("container2D");
const container3D = document.getElementById("container3D");

let walls = []
function convertPathsTo3D() {
  if (walls.length > 0) {
    walls.forEach((wall) => scene.remove(wall));
    walls = [];
  }

  paper.project.activeLayer.children.forEach((item) => {
    console.log(item);
    if (item instanceof paper.Path && item.segments.length >= 2 && 
	    item.segments[0].handleIn.isZero() && item.segments[0].handleOut.isZero() &&
                item.segments[1].handleIn.isZero() && item.segments[1].handleOut.isZero()) {
      // Ensure item is a Path with at least 2 segments
      // Check if it's a straight line (no handles, meaning no curve, so no verteces)
      const startX =
        (item.segments[0].point.x / paper.view.bounds.width) * 10 - 5;
      const startZ =
        -(item.segments[0].point.y / paper.view.bounds.height) * 10 + 5;
      const endX =
        (item.segments[1].point.x / paper.view.bounds.width) * 10 - 5;
      const endZ =
        -(item.segments[1].point.y / paper.view.bounds.height) * 10 + 5;

      const wallLength = Math.sqrt(
        Math.pow(endX - startX, 2) + Math.pow(endZ - startZ, 2)
      );
      const wallHeight = 2.5; // Height of the walls
      const wallThickness = 0.1; // Thickness of the walls

      const wallGeometry = new THREE.BoxGeometry(
        wallLength,
        wallHeight,
        wallThickness
      );
      wallGeometry.translate(0, wallHeight / 2, 0);
      const wallMaterial = new THREE.MeshStandardMaterial({
        color: 0x888888,
        opacity: 0.5,
        transparent: true,
      });
      const wall = new THREE.Mesh(wallGeometry, wallMaterial);

      wall.position.x = (startX + endX);
      wall.position.y =  0;
      wall.position.z = (startZ + endZ);

      const angle = Math.atan2(endZ - startZ, endX - startX);
      wall.rotation.y = -angle;
      wall.scale.set(2,2,2);
      // After scaling, set the wall's position Y to half of the scaled height
      // wall.position.y = wallHeight * wall.scale.y / 2;
      wall.name = "wall";
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
    container3D.style.display = "block";
    switchButton.textContent = "Switch to 2D";
  } else {
    canvas2D.style.display = "block";
    container2D.style.display = "block";
    canvas3D.style.display = "none";
    container3D.style.display = "none";
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
