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

#include "Preprocess.h"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ccc {

namespace {

bool ReadWholeFile(const std::string &path, std::string &out_text) {
  std::ifstream in(path);
  if (!in.is_open()) {
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  out_text = ss.str();
  return true;
}

bool IsIdentifierStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool IsIdentifierChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

std::string Trim(std::string_view v) {
  size_t begin = 0;
  while (begin < v.size() &&
         std::isspace(static_cast<unsigned char>(v[begin]))) {
    ++begin;
  }
  size_t end = v.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(v[end - 1]))) {
    --end;
  }
  return std::string(v.substr(begin, end - begin));
}

bool StartsWith(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() &&
         text.compare(0, prefix.size(), prefix) == 0;
}

bool StartsWithWord(std::string_view line, std::string_view word) {
  if (!StartsWith(line, word)) {
    return false;
  }
  if (line.size() == word.size()) {
    return true;
  }
  const unsigned char next = static_cast<unsigned char>(line[word.size()]);
  return !std::isalnum(next) && next != '_';
}

std::optional<std::pair<std::string, std::string>>
ParseDefineSpec(const std::string &spec) {
  const size_t eq = spec.find('=');
  std::string name = spec.substr(0, eq);
  if (name.empty() || !IsIdentifierStart(name.front())) {
    return std::nullopt;
  }
  for (char c : name) {
    if (!IsIdentifierChar(c)) {
      return std::nullopt;
    }
  }

  std::string value = "1";
  if (eq != std::string::npos) {
    value = spec.substr(eq + 1);
  }
  return std::make_pair(std::move(name), std::move(value));
}

struct IfFrame {
  bool parent_active = true;
  bool branch_taken = false;
  bool current_active = true;
  bool else_seen = false;
};

struct PreprocessContext {
  std::unordered_map<std::string, std::string> macros;
  std::unordered_set<std::string> function_like_macros;
  std::vector<std::filesystem::path> include_dirs;
  std::unordered_set<std::string> include_stack;
  std::unordered_set<std::string> pragma_once_files;
};

struct IncludeSpec {
  std::string name;
  bool is_system = false;
};

bool IsActive(const std::vector<IfFrame> &frames) {
  if (frames.empty()) {
    return true;
  }
  return frames.back().current_active;
}

void AddIncludeDirIfExists(std::vector<std::filesystem::path> &dirs,
                           std::unordered_set<std::string> &seen,
                           const std::filesystem::path &path) {
  if (path.empty() || !std::filesystem::exists(path)) {
    return;
  }
  const std::string key = std::filesystem::weakly_canonical(path).string();
  if (seen.insert(key).second) {
    dirs.push_back(path);
  }
}

void AddDefaultSystemIncludeDirs(PreprocessContext &ctx) {
  std::unordered_set<std::string> seen;
  for (const auto &dir : ctx.include_dirs) {
    if (std::filesystem::exists(dir)) {
      seen.insert(std::filesystem::weakly_canonical(dir).string());
    }
  }

  AddIncludeDirIfExists(ctx.include_dirs, seen, "/usr/local/include");
  AddIncludeDirIfExists(ctx.include_dirs, seen, "/usr/include");
  AddIncludeDirIfExists(ctx.include_dirs, seen,
                        "/usr/include/x86_64-linux-gnu");
  AddIncludeDirIfExists(ctx.include_dirs, seen,
                        "/usr/include/aarch64-linux-gnu");

  const std::filesystem::path gcc_root("/usr/lib/gcc");
  std::vector<std::string> preferred_targets;
#if defined(__aarch64__)
  preferred_targets.push_back("aarch64-linux-gnu");
#elif defined(__x86_64__)
  preferred_targets.push_back("x86_64-linux-gnu");
#endif

  if (std::filesystem::exists(gcc_root)) {
    for (const auto &target_entry :
         std::filesystem::directory_iterator(gcc_root)) {
      if (!target_entry.is_directory()) {
        continue;
      }
      if (!preferred_targets.empty()) {
        const std::string target_name = target_entry.path().filename().string();
        bool allowed = false;
        for (const auto &pref : preferred_targets) {
          if (target_name == pref) {
            allowed = true;
            break;
          }
        }
        if (!allowed) {
          continue;
        }
      }
      for (const auto &ver_entry :
           std::filesystem::directory_iterator(target_entry.path())) {
        if (!ver_entry.is_directory()) {
          continue;
        }
        AddIncludeDirIfExists(ctx.include_dirs, seen,
                              ver_entry.path() / "include");
        AddIncludeDirIfExists(ctx.include_dirs, seen,
                              ver_entry.path() / "include-fixed");
      }
    }
  }

  const std::filesystem::path clang_root("/usr/lib/clang");
  if (std::filesystem::exists(clang_root)) {
    for (const auto &ver_entry :
         std::filesystem::directory_iterator(clang_root)) {
      if (!ver_entry.is_directory()) {
        continue;
      }
      AddIncludeDirIfExists(ctx.include_dirs, seen,
                            ver_entry.path() / "include");
    }
  }
}

