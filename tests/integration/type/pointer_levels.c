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

char** passthrough(char** p) {
  return p;
}

int main() {
  char** p;
  char** q = passthrough(p);
  if (q == p) {
    return 0;
  }
  return 1;
}
