const canvas2D = document.getElementById("canvas2D");
export { canvas2D }; // Export canvas2D

paper.setup(canvas2D);

// Create a grid background in Paper.js
export function createGrid(spacing = 50, color = "#e0e0e0") {
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