long long ParseIntegerLiteral(std::string_view s) {
  const std::string text = Trim(s);
  if (text.empty()) {
    return 0;
  }
  errno = 0;
  char *end = nullptr;
  const long long v = std::strtoll(text.c_str(), &end, 0);
  if (errno != 0 || end == text.c_str() || *end != '\0') {
    return 0;
  }
  return v;
}

class PPExprParser {
public:
  PPExprParser(std::string_view expr,
               const std::unordered_map<std::string, std::string> &macros)
      : expr_(expr), macros_(macros) {}

  long long Parse() {
    const long long value = ParseOr();
    SkipSpaces();
    return value;
  }

private:
  void SkipSpaces() {
    while (pos_ < expr_.size() &&
           std::isspace(static_cast<unsigned char>(expr_[pos_]))) {
      ++pos_;
    }
  }

  bool Consume(std::string_view token) {
    SkipSpaces();
    if (StartsWith(expr_.substr(pos_), token)) {
      pos_ += token.size();
      return true;
    }
    return false;
  }

  std::string ParseIdentifier() {
    SkipSpaces();
    if (pos_ >= expr_.size() || !IsIdentifierStart(expr_[pos_])) {
      return {};
    }
    const size_t begin = pos_;
    ++pos_;
    while (pos_ < expr_.size() && IsIdentifierChar(expr_[pos_])) {
      ++pos_;
    }
    return std::string(expr_.substr(begin, pos_ - begin));
  }

  long long MacroNumericValue(const std::string &name) {
    const auto it = macros_.find(name);
    if (it == macros_.end()) {
      return 0;
    }
    return ParseIntegerLiteral(it->second);
  }

  long long ParsePrimary() {
    SkipSpaces();
    if (Consume("(")) {
      const long long v = ParseOr();
      Consume(")");
      return v;
    }

    if (StartsWith(expr_.substr(pos_), "defined")) {
      pos_ += 7;
      SkipSpaces();
      std::string name;
      if (Consume("(")) {
        name = ParseIdentifier();
        Consume(")");
      } else {
        name = ParseIdentifier();
      }
      return macros_.find(name) != macros_.end() ? 1 : 0;
    }

    if (pos_ < expr_.size() && IsIdentifierStart(expr_[pos_])) {
      const std::string ident = ParseIdentifier();
      return MacroNumericValue(ident);
    }

    const size_t begin = pos_;
    while (pos_ < expr_.size()) {
      const char c = expr_[pos_];
      if (std::isspace(static_cast<unsigned char>(c)) || c == ')' || c == '(' ||
          c == '!' || c == '&' || c == '|') {
        break;
      }
      ++pos_;
    }
    return ParseIntegerLiteral(expr_.substr(begin, pos_ - begin));
  }

  long long ParseUnary() {
    SkipSpaces();
    if (Consume("!")) {
      return ParseUnary() ? 0 : 1;
    }
    return ParsePrimary();
  }

