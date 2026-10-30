// SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
// SPDX-License-Identifier: BSD-3-Clause

export default function godotImportRewritePlugin({types: t}) {
  function buildHelper() {
    return t.variableDeclaration("const", [
      t.variableDeclarator(
        t.identifier("__godot_module__"),
        t.newExpression(t.identifier("Proxy"), [
          t.callExpression(
            t.memberExpression(t.identifier("Object"), t.identifier("create")),
            [t.nullLiteral()],
          ),
          t.objectExpression([
            t.objectMethod(
              "method",
              t.identifier("get"),
              [t.identifier("_target"), t.identifier("key")],
              t.blockStatement([
                t.returnStatement(
                  t.callExpression(t.identifier("load_type"), [
                    t.callExpression(t.identifier("String"), [t.identifier("key")]),
                  ]),
                ),
              ]),
            ),
          ]),
        ]),
      ),
    ]);
  }

  function buildBinding(specifier) {
    if (t.isImportSpecifier(specifier)) {
      const importedName = t.isIdentifier(specifier.imported)
        ? specifier.imported.name
        : specifier.imported.value;

      return t.variableDeclaration("const", [
        t.variableDeclarator(
          t.identifier(specifier.local.name),
          t.callExpression(t.identifier("load_type"), [t.stringLiteral(importedName)]),
        ),
      ]);
    }

    return t.variableDeclaration("const", [
      t.variableDeclarator(t.identifier(specifier.local.name), t.identifier("__godot_module__")),
    ]);
  }

  function isTypeOnlySpecifier(specifier) {
    return t.isImportSpecifier(specifier) && specifier.importKind === "type";
  }

  const runtimeFunctions = new Set([
    "load_type", "load_addon_type", "to_callable", "log_error", "log_warn", "log_info",
  ]);

  function importedName(specifier) {
    return specifier.imported.name ?? specifier.imported.value;
  }

  function validateRuntimeFunction(path, name) {
    if (!runtimeFunctions.has(name)) {
      throw path.buildCodeFrameError(`Unknown puerts function: ${name}`);
    }
  }

  function globalReference(path, name, importBinding) {
    const binding = path.scope.getBinding(name);
    const declaration = binding?.path;
    const isRuntimeImport = declaration?.isImportSpecifier()
      && declaration.parentPath.node.source.value === "puerts"
      && importedName(declaration.node) === name;
    if (binding && binding !== importBinding && !isRuntimeImport) {
      throw path.buildCodeFrameError(`Cannot rewrite ${name}: a local binding shadows the runtime global.`);
    }
    return t.identifier(name);
  }

  function namespaceMember(reference, source) {
    const member = reference.parentPath;
    if (!member.isMemberExpression() || member.node.object !== reference.node) {
      throw reference.buildCodeFrameError(`${source} is a compile-time namespace; access a member directly.`);
    }
    const {property, computed} = member.node;
    let name;
    if (!computed && t.isIdentifier(property)) {
      name = property.name;
    } else if (t.isStringLiteral(property)) {
      name = property.value;
    } else {
      throw member.buildCodeFrameError("Virtual namespace members must use a static name.");
    }
    const parent = member.parentPath;
    if (parent.isAssignmentExpression({left: member.node})
        || parent.isUpdateExpression()
        || parent.isUnaryExpression({operator: "delete"})) {
      throw member.buildCodeFrameError("Virtual namespace members are read-only.");
    }
    return {path: member, name};
  }

  function rewriteVirtualImport(importPath, source) {
    if (importPath.node.importKind === "type") {
      importPath.remove();
      return;
    }
    for (const specifierPath of importPath.get("specifiers")) {
      const specifier = specifierPath.node;
      if (isTypeOnlySpecifier(specifier)) continue;
      if (t.isImportDefaultSpecifier(specifier)) {
        throw specifierPath.buildCodeFrameError(`${source} has no default export.`);
      }
      const binding = importPath.scope.getBinding(specifier.local.name);
      if (binding.constantViolations.length) {
        throw specifierPath.buildCodeFrameError("Virtual imports are read-only.");
      }
      const isNamespace = t.isImportNamespaceSpecifier(specifier);
      const name = isNamespace ? null : importedName(specifier);
      if (source === "puerts" && !isNamespace) {
        validateRuntimeFunction(specifierPath, name);
      }
      for (const reference of binding.referencePaths) {
        if (reference.findParent((path) => path.isTSType())) continue;
        if (reference.parentPath.isExportSpecifier()) {
          throw reference.buildCodeFrameError("Re-export virtual imports through an explicit local binding.");
        }
        const target = isNamespace
          ? namespaceMember(reference, source)
          : {path: reference, name};
        let replacement;
        if (source === "puerts") {
          if (isNamespace) validateRuntimeFunction(target.path, target.name);
          replacement = globalReference(target.path, target.name, binding);
        } else {
          replacement = t.callExpression(
            globalReference(target.path, "load_addon_type", binding),
            [t.stringLiteral(target.name)],
          );
        }
        if (target.path.parentPath.isObjectProperty()) {
          target.path.parentPath.node.shorthand = false;
        }
        target.path.replaceWith(replacement);
      }
    }
    importPath.remove();
  }

  return {
    name: "godot-import-rewrite",
    visitor: {
      /** @param {import("@babel/traverse").NodePath<import("@babel/types").Program>} programPath */
      Program(programPath) {
        let needsHelper = false;

        for (const statementPath of programPath.get("body")) {
          if (statementPath.isExportNamedDeclaration() || statementPath.isExportAllDeclaration()) {
            if (["puerts", "PAddon"].includes(statementPath.node.source?.value)
                && statementPath.node.exportKind !== "type") {
              throw statementPath.buildCodeFrameError("Re-export virtual imports through an explicit local binding.");
            }
          }
          if (!statementPath.isImportDeclaration()) continue;
          const source = statementPath.node.source.value;
          if (source === "puerts" || source === "PAddon") {
            rewriteVirtualImport(statementPath, source);
            continue;
          }
          if (source !== "godot") continue;

          const specifiers =
            statementPath.node.importKind === "type"
              ? []
              : statementPath.node.specifiers.filter(
                  (specifier) => !isTypeOnlySpecifier(specifier),
                );

          needsHelper ||= specifiers.some(
            (specifier) => !t.isImportSpecifier(specifier),
          );

          if (specifiers.length === 0) {
            statementPath.remove();
          } else {
            statementPath.replaceWithMultiple(specifiers.map(buildBinding));
          }
        }

        if (needsHelper) {
          programPath.unshiftContainer("body", buildHelper());
        }
      },
    },
  };
}
