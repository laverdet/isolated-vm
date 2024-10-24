import type { SourceOrigin } from "./script.js";
import * as ivm from "./backend.js";
import { compileModule, compileScript, createAgent, createRealm } from "./backend.js";
import { Module } from "./module.js";
import { Realm } from "./realm.js";
import { Script } from "./script.js";

const { extractRealmInternal } = Realm;

export namespace Agent {
	export namespace CreateOptions {
		export type Clock = Clock.Deterministic | Clock.Microtask | Clock.Realtime | Clock.System;
		export namespace Clock {
			export interface Deterministic {
				type: "deterministic";
				epoch: Date;
				interval: number;
			}

			export interface Microtask {
				type: "microtask";
				epoch?: Date | undefined;
			}

			export interface Realtime {
				type: "realtime";
				epoch: Date;
			}

			export interface System {
				type: "system";
			}
		}
	}

	export interface CreateOptions {
		clock?: CreateOptions.Clock | undefined;
		randomSeed?: number | undefined;
	}

	export interface CompileScriptOptions {
		origin?: SourceOrigin;
	}
}

export class Agent {
	readonly #agent;

	/** @internal */
	private	constructor(agent: ivm.Agent) {
		this.#agent = agent;
	}

	static async create(options?: Agent.CreateOptions): Promise<Agent> {
		const agent = await createAgent(options);
		return new Agent(agent);
	}

	async createRealm(): Promise<Realm> {
		const realm = await createRealm(this.#agent);
		return new Realm(realm);
	}

	async compileModule(realm: Realm, code: string): Promise<unknown> {
		const module = await compileModule(this.#agent, extractRealmInternal(realm), code);
		return module;
		// return new Module(this.#agent, module);
	}

	async compileScript(code: string, options?: Agent.CompileScriptOptions): Promise<Script> {
		const script = await compileScript(this.#agent, code, options);
		return new Script(this.#agent, script);
	}

	// eslint-disable-next-line @typescript-eslint/require-await
	async dispose(): Promise<boolean> {
		// :)
		return true;
	}

	async [Symbol.asyncDispose](): Promise<void> {
		await this.dispose();
	}
}