  long long ParseMul() {
    long long lhs = ParseUnary();
    while (true) {
      if (Consume("*")) {
        lhs *= ParseUnary();
      } else if (Consume("/")) {
        const long long rhs = ParseUnary();
        lhs = rhs == 0 ? 0 : (lhs / rhs);
      } else if (Consume("%")) {
        const long long rhs = ParseUnary();
        lhs = rhs == 0 ? 0 : (lhs % rhs);
      } else {
        break;
      }
    }
    return lhs;
  }

  long long ParseAdd() {
    long long lhs = ParseMul();
    while (true) {
      if (Consume("+")) {
        lhs += ParseMul();
      } else if (Consume("-")) {
        lhs -= ParseMul();
      } else {
        break;
      }
    }
    return lhs;
  }

  long long ParseRel() {
    long long lhs = ParseAdd();
    while (true) {
      if (Consume("<=")) {
        lhs = lhs <= ParseAdd() ? 1 : 0;
      } else if (Consume(">=")) {
        lhs = lhs >= ParseAdd() ? 1 : 0;
      } else if (Consume("<")) {
        lhs = lhs < ParseAdd() ? 1 : 0;
      } else if (Consume(">")) {
        lhs = lhs > ParseAdd() ? 1 : 0;
      } else {
        break;
      }
    }
    return lhs;
  }

  long long ParseEq() {
    long long lhs = ParseRel();
    while (true) {
      if (Consume("==")) {
        lhs = lhs == ParseRel() ? 1 : 0;
      } else if (Consume("!=")) {
        lhs = lhs != ParseRel() ? 1 : 0;
      } else {
        break;
      }
    }
    return lhs;
  }

  long long ParseAnd() {
    long long lhs = ParseEq();
    while (Consume("&&")) {
      const long long rhs = ParseEq();
      lhs = (lhs != 0 && rhs != 0) ? 1 : 0;
    }
    return lhs;
  }

  long long ParseOr() {
    long long lhs = ParseAnd();
    while (Consume("||")) {
      const long long rhs = ParseAnd();
      lhs = (lhs != 0 || rhs != 0) ? 1 : 0;
    }
    return lhs;
  }

  std::string_view expr_;
  size_t pos_ = 0;
  const std::unordered_map<std::string, std::string> &macros_;
};

bool EvaluateIfExpr(
    std::string_view expr,
    const std::unordered_map<std::string, std::string> &macros) {
  PPExprParser parser(expr, macros);
  return parser.Parse() != 0;
}

bool ParseDefineDirective(const std::string &payload, std::string &name,
                          std::string &value, bool &is_function_like) {
  is_function_like = false;
  size_t i = 0;
  while (i < payload.size() &&
         std::isspace(static_cast<unsigned char>(payload[i]))) {
    ++i;
  }
  if (i >= payload.size() || !IsIdentifierStart(payload[i])) {
    return false;
  }

  const size_t name_begin = i;
  ++i;
  while (i < payload.size() && IsIdentifierChar(payload[i])) {
    ++i;
  }
  name = payload.substr(name_begin, i - name_begin);

  if (i < payload.size() && payload[i] == '(') {
    is_function_like = true;
    int depth = 1;
    ++i;
    while (i < payload.size() && depth > 0) {
      if (payload[i] == '(') {
        ++depth;
      } else if (payload[i] == ')') {
        --depth;
      }
      ++i;
    }
    if (depth != 0) {
      return false;
    }
  }

  while (i < payload.size() &&
         std::isspace(static_cast<unsigned char>(payload[i]))) {
    ++i;
  }
  value = i < payload.size() ? payload.substr(i) : "1";
  if (value.empty()) {
    value = "1";
  }
  return true;
}

void AppendPreprocessError(const std::string &path, size_t line_number,
                           const std::string &message,
                           std::string &error_message) {
  error_message +=
      path + ":" + std::to_string(line_number) + ":1: error: " + message + "\n";
}

