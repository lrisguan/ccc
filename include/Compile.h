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

#pragma once

#include <string>
#include <vector>

namespace ccc {

struct CompileRequest {
  std::string input_file;
  std::string module_name;
  std::string object_path;
  unsigned opt_level = 2;
  std::string ir_output_file;
  std::vector<std::string> include_dirs;
  std::vector<std::string> macro_defines;
  std::vector<std::string> macro_undefines;
};

bool CompileToObjectFile(const CompileRequest &request,
                         std::string &error_message);

} // namespace ccc
