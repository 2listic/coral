// Initialize Paper.js for 2D drawing
const canvas2D = document.getElementById("canvas2D");
paper.setup(canvas2D);

const canvas3D = document.getElementById("canvas3D");
    let scene, camera, renderer, controls;
    let walls = [];

let gui, raycaster, mouse, draggableObject = null, offset;
let isDragging = false;

function init3D() {
  scene = new THREE.Scene();
  camera = new THREE.PerspectiveCamera(
    75,
    window.innerWidth / window.innerHeight,
    0.1,
    1000
  );

  renderer = new THREE.WebGLRenderer({ canvas: canvas3D });
  renderer.setSize(window.innerWidth, window.innerHeight);

  // Orbit controls
  controls = new THREE.OrbitControls(camera, renderer.domElement);
  camera.position.set(10, 10, 10);
  controls.update();

  // Add lights
  const ambientLight = new THREE.AmbientLight(0x404040);
  scene.add(ambientLight);

  const directionalLight = new THREE.DirectionalLight(0xffffff, 1);
  directionalLight.position.set(5, 10, 7.5);
  scene.add(directionalLight);

  // Add a floor
  const floorGeometry = new THREE.PlaneGeometry(20, 20);
  const floorMaterial = new THREE.MeshStandardMaterial({
    color: 0xdddddd,
    side: THREE.DoubleSide,
  });
  const floor = new THREE.Mesh(floorGeometry, floorMaterial);
  floor.rotation.x = -Math.PI / 2;
  floor.position.y = 0;
  scene.add(floor);

  // Raycaster and mouse initialization
  raycaster = new THREE.Raycaster();
  mouse = new THREE.Vector2();

  // Ensure that dat.GUI is only initialized once
  if (!gui) {
    gui = new dat.GUI({ autoPlace: false });

    // Attach dat.GUI to the guiContainer outside the canvas
    const guiContainer = document.createElement('div');
    guiContainer.classList.add('dat-gui');
    canvas3D.parentElement.appendChild(guiContainer);
    guiContainer.appendChild(gui.domElement);

    const options = {
      objectType: 'cube', // Default selection
      addObject: function () { 
        addObjectToScene(options.objectType);
      }
    };

    gui.add(options, 'objectType', ['cube', 'sphere', 'cone']);
    gui.add(options, 'addObject');
  }

  animate();

  // Event listeners for drag functionality
  window.addEventListener('mousedown', onMouseDown);
  window.addEventListener('mousemove', onMouseMove);
  window.addEventListener('mouseup', onMouseUp);
}

function onMouseDown(event) {
  event.preventDefault();
  
  // Convert mouse coordinates to normalized device coordinates
  mouse.x = (event.clientX / window.innerWidth) * 2 - 1;
  mouse.y = -(event.clientY / window.innerHeight) * 2 + 1;
  
  // Update the raycaster with camera and mouse
  raycaster.setFromCamera(mouse, camera);
  
  // Check for intersection with objects in the scene
  const intersects = raycaster.intersectObjects(scene.children, true);
  
  if (intersects.length > 0) {
    // Select the first intersected object
    draggableObject = intersects[0].object;

    // Calculate the offset between object and mouse position
    offset = intersects[0].point.clone().sub(draggableObject.position);
    isDragging = true;
  }
}

function onMouseMove(event) {
  if (isDragging && draggableObject) {
    // Convert mouse coordinates to normalized device coordinates
    mouse.x = (event.clientX / window.innerWidth) * 2 - 1;
    mouse.y = -(event.clientY / window.innerHeight) * 2 + 1;
    
    // Update the raycaster
    raycaster.setFromCamera(mouse, camera);
    
    // Get the plane the object is moving on (assumed to be the floor)
    const plane = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0); // Horizontal plane at y = 0
    
    const intersection = new THREE.Vector3();
    raycaster.ray.intersectPlane(plane, intersection);
    
    // Set the new position of the object based on the mouse movement
    draggableObject.position.copy(intersection.sub(offset));
  }
}

function onMouseUp() {
  isDragging = false;
  draggableObject = null;
}

function animate() {
  requestAnimationFrame(animate);
  controls.update();
  renderer.render(scene, camera);
}

