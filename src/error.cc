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

#include "error.h"

#include <sstream>

namespace ccc {

void DiagnosticEngine::ReportError(SourceLocation loc, std::string message) {
	diagnostics_.push_back(
			Diagnostic{DiagnosticLevel::Error, loc, std::move(message)});
}

void DiagnosticEngine::ReportWarning(SourceLocation loc, std::string message) {
	diagnostics_.push_back(
			Diagnostic{DiagnosticLevel::Warning, loc, std::move(message)});
}

bool DiagnosticEngine::HasErrors() const {
	for (const auto& d : diagnostics_) {
		if (d.level == DiagnosticLevel::Error) {
			return true;
		}
	}
	return false;
}

const std::vector<Diagnostic>& DiagnosticEngine::Diagnostics() const {
	return diagnostics_;
}

std::string DiagnosticEngine::FormatAll(const std::string& file_name) const {
	std::ostringstream oss;
	for (const auto& d : diagnostics_) {
		oss << file_name << ":" << d.location.line << ":" << d.location.column
				<< ": "
				<< (d.level == DiagnosticLevel::Error ? "error" : "warning") << ": "
				<< d.message << "\n";
	}
	return oss.str();
}

void DiagnosticEngine::Clear() {
	diagnostics_.clear();
}

}  // namespace ccc
