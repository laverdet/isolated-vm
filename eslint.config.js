// @ts-check
import process from "node:process";
import js from "@eslint/js";
import stylisticPlugin from "@stylistic/eslint-plugin";
import typeScriptPlugin from "@typescript-eslint/eslint-plugin";
import typeScriptParser from "@typescript-eslint/parser";
// @ts-expect-error
import importPlugin from "eslint-plugin-import";

// @ts-expect-error
const nodeModules = Object.keys(process.binding("natives")).filter(name => !/^(?:_|internal\/)/.test(name));

/** @typedef {import("eslint").Linter.Config} Config */
/** @typedef {import("eslint").Linter.RuleEntry} RuleEntry */
/** @typedef {import("eslint").Linter.RulesRecord} RulesRecord */

/** @type {RulesRecord} */
const javaScriptRules = {
	// Handled by TypeScript, obviously
	"no-undef": "off",

	// Generic good practices and style
	"arrow-body-style": "warn",
	"array-callback-return": [ "warn", { allowVoid: true } ],
	"block-scoped-var": "warn",
	curly: [ "warn", "multi-line", "consistent" ],
	"dot-notation": "warn",
	eqeqeq: [ "warn", "smart" ],
	"func-name-matching": "warn",
	"id-denylist": [ "warn", "xxx", "foo", "bar" ],
	"logical-assignment-operators": [ "warn", "always", { enforceForIfStatements: true } ],
	"no-array-constructor": "warn",
	"no-caller": "warn",
	"no-cond-assign": [ "warn", "except-parens" ],
	"no-constant-binary-expression": "warn",
	"no-constant-condition": [ "warn", { checkLoops: false } ],
	"no-constructor-return": "error",
	"no-control-regex": "off",
	"no-empty": [ "warn", { allowEmptyCatch: true } ],
	"no-eval": "warn",
	"no-extend-native": "warn",
	"no-extra-bind": "warn",
	"no-extra-label": "warn",
	"no-implied-eval": "warn",
	"no-iterator": "warn",
	"no-label-var": "warn",
	"no-labels": [ "warn", { allowLoop: true, allowSwitch: true } ],
	"no-lone-blocks": "warn",
	"no-lonely-if": "warn",
	"no-multi-str": "warn",
	"no-native-reassign": "warn",
	"no-negated-condition": "warn",
	"no-negated-in-lhs": "warn",
	"no-new-func": "warn",
	"no-new-wrappers": "warn",
	"no-new": "warn",
	"no-object-constructor": "warn",
	"no-octal-escape": "warn",
	"no-param-reassign": "warn",
	"no-promise-executor-return": [ "warn", { allowVoid: true } ],
	"no-restricted-imports": [ "warn", {
		paths: [
			...nodeModules.map(name => ({
				name,
				message: `Please use 'node:${name}' instead.`,
			})),
		],
	} ],
	"no-script-url": "warn",
	"no-self-compare": "warn",
	"no-sequences": "warn",
	"no-template-curly-in-string": "warn",
	"no-throw-literal": "warn",
	"no-undef-init": "warn",
	"no-unreachable-loop": "warn",
	"no-unused-expressions": [ "warn", {
		allowShortCircuit: true,
		allowTernary: true,
		allowTaggedTemplates: true,
	} ],
	"no-unused-vars": [ "warn", {
		argsIgnorePattern: "^_",
		caughtErrors: "all",
		ignoreRestSiblings: true,
	} ],
	"no-use-before-define": [ "warn", {
		functions: false,
		classes: false,
		variables: false,
	} ],
	"no-useless-call": "warn",
	"no-useless-computed-key": "warn",
	"no-useless-concat": "warn",
	"no-useless-constructor": "warn",
	"no-useless-rename": [ "warn", {
		ignoreDestructuring: false,
		ignoreImport: false,
		ignoreExport: false,
	} ],
	"no-useless-return": "warn",
	"object-shorthand": [ "warn", "always" ],
	"operator-assignment": "warn",
	"prefer-arrow-callback": [ "warn", { allowNamedFunctions: true } ],
	"prefer-const": [ "warn", { destructuring: "all" } ],
	"prefer-destructuring": [ "warn", { array: false } ],
	"prefer-exponentiation-operator": "warn",
	"prefer-numeric-literals": "warn",
	"prefer-object-spread": "warn",
	"prefer-promise-reject-errors": "warn",
	"prefer-regex-literals": "warn",
	radix: "warn",
	"sort-imports": [ "warn", { ignoreDeclarationSort: true } ],
	strict: [ "warn", "never" ],
	"symbol-description": "warn",
	yoda: "warn",
	"unicode-bom": [ "warn", "never" ],

	// https://github.com/benmosher/eslint-plugin-import/tree/master/docs/rules
	"import/extensions": [ "warn", "ignorePackages" ],
	"import/first": "warn",
	"import/newline-after-import": "warn",
	"import/no-anonymous-default-export": "warn",
	// Same as `no-duplicate-imports`, but offers an autofix
	"import/no-duplicates": "warn",
	"import/order": [ "warn", {
		alphabetize: {
			order: "asc",
		},
		groups: [
			"type",
			"builtin",
			"external",
			"internal",
			"parent",
			"sibling",
			"index",
		],
		pathGroups: [ { pattern: "#*", group: "internal" } ],
		pathGroupsExcludedImportTypes: [ "type" ],
	} ],
};

