// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import { normalizeOperators } from "../../puerts-godot-operator-model/index.js";
import { mapCtorType, mapMethodArgType, mapMethodReturnType, methodPointerExprFromApi } from "./type_mapping.js";
import { sanitizeIdentifier, toSnakeCase } from "./text_utils.js";
import type {
	ApiClassLike,
	ApiBuiltinClass,
	ApiMethod,
	BoundPropertyBinding,
	ClassSource,
	GeneratedClassBinding,
	GeneratedEnumBinding,
	HeaderAnalysis,
} from "./types.js";

function enumTagStructName(className: string, enumName: string): string {
	return `${sanitizeIdentifier(toSnakeCase(className))}_${sanitizeIdentifier(toSnakeCase(enumName))}_enum_tag_generated`;
}

function enumRegisterFunctionName(className: string, enumName: string): string {
	return `register_${sanitizeIdentifier(toSnakeCase(className))}_${sanitizeIdentifier(toSnakeCase(enumName))}_enum_generated`;
}

function signalNameSymbol(className: string, signalName: string): string {
	return `puerts_signal_name_${sanitizeIdentifier(toSnakeCase(className))}_${sanitizeIdentifier(signalName)}`;
}

function isBuiltinClass(cls: ApiClassLike): cls is ApiBuiltinClass {
	return "operators" in cls;
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
		const wrapperName = `${classKey}_${sanitizeIdentifier(toSnakeCase(operator.scriptName))}_${index}_generated`;
		const returnType = mapMethodReturnType(operator.returnType);
		const argDecl =
			operator.kind === "unary"
				? `${mapMethodArgType(operator.leftType)} value`
				: `${mapMethodArgType(operator.leftType)} left, ${mapMethodArgType(operator.rightType ?? "Variant")} right`;
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
		if (overloads.length === 1) {
			bindingLines.push(`\t\t\t.function("${scriptName}", puerts::make_function<&${overloads[0]}>())`);
			continue;
		}
		const exprs = overloads.map((name) => `puerts::make_overload<&${name}>()`);
		bindingLines.push(`\t\t\t.function("${scriptName}", puerts::combine_overloads(${exprs.join(", ")}))`);
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
		const tagType = `puerts_generated_enum_tags::${tagStruct}`;
		const functionName = enumRegisterFunctionName(cls.name, enumName);
		const staticPropertyLines = enumValues
			.map((v) => `\t\t\t.static_property("${v.name}", puerts::make_enum_constant<${Math.trunc(v.value)}>())`)
			.join("\n");
		const code =
			`inline void ${functionName}() {\n` +
			`\tpuerts::define_class<${tagType}>()\n` +
			`${staticPropertyLines}\n` +
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
	methods: ApiMethod[],
	headerAnalysis: HeaderAnalysis,
	bindingLines: string[],
): void {
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
		if (methodGroup.length === 0) continue;

		if (methodGroup.length === 1) {
			const method = methodGroup[0];
			const methodRefCount = headerAnalysis.methodNameCounts.get(name) ?? 0;
			const needsDisambiguation = methodRefCount > 1;

			if (method.is_static) {
				if (needsDisambiguation) {
					const ptr = methodPointerExprFromApi(className, method);
					bindingLines.push(`\t\t\t.function("${name}", puerts::make_function<${ptr}>())`);
				} else {
					bindingLines.push(`\t\t\t.function("${name}", puerts::make_function<&godot::${className}::${name}>())`);
				}
			} else if (needsDisambiguation) {
				const ptr = methodPointerExprFromApi(className, method);
				bindingLines.push(`\t\t\t.method("${name}", puerts::make_method<${ptr}>())`);
			} else {
				bindingLines.push(`\t\t\t.method("${name}", puerts::make_method<&godot::${className}::${name}>())`);
			}
			continue;
		}

		const staticMethods = methodGroup.filter((m) => !!m.is_static);
		const instanceMethods = methodGroup.filter((m) => !m.is_static);

		if (staticMethods.length > 0 && instanceMethods.length > 0) {
			const chosen = staticMethods.length >= instanceMethods.length ? staticMethods : instanceMethods;
			const chosenKind = staticMethods.length >= instanceMethods.length ? "static" : "instance";
			console.warn(`Warning: mixed static/instance overloads for ${className}.${name}, chose ${chosenKind} overloads.`);
			if (chosenKind === "static") {
				const exprs = chosen.map((m) => `puerts::make_overload<${methodPointerExprFromApi(className, m)}>()`);
				bindingLines.push(`\t\t\t.function("${name}", puerts::combine_overloads(${exprs.join(", ")}))`);
			} else {
				const exprs = chosen.map((m) => `puerts::make_method_overload<${methodPointerExprFromApi(className, m)}>()`);
				bindingLines.push(`\t\t\t.method("${name}", puerts::combine_overloads(${exprs.join(", ")}))`);
			}
			continue;
		}

		if (staticMethods.length > 0) {
			const exprs = staticMethods.map((m) => `puerts::make_overload<${methodPointerExprFromApi(className, m)}>()`);
			bindingLines.push(`\t\t\t.function("${name}", puerts::combine_overloads(${exprs.join(", ")}))`);
			continue;
		}

		const exprs = instanceMethods.map((m) => `puerts::make_method_overload<${methodPointerExprFromApi(className, m)}>()`);
		bindingLines.push(`\t\t\t.method("${name}", puerts::combine_overloads(${exprs.join(", ")}))`);
	}
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
	const varargNameDefs: string[] = [];
	const operatorHelpers = pushOperatorBindings(cls, bindingLines);
	const baseClassName = cls.inherits?.trim();

	if (baseClassName && baseClassName !== className) {
		bindingLines.push(`\t\t\t.extends<godot::${baseClassName}>()`);
	}

	const ctors = cls.constructors ?? [];
	const shouldEmitImplicitDefaultCtor = ctors.length === 0 && cls.is_instantiable !== false;
	if (shouldEmitImplicitDefaultCtor) {
		bindingLines.push(`\t\t\t.constructor<>()`);
	} else if (ctors.length > 0) {
		const ctorSpecs = ctors
			.map((ctor) => {
				const args = ctor.arguments ?? [];
				if (args.length === 0) {
					return `puerts::make_constructor_overload<godot::${className}>()`;
				}
				const mapped = args.map((a) => mapCtorType(a.type));
				return `puerts::make_constructor_overload<godot::${className}, ${mapped.join(", ")}>()`;
			})
			.join(",\n\t\t\t\t\t");
		bindingLines.push(`\t\t\t.constructor(puerts::combine_constructors(\n\t\t\t\t\t${ctorSpecs}))`);
	}

	pushMethodBindings(className, methods, headerAnalysis, bindingLines);

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

		const symbol = `puerts_vararg_method_name_${sanitizeIdentifier(toSnakeCase(className))}_${sanitizeIdentifier(method.name)}`;
		varargNameDefs.push(`inline constexpr char ${symbol}[] = "${method.name}";`);
		bindingLines.push(
			`\t\t\t.method("${method.name}", puerts::make_vararg_method<godot::${className}, ${mapMethodReturnType(method.return_type)}, ${symbol}, ${(method.arguments ?? []).length}>())`,
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
		bindingLines.push(`\t\t\t.static_property("${enumBinding.enumName}", puerts::make_enum_group<${enumBinding.tagType}>())`);
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
		varargNameDefs.push(`inline constexpr char ${signalSymbol}[] = "${signalName}";`);
		bindingLines.push(`\t\t\t.property("${signalName}", puerts::make_signal_property<godot::${className}, ${signalSymbol}>())`);
		boundPropertyNames.add(signalName);
	}

	bindingLines.push("\t\t\t.register_type();");

	const fnName = `register_${sanitizeIdentifier(toSnakeCase(className))}_type_generated`;
	const prefixLines = [...varargNameDefs, ...operatorHelpers];
	const prefix = prefixLines.length > 0 ? `${prefixLines.join("\n")}\n` : "";
	const code = `${prefix}inline void ${fnName}() {\n\tpuerts::define_class<godot::${className}>()\n${bindingLines.join("\n")}\n}\n`;
	return { className, classSource, functionName: fnName, code };
}
