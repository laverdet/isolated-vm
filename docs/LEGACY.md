Why Did Everything Change?
==========================

This is going to be a messy brain dump.

I started on this rewrite in 2024, first by deleting most of the source tree
[[146c5621](https://github.com/laverdet/isolated-vm/commit/146c56216ede83439f1dae2b951c3782da478761)]
and then with a promising proof of concept which could run two different versions of v8 in the same
process
[15116d90](https://github.com/laverdet/isolated-vm/commit/15116d902df5619dea7f7d6996aaca0b2af18c9e).
It's something I've been working on in my spare time.

Architecture
------------

`@isolated-vm/experimental` now ships with its own version of v8. This runs side-by-side with
nodejs's v8, they do not share any resources.

The binary is a lot larger because of this. The stripped binary is around 44MB, packed from npm it
would be about 18MB. v8's sample application, d8, is around the same size so I think this is just
how big the v8 engine is nowadays.

v8 requires support from an "embedder" to orchestrate all the isolates in a given process. In
`isolated-vm` the embedder is nodejs, which limits my options for controlling isolate lifecycles. In
`@isolated-vm/experimental` we have full control over embedder hooks. In addition to crucial memory
and garbage collection hooks we also get access to other nice-to-have's like granular &
deterministic control over clocks and the pseudorandom number generator.

Also, the backend is now (almost) entirely [napi-based](https://nodejs.org/api/n-api.html). This
feature along with a full copy of v8 means that arbitrary versions of `@isolated-vm/experimental`
will continue to run on newer versions of nodejs. Even [Bun](https://bun.com) will work using the
same binaries that nodejs uses. That's something that just couldn't be done with the previous
architecture. We could even swap out v8 with another JavaScript engine if we wanted.


Naming
------

`Isolate` is now called `Agent`, and `Realm` is now called `Context`. The term "agent" doesn't have
anything to do with AI, it's what it's called in the specification [[9.6
Agents](https://tc39.es/ecma262/#sec-agents)] so blame them. Also, `Reference.prototype.apply` has
been renamed to `invoke` because it was far too easy to confuse that method with the built-in
`Function.prototype.apply` even though they had wildly different behaviors.


References & Promises
---------------------

References from nodejs can no longer be passed to an agent. It just creates a big mess because
garbage collection is split between agents.

Promises are no longer *transferrable*. Always, this feature created problems with execution limits in an isolate:

```ts
import ivm from 'isolated-vm';
const now = Date.now();
console.log('start');
const isolate = new ivm.Isolate({ memoryLimit: 128 });
const context = await isolate.createContext();
await context.eval('new Promise(resolve=>{})', { timeout: 100, promise: true });
console.log(Date.now() - now);
```

What should the output of this program be? 100ms or so? Nope!
```
$ node example.mjs
start
Warning: Detected unsettled top-level await at file:///example.mjs:6
await context.eval('new Promise(resolve=>{})', { timeout: 100, promise: true });
^
```

The main problem, and there are other problems, is that promises and asynchronous operations are a
contract between the *capability* (resolver) and the *reaction* (deferred, promise, thenable). By
transferring a promise from untrusted code into your application you have given that code control
over some part of your application.


Errors
------

Functions which interact with client code will return a *completion record*. If your user's code
throws an error then the error will just be a property on the returned object. This creates a clear
line between "you messed up" and "your user messed up".

Probably, you've written code like this. I know I have. It is ridiculous:
```ts
try {
	// [...]
} catch (error: any) {
	if (
		error.message === 'Isolate is disposed' ||
		error.message === 'Isolate was disposed during execution' ||
		error.message === 'Isolate was disposed during execution due to memory limit'
	) {
		return undefined;
	} else {
		throw error;
	}
}
```

Now it would be something closer to this:
```ts
const completion = await script.run(realm);
if (completion?.complete) {
	return completion.result;
} else {
	// Handle the error: `completion.error`
}
```


Runtime
-------

A stronger concept of *capabilities* in `@isolated-vm/experimental` opens the door for built-in
support of certain DOM and other "web" APIs. The test suite includes an example of the `setTimeout`
family of functions:
[test/50.timers.ts](https://github.com/laverdet/isolated-vm/blob/experimental/packages/isolated-vm/test/50.timers.ts).
A longer-term goal is some measure of compatibility with the ["minimum common web
API"](https://min-common-api.proposal.wintertc.org) as specified by WinterTC.