/** @type {RulesRecord} */
const formattingRules = {
	"no-extra-semi": "off",

	"@stylistic/array-bracket-newline": [ "warn", "consistent" ],
	"@stylistic/array-bracket-spacing": [ "warn", "always" ],
	"@stylistic/arrow-parens": [ "warn", "as-needed" ],
	"@stylistic/arrow-spacing": "warn",
	"@stylistic/block-spacing": "warn",
	"@stylistic/brace-style": [ "warn", "1tbs", { allowSingleLine: true } ],
	"@stylistic/comma-dangle": [ "warn", "always-multiline" ],
	"@stylistic/comma-spacing": "warn",
	"@stylistic/comma-style": "warn",
	"@stylistic/computed-property-spacing": "warn",
	"@stylistic/dot-location": [ "warn", "property" ],
	"@stylistic/eol-last": "warn",
	"@stylistic/func-call-spacing": "warn",
	"@stylistic/generator-star-spacing": [ "warn", { anonymous: "neither", before: true, after: false } ],
	"@stylistic/indent": [ "warn", "tab", {
		SwitchCase: 1,
		flatTernaryExpressions: true,
		ignoredNodes: [
			"CallExpression[typeParameters]",
			"TSTypeAnnotation",
			"TSTypeParameterInstantiation",
		],
	} ],
	"@stylistic/key-spacing": [ "warn", { mode: "strict" } ],
	"@stylistic/keyword-spacing": "warn",
	"@stylistic/linebreak-style": "warn",
	"@stylistic/lines-between-class-members": [ "warn", "always", { exceptAfterSingleLine: true } ],
	"@stylistic/member-delimiter-style": "warn",
	"@stylistic/new-parens": "warn",
	"@stylistic/no-extra-parens": [ "warn", "all", {
		ignoreJSX: "all",
		nestedBinaryExpressions: false,
	} ],
	"@stylistic/no-extra-semi": "warn",
	"@stylistic/no-floating-decimal": "warn",
	"@stylistic/no-mixed-operators": [ "warn", {
		allowSamePrecedence: false,
		groups: [
			[ "&", "|", "^", "~", "<<", ">>", ">>>" ],
			[ "==", "!=", "===", "!==", ">", ">=", "<", "<=" ],
			[ "&&", "||" ],
			[ "in", "instanceof" ],
		],
	} ],
	"@stylistic/no-multi-spaces": "warn",
	"@stylistic/no-multiple-empty-lines": [ "warn", { max: 1, maxEOF: 0 } ],
	"@stylistic/no-trailing-spaces": "warn",
	"@stylistic/no-whitespace-before-property": "warn",
	"@stylistic/object-curly-newline": [ "warn", { consistent: true } ],
	"@stylistic/object-curly-spacing": [ "warn", "always" ],
	"@stylistic/operator-linebreak": [ "warn", "after", {
		overrides: {
			"?": "before",
			":": "ignore",
			"||": "ignore",
			"&&": "ignore",
		},
	} ],
	"@stylistic/padding-line-between-statements": "warn",
	"@stylistic/quote-props": [ "warn", "as-needed" ],
	"@stylistic/quotes": [ "warn", "double", { avoidEscape: true } ],
	"@stylistic/rest-spread-spacing": [ "warn", "never" ],
	"@stylistic/semi": [ "warn", "always" ],
	"@stylistic/semi-spacing": "warn",
	"@stylistic/semi-style": "warn",
	"@stylistic/space-before-blocks": "warn",
	"@stylistic/space-before-function-paren": [ "warn", {
		anonymous: "never",
		named: "never",
	} ],
	"@stylistic/space-in-parens": "warn",
	"@stylistic/space-infix-ops": "warn",
	"@stylistic/space-unary-ops": "warn",
	"@stylistic/switch-colon-spacing": "warn",
	"@stylistic/template-curly-spacing": "warn",
	"@stylistic/template-tag-spacing": "warn",
	"@stylistic/yield-star-spacing": "warn",

	// TypeScript
	"@stylistic/type-annotation-spacing": "warn",
	"@stylistic/type-generic-spacing": "warn",
	"@stylistic/type-named-tuple-spacing": "warn",
};

