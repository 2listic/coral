const canvas3D = document.getElementById("canvas3D");

let scene, camera, renderer, controls, transformControls;
let raycaster;
let draggableObject = null;
let models = [];

export {scene,camera,canvas3D, renderer};

var min_model_position = new THREE.Vector3( - 100,  0, -100);
var max_model_position = new THREE.Vector3( 100, 5.5, 100);

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
  transformControls.showX = true;
  transformControls.showY = false;
  transformControls.showZ = true;  
  scene.add(transformControls);
	 
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


function addObjectToScene(model) {
  console.log(model)
// Use a material that responds to light
  let material_obj = new THREE.MeshStandardMaterial({
    color: 0x6e6e6e,    // Gray color
    metalness: 0.5,     // How metallic the material appears (0 = non-metal, 1 = metal)
    roughness: 0.7,     // How rough the surface is (0 = smooth, 1 = rough)
  });

  // let material_obj = new THREE.MeshBasicMaterial( { color: 0x6E6E6E} ); 
  const objLoader = new THREE.OBJLoader();
  objLoader.load(`public/${model}.obj`, function(object) {
     	object.traverse( function ( child ) {
           if ( child.isMesh ) child.material = material_obj;
	});
	switch (model){
	  case "Chair":
	        object.scale.setScalar(0.04);	
		break;
	  case "Cooler":
		object.scale.setScalar(0.02);
		break;
	  case "Table":
		object.scale.setScalar(0.8);
        }
	scene.add(object);
        models.push(object);
  });
}

function deleteCube() {
  const intersects = raycaster.intersectObjects(models, true);

  // Check if there are any intersected objects
  if (intersects.length > 0) {
    // Get the parent object that was added to the models array
    let draggableObject = intersects[0].object;

    // Traverse up the hierarchy to find the root parent that was added to models
    while (draggableObject.parent && !models.includes(draggableObject)) {
      draggableObject = draggableObject.parent;
    }

    // Remove the object from the scene if it's part of models
    if (models.includes(draggableObject)) {
      console.log(draggableObject);
      scene.remove(draggableObject);

      // Find and remove the object from the models array
      const index = models.indexOf(draggableObject);
      if (index > -1) {
        models.splice(index, 1);
      }

      // Detach transform controls and reset the draggableObject variable
      transformControls.detach();
      draggableObject = null;
    }
  }
}



// function deleteCube() {
//    
//     const intersects = raycaster.intersectObjects(models, true);
//     draggableObject = intersects[0].object;
//     if (draggableObject) {
// 	console.log(draggableObject);
// 	scene.remove(draggableObject);
//
// 	const index = models.indexOf(draggableObject);
// 	if (index > -1) {
// 	    models.splice(index, 1);
// 	}
//
// 	// Detach transform controls
// 	transformControls.detach();
// 	// Update the selectedCube variable
// 	draggableObject = null;
//     }
// }

init3D();


// Add event listener for keypress
window.addEventListener('keydown', (event) => {
    switch (event.key) {
        case 't': // If "t" is pressed, switch to translation mode
            transformControls.setMode('translate');
            transformControls.showX = true;
	    transformControls.showY = false;
	    transformControls.showZ = true;  
	break;
        case 'r': // If "r" is pressed, switch to rotation mode
            transformControls.setMode('rotate');
            transformControls.showX = false;
	    transformControls.showY = true;
	    transformControls.showZ = false;  
	break;
        case 'y': // If "t" is pressed, switch to translation mode
            transformControls.setMode('translate');
            transformControls.showX = false;
	    transformControls.showY = true;
	    transformControls.showZ = false;  
	break;
    }
});

let add_model = document.getElementById("add_model");
add_model.addEventListener("click", () => {
	let model = document.getElementById('selectedImageText').getAttribute('data-alt');
	addObjectToScene(model);},
	false);

let remove_model = document.getElementById("delete_model");
remove_model.addEventListener("click", deleteCube, false);

// Resize canvas on window resize
window.addEventListener('resize', function () {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});



