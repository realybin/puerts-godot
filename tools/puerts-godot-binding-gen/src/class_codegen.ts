// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

import {normalizeOperators} from "../../puerts-godot-operator-model/index.js";

import {
	sanitizeIdentifier,
	toSnakeCase
} from "./text_utils.js";
import {
	mapApiMethodReturnType,
	mapCtorType,
	mapMethodArgType,
	mapMethodReturnType,
	methodPointerExprFromApi
} from "./type_mapping.js";
import type {
	ApiBuiltinClass,
	ApiClassLike,
	ApiConstant,
	ApiMethod,
	BoundPropertyBinding,
	ClassSource,
	GeneratedClassBinding,
	GeneratedEnumBinding,
	HeaderAnalysis,
} from "./types.js";

function enumTagStructName(className: string, enumName: string): string {
	return `${sanitizeIdentifier(toSnakeCase(className))}_${sanitizeIdentifier(toSnakeCase(enumName))}_enum_tag`;
}

function enumRegisterFunctionName(className: string, enumName: string): string {
	return `register_enum_${sanitizeIdentifier(toSnakeCase(className))}_${sanitizeIdentifier(toSnakeCase(enumName))}`;
}

function signalNameSymbol(className: string, signalName: string): string {
	return `${sanitizeIdentifier(toSnakeCase(className))}_${sanitizeIdentifier(toSnakeCase(signalName))}_signal_name`;
}

function isBuiltinClass(cls: ApiClassLike): cls is ApiBuiltinClass {
	return "operators" in cls;
}

function collectEnumConstantNames(cls: ApiClassLike): Set<string> {
	return new Set((cls.enums ?? []).flatMap((entry) => (entry.values ?? []).map((value) => value.name)));
}

function builtinConstantExpression(value: string): string {
	return value
		.trim()
		.replace(/^[A-Za-z_][A-Za-z0-9_]*\(/, "{ ")
		.replace(/\)$/, " }")
		.replace(/-2147483648\b/g, "INT32_MIN")
		.replace(/\b2147483647\b/g, "INT32_MAX")
		.replace(/\binf\b/g, "INFINITY");
}

function constantExpression(constant: ApiConstant, classSource: ClassSource): string {
	if (typeof constant.value === "number") {
		return String(constant.value);
	}
	return classSource === "builtin" ? builtinConstantExpression(constant.value) : constant.value.trim();
}

// Integer constants need no generated helper.
function constantLiteral(constant: ApiConstant): string | null {
	if (typeof constant.value === "number") {
		return String(Math.trunc(constant.value));
	}
	const text = constant.value.trim();
	return /^-?\d+$/.test(text) ? text : null;
}

function pushConstantBindings(
	cls: ApiClassLike,
	classSource: ClassSource,
	bindingLines: string[],
): string[] {
	const helpers: string[] = [];
	const enumConstantNames = collectEnumConstantNames(cls);
	const classKey = sanitizeIdentifier(toSnakeCase(cls.name));

	for (const constant of cls.constants ?? []) {
		if (enumConstantNames.has(constant.name)) {
			continue;
		}

		const literal = constantLiteral(constant);
		if (literal !== null) {
			bindingLines.push(`\t\t\t.constant<${literal}>("${constant.name}")`);
			continue;
		}

		const constantKey = sanitizeIdentifier(toSnakeCase(constant.name));
		const getterName = `${classKey}_${constantKey}`;
		const returnType = constant.type ? mapMethodReturnType(constant.type) : "int64_t";
		helpers.push(
			`inline ${returnType} ${getterName}() {\n` +
			`\treturn ${constantExpression(constant, classSource)};\n` +
			`}`,
		);
		bindingLines.push(`\t\t\t.constant<&puerts_generated::${getterName}>("${constant.name}")`);
	}

	return helpers;
}

function parameterDeclaration(type: string, name: string): string {
	return `${type}${/[&*]\s*$/.test(type) ? "" : " "}${name}`;
}

const EXPLICIT_BUILTIN_DEFAULT_ARGUMENTS = new Map<string, { type: string; value: string; expression: string }>([
	["Transform2D.looking_at", { type: "Vector2", value: "Vector2(0, 0)", expression: "godot::Vector2()" }],
	["Dictionary.get", { type: "Variant", value: "null", expression: "godot::Variant()" }],
	["Dictionary.get_or_add", { type: "Variant", value: "null", expression: "godot::Variant()" }],
	["Array.reduce", { type: "Variant", value: "null", expression: "godot::Variant()" }],
]);

