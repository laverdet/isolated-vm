[![npm version](https://img.shields.io/npm/v/isolated-vm.svg)](https://www.npmjs.com/package/isolated-vm) [![isc license](https://img.shields.io/npm/l/isolated-vm.svg)](https://github.com/laverdet/isolated-vm/blob/master/LICENSE) [![travis build](https://img.shields.io/travis/laverdet/isolated-vm/master.svg)](https://travis-ci.org/laverdet/isolated-vm) [![npm downloads](https://img.shields.io/npm/dm/isolated-vm.svg)](https://www.npmjs.com/package/isolated-vm)

isolated-vm -- Access to multiple isolates in nodejs
====================================================

[![NPM](https://nodei.co/npm/isolated-vm.png)](https://www.npmjs.com/package/isolated-vm)

`isolated-vm` is a library for nodejs which gives you access to v8's `Isolate` interface. This
allows you to create JavaScript environments which are completely *isolated* from each other. You
might find this module useful if you need to run some untrusted code in a secure way. You may also
find this module useful if you need to run some JavaScript simultaneously in multiple threads. You
may find this project *very* useful if you need to do both at the same time!


REQUIREMENTS
----------------------------------

This project works best on nodejs LTS version 8.11.2 (or later) *or* nodejs current version 10.x
(any version). It will compile and generally run alright on earlier 8.x versions but under load you
will run into crashes and hangs due to some bugs in v8.

Furthermore, to install this module you will need a compiler installed. If you run into errors while
running `npm install isolated-vm` it is likely you don't have a compiler set up, or your compiler is
too old.

* Windows + OS X users should follow the instuctions here: [node-gyp](https://github.com/nodejs/node-gyp)
* Ubuntu users should run: `sudo apt-get install g++ build-essential`
* Alpine users should run: `sudo apk add python make g++`

If you are using Ubuntu please note that version 16 (Xenial) or greater is required. This project
will *not* build using gcc 4.9.


WHO IS USING ISOLATED-VM
------------------------

* [Screeps](https://screeps.com/) - Screeps is an online JavaScript-based MMO+RPG game. They are
using isolated-vm to run arbitrary player-supplied code in secure enviroments which can persistent
for several days at a time.

* [Fly](https://fly.io/) - Fly is a programmable CDN which hosts dynamic endpoints as opposed to
just static resources. They are using isolated-vm to run globally distributed applications, where
each application may have wildly different traffic patterns.


API DOCUMENTATION
-----------------

Since isolates share no resources with each other, most of this API is built to provide primitives
which make marshalling data between many isolates quick and easy. The only way to pass data from one
isolate to another is to first make that data *transferable*. Primitives (except for `Symbol`) are
always transferable. This means if you invoke a function in a different isolate with a number or
string as the argument, it will work fine. If you need to pass more complex information you will
have to first make the data transferable with one of the methods here.

Most methods will provide both a synchronous and an asynchronous version. Calling the synchronous
functions will block your thread while the method runs and eventually returns a value. The
asynchronous functions will return a
[Promise](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)
while the work runs in a separate thread pool.

There are some rules about which functions may be called from certain contexts:

1. Asynchronous functions may be called at any time
2. Synchronous functions usually may not be called from an asynchronous function
3. You may call a synchronous function from an asynchronous function as long as that function
	 belongs to current isolate
4. You may call a synchronous function belonging to the default nodejs isolate at any time

Additionally, some methods will provide an "ignored" version which runs asynchronously but returns
no promise. This can be a good option when the calling isolate would ignore the promise anyway,
since the ignored versions can skip an extra thread synchronization. Just be careful because this
swallows any thrown exceptions which might make problems hard to track down.

It's also worth noting that all asynchronous invocations will run in the order they were queued,
regardless of whether or not you wait on them. So, for instance, you could call several "ignored"
methods in a row and then `await` on a final async method to observe some side-effect of the
ignored methods.


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


##### `isolate.compileModule(code)` *[Promise](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)*
##### `isolate.compileModuleSync(code)`
* `code` *[string]* - The JavaScript code to compile.
* `options` *[object]*
	* `filename` *[string]* - Optional filename of this script, used in stack traces
	* `columnOffset` *[number]* - Optional column offset of this script
	* `lineOffset` *[number]* - Optional line offset of this script

* **return** A [`Module`](#class-module-transferable) object.

Note that a [`Module`](#class-script-transferable) can only run in the isolate which created it.

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

##### `isolate.cpuTime` *[Array]*
##### `isolate.wallTime` *[Array]*
The total CPU and wall time spent in this isolate. CPU time is the amount of time the isolate has
spent actively doing work on the CPU. Wall time is the amount of time the isolate has been running,
including passive time spent waiting (think "wall" like a clock on the wall). For instance, if an
isolate makes a call into another isolate, wall time will continue increasing while CPU time will
remain the same.

The return format is `[ seconds, nanoseconds ]`, which is the same as the nodejs method
[`process.hrtime`](https://nodejs.org/api/process.html#process_process_hrtime_time). To convert this
value to milliseconds you could do something like: `(ret[0] + ret[1] / 1e9) * 1000`. Some precision
is lost in this conversion but for most applications it's probably not a big deal.

Note that CPU time may vary drastically if there is contention for the CPU. This could occur if
other processes are trying to do work, or if you have more than `require('os').cpus().length`
isolates currently doing work in the same nodejs process.

##### `isolate.referenceCount` *[number]*
Returns the total count of active `Reference` instances that belong to this isolate. Note that in
certain cases many `Reference` instances in JavaScript will point to the same underlying reference
handle, in which case this number will only reflect the underlying reference handle. This happens
when you transfer a `Reference` instance via some method which accepts transferable values. This
will also include underlying reference handles created by isolated-vm like `Script` or `Context`
objects.


### Class: `Context` *[transferable]*
A context is a sandboxed execution environment within an isolate. Each context contains its own
built-in objects and global space.

##### `context.global` *[`Reference`](#class-reference-transferable)*
[`Reference`](#class-reference-transferable) to this context's global object. Note that if you call
`context.release()` the global reference will be released as well.

##### `context.release()`

Releases this reference to the context. You can call this to free up v8 resources immediately, or
you can let the garbage collector handle it when it feels like it. Note that if there are other
references to this context it will not be disposed. This only affects this reference to the context.

### Class: `Script` *[transferable]*
A script is a compiled chunk of JavaScript which can be executed in any context within a single
isolate.

##### `script.release()`

Releases the reference to this script, allowing the script data to be garbage collected. Functions
and data created in the isolate by previous invocations to `script.run(...)` will still be alive in
their respective contexts-- this only means that you can't invoke `script.run(...)` again with this
reference.

##### `script.run(context, options)` *[Promise](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)*
##### `script.runIgnored(context, options)`
##### `script.runSync(context, options)`
* `context` *[`Context`](#class-context-transferable)* - The context in which this script will run.
* `options` *[object]*
	* `release` *[boolean]* - If true `release()` will automatically be called on this instance.
	* `timeout` *[number]* - Maximum amount of time this script is allowed to run before execution is
	canceled. Default is no timeout.
* **return** *[transferable]*

Runs a given script within a context. This will return the last value evaluated in a given script,
as long as that value was transferable, otherwise `undefined` will be returned. For instance if your
script was "let foo = 1; let bar = 2; bar = foo + bar" then the return value will be 3 because that
is the last expression.


### Class: `Module` *[transferable]*
A JavaScript module. Note that a [`Module`](#class-module-transferable) can only run in the isolate which created it.


##### `module.dependencySpecifiers`
A read-only array of all dependency specifiers the module has.

    const code = `import something from './something';`;
    const module = await isolate.compileModule(code);
    const dependencySpecifiers = module.dependencySpecifiers;
    // dependencySpecifiers => ["./something"];

##### `module.setDependency(specifier, module)`
* `specifier` - The dependency specifier.
* `module` - Another *[`Module`](#class-module-transferable)* instance.
* **return** *[undefined]*

Set the dependency the referrer-module should resolve use when [specifier] is requested. Module
must belong to the same context if it has already been instantiated.

##### `module.instantiate(context)` *[Promise](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)*
##### `module.instantiateSync(context)`
* `context` *[`Context`](#class-context-transferable)* - The context the module should use.
* **return** *[undefined]*

Instantiate the module together with all its dependencies.

##### `module.evaluate(options)` *[Promise](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)*
##### `module.evaluateSync(options)`
* `options` *[object]* - Optional.
	* `timeout` *[number]* - Maximum amount of time this module is allowed to run before execution is
	canceled. Default is no timeout.
* **return** last expression

Evaluate the module and return the last expression (same as script.run).


##### `module.namespace`
Returns a [`Reference`](#class-reference-transferable) containing all exported values.


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
* `options` *[object]*
	* `release` *[boolean]* - If true `release()` will automatically be called on this instance.
* **return** The value referenced by this handle.

Will attempt to return the actual value or object pointed to by this reference. Note that in order
to call this function the reference must be owned by the current isolate, otherwise an error will be
thrown.

##### `reference.derefInto()`
* `options` *[object]*
	* `release` *[boolean]* - If true `release()` will automatically be called on this instance.
* **return** *[transferable]*

Returns an object, which when passed to another isolate will cause that isolate to dereference the
handle.

##### `reference.release()`

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
##### `reference.applySyncPromise(receiver, arguments)`
* `receiver` *[transferable]* - The value which will be `this`.
* `arguments` *[array]* - Array of transferables which will be passed to the function.
* `options` *[object]*
	* `timeout` *[number]* - Maximum amount of time this function is allowed to run before execution is
	canceled. Default is no timeout.
* **return** *[transferable]*

Will attempt to invoke an object as if it were a function. If the return value is transferable it
will be returned to the caller of `apply`, otherwise an error will be thrown.

`applySyncPromise` is a special version of `applySync` which may only be invoked on functions
belonging to the default isolate AND may only be invoked from a non-default thread. Functions
invoked in this way may return a promise and the invoking isolate will wait for that promise to
resolve before resuming execution. You can use this to implement functions like `readFileSync` in a
way that doesn't block the default isolate. Note that the invoking isolate will not respond to any
async functions until this promise is resolved, however synchronous functions will still function
correctly. Misuse of this feature may result in deadlocked isolates, though the default isolate
will never be at risk of a deadlock.


### Class: `ExternalCopy` *[transferable]*
Instances of this class represent some value that is stored outside of any v8 isolate. This value
can then be quickly copied into any isolate.

##### `new ivm.ExternalCopy(value, options)`
* `value` - The value to copy.
* `options` *[object]*
	* `transferOut` *[boolean]* - If true this will release ownership of the given resource from this
	isolate. This operation completes in constant time since it doesn't have to copy an arbitrarily
	large object. This only applies to ArrayBuffer and TypedArray instances.

Primitive values can be copied exactly as they are. Date objects will be copied as as Dates.
ArrayBuffers, TypedArrays, and DataViews will be copied in an efficient format. SharedArrayBuffers
will simply copy a reference to the existing memory and when copied into another isolate the new
SharedArrayBuffer will point to the same underlying data. After passing a SharedArrayBuffer to
ExternalCopy for the first time isolated-vm will take over management of the underlying memory
block, so a "copied" SharedArrayBuffer can outlive the isolate that created the memory originally.

All other objects will be copied in seralized form using the [structured clone algorithm](https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API/Structured_clone_algorithm).

##### `ExternalCopy.totalExternalSize` *[number]*

This is a static property which will return the total number of bytes that isolated-vm has allocated
outside of v8 due to instances of `ExternalCopy`.

##### `externalCopy.copy(options)`
* `options` *[object]*
	* `release` *[boolean]* - If true `release()` will automatically be called on this instance.
	* `transferIn` *[boolean]* - If true this will transfer the resource directly into this isolate,
	invalidating the ExternalCopy handle.
* **return** - JavaScript value of the external copy.

Internalizes the ExternalCopy data into this isolate.

##### `externalCopy.copyInto(options)`
* `options` *[object]*
	* `release` *[boolean]* - If true `release()` will automatically be called on this instance.
	* `transferIn` *[boolean]* - If true this will transfer the resource directly into this isolate,
	invalidating the ExternalCopy handle.
* **return** *[transferable]*

Returns an object, which when passed to another isolate will cause that isolate to internalize a
copy of this value.

#### `externalCopy.release()`

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
let jail = context.global;

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
		// `applyIgnored` calls `log` asynchronously but doesn't return a promise-- it ignores the
		// return value or thrown exception from `log`.
		log.applyIgnored(undefined, args.map(arg => new ivm.ExternalCopy(arg).copyInto()));
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
	let script = await isolate.compileScript(sum+ '');
	await script.run(context);
	let fnReference = await context.global.get('sum');

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

Included in the repository is an example of how you can write quicksort using a SharedArrayBuffer to
sort over multiple threads. See: [parallel-sort-example.js](https://github.com/laverdet/isolated-vm/blob/master/parallel-sort-example.js).
