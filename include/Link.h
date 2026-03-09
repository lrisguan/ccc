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

#pragma once

#include <string>
#include <vector>

namespace ccc {

bool LinkExecutableWithLld(const std::vector<std::string> &object_paths,
                           const std::vector<std::string> &linker_args,
                           const std::string &output_path,
                           std::string &error_message);

} // namespace ccc
