import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const {
	echoAscii,
	echoUtf8,
	echoUtf16,
	addInts,
	addUints,
	negateBool,
	bigintAdd,
	describeDirection,
	midpoint,
	mixColors,
	centroid,
	area,
	describeValue,
	elapsedMs,
	Canvas,
} = require("./build/example.node");

// Strings
console.log("echoAscii:", echoAscii("hello"));
console.log("echoUtf8:", echoUtf8("hello utf8"));
console.log("echoUtf16:", echoUtf16("hello utf16"));

// Integrals
console.log("addInts:", addInts(3, 4));
console.log("addUints:", addUints(10, 20));
console.log("negateBool:", negateBool(true));

// Bigints
console.log("bigintAdd:", bigintAdd(9007199254740993n, 1n));

// Optional + enum
console.log("describeDirection(north):", describeDirection("north"));
console.log("describeDirection(undefined):", describeDirection(undefined));

// Structs
console.log("midpoint:", midpoint({ x: 0, y: 0 }, { x: 10, y: 10 }));
console.log("mixColors:", mixColors({ r: 100, g: 200, b: 50 }, { r: 200, g: 100, b: 150 }));

// Arrays
console.log("centroid:", centroid([ { x: 0, y: 0 }, { x: 6, y: 0 }, { x: 3, y: 6 } ]));

// Discriminated union
console.log("area(circle r=5):", area({ type: "circle", center: { x: 0, y: 0 }, radius: 5 }));
console.log("area(rect 4x3):", area({ type: "rectangle", topLeft: { x: 0, y: 0 }, width: 4, height: 3 }));

// Variant
console.log("describeValue(string):", describeValue("hello"));
console.log("describeValue(number):", describeValue(42));

// Date
const past = new Date(Date.now() - 1000);
console.log("elapsedMs:", elapsedMs(past));

// Class (object wrap)
const canvas = Canvas.create("my-canvas");
console.log("canvas.getName():", canvas.getName());
console.log("canvas.count() initial:", canvas.count());
canvas.add({ type: "circle", center: { x: 1, y: 2 }, radius: 3 });
canvas.add({ type: "rectangle", topLeft: { x: 0, y: 0 }, width: 5, height: 2 });
console.log("canvas.count() after adds:", canvas.count());
console.log("canvas.getCreatedAt():", canvas.getCreatedAt());