std::string
ExpandMacrosOnce(const std::string &line,
                 const std::unordered_map<std::string, std::string> &macros,
                 const std::unordered_set<std::string> &function_like_macros) {
  constexpr size_t kMaxExpandedLineLength = 1 << 20;
  std::string out;
  out.reserve(line.size());

  size_t i = 0;
  while (i < line.size()) {
    const char c = line[i];
    if (c == '"') {
      out.push_back(c);
      ++i;
      while (i < line.size()) {
        out.push_back(line[i]);
        if (line[i] == '"' && line[i - 1] != '\\') {
          ++i;
          break;
        }
        ++i;
      }
      continue;
    }

    if (c == '\'') {
      out.push_back(c);
      ++i;
      while (i < line.size()) {
        out.push_back(line[i]);
        if (line[i] == '\'' && line[i - 1] != '\\') {
          ++i;
          break;
        }
        ++i;
      }
      continue;
    }

    if (IsIdentifierStart(c)) {
      const size_t begin = i;
      ++i;
      while (i < line.size() && IsIdentifierChar(line[i])) {
        ++i;
      }
      const std::string name = line.substr(begin, i - begin);
      const auto it = macros.find(name);
      if (it != macros.end() &&
          function_like_macros.find(name) == function_like_macros.end()) {
        out += it->second;
      } else {
        out += name;
      }
      if (out.size() > kMaxExpandedLineLength) {
        return out;
      }
      continue;
    }

    out.push_back(c);
    ++i;
  }

  return out;
}

std::string
ExpandMacros(const std::string &line,
             const std::unordered_map<std::string, std::string> &macros,
             const std::unordered_set<std::string> &function_like_macros) {
  constexpr size_t kMaxExpandedLineLength = 1 << 20;
  std::string current = line;
  for (int i = 0; i < 16; ++i) {
    std::string next = ExpandMacrosOnce(current, macros, function_like_macros);
    if (next.size() > kMaxExpandedLineLength) {
      break;
    }
    if (!current.empty() && next.size() > current.size() * 8) {
      break;
    }
    if (next == current) {
      break;
    }
    current = std::move(next);
  }
  return current;
}

std::string StripComments(const std::string &line, bool &in_block_comment) {
  std::string out;
  out.reserve(line.size());

  bool in_string = false;
  bool in_char = false;
  bool escaped = false;

  size_t i = 0;
  while (i < line.size()) {
    const char c = line[i];

    if (in_block_comment) {
      if (c == '*' && i + 1 < line.size() && line[i + 1] == '/') {
        in_block_comment = false;
        i += 2;
      } else {
        ++i;
      }
      continue;
    }

    if (in_string) {
      out.push_back(c);
      if (!escaped && c == '"') {
        in_string = false;
      }
      escaped = (!escaped && c == '\\');
      if (c != '\\') {
        escaped = false;
      }
      ++i;
      continue;
    }

    if (in_char) {
      out.push_back(c);
      if (!escaped && c == '\'') {
        in_char = false;
      }
      escaped = (!escaped && c == '\\');
      if (c != '\\') {
        escaped = false;
      }
      ++i;
      continue;
    }

    if (c == '/' && i + 1 < line.size() && line[i + 1] == '/') {
      break;
    }
    if (c == '/' && i + 1 < line.size() && line[i + 1] == '*') {
      in_block_comment = true;
      i += 2;
      continue;
    }
    if (c == '"') {
      in_string = true;
      escaped = false;
      out.push_back(c);
      ++i;
      continue;
    }
    if (c == '\'') {
      in_char = true;
      escaped = false;
      out.push_back(c);
      ++i;
      continue;
    }

    out.push_back(c);
    ++i;
  }

  return out;
}

