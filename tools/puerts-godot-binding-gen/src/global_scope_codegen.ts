// SPDX-FileCopyrightText: Copyright (c) 2026 realybin and contributors
// SPDX-License-Identifier: BSD-3-Clause

import { basename } from "node:path";

import { resolveApiVersion, sanitizeIdentifier, toIncludeGuardMacro, toSnakeCase } from "./text_utils.js";
import type { ApiData, ApiEnum, ApiMethod, CliArgs } from "./types.js";

type GlobalEnumBinding = {
	name: string;
	propertyName: string;
	tagStructName: string;
	tagType: string;
	registerFunctionName: string;
	scriptTypeLine: string;
	code: string;
};

function utilityCppMethodName(name: string): string {
	return name === "typeof" ? "type_of" : name;
}

function utilityCallbackName(name: string): string {
	return `puerts_global_scope_utility_${sanitizeIdentifier(toSnakeCase(name))}_generated`;
}

function singletonSymbolName(name: string): string {
	return `puerts_global_scope_singleton_name_${sanitizeIdentifier(toSnakeCase(name))}_generated`;
}

function buildGlobalEnumBinding(enumDef: ApiEnum): GlobalEnumBinding {
	const nameKey = sanitizeIdentifier(toSnakeCase(enumDef.name.replaceAll(".", "_")));
	const propertyName = enumDef.name.includes(".") ? enumDef.name.split(".").at(-1)! : enumDef.name;
	const tagStructName = `puerts_global_scope_${nameKey}_enum_tag_generated`;
	const tagType = `puerts_generated_global_scope_types::${tagStructName}`;
	const registerFunctionName = `register_${nameKey}_global_scope_enum_generated`;
	const staticPropertyLines = (enumDef.values ?? [])
		.map((value) => `\t\t\t.static_property("${value.name}", puerts::make_enum_constant<${Math.trunc(value.value)}>())`)
		.join("\n");

	return {
		name: enumDef.name,
		propertyName,
		tagStructName,
		tagType,
		registerFunctionName,
		scriptTypeLine: `PUERTS_SCRIPT_TYPE(${tagType}, "GlobalScope.${enumDef.name}")`,
		code:
			`inline void ${registerFunctionName}() {\n` +
			`\tpuerts::define_class<${tagType}>()\n` +
			`${staticPropertyLines}\n` +
			`\t\t\t.register_type();\n` +
			`}\n`,
	};
}

function utilityLookupLine(fn: ApiMethod): string {
	return `\tstatic GDExtensionPtrUtilityFunction utility_function = godot::internal::gdextension_interface_variant_get_ptr_utility_function(godot::StringName("${fn.name}")._native_ptr(), ${fn.hash ?? 0});`;
}

function pushCallbackContext(lines: string[]): void {
	lines.push("\tpuerts::internal::callback_context context(apis, info);");
	lines.push("\tif (!context.require()) {");
	lines.push("\t\treturn;");
	lines.push("\t}");
}

function pushVariantArgs(lines: string[], argCountExpr: string): void {
	lines.push("\tpuerts_eastl::vector<godot::Variant> args;");
	lines.push("\tpuerts_eastl::vector<const godot::Variant *> arg_ptrs;");
	lines.push(`\targs.resize(static_cast<size_t>(${argCountExpr}));`);
	lines.push(`\targ_ptrs.resize(static_cast<size_t>(${argCountExpr}));`);
	lines.push(`\tfor (int i = 0; i < ${argCountExpr}; ++i) {`);
	lines.push("\t\tconst size_t index = static_cast<size_t>(i);");
	lines.push("\t\targs[index] = puerts::static_binding_access::script_to_variant(context.environment, context.env, context.get_arg(i));");
	lines.push("\t\targ_ptrs[index] = &args[index];");
	lines.push("\t}");
}

function pushUtilityLookup(lines: string[], fn: ApiMethod): void {
	lines.push(utilityLookupLine(fn));
	lines.push("\tif (utility_function == nullptr) {");
	lines.push(`\t\tapis->throw_by_string(info, "Failed to bind utility function: ${fn.name}");`);
	lines.push("\t\treturn;");
	lines.push("\t}");
}

