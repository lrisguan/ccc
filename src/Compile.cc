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

#include "Compile.h"

#include <filesystem>

#include "IRGenerator.h"
#include "Lexer.h"
#include "Parser.h"
#include "Preprocess.h"
#include "SemanticAnalyzer.h"
#include "error.h"

namespace ccc {

bool CompileToObjectFile(const CompileRequest &request,
                         std::string &error_message) {
  std::string source;
  if (!PreprocessSource(request, source, error_message)) {
    return false;
  }

  DiagnosticEngine diag;
  Lexer lexer(source, diag);
  Parser parser(lexer, diag);
  std::unique_ptr<Program> program = parser.ParseProgram();
  if (diag.HasErrors()) {
    error_message = diag.FormatAll(request.input_file);
    return false;
  }

  SemanticAnalyzer sema(diag);
  if (!sema.Analyze(*program)) {
    error_message = diag.FormatAll(request.input_file);
    return false;
  }

  IRGenerator irgen(diag);
  const std::string module_name =
      request.module_name.empty()
          ? std::filesystem::path(request.input_file).filename().string()
          : request.module_name;
  if (!irgen.GenerateModule(*program, module_name)) {
    error_message = diag.FormatAll(request.input_file);
    return false;
  }

  if (!irgen.RunOptimizations(request.opt_level)) {
    error_message = diag.FormatAll(request.input_file);
    return false;
  }

  if (!request.ir_output_file.empty() &&
      !irgen.EmitIRFile(request.ir_output_file)) {
    error_message = "failed to emit LLVM IR file: " + request.ir_output_file;
    return false;
  }

  if (!irgen.EmitObjectFile(request.object_path)) {
    error_message = "failed to emit object file: " + request.object_path;
    return false;
  }

  return true;
}

} // namespace ccc