/** @type {RulesRecord} */
const typeScriptRules = acceptTypeScriptRules({
	// These functions have sufficient type checks
	"array-callback-return": "off",
	// Obviated by ts(2845)
	"use-isnan": "off",
	// TypeScript makes runtime prop checking obsolete
	"react/prop-types": "off",
	// This shouldn't trigger in `.d.ts` files
	"no-unused-private-class-members": "off",
	// Handle for grouping types (and only types!!)
	"@typescript-eslint/no-namespace": "off",

	// TypeScript rules which supersede an eslint rule
	"@typescript-eslint/naming-convention": [ "warn", {
		format: [ "PascalCase" ],
		selector: "typeLike",
	} ],
	"@typescript-eslint/no-unused-expressions": [ "warn", {
		allowShortCircuit: true,
		allowTernary: true,
		allowTaggedTemplates: true,
	} ],
	"@typescript-eslint/no-unused-vars": [ "warn", {
		argsIgnorePattern: "^_",
		caughtErrors: "all",
		ignoreRestSiblings: true,
	} ],
	"@typescript-eslint/no-use-before-define": [ "warn", {
		classes: false,
		functions: false,
		variables: false,
	} ],

	// TypeScript-exclusive
	"@typescript-eslint/array-type": "warn",
	"@typescript-eslint/ban-ts-comment": "off",
	"@typescript-eslint/class-literal-property-style": "warn",
	"@typescript-eslint/consistent-generic-constructors": "warn",
	"@typescript-eslint/consistent-indexed-object-style": "warn",
	"@typescript-eslint/consistent-type-assertions": "warn",
	"@typescript-eslint/explicit-member-accessibility": [ "warn", {
		accessibility: "no-public",
		overrides: { parameterProperties: "explicit" },
	} ],
	"@typescript-eslint/explicit-module-boundary-types": "off",
	// nb: This is not simply a style rule, as the recommended style enforces contravariance instead
	// of bivariance.
	// https://github.com/microsoft/TypeScript/pull/18654
	"@typescript-eslint/member-ordering": "warn",
	"@typescript-eslint/method-signature-style": "warn",
	"@typescript-eslint/no-confusing-non-null-assertion": "warn",
	"@typescript-eslint/no-empty-function": "off",
	"@typescript-eslint/no-explicit-any": "off",
	"@typescript-eslint/no-extraneous-class": "warn",
	"@typescript-eslint/no-invalid-void-type": [ "warn", { allowAsThisParameter: true } ],
	"@typescript-eslint/no-non-null-asserted-nullish-coalescing": "warn",
	"@typescript-eslint/no-unsafe-unary-minus": "warn",
	"@typescript-eslint/no-useless-constructor": "warn",
	"@typescript-eslint/no-useless-empty-export": "warn",
	"@typescript-eslint/parameter-properties": "warn",
	"@typescript-eslint/prefer-enum-initializers": "warn",
	"@typescript-eslint/prefer-for-of": "warn",
	"@typescript-eslint/prefer-function-type": "warn",
	"@typescript-eslint/prefer-literal-enum-member": "warn",
	// I would love to use this but boy is it lousy
	"@typescript-eslint/prefer-readonly-parameter-types": "off",
	"@typescript-eslint/prefer-ts-expect-error": "warn",
	"@typescript-eslint/sort-type-constituents": "warn",
	"@typescript-eslint/unified-signatures": "warn",

	"import/consistent-type-specifier-style": [ "warn", "prefer-top-level" ],
});