bool ResolveIncludePath(const std::filesystem::path &current_file,
                        const std::string &include_name, bool is_system,
                        bool include_next,
                        const std::vector<std::filesystem::path> &include_dirs,
                        std::filesystem::path &resolved) {
  if (include_name.empty()) {
    return false;
  }

  std::filesystem::path include_path(include_name);
  if (include_path.is_absolute() && std::filesystem::exists(include_path)) {
    resolved = std::filesystem::weakly_canonical(include_path);
    return true;
  }

  if (!is_system) {
    if (!include_next) {
      const std::filesystem::path local_candidate =
          current_file.parent_path() / include_name;
      if (std::filesystem::exists(local_candidate)) {
        resolved = std::filesystem::weakly_canonical(local_candidate);
        return true;
      }
    }
  }

  bool can_search_dir = !include_next;
  std::filesystem::path current_dir;
  if (include_next) {
    current_dir = std::filesystem::weakly_canonical(current_file.parent_path());
  }

  for (const auto &dir : include_dirs) {
    std::filesystem::path canonical_dir = dir;
    if (std::filesystem::exists(dir)) {
      canonical_dir = std::filesystem::weakly_canonical(dir);
    }

    if (!can_search_dir) {
      if (canonical_dir == current_dir) {
        can_search_dir = true;
      }
      continue;
    }

    const std::filesystem::path candidate = dir / include_name;
    if (std::filesystem::exists(candidate)) {
      resolved = std::filesystem::weakly_canonical(candidate);
      return true;
    }
  }

  return false;
}

bool ParseIncludeName(std::string_view arg, IncludeSpec &include_spec) {
  const std::string t = Trim(arg);
  if (t.size() >= 2 && t.front() == '"') {
    const size_t end = t.find('"', 1);
    if (end == std::string::npos) {
      return false;
    }
    include_spec.name = t.substr(1, end - 1);
    include_spec.is_system = false;
    return true;
  }
  if (t.size() >= 2 && t.front() == '<') {
    const size_t end = t.find('>', 1);
    if (end == std::string::npos) {
      return false;
    }
    include_spec.name = t.substr(1, end - 1);
    include_spec.is_system = true;
    return true;
  }
  return false;
}

bool PreprocessFile(const std::filesystem::path &path, PreprocessContext &ctx,
                    std::string &output, std::string &error_message,
                    bool emit_non_directive_lines);

