[![npm version](https://img.shields.io/npm/v/@isolated-vm/experimental)](https://www.npmjs.com/package/@isolated-vm/experimental)
[![github action](https://github.com/laverdet/isolated-vm/actions/workflows/build-test-release.yml/badge.svg)](https://github.com/laverdet/isolated-vm/actions/workflows/build-test-release.yml)
[![isc license](https://img.shields.io/npm/l/@isolated-vm/experimental)](https://github.com/laverdet/isolated-vm/blob/experimental/LICENSE)
[![npm downloads](https://img.shields.io/npm/dm/@isolated-vm/experimental)](https://www.npmjs.com/package/@isolated-vm/experimental)

`@isolated-vm/experimental` -- A new `isolated-vm`
==================================================

This is an early release of a from-scratch rewrite of `isolated-vm`. Development is taking place in
the [`experimental` branch](https://github.com/laverdet/isolated-vm/tree/experimental) of this
repository. Periodic releases are published on npm at
[`@isolated-vm/experimental`](https://www.npmjs.com/package/@isolated-vm/experimental). Some
explanation of the reasoning is dumped at [docs/LEGACY.md](./docs/LEGACY.md). There are still a
bunch of missing features but the foundation is solid.

See also
[@auto_js/napi](https://github.com/laverdet/isolated-vm/tree/experimental/packages/auto_js/napi),
which lives in this branch in `packages`. It's an abstraction of the JavaScript to C++ marshalling I
built to support development of this module. That part is probably like 80% of the work that went
into this whole thing. If you have an interest in writing C++ which interfaces with JavaScript it
might be interesting to you.

```ts
// $ cat example.mts
import { Agent, expectComplete } from "@isolated-vm/experimental";
import { makeDirectResolver, makeLinker, makeStaticLoader } from "@isolated-vm/experimental/utility/linker";

const agent = await Agent.create();
const realm = await agent.createRealm();
const log = await realm.createCapability(
	() => ({
		log(...args: unknown[]) { console.log(">", ...args); },
	}),
	{ origin: "example:log" },
);
const module = expectComplete(await agent.compileModule(`
	import { log } from "example:log";
	log("Hello from v8!");
`));
const linker = makeLinker(
	makeDirectResolver(),
	makeStaticLoader({ "example:log": log }),
);
await module.link(realm, linker);
expectComplete(await module.evaluate(realm));

// $ node example.mts
// > Hello from v8!

// $ bun example.mts
// > Hello from v8!
// Note: you will need bun canary installed, v1.3.14 will not work because of a broken API.
// `bun upgrade --canary`
```


Sponsorship
-----------

[![GitHub Sponsors](https://img.shields.io/github/sponsors/laverdet?logo=githubsponsors&color=ec6cb9)](https://github.com/sponsors/laverdet)

Please sponsor this project if you feel like it.
