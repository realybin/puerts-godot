extends RefCounted
class_name PuertsNativeEsmLoader

var root_prefix := "res://"


func Resolve(specifier: String, referrer: String) -> String:
	var path := specifier
	if specifier.begins_with("./") or specifier.begins_with("../"):
		path = referrer.get_base_dir().path_join(specifier)
	elif not specifier.begins_with("res://"):
		path = root_prefix.path_join(specifier.trim_prefix("/"))

	path = path.simplify_path()
	return path if FileAccess.file_exists(path) else ""


func ReadFile(path: String) -> String:
	return FileAccess.get_file_as_string(path)


func GetDebugPath(path: String) -> String:
	return ProjectSettings.globalize_path(path)