bool PreprocessText(std::string_view virtual_path, const std::string &content,
                    PreprocessContext &ctx, std::string &output,
                    std::string &error_message, bool emit_non_directive_lines) {
  const std::string canonical_str(virtual_path);
  if (ctx.include_stack.find(canonical_str) != ctx.include_stack.end()) {
    error_message = "include cycle detected at: " + canonical_str;
    return false;
  }

  ctx.include_stack.insert(canonical_str);

  std::istringstream in(content);
  std::string raw_line;
  std::string line;
  size_t line_number = 0;
  size_t physical_line = 0;
  size_t logical_start_line = 0;
  std::vector<IfFrame> if_frames;
  bool in_block_comment = false;

  while (std::getline(in, raw_line)) {
    ++physical_line;
    if (line.empty()) {
      logical_start_line = physical_line;
    }

    line += raw_line;
    if (!line.empty() && line.back() == '\\') {
      line.pop_back();
      continue;
    }
    line_number = logical_start_line;
    const std::string logical_line = line;
    line.clear();
    const std::string cleaned_line =
        StripComments(logical_line, in_block_comment);

    std::string_view v(cleaned_line);
    size_t i = 0;
    while (i < v.size() && std::isspace(static_cast<unsigned char>(v[i]))) {
      ++i;
    }

    const bool is_directive = i < v.size() && v[i] == '#';
    const bool active = IsActive(if_frames);
    if (!is_directive) {
      if (active && emit_non_directive_lines) {
        output +=
            ExpandMacros(cleaned_line, ctx.macros, ctx.function_like_macros);
        output.push_back('\n');
      }
      continue;
    }

    std::string_view directive = v.substr(i + 1);
    while (!directive.empty() &&
           std::isspace(static_cast<unsigned char>(directive.front()))) {
      directive.remove_prefix(1);
    }

    if (StartsWithWord(directive, "ifdef")) {
      const std::string name = Trim(directive.substr(5));
      const bool parent_active = IsActive(if_frames);
      const bool cond = ctx.macros.find(name) != ctx.macros.end();
      if_frames.push_back(IfFrame{parent_active, parent_active && cond,
                                  parent_active && cond, false});
      continue;
    }

    if (StartsWithWord(directive, "ifndef")) {
      const std::string name = Trim(directive.substr(6));
      const bool parent_active = IsActive(if_frames);
      const bool cond = ctx.macros.find(name) == ctx.macros.end();
      if_frames.push_back(IfFrame{parent_active, parent_active && cond,
                                  parent_active && cond, false});
      continue;
    }

    if (StartsWithWord(directive, "if")) {
      const bool parent_active = IsActive(if_frames);
      const bool cond = EvaluateIfExpr(directive.substr(2), ctx.macros);
      if_frames.push_back(IfFrame{parent_active, parent_active && cond,
                                  parent_active && cond, false});
      continue;
    }

    if (StartsWithWord(directive, "elif")) {
      if (if_frames.empty()) {
        AppendPreprocessError(canonical_str, line_number,
                              "#elif without matching #if", error_message);
        ctx.include_stack.erase(canonical_str);
        return false;
      }
      IfFrame &f = if_frames.back();
      if (f.else_seen) {
        AppendPreprocessError(canonical_str, line_number, "#elif after #else",
                              error_message);
        ctx.include_stack.erase(canonical_str);
        return false;
      }
      const bool cond = EvaluateIfExpr(directive.substr(4), ctx.macros);
      const bool take = f.parent_active && !f.branch_taken && cond;
      f.current_active = take;
      f.branch_taken = f.branch_taken || take;
      continue;
    }

    if (StartsWithWord(directive, "else")) {
      if (if_frames.empty()) {
        AppendPreprocessError(canonical_str, line_number,
                              "#else without matching #if", error_message);
        ctx.include_stack.erase(canonical_str);
        return false;
      }
      if (if_frames.back().else_seen) {
        AppendPreprocessError(canonical_str, line_number,
                              "duplicate #else in conditional block",
                              error_message);
        ctx.include_stack.erase(canonical_str);
        return false;
      }
      if_frames.back().else_seen = true;
      if_frames.back().current_active =
          if_frames.back().parent_active && !if_frames.back().branch_taken;
      if_frames.back().branch_taken = true;
      continue;
    }

    if (StartsWithWord(directive, "endif")) {
      if (if_frames.empty()) {
        AppendPreprocessError(canonical_str, line_number,
                              "#endif without matching #if", error_message);
        ctx.include_stack.erase(canonical_str);
        return false;
      }
      if_frames.pop_back();
      continue;
    }

    if (!active) {
      continue;
    }

    if (StartsWithWord(directive, "pragma once")) {
      ctx.pragma_once_files.insert(canonical_str);
      continue;
    }

    if (StartsWithWord(directive, "include") ||
        StartsWithWord(directive, "include_next")) {
      IncludeSpec include_spec;
      std::string_view include_arg;
      bool is_include_next = false;
      if (StartsWithWord(directive, "include_next")) {
        include_arg = directive.substr(12);
        is_include_next = true;
      } else {
        include_arg = directive.substr(7);
      }
      if (!ParseIncludeName(include_arg, include_spec)) {
        AppendPreprocessError(canonical_str, line_number,
                              "unsupported #include format", error_message);
        ctx.include_stack.erase(canonical_str);
        return false;
      }

      std::filesystem::path include_path;
      if (ResolveIncludePath(std::filesystem::path(canonical_str),
                             include_spec.name, include_spec.is_system,
                             is_include_next, ctx.include_dirs, include_path)) {
        const bool child_emit_non_directive_lines =
            emit_non_directive_lines && !include_spec.is_system;
        if (!PreprocessFile(include_path, ctx, output, error_message,
                            child_emit_non_directive_lines)) {
          ctx.include_stack.erase(canonical_str);
          return false;
        }
        continue;
      }

      AppendPreprocessError(canonical_str, line_number,
                            "include not found: " + include_spec.name,
                            error_message);
      ctx.include_stack.erase(canonical_str);
      return false;
    }

    if (StartsWithWord(directive, "define")) {
      const std::string payload = Trim(directive.substr(6));
      std::string name;
      std::string value;
      bool is_function_like = false;
      if (!ParseDefineDirective(payload, name, value, is_function_like)) {
        AppendPreprocessError(canonical_str, line_number,
                              "invalid macro name in #define", error_message);
        ctx.include_stack.erase(canonical_str);
        return false;
      }
      ctx.macros[name] = value;
      if (is_function_like) {
        ctx.function_like_macros.insert(name);
      } else {
        ctx.function_like_macros.erase(name);
      }
      continue;
    }

    if (StartsWithWord(directive, "undef")) {
      const std::string name = Trim(directive.substr(5));
      if (!name.empty()) {
        ctx.macros.erase(name);
        ctx.function_like_macros.erase(name);
      }
      continue;
    }

    AppendPreprocessError(canonical_str, line_number,
                          "unsupported preprocessor directive", error_message);
    ctx.include_stack.erase(canonical_str);
    return false;
  }

  if (!line.empty()) {
    line_number = logical_start_line;
    const std::string cleaned_line = StripComments(line, in_block_comment);
    std::string_view v(cleaned_line);
    size_t i = 0;
    while (i < v.size() && std::isspace(static_cast<unsigned char>(v[i]))) {
      ++i;
    }
    const bool is_directive = i < v.size() && v[i] == '#';
    const bool active = IsActive(if_frames);
    if (!is_directive) {
      if (active && emit_non_directive_lines) {
        output +=
            ExpandMacros(cleaned_line, ctx.macros, ctx.function_like_macros);
        output.push_back('\n');
      }
    }
  }

  if (!if_frames.empty()) {
    if (!emit_non_directive_lines) {
      // System headers are parsed in macro-only mode. If we fail to model
      // every conditional form exactly, keep going instead of blocking build.
      ctx.include_stack.erase(canonical_str);
      return true;
    }
    AppendPreprocessError(canonical_str, line_number,
                          "unterminated #if/#ifdef/#ifndef block",
                          error_message);
    ctx.include_stack.erase(canonical_str);
    return false;
  }

  ctx.include_stack.erase(canonical_str);
  return true;
}

