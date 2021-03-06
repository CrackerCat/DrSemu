#pragma once

#include <vector>
#include <string>
#include <fstream>

#include <filesystem>
#include "../LauncherCLI/utils.hpp"
namespace fs = std::filesystem;

#define PY_SSIZE_T_CLEAN
#include "Python.h"

inline std::vector<std::string> get_lines_from_file(const std::string& file_path)
{
	std::ifstream file(file_path);
	std::string line;
	std::vector<std::string> lines{};
	while (std::getline(file, line))
	{
		if (!line.empty())
			lines.emplace_back(line);
	}
	return lines;
}

inline std::string python_rules_verdict(const fs::path& rules_directory, const std::string& reports_directory)
{
	const char function_name[] = "check";

	Py_Initialize();

	// fill from a py_imports.config file
	const auto imports_file = rules_directory.string() + R"(\py_imports.config)";
	std::vector<std::string> names{};
	if (fs::exists(imports_file))
	{
		names = get_lines_from_file(imports_file);
	}

	// import modules
	std::vector<PyObject*> python_loaded_modules{};
	for (auto& name : names)
	{
		const auto python_file = PyUnicode_DecodeFSDefault(name.c_str());
		const auto loaded_module = PyImport_Import(python_file);
		if (loaded_module != nullptr)
		{
			python_loaded_modules.emplace_back(loaded_module);
		}
		Py_DECREF(python_file);
	}

	std::string verdict = "NO DETECTIONS";

	PySys_SetPath(rules_directory.wstring().c_str());

	for (auto& path : fs::directory_iterator(rules_directory))
	{
		auto file_name = path.path().filename().string();
		if (file_name.ends_with(".py") && file_name != "dr_semu_utils.py")
		{
			file_name = path.path().filename().replace_extension("").string();
			const auto python_rule_file = PyUnicode_DecodeFSDefault(file_name.c_str());
			const auto ptr_module = PyImport_Import(python_rule_file);
			Py_DECREF(python_rule_file);

			if (ptr_module != nullptr)
			{
				const auto ptr_python_function = PyObject_GetAttrString(ptr_module, function_name);

				if (ptr_python_function && PyCallable_Check(ptr_python_function))
				{
					const auto ptr_arguments = PyTuple_New(1);
					const auto string_argument = PyBytes_FromString(reports_directory.c_str());
					PyTuple_SetItem(ptr_arguments, 0, string_argument);

					const auto call_result = PyObject_CallObject(ptr_python_function, ptr_arguments);
					Py_DECREF(ptr_arguments);
					if (call_result != nullptr)
					{
						const auto result_char_string = PyBytes_AsString(call_result);
						verdict = std::string(result_char_string);
						Py_DECREF(call_result);
						if (!verdict.empty() && verdict != "CLEAN")
						{
							Py_DECREF(ptr_python_function);
							Py_DECREF(ptr_module);
							break;
						}
					}
					else
					{
						PyErr_Print();
					}
				}
				else
				{
					PyErr_Print();
				}
				Py_DECREF(ptr_python_function);
				Py_DECREF(ptr_module);
			}
			else
			{
				PyErr_Print();
			}
		}
	}


	for (auto python_loaded_module : python_loaded_modules)
	{
		Py_DECREF(python_loaded_module);
	}

	Py_FinalizeEx();

	return verdict;
}