function buildVarargUtilityHelper(fn: ApiMethod): string {
	const callbackName = utilityCallbackName(fn.name);
	const returnType = fn.return_type ?? "void";
	const lines: string[] = [];

	lines.push(`inline void ${callbackName}(pesapi_ffi *apis, pesapi_callback_info info) {`);
	pushCallbackContext(lines);
	pushUtilityLookup(lines, fn);
	pushVariantArgs(lines, "context.arg_count");

	if (returnType === "String") {
		lines.push("\tgodot::String result;");
		lines.push("\tutility_function(&result, reinterpret_cast<GDExtensionConstVariantPtr *>(arg_ptrs.data()), context.arg_count);");
		lines.push("\tpuerts::static_binding_access::add_variant_return(apis, info, context.env, context.environment, godot::Variant(result));");
	} else if (returnType === "Variant") {
		lines.push("\tgodot::Variant result;");
		lines.push("\tutility_function(&result, reinterpret_cast<GDExtensionConstVariantPtr *>(arg_ptrs.data()), context.arg_count);");
		lines.push("\tpuerts::static_binding_access::add_variant_return(apis, info, context.env, context.environment, result);");
	} else {
		lines.push("\tgodot::Variant ignored_result;");
		lines.push("\tutility_function(&ignored_result, reinterpret_cast<GDExtensionConstVariantPtr *>(arg_ptrs.data()), context.arg_count);");
	}

	lines.push("}");
	return lines.join("\n");
}

function buildSpecialUtilityHelper(fn: ApiMethod): string {
	const callbackName = utilityCallbackName(fn.name);
	const expectedArgs = fn.arguments?.length ?? 0;
	const lines: string[] = [];

	lines.push(`inline void ${callbackName}(pesapi_ffi *apis, pesapi_callback_info info) {`);
	pushCallbackContext(lines);
	lines.push(`\tif (context.arg_count != ${expectedArgs}) {`);
	lines.push(`\t\tapis->throw_by_string(info, "${fn.name} expects exactly ${expectedArgs} argument(s).");`);
	lines.push("\t\treturn;");
	lines.push("\t}");
	pushUtilityLookup(lines, fn);
	pushVariantArgs(lines, `${expectedArgs}`);
	lines.push("\tbool result = false;");
	lines.push(`\tutility_function(&result, reinterpret_cast<GDExtensionConstTypePtr *>(arg_ptrs.data()), ${expectedArgs});`);
	lines.push("\tapis->add_return(info, apis->create_boolean(context.env, result));");
	lines.push("}");
	return lines.join("\n");
}

function buildUtilityBinding(fn: ApiMethod): { bindingLine: string; helperCode?: string } {
	if (!fn.is_vararg && fn.name !== "is_instance_valid") {
		return {
			bindingLine: `\t\t\t.function("${fn.name}", puerts::make_function<&godot::UtilityFunctions::${utilityCppMethodName(fn.name)}>())`,
		};
	}

	return {
		bindingLine: `\t\t\t.function("${fn.name}", ${utilityCallbackName(fn.name)})`,
		helperCode: fn.name === "is_instance_valid" ? buildSpecialUtilityHelper(fn) : buildVarargUtilityHelper(fn),
	};
}

function splitGlobalEnumBindings(globalEnums: GlobalEnumBinding[]): {
	topLevel: GlobalEnumBinding[];
	variantType: GlobalEnumBinding;
	variantOperator: GlobalEnumBinding;
} {
	const topLevel: GlobalEnumBinding[] = [];
	let variantType: GlobalEnumBinding | undefined;
	let variantOperator: GlobalEnumBinding | undefined;

	for (const binding of globalEnums) {
		switch (binding.name) {
			case "Variant.Type":
				variantType = binding;
				break;
			case "Variant.Operator":
				variantOperator = binding;
				break;
			default:
				if (!binding.name.startsWith("Variant.")) {
					topLevel.push(binding);
				}
				break;
		}
	}

	if (variantType == null || variantOperator == null) {
		throw new Error("GlobalScope generation requires Variant.Type and Variant.Operator enums.");
	}

	return { topLevel, variantType, variantOperator };
}

