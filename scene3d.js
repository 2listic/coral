const canvas3D = document.getElementById("canvas3D");

let scene, camera, renderer, controls, transformControls;
let gui, raycaster;
let draggableObject = null;
let models = [];

export {scene,camera,canvas3D, renderer};

var min_model_position = new THREE.Vector3( - 10, - 2.5, -10);
var max_model_position = new THREE.Vector3( 10, 2.5, 10);

function render() {
  renderer.render(scene, camera);
}

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
           
  // Transform controls
  transformControls = new THREE.TransformControls(camera, renderer.domElement);
  transformControls.addEventListener('change', render);
  transformControls.addEventListener('dragging-changed', function (event) {
	controls.enabled = !event.value;
    });
  scene.add(transformControls);
 
  // Add lights
  const ambientLight = new THREE.AmbientLight(0x404040);
  scene.add(ambientLight);

  const directionalLight = new THREE.DirectionalLight(0xffffff, 1);
  directionalLight.position.set(5, 10, 7.5);
  scene.add(directionalLight);

  // Add a floor
  const floorGeometry = new THREE.PlaneGeometry(30, 30);
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

    gui.add(options, 'objectType', ['cube', 'sphere', 'cone']).name("Models");
    gui.add(options, 'addObject').name("Add");
  }

  animate();

  // Event listeners for drag functionality
  window.addEventListener('click', onMouseClick);
}

function onMouseClick(event) {
//  event.preventDefault();
  const rect = renderer.domElement.getBoundingClientRect();
  const mouse = new THREE.Vector2(
	((event.clientX - rect.left) / rect.width) * 2 - 1,
	-((event.clientY - rect.top) / rect.height) * 2 + 1
    );

  // Update the raycaster with camera and mouse
  raycaster.setFromCamera(mouse, camera);
  
  // Check for intersection with objects in the scene
  const intersects = raycaster.intersectObjects(models, true);
  
  if (intersects.length > 0) {
	if (draggableObject) {
		transformControls.detach();
	}
    // Select the first intersected object
        draggableObject = intersects[0].object;
        transformControls.attach(draggableObject);
        transformControls.object.position.clamp(min_model_position, max_model_position
	)
    }
    else {
        if (draggableObject) {
		transformControls.detach();
		draggableObject = null;	
	}
  }
}


function animate() {
  requestAnimationFrame(animate);
  render();
}


function addObjectToScene(type) {
  console.log("PIPPO")
  let geometry;
  type = "cube";
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
  models.push(mesh);
}

init3D();

let add_model = document.getElementById("add_model");
add_model.addEventListener("click", addObjectToScene, false);
// Resize canvas on window resize
window.addEventListener('resize', function () {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});