bool PreprocessFile(const std::filesystem::path &path, PreprocessContext &ctx,
                    std::string &output, std::string &error_message,
                    bool emit_non_directive_lines) {
  const std::filesystem::path canonical =
      std::filesystem::weakly_canonical(path);
  const std::string canonical_str = canonical.string();
  if (ctx.pragma_once_files.find(canonical_str) !=
      ctx.pragma_once_files.end()) {
    return true;
  }
  if (ctx.include_stack.find(canonical_str) != ctx.include_stack.end()) {
    error_message = "include cycle detected at: " + canonical_str;
    return false;
  }

  std::string content;
  if (!ReadWholeFile(canonical_str, content)) {
    error_message = "failed to read include file: " + canonical_str;
    return false;
  }

  return PreprocessText(canonical_str, content, ctx, output, error_message,
                        emit_non_directive_lines);
}

} // namespace

bool PreprocessSource(const CompileRequest &request, std::string &source,
                      std::string &error_message) {
  PreprocessContext ctx;
  ctx.include_dirs.reserve(request.include_dirs.size());
  for (const auto &dir : request.include_dirs) {
    ctx.include_dirs.push_back(std::filesystem::path(dir));
  }
  AddDefaultSystemIncludeDirs(ctx);

  for (const auto &spec : request.macro_defines) {
    const auto parsed = ParseDefineSpec(spec);
    if (!parsed.has_value()) {
      error_message = "invalid -D macro definition: " + spec;
      return false;
    }
    ctx.macros[parsed->first] = parsed->second;
  }

  for (const auto &name : request.macro_undefines) {
    if (!name.empty()) {
      ctx.macros.erase(name);
    }
  }

  source.clear();
  if (!PreprocessFile(std::filesystem::path(request.input_file), ctx, source,
                      error_message, true)) {
    return false;
  }
  return true;
}

} // namespace ccc
