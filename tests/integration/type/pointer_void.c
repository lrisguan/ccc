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

void* as_void(char* p) {
  return p;
}

char* as_char(void* p) {
  return p;
}

int main() {
  char* s = "ok";
  void* v = as_void(s);
  char* t = as_char(v);
  if (t) {
    return 0;
  }
  return 1;
}