function firstDefaultArgumentIndex(className: string, method: ApiMethod): number | null {
	const args = method.arguments ?? [];
	const firstDefault = args.findIndex((arg) => arg.default_value !== undefined);
	if (firstDefault < 0) {
		return null;
	}
	if (args.slice(firstDefault).some((arg) => arg.default_value === undefined)) {
		throw new Error(`Default arguments must be trailing for ${className}.${method.name}.`);
	}
	return firstDefault;
}

function explicitBuiltinDefaultArgument(
	className: string,
	classSource: ClassSource,
	method: ApiMethod,
): string | null {
	const key = `${className}.${method.name}`;
	const spec = EXPLICIT_BUILTIN_DEFAULT_ARGUMENTS.get(key);
	if (classSource !== "builtin" || !spec) {
		return null;
	}

	const args = method.arguments ?? [];
	const last = args.at(-1);
	if (!last || last.type !== spec.type || last.default_value !== spec.value) {
		throw new Error(`Unexpected explicit default argument metadata for ${key}.`);
	}
	return spec.expression;
}

function defaultMethodHelperName(className: string, methodName: string, arity: number): string {
	return `${sanitizeIdentifier(toSnakeCase(className))}_${sanitizeIdentifier(toSnakeCase(methodName))}_default${arity}`;
}

function buildDefaultMethodHelpers(
	className: string,
	classSource: ClassSource,
	method: ApiMethod,
	firstDefault: number,
): { helpers: string[]; callables: string[] } {
	const args = method.arguments ?? [];
	const helpers: string[] = [];
	const callables: string[] = [];
	const explicitDefault = explicitBuiltinDefaultArgument(className, classSource, method);

	for (let arity = firstDefault; arity < args.length; arity++) {
		const helperName = defaultMethodHelperName(className, method.name, arity);
		const declarations = args
			.slice(0, arity)
			.map((arg, index) => parameterDeclaration(mapMethodArgType(arg.type), `p_arg_${index}`));
		const invocationArgs = args.slice(0, arity).map((_, index) => `p_arg_${index}`);
		if (explicitDefault) {
			invocationArgs.push(explicitDefault);
		}

		let invocation: string;
		if (method.is_static) {
			invocation = `godot::${className}::${method.name}(${invocationArgs.join(", ")})`;
		} else {
			const receiverType = `${method.is_const ? "const " : ""}godot::${className} &`;
			declarations.unshift(parameterDeclaration(receiverType, "p_self"));
			invocation = `p_self.${method.name}(${invocationArgs.join(", ")})`;
		}
		callables.push(`&puerts_generated::${helperName}`);

		helpers.push(
			`inline decltype(auto) ${helperName}(${declarations.join(", ")}) {\n` +
			`\treturn ${invocation};\n` +
			`}`,
		);
	}

	return { helpers, callables };
}

function pushOperatorBindings(
	cls: ApiClassLike,
	bindingLines: string[],
): string[] {
	if (!isBuiltinClass(cls) || (cls.operators?.length ?? 0) === 0) {
		return [];
	}

	const className = cls.name;
	const helpers: string[] = [];
	const byScriptName = new Map<string, string[]>();
	const classKey = sanitizeIdentifier(toSnakeCase(className));

	normalizeOperators(className, cls.operators).forEach((operator, index) => {
		const wrapperName = `${classKey}_${sanitizeIdentifier(toSnakeCase(operator.scriptName))}_${index}`;
		const returnType = mapMethodReturnType(operator.returnType);
		const argDecl =
			operator.kind === "unary"
				? parameterDeclaration(mapMethodArgType(operator.leftType), "value")
				: `${parameterDeclaration(mapMethodArgType(operator.leftType), "left")}, ${parameterDeclaration(mapMethodArgType(operator.rightType ?? "Variant"), "right")}`;
		const invokeArgs = operator.kind === "unary" ? "value" : "left, right";
		helpers.push(
			`inline ${returnType} ${wrapperName}(${argDecl}) {\n` +
			`\treturn puerts::evaluate_operator<godot::Variant::${operator.variantEnum}, ${returnType}>(${invokeArgs});\n` +
			`}`,
		);
		const overloads = byScriptName.get(operator.scriptName) ?? [];
		overloads.push(wrapperName);
		byScriptName.set(operator.scriptName, overloads);
	});

	for (const [scriptName, overloads] of byScriptName) {
		const callables = overloads.map((name) => `&puerts_generated::${name}`).join(", ");
		bindingLines.push(`\t\t\t.static_method<${callables}>("${scriptName}")`);
	}

	return helpers;
}