function addObjectToScene(type) {
  let geometry;
  switch (type) {
    case 'cube':
      geometry = new THREE.BoxGeometry(1, 1, 1);
      break;
    case 'sphere':
      geometry = new THREE.SphereGeometry(1, 32, 32);
      break;
    case 'cone':
      geometry = new THREE.ConeGeometry(1, 2, 32);
      break;
  }
  const material = new THREE.MeshStandardMaterial({ color: 0x00ff00 });
  const mesh = new THREE.Mesh(geometry, material);
  scene.add(mesh);
}

init3D();

// Resize canvas on window resize
window.addEventListener('resize', function () {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});


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

// Create a grid background in Paper.js
function createGrid(spacing = 50, color = "#e0e0e0") {
  const bounds = paper.view.bounds;
  const gridGroup = new paper.Group();

  for (let x = bounds.left; x <= bounds.right; x += spacing) {
    const start = new paper.Point(x, bounds.top);
    const end = new paper.Point(x, bounds.bottom);
    const line = new paper.Path.Line(start, end);
    line.strokeColor = color;
    line.strokeWidth = 1;
    gridGroup.addChild(line);
  }

  for (let y = bounds.top; y <= bounds.bottom; y += spacing) {
    const start = new paper.Point(bounds.left, y);
    const end = new paper.Point(bounds.right, y);
    const line = new paper.Path.Line(start, end);
    line.strokeColor = color;
    line.strokeWidth = 1;
    gridGroup.addChild(line);
  }

  gridGroup.sendToBack();
}

createGrid();

// Store the drawn paths (lines)
let currentPath;
let currentText;
let vertices = []; // Array to store all vertices
let lines = []; // Array to store all lines
let selectedVertex = null; // Track the selected vertex for dragging
let selectedLine = null; // Track the selected line for length adjustment
let isDraggingVertex = false; // Flag for vertex dragging
let isDraggingLine = false; // Flag for line dragging

// Create a draggable vertex
function createVertex(point) {
  const vertex = new paper.Path.Circle({
    center: point,
    radius: 5,
    fillColor: "blue",
    data: { connectedPaths: [] },
  });
  vertex.onMouseDown = function (event) {
    if (selectedLine) {
      selectedLine = null; // Deselect line if any
    }
    selectedVertex = vertex;
    isDraggingVertex = true;
  };

  vertex.onMouseDrag = function (event) {
    if (isDraggingVertex) {
      vertex.position = event.point;
      snapToNearbyVertices(vertex);
      updateConnectedLines(vertex);
    }
  };

  vertex.onMouseUp = function (event) {
    isDraggingVertex = false;
  };

  vertices.push(vertex);
  return vertex;
}

function snapToNearbyVertices(vertex) {
  vertices.forEach((v) => {
    if (v !== vertex) {
      const distance = vertex.position.getDistance(v.position);
      if (distance < 10) {
        // Snap distance threshold
        // Move dragged vertex to the position of the stationary vertex
        vertex.position = v.position; // Move stationary vertex to the position of the dragged vertex

        // Update lines connected to the stationary vertex
        updateConnectedLines(v);

        // Remove the dragged vertex
        vertices.splice(vertices.indexOf(vertex), 1);
        vertex.remove(); // Remove the dragged vertex from the canvas
        vertex = v; // Set the stationary vertex as the current one
      }
    }
  });
}

function updateLengthText(path) {
  const lengthInPixels = path.length;
  const metersPerPixel = 10 / paper.view.bounds.width;
  const lengthInMeters = (lengthInPixels * metersPerPixel).toFixed(2);
  currentText.content = lengthInMeters + " m";
  currentText.point = path.getPointAt(path.length / 2);
}

function createLine(startVertex, endVertex) {
  const path = new paper.Path();
  path.strokeColor = "black";
  path.strokeWidth = 2;
  path.add(startVertex.position);
  path.add(endVertex.position);
  lines.push(path);

  startVertex.data.connectedPaths.push({ path, index: 0 });
  endVertex.data.connectedPaths.push({ path, index: 1 });

  // Add length text
  const lengthText = new paper.PointText({
    point: path.getPointAt(path.length / 2),
    content: "",
    fillColor: "black",
    fontSize: 12,
  });
  path.data.lengthText = lengthText;
  updateLengthText(path);

  return path;
}

