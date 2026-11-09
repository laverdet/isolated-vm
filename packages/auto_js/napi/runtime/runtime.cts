// This file is `runtime_dist_runtime_cjs` in C++

/**
 * Invokes `callback(...args)` and settles the result, which may be a promise, through `resolve` or
 * `reject`. A synchronous throw is also routed to `reject`.
 */
function asyncToCallbacks(
	callback: (...args: unknown[]) => unknown,
	args: unknown[],
	resolve: (value: unknown) => void,
	reject: (reason: unknown) => void,
): void {
	void async function() {
		try {
			resolve(await callback(...args));
		} catch (error) {
			reject(error);
		}
	}();
}

const runtime: {
	asyncToCallbacks: typeof asyncToCallbacks;
} = {
	asyncToCallbacks,
};
export = runtime;
