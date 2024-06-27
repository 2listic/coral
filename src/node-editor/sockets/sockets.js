import Rete from "rete";

var NumSocket = new Rete.Socket("Number");
var ShapeSocket = new Rete.Socket("Shape value");
var GridSocket = new Rete.Socket("Grid value");
var BoolSocket = new Rete.Socket("Boolean value");

export { GridSocket };
export { ShapeSocket };
export { NumSocket };
export { BoolSocket };
