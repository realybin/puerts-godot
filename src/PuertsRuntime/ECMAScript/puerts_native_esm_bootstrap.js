globalThis.__puer_resolve_module_url__ = function (specifier, referrer) {
	const resolved = globalThis.__puer_module_loader__.call("Resolve", specifier, referrer);
	if (!resolved) {
		throw new Error(`import ${specifier} failed: module not found`);
	}
	return resolved;
};

globalThis.__puer_resolve_module_content__ = function (specifier, debugpathRef) {
	debugpathRef[0] = globalThis.__puer_module_loader__.call("GetDebugPath", specifier);
	return globalThis.__puer_module_loader__.call("ReadFile", specifier);
};
