/**
 * ccc
 * Copyright (c) 2026 lrisguan <lrisguan@outlook.com>
 * 
 * This program is released under the terms of the  GNU General Public License version 2(GPLv2).
 * See https://opensource.org/licenses/GPL-2.0 for more information.
 * 
 * Project homepage: https://github.com/lrisguan/ccc
 * Description: A frontend of C
 */

#include "Link.h"

#include <array>
#include <filesystem>

#include "lld/Common/Driver.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Host.h"

LLD_HAS_DRIVER(elf)

namespace ccc {

namespace {

struct LinuxRuntimePaths {
  std::string dynamic_linker;
  std::string crt1;
  std::string crti;
  std::string crtn;
  std::string crtbegin;
  std::string crtend;
  std::string gcc_lib_dir;
  std::string sys_lib_dir;
  std::string root_lib_dir;
};

bool FindLatestGccLibDir(const std::string &gcc_base, std::string &out_dir) {
  if (!std::filesystem::exists(gcc_base)) {
    return false;
  }

  std::string best_dir;
  for (const auto &entry : std::filesystem::directory_iterator(gcc_base)) {
    if (!entry.is_directory()) {
      continue;
    }
    const std::filesystem::path candidate = entry.path();
    if (std::filesystem::exists(candidate / "crtbegin.o") &&
        std::filesystem::exists(candidate / "crtend.o")) {
      if (best_dir.empty() || candidate.string() > best_dir) {
        best_dir = candidate.string();
      }
    }
  }

  if (best_dir.empty()) {
    return false;
  }
  out_dir = best_dir;
  return true;
}

bool ResolveLinuxRuntimePaths(LinuxRuntimePaths &out_paths, std::string &err) {
  const std::string triple = llvm::sys::getDefaultTargetTriple();
  std::string arch;
  std::string dynamic_linker;
  if (triple.rfind("aarch64", 0) == 0) {
    arch = "aarch64-linux-gnu";
    dynamic_linker = "/lib/ld-linux-aarch64.so.1";
  } else if (triple.rfind("x86_64", 0) == 0) {
    arch = "x86_64-linux-gnu";
    dynamic_linker = "/lib64/ld-linux-x86-64.so.2";
  } else {
    err = "unsupported host triple for built-in lld linker path resolution: " +
          triple;
    return false;
  }

  out_paths.dynamic_linker = dynamic_linker;
  out_paths.crt1 = "/usr/lib/" + arch + "/crt1.o";
  out_paths.crti = "/usr/lib/" + arch + "/crti.o";
  out_paths.crtn = "/usr/lib/" + arch + "/crtn.o";
  out_paths.sys_lib_dir = "/usr/lib/" + arch;
  out_paths.root_lib_dir = "/lib/" + arch;

  if (!FindLatestGccLibDir("/usr/lib/gcc/" + arch, out_paths.gcc_lib_dir)) {
    err = "unable to locate GCC runtime directory under /usr/lib/gcc/" + arch;
    return false;
  }
  out_paths.crtbegin = out_paths.gcc_lib_dir + "/crtbegin.o";
  out_paths.crtend = out_paths.gcc_lib_dir + "/crtend.o";

  const std::vector<std::string> required = {
      out_paths.dynamic_linker, out_paths.crt1,     out_paths.crti,
      out_paths.crtn,           out_paths.crtbegin, out_paths.crtend};
  for (const auto &path : required) {
    if (!std::filesystem::exists(path)) {
      err = "missing runtime file for lld link: " + path;
      return false;
    }
  }

  return true;
}

} // namespace

bool LinkExecutableWithLld(const std::vector<std::string> &object_paths,
                           const std::vector<std::string> &linker_args,
                           const std::string &output_path,
                           std::string &error_message) {
  LinuxRuntimePaths rt;
  if (!ResolveLinuxRuntimePaths(rt, error_message)) {
    return false;
  }

  std::vector<std::string> args;
  args.reserve(object_paths.size() + linker_args.size() + 24);
  args.push_back("ld.lld");
  args.push_back("-o");
  args.push_back(output_path);
  args.push_back("-dynamic-linker");
  args.push_back(rt.dynamic_linker);
  args.push_back(rt.crt1);
  args.push_back(rt.crti);
  args.push_back(rt.crtbegin);

  for (const auto &object_path : object_paths) {
    args.push_back(object_path);
  }

  args.push_back("-L");
  args.push_back(rt.gcc_lib_dir);
  args.push_back("-L");
  args.push_back(rt.sys_lib_dir);
  args.push_back("-L");
  args.push_back(rt.root_lib_dir);

  for (const auto &arg : linker_args) {
    args.push_back(arg);
  }

  args.push_back("-lc");
  args.push_back("-lgcc");
  args.push_back("-lgcc_s");
  args.push_back(rt.crtend);
  args.push_back(rt.crtn);

  std::vector<const char *> raw_args;
  raw_args.reserve(args.size());
  for (const auto &arg : args) {
    raw_args.push_back(arg.c_str());
  }

  std::string out_text;
  std::string err_text;
  llvm::raw_string_ostream out_stream(out_text);
  llvm::raw_string_ostream err_stream(err_text);

  const std::array<lld::DriverDef, 1> drivers = {
      lld::DriverDef{lld::Gnu, &lld::elf::link},
  };
  const lld::Result result =
      lld::lldMain(llvm::ArrayRef<const char *>(raw_args), out_stream,
                   err_stream, llvm::ArrayRef<lld::DriverDef>(drivers));
  out_stream.flush();
  err_stream.flush();

  if (result.retCode != 0) {
    error_message = err_text.empty() ? "lld link failed" : err_text;
    return false;
  }
  return true;
}

} // namespace ccc
