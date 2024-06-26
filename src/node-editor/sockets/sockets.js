import Rete from "rete";

var NumSocket = new Rete.Socket("Number");
var ShapeSocket = new Rete.Socket("Shape value");
let GridSocket = new Rete.Socket("rid value");

export { GridSocket };
export { ShapeSocket };
export { NumSocket };
