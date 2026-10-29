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
	mapMethodReturnType
} from "./type_mapping.js";
import type {
	ApiBuiltinClass,
	ApiClassLike,
	ApiConstant,
	ApiMethod,
	MethodAdapter,
	BoundPropertyBinding,
	ClassSource,
	GeneratedClassBinding,
	GeneratedEnumBinding,
} from "./types.js";

function enumRegisterFunctionName(className: string, enumName: string): string {
	return `register_enum_${sanitizeIdentifier(toSnakeCase(className))}_${sanitizeIdentifier(toSnakeCase(enumName))}`;
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
			bindingLines.push(`\t\t\t.constant("${constant.name}", ${literal})`);
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

function explicitDefaultArgument(
	className: string,
	method: ApiMethod,
	adapter: MethodAdapter,
): string | null {
	const key = `${className}.${method.name}`;
	const spec = adapter.default_argument;
	if (!spec) {
		return null;
	}

	const args = method.arguments ?? [];
	const last = args.at(-1);
	if (!last || last.type !== spec.type || last.default_value !== spec.value) {
		throw new Error(`Unexpected explicit default argument metadata for ${key}.`);
	}
	return spec.expression;
}

function buildMethodHelpers(
	className: string,
	method: ApiMethod,
	overloadIndex: number,
	adapter: MethodAdapter = {},
): { helpers: string[]; callables: string[] } {
	const args = method.arguments ?? [];
	const firstArity = firstDefaultArgumentIndex(className, method) ?? args.length;
	const helpers: string[] = [];
	const callables: string[] = [];
	const explicitDefault = explicitDefaultArgument(className, method, adapter);
	const cppName = adapter.cpp_name ?? method.name;
	const nativeMethod = `&godot::${className}::${cppName}`;
	const signature = adapter.native_signature
		? `decltype(static_cast<${adapter.native_signature}>(${nativeMethod}))`
		: `decltype(${nativeMethod})`;

	for (let arity = firstArity; arity <= args.length; arity++) {
		const helperName = `${sanitizeIdentifier(toSnakeCase(className))}_${sanitizeIdentifier(method.name)}_overload${overloadIndex}_arity${arity}`;
		const declarations = args.slice(0, arity).map((arg, index) => parameterDeclaration(mapMethodArgType(arg.type), `p_arg_${index}`));
		const invocationArgs = args.slice(0, arity).map((argument, index) => {
			return argument.type === "int" || argument.type === "float"
				? `static_cast<puerts::internal::native_parameter_t<${signature}, ${index}>>(p_arg_${index})`
				: `p_arg_${index}`;
		});
		if (explicitDefault && arity < args.length) {
			invocationArgs.push(explicitDefault);
		}

		let invocation: string;
		if (method.is_static) {
			invocation = `godot::${className}::${cppName}(${invocationArgs.join(", ")})`;
		} else {
			declarations.unshift(parameterDeclaration(`${method.is_const ? "const " : ""}godot::${className} &`, "p_self"));
			invocation = `p_self.${cppName}(${invocationArgs.join(", ")})`;
		}
		// Godot script scalars are signed int64 and double, regardless of C++ storage precision.
		const returnType = method.return_type ?? method.return_value?.type;
		if (returnType === "int" || returnType === "float") {
			invocation = `static_cast<${returnType === "int" ? "int64_t" : "double"}>(${invocation})`;
		}
		callables.push(`&puerts_generated::${helperName}`);
		helpers.push(`inline decltype(auto) ${helperName}(${declarations.join(", ")}) {\n\treturn ${invocation};\n}`);
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
			throw new Error(`Duplicate enum: ${cls.name}.${enumName}.`);
		}
		seenEnumNames.add(enumName);

		const enumValues = classEnum.values ?? [];
		if (enumValues.length === 0) {
			throw new Error(`Enum has no values: ${cls.name}.${enumName}.`);
		}

		const registrationFunctionName = enumRegisterFunctionName(cls.name, enumName);
		const constantLines = enumValues
			.map((v) => `\t\t\t.constant("${v.name}", ${Math.trunc(v.value)})`)
			.join("\n");
		const code =
			`inline void ${registrationFunctionName}() {\n` +
			`\tpuerts::define_namespace("${cls.name}.${enumName}")\n` +
			`${constantLines}\n` +
			`\t\t\t.register_type();\n` +
			`}\n`;

		enumBindings.push({
			enumName,
			registrationFunctionName,
			code,
		});
	}

	return enumBindings;
}

