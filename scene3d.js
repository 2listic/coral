const canvas3D = document.getElementById("canvas3D");
export {canvas3D};

let scene, camera, renderer, controls;
let gui, raycaster, mouse, draggableObject = null, offset;
let isDragging = false;
export {scene};

export function init3D() {
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



