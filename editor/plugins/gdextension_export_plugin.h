/*************************************************************************/
/*  gdextension_export_plugin.h                                          */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef GDEXTENSION_EXPORT_PLUGIN_H
#define GDEXTENSION_EXPORT_PLUGIN_H

#include "editor/export/editor_export.h"

class GDExtensionExportPlugin : public EditorExportPlugin {
protected:
	virtual void _export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features);
	virtual String _get_name() const { return "GDExtension"; }
};

void GDExtensionExportPlugin::_export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) {
	if (p_type != "NativeExtension") {
		return;
	}

	Ref<ConfigFile> config;
	config.instantiate();

	Error err = config->load(p_path);
	ERR_FAIL_COND_MSG(err, "Failed to load GDExtension file: " + p_path);

	ERR_FAIL_COND_MSG(!config->has_section_key("configuration", "entry_symbol"), "Failed to export GDExtension file, missing entry symbol: " + p_path);

	String entry_symbol = config->get_value("configuration", "entry_symbol");

	PackedStringArray tags;
	String library_path = NativeExtension::find_extension_library(
			p_path, config, [p_features](String p_feature) { return p_features.has(p_feature); }, &tags);
	if (!library_path.is_empty()) {
		add_shared_object(library_path, tags);

		if (p_features.has("iOS") && (library_path.ends_with(".a") || library_path.ends_with(".xcframework"))) {
			String additional_code = "extern void register_dynamic_symbol(char *name, void *address);\n"
									 "extern void add_ios_init_callback(void (*cb)());\n"
									 "\n"
									 "extern \"C\" void $ENTRY();\n"
									 "void $ENTRY_init() {\n"
									 "  if (&$ENTRY) register_dynamic_symbol((char *)\"$ENTRY\", (void *)$ENTRY);\n"
									 "}\n"
									 "struct $ENTRY_struct {\n"
									 "  $ENTRY_struct() {\n"
									 "    add_ios_init_callback($ENTRY_init);\n"
									 "  }\n"
									 "};\n"
									 "$ENTRY_struct $ENTRY_struct_instance;\n\n";
			additional_code = additional_code.replace("$ENTRY", entry_symbol);
			add_ios_cpp_code(additional_code);

			String linker_flags = "-Wl,-U,_" + entry_symbol;
			add_ios_linker_flags(linker_flags);
		}
	} else {
		Vector<String> features_vector;
		for (const String &E : p_features) {
			features_vector.append(E);
		}
		ERR_FAIL_MSG(vformat("No suitable library found. The libraries' tags referred to an invalid feature flag. Possible feature flags for your platform: %s", p_path, String(", ").join(tags)));
	}

	List<String> dependencies;
	if (config->has_section("dependencies")) {
		config->get_section_keys("dependencies", &dependencies);
	}

	for (const String &E : dependencies) {
		Vector<String> dependency_tags = E.split(".");
		bool all_tags_met = true;
		for (int i = 0; i < dependency_tags.size(); i++) {
			String tag = dependency_tags[i].strip_edges();
			if (!p_features.has(tag)) {
				all_tags_met = false;
				break;
			}
		}

		if (all_tags_met) {
			Dictionary dependency = config->get_value("dependencies", E);
			for (const Variant *key = dependency.next(nullptr); key; key = dependency.next(key)) {
				String dependency_path = *key;
				String target_path = dependency[*key];
				if (dependency_path.is_relative_path()) {
					dependency_path = p_path.get_base_dir().path_join(dependency_path);
				}
				add_shared_object(dependency_path, dependency_tags, target_path);
			}
			break;
		}
	}
}

#endif // GDEXTENSION_EXPORT_PLUGIN_H
