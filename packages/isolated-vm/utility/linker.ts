import type { AbstractModule, Agent, Module } from "@isolated-vm/experimental";
import * as fs from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { mappedComparator, selectKey, stringComparator } from "./comparator.js";

type LoaderAsync<Type extends AbstractModule | undefined> = (path: string, attributes?: Module.ImportAttributes) => Promise<Type>;
type LoaderSync<Type extends AbstractModule | undefined> = (path: string, attributes?: Module.ImportAttributes) => Type;
type AnyLoader<Type extends AbstractModule | undefined> = LoaderAsync<Type> | LoaderSync<Type>;
type ResolverSync<Type extends string | undefined> = (specifier: string, referrer?: string, attributes?: Module.ImportAttributes) => Type;
type ResolverAsync<Type extends string | undefined> = (specifier: string, referrer?: string, attributes?: Module.ImportAttributes) => Promise<Type>;
type AnyResolver<Type extends string | undefined> = ResolverAsync<Type> | ResolverSync<Type>;

function cacheKeyFor(name: string, attributes: Module.ImportAttributes | undefined): string {
	const sortedAttributes = Object.entries(attributes ?? {}).sort(mappedComparator(stringComparator, selectKey));
	return `${name}#${JSON.stringify(sortedAttributes)}`;
}

export function makeCompositeLinker(...linkers: Module.Linker[]): Module.Linker {
	return async (specifier, origin, attributes) => {
		for (const linker of linkers) {
			const module = await linker(specifier, origin, attributes);
			if (module) {
				return module;
			}
		}
	};
}

export function makeLinker<Type extends AbstractModule | undefined>(resolver: AnyResolver<string>, loader: AnyLoader<Type>): Module.Linker {
	return async (specifier, origin, attributes) => {
		const path = await resolver(specifier, origin, attributes);
		return loader(path, attributes);
	};
}

export function makeDirectResolver(): ResolverSync<string> {
	return specifier => specifier;
}

export function makeLocalResolver(meta: ImportMeta): ResolverSync<string> {
	return (specifier, referrer) => meta.resolve(specifier, referrer);
}

export function makeCachedLoader<Type extends AbstractModule | undefined>(loader: LoaderAsync<Type>): LoaderAsync<Type> {
	const modules = new Map<string, Promise<Type>>();
	return (name, attributes) => {
		const cacheKey = cacheKeyFor(name, attributes);
		const cached = modules.get(cacheKey);
		if (cached) {
			return cached;
		}
		const module = function() {
			try {
				return loader(name, attributes);
			} catch (error) {
				// eslint-disable-next-line @typescript-eslint/prefer-promise-reject-errors
				return Promise.reject(error);
			}
		}();
		modules.set(cacheKey, module);
		return module;
	};
}

export function makeFileSystemCompilationLoader(agent: Agent): LoaderAsync<Module> {
	return makeCachedLoader(async path => {
		const origin = { name: path };
		const sourceText = await fs.readFile(fileURLToPath(path), "utf8");
		const result = await agent.compileModule(sourceText, { origin });
		if (result?.complete) {
			return result.result;
		} else {
			throw new SyntaxError(`Module compilation failed for '${path}'`, { cause: result?.error });
		}
	});
}

export function makeStaticLoader(pathsToModules: Record<string, AbstractModule>): LoaderSync<AbstractModule | undefined> {
	const modules = new Map(Object.entries(pathsToModules));
	return specifier => modules.get(specifier);
}