export function generateEnumBindings(cls: ApiClassLike): GeneratedEnumBinding[] {
	const classEnums = cls.enums ?? [];
	const enumBindings: GeneratedEnumBinding[] = [];
	const seenEnumNames = new Set<string>();

	for (const classEnum of classEnums) {
		const enumName = classEnum.name?.trim();
		if (!enumName) {
			continue;
		}
		if (seenEnumNames.has(enumName)) {
			console.warn(`Warning: skipped duplicate enum for ${cls.name}.${enumName}.`);
			continue;
		}
		seenEnumNames.add(enumName);

		const enumValues = classEnum.values ?? [];
		if (enumValues.length === 0) {
			console.warn(`Warning: skipped enum without values for ${cls.name}.${enumName}.`);
			continue;
		}

		const tagStruct = enumTagStructName(cls.name, enumName);
		const tagType = `puerts_generated::${tagStruct}`;
		const functionName = enumRegisterFunctionName(cls.name, enumName);
		const constantLines = enumValues
			.map((v) => `\t\t\t.constant<${Math.trunc(v.value)}>("${v.name}")`)
			.join("\n");
		const code =
			`inline void ${functionName}() {\n` +
			`\tpuerts::define_type<${tagType}>()\n` +
			`${constantLines}\n` +
			`\t\t\t.register_type();\n` +
			`}\n`;

		enumBindings.push({
			className: cls.name,
			enumName,
			tagStructName: tagStruct,
			tagType,
			functionName,
			scriptTypeLine: `PUERTS_SCRIPT_TYPE(${tagType}, "${cls.name}.${enumName}")`,
			registerCall: `\t${functionName}();`,
			code,
		});
	}

	return enumBindings;
}

function pushMethodBindings(
	className: string,
	classSource: ClassSource,
	methods: ApiMethod[],
	headerAnalysis: HeaderAnalysis,
	bindingLines: string[],
): string[] {
	const helpers: string[] = [];
	const byName = new Map<string, ApiMethod[]>();
	const orderedNames: string[] = [];
	for (const method of methods) {
		if (!byName.has(method.name)) {
			byName.set(method.name, []);
			orderedNames.push(method.name);
		}
		byName.get(method.name)!.push(method);
	}

	for (const name of orderedNames) {
		const methodGroup = byName.get(name) ?? [];
		if (methodGroup.length === 0) {
			continue;
		}

		if (methodGroup.length === 1) {
			const method = methodGroup[0];
			const needsDisambiguation = (headerAnalysis.methodNameCounts.get(name) ?? 0) > 1;
			const callable = needsDisambiguation
				? methodPointerExprFromApi(className, method)
				: `&godot::${className}::${name}`;
			const firstDefault = firstDefaultArgumentIndex(className, method);
			if (firstDefault !== null) {
				if (needsDisambiguation) {
					throw new Error(`Default arguments on overloaded methods are unsupported for ${className}.${name}.`);
				}
				const generated = buildDefaultMethodHelpers(className, classSource, method, firstDefault);
				helpers.push(...generated.helpers);
				const callables = [...generated.callables, callable].join(", ");
				const bindingKind = method.is_static ? "static_method" : "method";
				bindingLines.push(`\t\t\t.${bindingKind}<${callables}>("${name}")`);
			} else {
				const bindingKind = method.is_static ? "static_method" : "method";
				bindingLines.push(`\t\t\t.${bindingKind}<${callable}>("${name}")`);
			}
			continue;
		}

		const staticMethods = methodGroup.filter((m) => !!m.is_static);
		const instanceMethods = methodGroup.filter((m) => !m.is_static);
		let selectedMethods = staticMethods.length > 0 ? staticMethods : instanceMethods;
		if (staticMethods.length > 0 && instanceMethods.length > 0) {
			const chosenKind = staticMethods.length >= instanceMethods.length ? "static" : "instance";
			selectedMethods = chosenKind === "static" ? staticMethods : instanceMethods;
			console.warn(`Warning: mixed static/instance overloads for ${className}.${name}, chose ${chosenKind} overloads.`);
		}
		const callables = selectedMethods.map((m) => methodPointerExprFromApi(className, m)).join(", ");
		const bindingKind = selectedMethods[0].is_static ? "static_method" : "method";
		bindingLines.push(`\t\t\t.${bindingKind}<${callables}>("${name}")`);
	}

	return helpers;
}

