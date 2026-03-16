/**
 * ccc
 * Copyright (c) 2026 lrisguan <lrisguan@outlook.com>
 * 
 * This program is released under the terms of the GNU General Public License version 2(GPLv2).
 * See https://opensource.org/licenses/GPL-2.0 for more information.
 * 
 * Project homepage: https://github.com/lrisguan/ccc
 * Description: A frontend of C
 */

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "Compile.h"
#include "Link.h"

namespace {

struct Options {
  std::vector<std::string> input_files;
  std::string output_file = "a.out";
  std::string ir_output_file;
  bool emit_object_only = false;
  unsigned opt_level = 2;
  std::vector<std::string> linker_args;
  std::vector<std::string> include_dirs;
  std::vector<std::string> macro_defines;
  std::vector<std::string> macro_undefines;
};

void PrintUsage(const char *argv0) {
  std::cerr
      << "Usage: " << argv0
      << " <input1.c> [input2.c ...] [-o output] [--emit-ir file.ll] "
         "[--emit-obj] [-O0|-O1|-O2|-O3] [-I dir] [-DNAME=VALUE] [-UNAME] "
         "[-lfoo] [-Ldir]\n";
}

bool ParseArgs(int argc, char **argv, Options &opts) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "-o") {
      if (i + 1 >= argc) {
        std::cerr << "missing value for -o\n";
        return false;
      }
      opts.output_file = argv[++i];
      continue;
    }
    if (arg == "--emit-ir") {
      if (i + 1 >= argc) {
        std::cerr << "missing value for --emit-ir\n";
        return false;
      }
      opts.ir_output_file = argv[++i];
      continue;
    }
    if (arg == "--emit-obj") {
      opts.emit_object_only = true;
      continue;
    }
    if (arg == "-O0" || arg == "-O1" || arg == "-O2" || arg == "-O3") {
      opts.opt_level = static_cast<unsigned>(arg[2] - '0');
      continue;
    }

    if (arg == "-I") {
      if (i + 1 >= argc) {
        std::cerr << "missing value for -I\n";
        return false;
      }
      opts.include_dirs.push_back(argv[++i]);
      continue;
    }
    if (arg.rfind("-I", 0) == 0 && arg.size() > 2) {
      opts.include_dirs.push_back(arg.substr(2));
      continue;
    }

    if (arg == "-D") {
      if (i + 1 >= argc) {
        std::cerr << "missing value for -D\n";
        return false;
      }
      opts.macro_defines.push_back(argv[++i]);
      continue;
    }
    if (arg.rfind("-D", 0) == 0 && arg.size() > 2) {
      opts.macro_defines.push_back(arg.substr(2));
      continue;
    }

    if (arg == "-U") {
      if (i + 1 >= argc) {
        std::cerr << "missing value for -U\n";
        return false;
      }
      opts.macro_undefines.push_back(argv[++i]);
      continue;
    }
    if (arg.rfind("-U", 0) == 0 && arg.size() > 2) {
      opts.macro_undefines.push_back(arg.substr(2));
      continue;
    }

    if (arg.rfind("-l", 0) == 0 || arg.rfind("-L", 0) == 0) {
      opts.linker_args.push_back(arg);
      continue;
    }
    if (!arg.empty() && arg[0] == '-') {
      std::cerr << "unknown option: " << arg << "\n";
      return false;
    }
    opts.input_files.push_back(arg);
  }

  if (opts.input_files.empty()) {
    std::cerr << "no input file provided\n";
    return false;
  }

  if (!opts.ir_output_file.empty() && opts.input_files.size() != 1) {
    std::cerr << "--emit-ir currently supports exactly one input file\n";
    return false;
  }

  if (opts.emit_object_only && opts.input_files.size() != 1) {
    std::cerr << "--emit-obj currently supports exactly one input file\n";
    return false;
  }

  return true;
}

std::string BuildObjectPath(const Options &opts, const std::string &input_file,
                            size_t index) {
  if (opts.emit_object_only) {
    return opts.output_file;
  }
  if (opts.input_files.size() == 1) {
    return opts.output_file + ".o";
  }

  std::filesystem::path input_path(input_file);
  const std::string stem = input_path.stem().string();
  return opts.output_file + "." + stem + "." + std::to_string(index) + ".o";
}

} // namespace

int main(int argc, char **argv) {
  Options opts;
  if (!ParseArgs(argc, argv, opts)) {
    PrintUsage(argv[0]);
    return 2;
  }

  std::vector<std::string> object_paths;
  object_paths.reserve(opts.input_files.size());

  for (size_t i = 0; i < opts.input_files.size(); ++i) {
    const std::string &input_file = opts.input_files[i];
    const std::string object_path = BuildObjectPath(opts, input_file, i);

    ccc::CompileRequest request;
    request.input_file = input_file;
    request.module_name =
        std::filesystem::path(input_file).filename().string() + "." +
        std::to_string(i);
    request.object_path = object_path;
    request.opt_level = opts.opt_level;
    request.include_dirs = opts.include_dirs;
    request.macro_defines = opts.macro_defines;
    request.macro_undefines = opts.macro_undefines;
    if (opts.input_files.size() == 1) {
      request.ir_output_file = opts.ir_output_file;
    }

    std::string compile_error;
    if (!ccc::CompileToObjectFile(request, compile_error)) {
      std::cerr << compile_error;
      if (!compile_error.empty() && compile_error.back() != '\n') {
        std::cerr << "\n";
      }
      return 1;
    }

    object_paths.push_back(object_path);
  }

  if (opts.emit_object_only) {
    return 0;
  }

  std::string link_error;
  if (!ccc::LinkExecutableWithLld(object_paths, opts.linker_args,
                                  opts.output_file, link_error)) {
    std::cerr << "link step failed: " << link_error;
    if (!link_error.empty() && link_error.back() != '\n') {
      std::cerr << "\n";
    }
    return 1;
  }

  return 0;
}
