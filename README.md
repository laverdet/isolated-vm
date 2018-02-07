isolated-vm -- Access to multiple isolates in nodejs
====================================================

`isolated-vm` is a library for nodejs which gives you access to v8's `Isolate` interface. This
allows you to create JavaScript environments which are completely *isolated* from each other.

API DOCUMENTATION
-----------------

Since isolates share no resources with each other, the primary goal of this API is to provide
primitives which make marshalling data between many isolates quick and easy. The only way to pass
data from one isolate to another is to first make that data *transferable*. Primitives (except for
`Symbol`) are always transferable. This means if you invoke a function in a different isolate with a
number or string as the argument, it will work fine. If you need to pass more complex information
you will have to first make the data transferable with one of the methods here.

Most methods will provide both a synchronous and an asynchronous version. Calling the synchronous
functions will block your thread while the method runs and eventually returns a value. The
asynchronous functions will return a
[Promise](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)
while the work runs in a separate thread pool. I recommend deciding which version of the API is the
best for your project and sticking to it throughout your code, i.e. don't mix and match async/sync
code. The only strict technical limitation, though, is that you may not call a synchronous method
from within a asynchronous method.

Additionally, some methods will provide an "ignored" version which runs asynchronously but returns
no promise. This can be a good option when the calling isolate would ignore the promise anyway,
since the ignored versions can skip an extra thread synchronization. Just be careful because this
swallows any thrown exceptions which might make problems hard to track down. It's also worth noting
that all asynchronous invocations will run in the order they were queued, regardless of whether or
not you wait on them. So, for instance, you could call several "ignored" methods in a row and then
`await` on a final async method to observe some side-effect of the ignored methods.


### Class: `Isolate` *[transferable]*
This is the main reference to an isolate. Every handle to an isolate is transferable, which means
you can give isolates references to each other. An isolate will remain valid as long as someone
holds a handle to the isolate or anything created inside that isolate. Once an isolate is lost the
garbage collector should eventually find it and clean up its memory. Since an isolate and all it
contains can represent quite a large chunk of memory though you may want to explicitly call the
`dispose()` method on isolates that you are finished with to get that memory back immediately.

##### `new ivm.Isolate(options)`
* `options` *[object]*
	* `memoryLimit` *[number]* - Memory limit that this isolate may use, in MB. Note that this is more
	of a guideline instead of a strict limit. A determined attacker could use 2-3 times this limit
	before their script is terminated. Against non-hostile code this limit should be pretty close. The
	default is 128MB and the mimium is 8MB.
	* `inspector` *[boolean]* - Enable v8 inspector support in this isolate. See
	`inspector-example.js` in this repository for an example of how to use this.
	* `snapshot` *[ExternalCopy[ArrayBuffer]]* - This is an optional snapshot created from
	`createSnapshot` which will be used to initialize the heap of this isolate.

##### `ivm.Isolate.createSnapshot(scripts, warmup_script)`
* `scripts` *[array]*
	* `code` *[string]* - Script to setup this snapshot
	* `filename` *[string]* - Optional filename of this script, used in stack traces
	* `columnOffset` *[number]* - Optional column offset of this script
	* `lineOffset` *[number]* - Optional line offset of this script
* `warmup_script` *[string]* - Optional script to "warmup" the snapshot by triggering code
compilation

Isolate snapshots are a very useful feature if you intend to create several isolates running common
libraries between them. A snapshot serializes the entire v8 heap including parsed code, global
variables, and compiled code. Check out the examples section for tips on using this.