function updateConnectedLines(vertex) {
  vertex.data.connectedPaths.forEach((pathInfo) => {
    const path = pathInfo.path;
    const index = pathInfo.index;
    const otherIndex = index === 0 ? 1 : 0;

    // Update the position of the path segments
    path.segments[index].point = vertex.position;

    // Update the position of the other end of the line
    const otherPoint = path.segments[otherIndex].point;
    const length = vertex.position.getDistance(otherPoint);
    const angle = Math.atan2(
      otherPoint.y - vertex.position.y,
      otherPoint.x - vertex.position.x
    );
    path.segments[otherIndex].point = new paper.Point(
      vertex.position.x + Math.cos(angle) * length,
      vertex.position.y + Math.sin(angle) * length
    );

    // Update length text
    updateLengthText(path);
  });
}

paper.view.onMouseDown = function (event) {
  if (selectedVertex) {
    // Start a new line from the selected vertex
    if (currentPath) {
      currentPath.removeSegment(1);
    }
    currentPath = new paper.Path();
    currentPath.strokeColor = "black";
    currentPath.strokeWidth = 2;
    currentPath.add(selectedVertex.position);

    // Initialize length text
    currentText = new paper.PointText({
      point: selectedVertex.position,
      content: "",
      fillColor: "black",
      fontSize: 12,
    });

    // Deselect the vertex
    selectedVertex = null;
  } else if (selectedLine) {
    // Select a line if clicked on it
    // Code to set selectedLine based on click detection on line
  } else {
    // Start a new line normally
    currentPath = new paper.Path();
    currentPath.strokeColor = "black";
    currentPath.strokeWidth = 2;
    currentPath.add(event.point);

    // Initialize length text
    currentText = new paper.PointText({
      point: event.point,
      content: "",
      fillColor: "black",
      fontSize: 12,
    });
  }
};

paper.view.onMouseDown = function (event) {
  if (selectedVertex) {
    // Start a new line from the selected vertex
    if (currentPath) {
      currentPath.removeSegment(1);
    }
    currentPath = new paper.Path();
    currentPath.strokeColor = "black";
    currentPath.strokeWidth = 2;
    currentPath.add(selectedVertex.position);

    // Initialize length text
    currentText = new paper.PointText({
      point: selectedVertex.position,
      content: "",
      fillColor: "black",
      fontSize: 12,
    });

    // Deselect the vertex
    selectedVertex = null;
  } else if (selectedLine) {
    // Select a line if clicked on it
    // Code to set selectedLine based on click detection on line
  } else {
    // Start a new line normally
    currentPath = new paper.Path();
    currentPath.strokeColor = "black";
    currentPath.strokeWidth = 2;
    currentPath.add(event.point);

    // Initialize length text
    currentText = new paper.PointText({
      point: event.point,
      content: "",
      fillColor: "black",
      fontSize: 12,
    });
  }
};

paper.view.onMouseDrag = function (event) {
  if (currentPath) {
    // Ensure the line remains straight
    if (currentPath.segments.length > 1) {
      currentPath.removeSegment(1);
    }
    currentPath.add(event.point);

    // Update length text during drag
    updateLengthText(currentPath);
  } else if (isDraggingVertex && selectedVertex) {
    // Update position of the dragging vertex
    selectedVertex.position = event.point;
    snapToNearbyVertices(selectedVertex); // Snap and merge vertices
    updateConnectedLines(selectedVertex); // Update connected lines
  } else if (selectedLine) {
    // Adjust the length of the selected line
    const start = selectedLine.firstSegment.point;
    const end = selectedLine.lastSegment.point;
    const angle = Math.atan2(event.point.y - start.y, event.point.x - start.x);
    const length = start.getDistance(event.point);
    selectedLine.lastSegment.point = new paper.Point(
      start.x + Math.cos(angle) * length,
      start.y + Math.sin(angle) * length
    );

    // Update length text during drag
    updateLengthText(selectedLine);
  }
};

paper.view.onMouseUp = function (event) {
  if (currentPath) {
    currentPath.closed = false; // Ensure the path remains open
    currentPath.simplify(); // Simplify to ensure it remains straight

    // Create draggable vertices at each end of the line
    const startVertex = createVertex(currentPath.firstSegment.point);
    const endVertex = createVertex(currentPath.lastSegment.point);

    // Store the length text in the path's data for easy access
    currentPath.data.lengthText = currentText;

    // Reset the current path and text variables
    currentPath = null;
    currentText = null;
  }
};

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
