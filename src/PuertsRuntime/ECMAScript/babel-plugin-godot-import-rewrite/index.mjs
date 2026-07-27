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

  return {
    name: "godot-import-rewrite",
    visitor: {
      /** @param {import("@babel/traverse").NodePath<import("@babel/types").Program>} programPath */
      Program(programPath) {
        let needsHelper = false;

        for (const statementPath of programPath.get("body")) {
          if (!statementPath.isImportDeclaration() || statementPath.node.source.value !== "godot") {
            continue;
          }

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
