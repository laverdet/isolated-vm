## v4.3.0
- v8 inspector API fixed in nodejs v16.x
- `release` method added to `Module`

## v4.2.0
- `accessors` option added to `reference.get()`

## v4.1.0
- Support for nodejs v16.x
- `onCatastrophicError` added
- Fix for `null` error thrown from callback

## v4.0.0
- `Callback` class addeed.
- When possible, `reference.get()` will return a function delegate instead of a `Reference`.
- `reference.get()` will no longer return inherited properties by default.
- `result` property on `eval` and `evalClosure` has been removed. The result is now just the return
value.
- All `isolated-vm` class prototypes, and most instances are frozen.
- `isolate.cpuTime` and `isolate.wallTime` now return bigints.
- Proxies and accessors are no longer tolerated via `reference.get`, and related functions.
