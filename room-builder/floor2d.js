const canvas2D = document.getElementById("canvas2D");
export { canvas2D }; // Export canvas2D

const GRID_SPACING = 30;
paper.setup(canvas2D);

// Create a grid background in Paper.js
export function createGrid(spacing = GRID_SPACING, color = "#e0e0e0") {
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
let contextMenu;
let selectedVertex = null; // Track the selected vertex for dragging
let selectedLine = null; // Track the selected line for length adjustment
let isDraggingVertex = false; // Flag for vertex dragging

function snapToGrid(point) {
  const snappedX = Math.round(point.x / GRID_SPACING) * GRID_SPACING;
  const snappedY = Math.round(point.y / GRID_SPACING) * GRID_SPACING;
  return new paper.Point(snappedX, snappedY);
}

function updateLengthText(path) {
  const lengthInPixels = path.length;
  const metersPerPixel = 10 / paper.view.bounds.width;
  const lengthInMeters = (lengthInPixels * metersPerPixel).toFixed(2);

  // Calculate offset perpendicular to the line to avoid overlap
  const direction = path.firstSegment.point.subtract(path.lastSegment.point);
  const offset = direction.normalize().rotate(90).multiply(30); // 30 pixel of offset

  currentText.content = lengthInMeters + " m";
  currentText.point = path.getPointAt(path.length / 2).add(offset); // Offset text position
}

function updateConnectedLines(vertex) {
  vertex.data.connectedPaths.forEach((pathInfo) => {
    const path = pathInfo.path;
    const index = pathInfo.index;

    // Update the position of the path segments
    path.segments[index].point = vertex.position;

    // Update length text
    updateLengthText(path);
  });
}

function createVertex(point) {
  const vertex = new paper.Path.Circle({
    center: point,
    radius: 5,
    fillColor: 'red'
  });

  vertex.data = { connectedPaths: [] }; // Initialize connected paths
  vertices.push(vertex);
  return vertex;
}

// Function to create the context menu
function createContextMenu(position) {
  // Remove any existing context menu
  if (contextMenu) {
    contextMenu.remove();
  }
  // Create a new context menu
  contextMenu = document.createElement("div");
  contextMenu.style.position = "absolute";
  contextMenu.style.top = `${position.y}px`;
  contextMenu.style.left = `${position.x}px`;
  contextMenu.style.background = "#fff";
  contextMenu.style.border = "1px solid #ccc";
  contextMenu.style.padding = "5px";
  contextMenu.style.boxShadow = "0px 2px 10px rgba(0, 0, 0, 0.2)";
  contextMenu.style.zIndex = 1000;

  // Add the delete option
  const deleteOption = document.createElement("div");
  deleteOption.textContent = "Delete Line";
  deleteOption.style.cursor = "pointer";
  deleteOption.onclick = () => {
    if (selectedLine) {
      deleteLine(selectedLine); // Call delete function when option is clicked
      selectedLine = null;
      contextMenu.remove();
    }
  };
  // Append the option to the menu
  contextMenu.appendChild(deleteOption);
  document.body.appendChild(contextMenu);
}

// Function to delete the line and its vertices
function deleteLine(line) {
  // Remove connected vertices and lines from the vertex data
  vertices.forEach((vertex) => {
    vertex.data.connectedPaths = vertex.data.connectedPaths.filter(
      (pathInfo) => pathInfo.path !== line
    );
    // If the vertex has no more connected paths, remove it
    if (vertex.data.connectedPaths.length === 0) {
      vertex.remove();
      vertices.splice(vertices.indexOf(vertex), 1); // Remove vertex from the vertices array
    }
  });

  // Remove the line and its length text from the canvas
  if (line.data.lengthText) {
    line.data.lengthText.remove();
  }
  line.remove();
}


paper.view.onMouseDown = function (event) {
  const snappedPoint = snapToGrid(event.point); // Snap starting point to the grid
 
  if (event.event.button === 2) {
    // Perform a hit test to find the line that was right-clicked
    const hitResult = paper.project.hitTest(event.point, {
      segments: true,
      stroke: true,
      fill: true,
      tolerance: 5,
    });

    if (hitResult && hitResult.item.className === "Path") {
      selectedLine = hitResult.item;
      createContextMenu(event.event); // Pass the mouse event to position the menu
      return;
    }
  }

  // If not a right-click or no line is clicked, remove the context menu
  if (contextMenu) {
    contextMenu.remove();
    contextMenu = null;
  }

  if (selectedVertex) {
    // Start a new line from the selected vertex
    if (currentPath) {
      currentPath.removeSegment(1);
    }
    currentPath = new paper.Path();
    currentPath.strokeColor = "black";
    currentPath.strokeWidth = 5;
    currentPath.add(snapToGrid(selectedVertex.position)); // Snap to grid

    // Initialize length text
    currentText = new paper.PointText({
      point: selectedVertex.position,
      content: "",
      fillColor: "black",
      fontSize: 16,
    });

    // Deselect the vertex
    selectedVertex = null;
  } else if (selectedLine) {
    // Code to handle selected line (not the focus here)
  } else {
    // Start a new line normally
    currentPath = new paper.Path();
    currentPath.strokeColor = "black";
    currentPath.strokeWidth = 5;
    currentPath.add(snappedPoint); // Start at the snapped grid point

    // Initialize length text
    currentText = new paper.PointText({
      point: snappedPoint,
      content: "",
      fillColor: "black",
      fontSize: 16,
    });
  }
};

paper.view.onMouseDrag = function (event) {
  if (currentPath) {
    // Ensure the line remains straight
    if (currentPath.segments.length > 1) {
      currentPath.removeSegment(1);
    }
    currentPath.add(snapToGrid(event.point)); // Snap endpoint to the nearest grid intersection

    // Update length text during drag
    updateLengthText(currentPath);
  } else if (isDraggingVertex && selectedVertex) {
    selectedVertex.position = snapToGrid(event.point); // Snap dragging vertex to grid
    updateConnectedLines(selectedVertex); // Update connected lines
  } else if (selectedLine) {
    // Adjust the length of the selected line (not the focus here)
  }
};

paper.view.onMouseUp = function () {
  if (currentPath) {
    const startVertex = snapToGrid(currentPath.firstSegment.point);
    const endVertex = snapToGrid(currentPath.lastSegment.point);

    currentPath.firstSegment.point = startVertex;
    currentPath.lastSegment.point = endVertex;

    // Connect path to vertices and add vertices to the list if new
    let start = vertices.find((v) => v.position.equals(startVertex));
    let end = vertices.find((v) => v.position.equals(endVertex));

    if (!start) start = createVertex(startVertex);
    if (!end) end = createVertex(endVertex);

    start.data.connectedPaths.push({ path: currentPath, index: 0 });
    end.data.connectedPaths.push({ path: currentPath, index: 1 });

    // Store the length text in the path's data for easy access
    currentPath.data.lengthText = currentText;

    // Reset the current path and text variables
    currentPath = null;
    currentText = null;
  }
};

canvas2D.addEventListener("contextmenu", (e) => {
  e.preventDefault();
});