export function generateClassBinding(
	cls: ApiClassLike,
	classSource: ClassSource,
	methods: ApiMethod[],
	varargMethods: ApiMethod[],
	properties: BoundPropertyBinding[],
	enumBindings: GeneratedEnumBinding[],
	headerAnalysis: HeaderAnalysis,
): GeneratedClassBinding {
	const className = cls.name;
	const bindingLines: string[] = [];
	const signalNameDefinitions: string[] = [];
	const operatorHelpers = pushOperatorBindings(cls, bindingLines);
	const constantHelpers = pushConstantBindings(cls, classSource, bindingLines);
	const baseClassName = cls.inherits?.trim();

	if (baseClassName && baseClassName !== className) {
		bindingLines.push(`\t\t\t.extends<godot::${baseClassName}>()`);
	}

	const ctors = cls.constructors ?? [];
	if (ctors.length === 0 && cls.is_instantiable !== false) {
		bindingLines.push(`\t\t\t.constructor<>()`);
	}
	for (const ctor of ctors) {
		const args = (ctor.arguments ?? []).map((arg) => mapCtorType(arg.type)).join(", ");
		bindingLines.push(`\t\t\t.constructor<${args}>()`);
	}

	const methodHelpers = pushMethodBindings(className, classSource, methods, headerAnalysis, bindingLines);

	const emittedMethodNames = new Set<string>(methods.map((m) => m.name));
	const seenVarargNames = new Set<string>();
	for (const method of varargMethods) {
		if (seenVarargNames.has(method.name)) {
			continue;
		}
		seenVarargNames.add(method.name);

		if (emittedMethodNames.has(method.name)) {
			console.warn(`Warning: skipped vararg method with duplicate name for ${className}.${method.name}.`);
			continue;
		}
		if (method.is_static) {
			console.warn(`Warning: skipped static vararg method for ${className}.${method.name} (unsupported).`);
			continue;
		}

		const writeBack = method.is_const ? ", false" : "";
		bindingLines.push(
			`\t\t\t.vararg_method<${mapApiMethodReturnType(method)}${writeBack}>("${method.name}", ${(method.arguments ?? []).length})`,
		);
		emittedMethodNames.add(method.name);
	}

	for (const property of properties) {
		bindingLines.push(property.bindingLine);
	}

	const boundPropertyNames = new Set<string>(properties.map((p) => p.name));
	for (const enumBinding of enumBindings) {
		if (boundPropertyNames.has(enumBinding.enumName)) {
			console.warn(`Warning: skipped enum group property due to name collision for ${className}.${enumBinding.enumName}.`);
			continue;
		}
		bindingLines.push(`\t\t\t.enum_type<${enumBinding.tagType}>("${enumBinding.enumName}")`);
		boundPropertyNames.add(enumBinding.enumName);
	}

	const seenSignalNames = new Set<string>();
	for (const signal of cls.signals ?? []) {
		const signalName = signal.name?.trim();
		if (!signalName) {
			continue;
		}
		if (seenSignalNames.has(signalName)) {
			continue;
		}
		seenSignalNames.add(signalName);
		if (boundPropertyNames.has(signalName)) {
			console.warn(`Warning: skipped signal property due to name collision for ${className}.${signalName}.`);
			continue;
		}
		const signalSymbol = signalNameSymbol(className, signalName);
		signalNameDefinitions.push(`inline constexpr char ${signalSymbol}[] = "${signalName}";`);
		bindingLines.push(`\t\t\t.signal<puerts_generated::${signalSymbol}>("${signalName}")`);
		boundPropertyNames.add(signalName);
	}

	bindingLines.push("\t\t\t.register_type();");

	const fnName = `register_type_${sanitizeIdentifier(toSnakeCase(className))}`;
	const prefixLines = [...signalNameDefinitions, ...operatorHelpers, ...constantHelpers, ...methodHelpers];
	const prefix = prefixLines.length > 0
		? `namespace puerts_generated {\n${prefixLines.join("\n")}\n} // namespace puerts_generated\n\n`
		: "";
	const code = `${prefix}inline void ${fnName}() {\n\tpuerts::define_type<godot::${className}>()\n${bindingLines.join("\n")}\n}\n`;
	return {className, classSource, functionName: fnName, code};
}