export function generateGlobalScopeBinding(args: CliArgs, data: ApiData): string {
	const includeGuard = toIncludeGuardMacro(basename(args.output));
	const apiVersion = resolveApiVersion(data);
	const globalEnums = (data.global_enums ?? []).map(buildGlobalEnumBinding);
	const { topLevel: topLevelEnumBindings, variantType, variantOperator } = splitGlobalEnumBindings(globalEnums);
	const utilityBindings = (data.utility_functions ?? []).map(buildUtilityBinding);
	const singletonNames = data.singletons ?? [];

	const lines: string[] = [];
	lines.push("// AUTO-GENERATED FILE. DO NOT EDIT.");
	lines.push("// Generated by tools/puerts-godot-binding-gen from extension_api.json.");
	lines.push(`// generated_at: ${new Date().toISOString()}`);
	lines.push(`// api_version: ${apiVersion}`);
	lines.push("// GlobalScope bindings are generated directly from extension_api.json.");
	lines.push("");
	lines.push(`#ifndef ${includeGuard}`);
	lines.push(`#define ${includeGuard}`);
	lines.push("");
	lines.push("#include <godot_cpp/classes/engine.hpp>");
	lines.push("#include <godot_cpp/godot.hpp>");
	lines.push("#include <godot_cpp/variant/string_name.hpp>");
	lines.push("#include <godot_cpp/variant/utility_functions.hpp>");
	lines.push("");
	lines.push("namespace puerts_generated_global_scope_types {");
	lines.push("struct GlobalScope {};");
	lines.push("struct GlobalScopeVariant {};");
	for (const binding of globalEnums) {
		lines.push(`struct ${binding.tagStructName} {};`);
	}
	lines.push("} // namespace puerts_generated_global_scope_types");
	lines.push("");
	for (const singleton of singletonNames) {
		lines.push(`inline constexpr char ${singletonSymbolName(singleton.name)}[] = "${singleton.name}";`);
	}
	if (singletonNames.length > 0) {
		lines.push("");
	}
	lines.push('PUERTS_SCRIPT_TYPE(puerts_generated_global_scope_types::GlobalScope, "GlobalScope")');
	lines.push('PUERTS_SCRIPT_TYPE(puerts_generated_global_scope_types::GlobalScopeVariant, "GlobalScope.Variant")');
	for (const binding of globalEnums) {
		lines.push(binding.scriptTypeLine);
	}
	lines.push("");
	lines.push("inline void puerts_global_scope_singleton_getter_generated(pesapi_ffi *apis, pesapi_callback_info info) {");
	lines.push("\tconst char *singleton_name = static_cast<const char *>(apis->get_userdata(info));");
	lines.push("\tif (singleton_name == nullptr) {");
	lines.push('\t\tapis->throw_by_string(info, "Singleton metadata is missing.");');
	lines.push("\t\treturn;");
	lines.push("\t}");
	lines.push("\tpuerts::internal::callback_context context(apis, info);");
	lines.push("\tif (!context.require()) {");
	lines.push("\t\treturn;");
	lines.push("\t}");
	lines.push("\tgodot::Engine *engine = godot::Engine::get_singleton();");
	lines.push("\tif (engine == nullptr) {");
	lines.push('\t\tapis->throw_by_string(info, "Godot Engine singleton is unavailable.");');
	lines.push("\t\treturn;");
	lines.push("\t}");
	lines.push("\tconst godot::StringName singleton_key(singleton_name);");
	lines.push("\tif (!engine->has_singleton(singleton_key)) {");
	lines.push('\t\tapis->throw_by_string(info, "Requested singleton is unavailable.");');
	lines.push("\t\treturn;");
	lines.push("\t}");
	lines.push("\tgodot::Object *singleton = engine->get_singleton(singleton_key);");
	lines.push("\tif (singleton == nullptr) {");
	lines.push("\t\tapis->add_return(info, apis->create_null(context.env));");
	lines.push("\t\treturn;");
	lines.push("\t}");
	lines.push("\tpuerts::static_binding_access::add_variant_return(apis, info, context.env, context.environment, godot::Variant(singleton));");
	lines.push("}");
	for (const binding of utilityBindings) {
		if (binding.helperCode) {
			lines.push("");
			lines.push(binding.helperCode);
		}
	}
	lines.push("");
	for (const binding of globalEnums) {
		lines.push(binding.code.trimEnd());
		lines.push("");
	}
	lines.push("inline void register_global_scope_variant_generated() {");
	lines.push("\tpuerts::define_class<puerts_generated_global_scope_types::GlobalScopeVariant>()");
	lines.push(`\t\t\t.static_property("Type", puerts::make_enum_group<${variantType.tagType}>())`);
	lines.push(`\t\t\t.static_property("Operator", puerts::make_enum_group<${variantOperator.tagType}>())`);
	lines.push("\t\t\t.register_type();");
	lines.push("}");
	lines.push("");
	lines.push(`inline void ${args.registerFunction}() {`);
	for (const binding of globalEnums) {
		lines.push(`\t${binding.registerFunctionName}();`);
	}
	lines.push("\tregister_global_scope_variant_generated();");
	lines.push("\tpuerts::define_class<puerts_generated_global_scope_types::GlobalScope>()");
	for (const binding of utilityBindings) {
		lines.push(binding.bindingLine);
	}
	for (const singleton of singletonNames) {
		lines.push(
			`\t\t\t.static_property("${singleton.name}", &puerts_global_scope_singleton_getter_generated, nullptr, (void *)${singletonSymbolName(singleton.name)})`,
		);
	}
	for (const binding of topLevelEnumBindings) {
		lines.push(`\t\t\t.static_property("${binding.propertyName}", puerts::make_enum_group<${binding.tagType}>())`);
	}
	lines.push('\t\t\t.static_property("Variant", puerts::make_enum_group<puerts_generated_global_scope_types::GlobalScopeVariant>())');
	lines.push("\t\t\t.register_type();");
	lines.push("}");
	lines.push("");
	lines.push(`#endif // ${includeGuard}`);
	lines.push("");
	return `${lines.join("\n")}`;
}