/** @type {RulesRecord} */
const typedTypeScriptRules = acceptTypeScriptRules({
	// Prevents fixing `@typescript-eslint/strict-boolean-expressions` errors with `!Boolean(...)`.
	"no-extra-boolean-cast": "off",
	// Re-enabled for non-d.ts files
	"no-unused-private-class-members": "warn",

	"@typescript-eslint/consistent-type-exports": "warn",
	"@typescript-eslint/dot-notation": "warn",
	"@typescript-eslint/no-confusing-void-expression": [ "warn", {
		ignoreArrowShorthand: true,
		ignoreVoidOperator: true,
	} ],
	"@typescript-eslint/no-inferrable-types": "warn",
	"@typescript-eslint/no-meaningless-void-operator": "warn",
	// `checksConditionals` - Conditionals are handled by ts(2801)
	"@typescript-eslint/no-misused-promises": [ "warn", { checksConditionals: false } ],
	"@typescript-eslint/no-mixed-enums": "warn",
	"@typescript-eslint/no-unnecessary-boolean-literal-compare": "warn",
	"@typescript-eslint/no-unnecessary-condition": [ "warn", { allowConstantLoopConditions: true } ],
	"@typescript-eslint/no-unnecessary-qualifier": "warn",
	"@typescript-eslint/no-unnecessary-template-expression": "warn",
	"@typescript-eslint/no-unnecessary-type-arguments": "warn",
	"@typescript-eslint/no-unsafe-argument": "warn",
	"@typescript-eslint/no-unsafe-assignment": "warn",
	"@typescript-eslint/no-unsafe-call": "warn",
	"@typescript-eslint/no-unsafe-enum-comparison": "warn",
	"@typescript-eslint/no-unsafe-member-access": "warn",
	"@typescript-eslint/no-unsafe-return": "warn",
	"@typescript-eslint/non-nullable-type-assertion-style": "warn",
	"@typescript-eslint/prefer-find": "warn",
	"@typescript-eslint/prefer-includes": "warn",
	"@typescript-eslint/prefer-nullish-coalescing": "warn",
	"@typescript-eslint/prefer-optional-chain": "warn",
	"@typescript-eslint/prefer-readonly": "warn",
	"@typescript-eslint/prefer-reduce-type-parameter": "warn",
	"@typescript-eslint/prefer-regexp-exec": "warn",
	"@typescript-eslint/prefer-return-this-type": "warn",
	"@typescript-eslint/prefer-string-starts-ends-with": "warn",
	"@typescript-eslint/require-array-sort-compare": "warn",
	"@typescript-eslint/restrict-template-expressions": [ "warn", {
		allowAny: true,
		allowNever: true,
		allowNullish: true,
		allowNumber: true,
	} ],
	"@typescript-eslint/return-await": "warn",
	"@typescript-eslint/strict-boolean-expressions": [ "warn", {
		// [default: true] - Documentation agrees these are safe. One actually might consider
		// *disabling* these even though they're safe.
		allowString: true,
		allowNumber: true,
		// [default: true] - Super duper safe.
		allowNullableObject: true,
		// [default: false] - "Unsafe" per the documentation, but seems legit in practice.
		allowNullableBoolean: true,
		// [default: false] - Ok if you are already walking with the devil then eslint can't help
		// you.
		allowAny: true,
	} ],
	"@typescript-eslint/switch-exhaustiveness-check": "warn",
});

// Rules inherited from plugins will be checked against this list. If they do *not* appear in the
// list they will be downgraded from "error" to "warn".
const allowedErrors = [
	"@typescript-eslint/no-redeclare",
	"constructor-super",
	"for-direction",
	"no-async-promise-executor",
	"no-class-assign",
	"no-compare-neg-zero",
	"no-const-assign",
	"no-constructor-return",
	"no-dupe-args",
	"no-dupe-class-members",
	"no-dupe-else-if",
	"no-duplicate-case",
	"no-func-assign",
	"no-import-assign",
	"no-inner-declarations",
	"no-invalid-regexp",
	"no-misleading-character-class",
	"no-nonoctal-decimal-escape",
	"no-obj-calls",
	"no-redeclare",
	"no-template-curly-in-string",
	"no-this-before-super",
	"no-unsafe-finally",
	"no-unsafe-negation",
	"unicode-bom",
	"use-isnan",
	"valid-typeof",
];