function pushMethodBindings(
	className: string,
	methods: ApiMethod[],
	bindingLines: string[],
	methodAdapters: ReadonlyMap<string, MethodAdapter>,
): string[] {
	const helpers: string[] = [];
	const methodsByName = new Map<string, ApiMethod[]>();
	for (const method of methods) {
		const overloads = methodsByName.get(method.name) ?? [];
		overloads.push(method);
		methodsByName.set(method.name, overloads);
	}
	for (const [name, overloads] of methodsByName) {
		const isStatic = !!overloads[0].is_static;
		if (overloads.some((method) => !!method.is_static !== isStatic)) {
			throw new Error(`Mixed static and instance overloads are unsupported: ${className}.${name}.`);
		}
		const callables: string[] = [];
		overloads.forEach((method, index) => {
			const generated = buildMethodHelpers(className, method, index, methodAdapters.get(`${className}.${name}`));
			helpers.push(...generated.helpers);
			callables.push(...generated.callables);
		});
		bindingLines.push(`\t\t\t.${isStatic ? "static_method" : "method"}<${callables.join(", ")}>("${name}")`);
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
	methodAdapters: ReadonlyMap<string, MethodAdapter> = new Map(),
): GeneratedClassBinding {
	const className = cls.name;
	const bindingLines: string[] = [];
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

	const methodHelpers = pushMethodBindings(className, methods, bindingLines, methodAdapters);

	const emittedMethodNames = new Set<string>(methods.map((m) => m.name));
	const seenVarargNames = new Set<string>();
	for (const method of varargMethods) {
		if (seenVarargNames.has(method.name)) {
			continue;
		}
		seenVarargNames.add(method.name);

		if (emittedMethodNames.has(method.name)) {
			throw new Error(`Duplicate fixed and vararg method: ${className}.${method.name}.`);
		}
		if (method.is_static) {
			throw new Error(`Static vararg methods are unsupported: ${className}.${method.name}.`);
		}

		const writeBack = method.is_const ? "false" : "true";
		bindingLines.push(
			`\t\t\t.vararg_method<${mapApiMethodReturnType(method)}, ${writeBack}, ${(method.arguments ?? []).length}>("${method.name}")`,
		);
		emittedMethodNames.add(method.name);
	}

	for (const property of properties) {
		bindingLines.push(property.bindingLine);
	}

	const boundPropertyNames = new Set<string>(properties.map((p) => p.name));
	for (const enumBinding of enumBindings) {
		if (boundPropertyNames.has(enumBinding.enumName)) {
			throw new Error(`Enum group name collision: ${className}.${enumBinding.enumName}.`);
		}
		bindingLines.push(`\t\t\t.nested_type("${enumBinding.enumName}", "${cls.name}.${enumBinding.enumName}")`);
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
			throw new Error(`Signal property name collision: ${className}.${signalName}.`);
		}
		bindingLines.push(`\t\t\t.signal("${signalName}")`);
		boundPropertyNames.add(signalName);
	}

	bindingLines.push("\t\t\t.register_type();");

	const fnName = `register_type_${sanitizeIdentifier(toSnakeCase(className))}`;
	const prefixLines = [...operatorHelpers, ...constantHelpers, ...methodHelpers];
	const prefix = prefixLines.length > 0
		? `namespace puerts_generated {\n${prefixLines.join("\n")}\n} // namespace puerts_generated\n\n`
		: "";
	const code = `${prefix}inline void ${fnName}() {\n\tpuerts::define_type<godot::${className}>()\n${bindingLines.join("\n")}\n}\n`;
	return {className, classSource, registrationFunctionName: fnName, code};
}
