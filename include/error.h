/**
 * ccc
 * Copyright (c) 2026 lrisguan <lrisguan@outlook.com>
 *
 * This program is released under the terms of the GNU General Public License
 * version 2(GPLv2). See https://opensource.org/licenses/GPL-2.0 for more
 * information.
 *
 * Project homepage: https://github.com/lrisguan/ccc
 * Description: A frontend of C
 */

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace ccc {

struct SourceLocation {
  size_t line = 1;
  size_t column = 1;
};

enum class DiagnosticLevel {
  Error,
  Warning,
};

struct Diagnostic {
  DiagnosticLevel level = DiagnosticLevel::Error;
  SourceLocation location;
  std::string message;
};

class DiagnosticEngine {
public:
  void ReportError(SourceLocation loc, std::string message);
  void ReportWarning(SourceLocation loc, std::string message);

  bool HasErrors() const;
  const std::vector<Diagnostic> &Diagnostics() const;
  std::string FormatAll(const std::string &file_name) const;
  void Clear();

private:
  std::vector<Diagnostic> diagnostics_;
};

} // namespace ccc