const jsGlob = [ "**/*.js", "**/*.cjs", "**/*.mjs" ];
const tsGlob = [ "**/*.ts", "**/*.cts", "**/*.mts" ];
const dTsGlob = [ "**/*.d.ts", "**/*.d.cts", "**/*.d.mts", "**/*.d.*.ts", "**/*.d.*.cts", "**/*.d.*.mts" ];
const everythingGlob = [ ...jsGlob, ...tsGlob ];

/** @type {Config[]} */
// eslint-disable-next-line import/no-anonymous-default-export
export default [
	// https://eslint.org/docs/latest/use/configure/configuration-files-new#globally-ignoring-files-with-ignores
	// "If `ignores` is used without any other keys in the configuration object, then the
	// patterns act as global ignores.
	{
		ignores: [
			"**/dist",
			"**/node_modules",
		],
	},

	// Settings
	{
		files: everythingGlob,
		languageOptions: {
			ecmaVersion: "latest",
			parserOptions: {
				sourceType: "module",
			},
		},
		plugins: {
			"@stylistic": stylisticPlugin,
			import: importPlugin,
		},
	},

	// TypeScript settings
	{
		files: [ ...tsGlob ],
		plugins: {
			"@typescript-eslint": /** @type {any} */(typeScriptPlugin),
		},
		languageOptions: {
			parser: typeScriptParser,
			parserOptions: {
				project: "./tsconfig.json",
			},
		},
	},

	// JavaScript rules
	{
		rules: {
			...acceptRecommended(js.configs.recommended.rules),
			...formattingRules,
			...javaScriptRules,
			// nb: Breaks JSDoc casts
			"@stylistic/no-extra-parens": "off",
		},
	},

	// TypeScript rules
	{
		files: tsGlob,
		rules: {
			// @ts-expect-error
			...acceptRecommended(typeScriptPlugin.configs["eslint-recommended"].overrides[0].rules),
			// @ts-expect-error
			...acceptRecommended(typeScriptPlugin.configs.recommended.rules),
			...formattingRules,
			...typeScriptRules,
		},
	},
	{
		files: tsGlob,
		ignores: dTsGlob,
		rules: {
			// @ts-expect-error
			...acceptRecommended(typeScriptPlugin.configs["recommended-requiring-type-checking"].rules),
			...typeScriptRules,
			...typedTypeScriptRules,
		},
	},
];

/**
 * Downgrade severity of "error" rules unless specified in `errors`.
 * @param {RulesRecord} rules
 * @returns {RulesRecord}
 */
function acceptRecommended(rules) {
	return Object.fromEntries(Object.entries(rules).map(([ name, entry ]) => {
		/** @returns {RuleEntry} */
		const select = () => {
			if (typeof entry === "string" || typeof entry === "number") {
				return entry === "error" || entry === 2 ? "warn" : entry;
			} else if (entry[0] === "error" || entry[0] === 2) {
				return [ "warn", ...entry.slice(1) ];
			} else {
				return entry;
			}
		};
		return [ name, allowedErrors.includes(name) ? entry : select() ];
	}));
}

/**
 * Disable built-in rules that are replaced by TypeScript-specific rules.
 * @param {RulesRecord} rules
 * @returns {RulesRecord}
 */
function acceptTypeScriptRules(rules) {
	const builtInRules = new Set(Object.keys(js.configs.all.rules));
	return Object.fromEntries(Object.entries(rules).flatMap(([ name, entry ]) => {
		const [ , plainName ] = /^@typescript-eslint\/(.+)/.exec(name) ?? [];
		return [
			...plainName !== undefined && builtInRules.has(plainName) ? [ [ plainName, "off" ] ] : [],
			...plainName !== undefined && builtInRules.has(`no-${plainName}`) ? [ [ `no-${plainName}`, "off" ] ] : [],
			[ name, entry ],
		];
	}));
}
