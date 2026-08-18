import type { Realm } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, expect } from "@isolated-vm/experimental";
import { expectComplete } from "@isolated-vm/experimental/test/fixtures";

type Subscriber = Parameters<Realm.CapabilityMake>[0];

const capabilityName = "isolated-vm:capability";

// A first agent warms the process up; the drained loop only manifests on a successor
await test("warm up", async () => {
	await using agent = await Agent.create();
	expect(await agent.createRealm());
});

// The subscription callback dispatches from the agent while the nodejs event loop is otherwise
// idle. The notification is lost if the capability channel does not keep the loop alive.
await test("notify with idle event loop", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	let notify: (value: unknown) => void;
	const notification = new Promise(resolve => {
		notify = resolve;
	});
	let subscriber: Subscriber;
	const capability = expect(await realm.createCapability(
		value => {
			subscriber = value;
			return { subscribe: value, notify };
		},
		{ origin: capabilityName }));
	const module = expectComplete(await agent.compileModule(`
		import { subscribe, notify } from ${JSON.stringify(capabilityName)};
		subscribe(message => notify(message));
	`));
	await module.link(realm, () => capability);
	await module.evaluate(realm);
	assert.equal(await subscriber!.send("hello"), true);
	assert.equal(await notification, "hello");
});
