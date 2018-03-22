'use strict';
// On nodejs v8.x.x you must run this example with --harmony_sharedarraybuffer
if (typeof SharedArrayBuffer === 'undefined') {
	console.error('Please run with --harmony_sharedarraybuffer flag.');
	if (process.argv[2] === '--harmony_sharedarraybuffer') {
		console.error('You have to put the flag *before* the script name since you\'re passing the flag to v8 and not to this script.');
	}
	process.exit(1);
}
const ivm = require('isolated-vm');

// Generate a 32mb array of random floating points
const arrayLength = 1024 * 1024 * 4;
let randomArray = new Float64Array(arrayLength);
for (let ii = 0; ii < randomArray.length; ++ii) {
	randomArray[ii] = Math.random();
}

// This function will throw if the array is not sorted, to verify our results
function verifySorted(array) {
	for (let ii = 0; ii < array.length - 1; ++ii) {
		if (array[ii + 1] < array[ii]) {
			throw new Error('Arrray is not sorted');
		}
	}
}

// This part of the algorithm is used by both the single and multi-threaded versions
function partition(array, left, right) {
	let pivot = array[(left + right) >> 1];
	let ii = left, jj = right;
	do {
		while (array[ii] < pivot) ++ii;
		while (array[jj] > pivot) --jj;
		if (ii >= jj) {
			return jj;
		}
		let tmp = array[ii];
		array[ii] = array[jj];
		array[jj] = tmp;
	} while (true);
}

// Single-threaded solution. This is also invoked from the multi-threaded version when the range to
// sort is under a certain threshold.
function quicksort(array, left, right) {
	if (left < right) {
		let ii = partition(array, left, right);
		quicksort(array, left, ii);
		quicksort(array, ii + 1, right);
	}
}

// Create a copy of the original array and time its performance
let copy1 = new Float64Array(randomArray);
let time1 = process.hrtime();
quicksort(copy1, 0, arrayLength - 1);
time1 = process.hrtime(time1);
verifySorted(copy1);
console.log(`Single-threaded took ${time1[0] * 1e3 + time1[1] / 1e6}ms`);

// Now an implementation that will spawn new isolates to run the work in parallel
function quicksortParallel(array, left, right, callback) {
	// We have to wrap this async function to call `callback`. If we simply made `quicksortParallel`
	// async instead that wouldn't work because `Promise` instances are *not* transferrable.
	(async function() {
		if (right - left > 1024 << 9) {
			// Range is large so the recursion stage will be run in parallel. `1024 << 9` was picked via
			// experimentation.

			// The following array map creates two new `ivm.Isolate` instances with the functions we
			// defined in this file. It returns an async function which invokes the `quicksortParallel`
			// function within the isolate.
			let threadPromises = [ 0, 0 ].map(async function() {
				let isolate = new ivm.Isolate;
				let [ context, script ] = await Promise.all([
					isolate.createContext(),
					isolate.compileScript(`${partition}; ${quicksort}; ${quicksortParallel};`),
				]);
				// Run the compiled script in the context we created
				await script.run(context);
				// Here we give the isolate we just spawned access to the entire `isolated-vm` library
				context.global.setIgnored('ivm', ivm);
				// This gets an `ivm.Reference` to the `quicksortParallel` function that lives in the new
				// isolate. We return a function that calls that function and returns a `Promise` which will
				// resolve when it is done.
				let fnReference = await context.global.get('quicksortParallel');
				return function(array, left, right) {
					return new Promise(function(resolve) {
						fnReference.apply(undefined, [ array, left, right, new ivm.Reference(resolve) ]);
					});
				};
			});
			// Partition happens in this thread
			let ii = partition(array, left, right);
			// When passing an instance of `SharedArrayBuffer`, `ExternalCopy` will actually reference the
			// underlying shared memory segment
			let reference = new ivm.ExternalCopy(array);
			// The recursion invokes two different isolates in parallel
			let threads = await Promise.all(threadPromises);
			await Promise.all([
				threads[0](reference.copyInto(), left, ii),
				threads[1](reference.copyInto(), ii + 1, right),
			]);
		} else if (left < right) {
			// Short range, no new threads will be spawned
			let ii = partition(array, left, right);
			quicksort(array, left, ii);
			quicksort(array, ii + 1, right);
		}
	}()).then(() => callback.apply(undefined, []));
}

// Create another copy of the random array, this time in shared memory. Again, we time the
// performance.
let copy2 = new Float64Array(new SharedArrayBuffer(randomArray.buffer.byteLength));
copy2.set(randomArray);
let time2 = process.hrtime();
quicksortParallel(copy2, 0, arrayLength - 1, function() {
	time2 = process.hrtime(time2);
	verifySorted(copy2);
	console.log(`Multi-threaded took ${time2[0] * 1e3 + time2[1] / 1e6}ms`);
});

/*
Sample output on a 4-core machine:
Single-threaded took 645.060701ms
Multi-threaded took 267.792654ms

Note that the multi-threaded solution includes the time to create and initialize all the isolates it
uses, which is not an insignificant cost. In a real application you could have isolates standing by
to accept requests to avoid that cost.
*/