##### `isolate.compileScript(code)` *[Promise](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)*
##### `isolate.compileScriptSync(code)`
* `code` *[string]* - The JavaScript code to compile.
* `options` *[object]*
	* `filename` *[string]* - Optional filename of this script, used in stack traces
	* `columnOffset` *[number]* - Optional column offset of this script
	* `lineOffset` *[number]* - Optional line offset of this script
	* `produceCachedData` *[boolean]* - Produce V8 cache data. Similar to the
	[VM.Script](https://nodejs.org/api/vm.html) option of the same name. If this is true then the
	returned script object will have `cachedData` set to an ExternalCopy handle. Note that this
	differs from the VM.Script option slightly in that `cachedDataProduced` is never set.
	* `cachedData` *[ExternalCopy[ArrayBuffer]]* - This will consume cached compilation data from a
	previous call to this function. Please don't use `produceCachedData` and `cachedData` options at
	the same time. `cachedDataRejected` will be set to `true` if the supplied data was rejected by
	V8.

* **return** A [`Script`](#class-script-transferable) object.

Note that a [`Script`](#class-script-transferable) can only run in the isolate which created it.

##### `isolate.createContext()` *[Promise](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)*
##### `isolate.createContextSync()`
* `options` *[object]*
	* `inspector` *[boolean]* - Enable the v8 inspector for this context. The inspector must have been
	enabled for the isolate as well.

* **return** A [`Context`](#class-context-transferable) object.

##### `isolate.dispose()`
Destroys this isolate and invalidates all references obtained from it.

##### `isolate.getHeapStatistics()` *[Promise](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)*
##### `isolate.getHeapStatisticsSync()`
* **return** [object]

##### `isolate.isDisposed` *[boolean]*
Flag that indicates whether this isolate has been disposed.

Returns heap statistics from v8. The return value is almost identical to the nodejs function
[v8.getHeapStatistics()](https://nodejs.org/dist/latest-v8.x/docs/api/v8.html#v8_v8_getheapstatistics).
This function returns one additional property: `externally_allocated_size` which is the total amount
of currently allocated memory which is not included in the v8 heap but counts against this isolate's
`memoryLimit`. ArrayBuffer instances over a certain size are externally allocated and will be
counted here.

### Class: `Context` *[transferable]*
A context is a sandboxed execution environment within an isolate. Each context contains its own
built-in objects and global space.

##### `context.globalReference()`
* **return** A [`Reference`](#class-reference-transferable) object.

Returns a [`Reference`](#class-reference-transferable) to this context's global object.

##### `context.release()`

Releases this reference to the context. You can call this to free up v8 resources immediately, or
you can let the garbage collector handle it when it feels like it. Note that if there are other
references to this context it will not be disposed. This only affects this reference to the context.

### Class: `Script` *[transferable]*
A script is a compiled chunk of JavaScript which can be executed in any context within a single
isolate.

##### `script.run(context)` *[Promise](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)*
##### `script.runIgnored(context)`
##### `script.runSync(context)`
* `context` *[`Context`](#class-context-transferable)* - The context in which this script will run.
* `options` *[object]*
	* `timeout` *[number]* - Maximum amount of time this script is allowed to run before execution is
	canceled. Default is no timeout.
* **return** *[transferable]*

Runs a given script within a context. This will return the last value evaluated in a given script,
as long as that value was transferable, otherwise `undefined` will be returned. For instance if your
script was "let foo = 1; let bar = 2; bar = foo + bar" then the return value will be 3 because that
is the last expression.

### Class: `Reference` *[transferable]*
A instance of [`Reference`](#class-reference-transferable) is a pointer to a value stored in any isolate.

##### `reference.typeof` *[string]*

This is the typeof the referenced value, and is available at any time from any isolate. Note that
this differs from the real `typeof` operator in that `null` is "null", and Symbols are "object".

##### `reference.copy()` *[Promise](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)*
##### `reference.copySync()`
* **return** JavaScript value of the reference.

Creates a copy of the referenced value and internalizes it into this isolate. This uses the same
copy rules as [`ExternalCopy`](#class-externalcopy-transferable).

##### `reference.deref()`
* **return** The value referenced by this handle.

Will attempt to return the actual value or object pointed to by this reference. Note that in order
to call this function the reference must be owned by the current isolate, otherwise an error will be
thrown.

##### `reference.derefInto()`
* **return** *[transferable]*

Returns an object, which when passed to another isolate will cause that isolate to dereference the
handle.

##### `reference.dispose()`

Releases this reference. If you're passing around a lot of references between isolates it's wise to
release the references when you are done. Otherwise you may run into issues with isolates running
out of memory because other isolates haven't garbage collected recently. After calling this method
all attempts to access the reference will throw an error.

##### `reference.get(property)` *[Promise](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)*
##### `reference.getSync(property)`
* `property` *[transferable]* - The property to access on this object.
* **return** A [`Reference`](#class-reference-transferable) object.

Will access a reference as if using `reference[property]` and return a reference to that value.

##### `reference.set(property, value)` *[Promise](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)*
##### `reference.setIgnored(property, value)`
##### `reference.setSync(property, value)`
* `property` *[transferable]* - The property to set on this object.
* `value` *[transferable]* - The value to set on this object.
* **return** `true` or `false`

Returns a boolean indicating whether or not this operation succeeded. I'm actually not really sure
when `false` would be returned, I'm just giving you the result back straight from the v8 API.

##### `reference.apply(receiver, arguments)` *[Promise](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)*
##### `reference.applyIgnored(receiver, arguments)`
##### `reference.applySync(receiver, arguments)`
* `receiver` *[transferable]* - The value which will be `this`.
* `arguments` *[array]* - Array of transferables which will be passed to the function.
* `options` *[object]*
	* `timeout` *[number]* - Maximum amount of time this function is allowed to run before execution is
	canceled. Default is no timeout.
* **return** *[transferable]*

Will attempt to invoke an object as if it were a function. If the return value is transferable it
will be returned to the called of `apply`, otherwise an error will be thrown.

### Class: `ExternalCopy` *[transferable]*
Instances of this class represent some value that is stored outside of any v8 isolate. This value
can then be quickly copied into any isolate.

##### `new ivm.ExternalCopy(value, options)`
* `value` - The value to copy.
* `options` *[object]*
	* `transfer` *[boolean]* - If true this will release ownership of the given resource from this
	isolate. This operation completes in constant time since it doesn't have to copy an arbitrarily
	large object. This only applies to ArrayBuffer and TypedArray instances.

Primitive values can be copied exactly as they are. Date objects will be copied as as Dates.
ArrayBuffers, TypedArrays, and DataViews will be copied in an efficient format. All other objects
will be copied in seralized form using the [structured clone algorithm](https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API/Structured_clone_algorithm).

##### `externalCopy.copy(options)`
* `options` *[object]*
	* `transfer` *[boolean]* - If true this will transfer the resource directly into this isolate,
	invalidating the ExternalCopy handle.
* **return** - JavaScript value of the external copy.

Internalizes the ExternalCopy data into this isolate.

##### `externalCopy.copyInto(options)`
* `options` *[object]*
	* `transfer` *[boolean]* - If true this will transfer the resource directly into this isolate,
	invalidating the ExternalCopy handle.
* **return** *[transferable]*

Returns an object, which when passed to another isolate will cause that isolate to internalize a
copy of this value.

#### `externalCopy.dispose()`

Releases the reference to this copy. If there are other references to this copy elsewhere the copy
will still remain in memory, but this handle will no longer be active. Disposing ExternalCopy
instances isn't super important, v8 is a lot better at cleaning these up automatically because
there's no inter-isolate dependencies.


EXAMPLES
--------

Below is a sample program which shows basic usage of the library.

```js
// Create a new isolate limited to 128MB
let ivm = require('isolated-vm');
let isolate = new ivm.Isolate({ memoryLimit: 128 });

// Create a new context within this isolate. Each context has its own copy of all the builtin
// Objects. So for instance if one context does Object.prototype.foo = 1 this would not affect any
// other contexts.
let context = isolate.createContextSync();

// Get a Reference{} to the global object within the context.
let jail = context.globalReference();

// This make the global object available in the context as `global`. We use `derefInto()` here
// because otherwise `global` would actually be a Reference{} object in the new isolate.
jail.setSync('global', jail.derefInto());

// The entire ivm module is transferable! We transfer the module to the new isolate so that we
// have access to the library from within the isolate.
jail.setSync('_ivm', ivm);

// We will create a basic `log` function for the new isolate to use.
jail.setSync('_log', new ivm.Reference(function(...args) {
	console.log(...args);
}));

// This will bootstrap the context. Prependeng 'new ' to a function is just a convenient way to
// convert that function into a self-executing closure that is still syntax highlighted by
// editors. It drives strict mode and linters crazy though.
let bootstrap = isolate.compileScriptSync('new '+ function() {
	// Grab a reference to the ivm module and delete it from global scope. Now this closure is the
	// only place in the context with a reference to the module. The `ivm` module is very powerful
	// so you should not put it in the hands of untrusted code.
	let ivm = _ivm;
	delete _ivm;

	// Now we create the other half of the `log` function in this isolate. We'll just take every
	// argument, create an external copy of it and pass it along to the log function above.
	let log = _log;
	delete _log;
	global.log = function(...args) {
		// We use `copyInto()` here so that on the other side we don't have to call `copy()`. It
		// doesn't make a difference who requests the copy, the result is the same.
		log.apply(undefined, args.map(arg => new ivm.ExternalCopy(arg).copyInto()));
	};
});

// Now we can execute the script we just compiled:
bootstrap.runSync(context);

// And let's test it out:
isolate.compileScriptSync('log("hello world")').runSync(context);
// > hello world

// Let's see what happens when we try to blow the isolate's memory
let hostile = isolate.compileScriptSync('new '+ function() {
	let storage = [];
	let twoMegabytes = 1024 * 1024 * 2;
	while (true) {
		let array = new Uint8Array(twoMegabytes);
		for (let ii = 0; ii < twoMegabytes; ii += 4096) {
			array[ii] = 1; // we have to put something in the array to flush to real memory
		}
		storage.push(array);
		log('I\'ve wasted '+ (storage.length * 2)+ 'MB');
	}
});

// Using the async version of `run` so that calls to `log` will get to the main node isolate
hostile.run(context).catch(err => console.error(err));
// I've wasted 2MB
// I've wasted 4MB
// ...
// I've wasted 122MB
// I've wasted 124MB
// RangeError: Array buffer allocation failed
```

Another example which shows how calls to the asynchronous methods will execute in separate threads
providing you with parallelism. Note that each isolate only "owns" a thread while it is executing.
So you could have hundreds of isolates sitting idle and they would not be using a thread.
```js
// A simple function to sum a range of numbers. This can also be expressed as:
// (max * (max - 1) - min * (min - 1)) / 2
// But this is an easy way to show off the async features of the module.
function sum(min, max) {
	let sum = 0;
	for (let ii = min; ii < max; ++ii) {
		sum += ii;
	}
	return sum;
}

// I chose this number because it's big but also small enough that we don't go past JS's integer
// limit.
let num = Math.pow(2, 27);

// First we execute a single thread run
let start1 = new Date;
let result = sum(0, num);
console.log('Calculated '+ result+ ' in '+ (Date.now() - start1)+ 'ms');

// Now we do the same thing over 4 threads
let start2 = new Date;
let ivm = require('isolated-vm');
let numThreads = 4;
let promises = Array(numThreads).fill().map(async function(_, ii) {

	// Set up 4 isolates with the `sum` function from above
	let isolate = new ivm.Isolate();
	let context = await isolate.createContext();
	let jail = context.globalReference();
	let script = await isolate.compileScript(sum+ '');
	await script.run(context);
	let fnReference = await jail.get('sum');

	// Run one slice of the sum loop
	let min = Math.floor(num / numThreads * ii);
	let max = Math.floor(num / numThreads * (ii + 1));
	return await fnReference.apply(undefined, [ min, max ]);
});
Promise.all(promises).then(function(sums) {
	let result = sums.reduce((a, b) => a + b, 0);
	console.log('Calculated '+ result+ ' in '+ (Date.now() - start2)+ 'ms');
});
// They get the same answer but the async version can do it much faster! Even
// with the overhead of building 4 isolates
// > Calculated 9007199187632128 in 1485ms
// > Calculated 9007199187632128 in 439ms
```

A quick example which shows how the snapshot feature works.
```js
let ivm = require('isolated-vm');

// Create a new snapshot which adds the `sum` function to all contexts created
let snapshot = ivm.Isolate.createSnapshot([ { code: 'function sum(a,b) { return a + b }' } ]);

// v8 provides the ability to "warmup" snapshots by calling the functions inside your snapshot which
// will trigger a code compilation, you can use the second parameter for that. A much easier way to
// included compiled code in the snapshot would be to run node with `--nolazy` when you generate the
// snapshot, then you can load the snapshot into your node instances using no special flags.
//
// `snapshot` is an ExternalCopy[ArrayBuffer]. If you wanted to save this snapshot to a file you
// would do this:
// fs.writeFileSync('snapshot.bin', Buffer.from(snapshot.copy()))
//
// And then to read it in again:
// snapshot = new ivm.ExternalCopy(fs.readFileSync('snapshot.bin').buffer);

// Here we'll create a new isolate using this snapshot and confirm that our `sum` function is there
let isolate = new ivm.Isolate({ snapshot });
let context = isolate.createContextSync();
let script = isolate.compileScriptSync('sum(1, 2)');
console.log(script.runSync(context));
// logs: 3
```
