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

int takes_int_ptr(int *p) {
  return p != 0;
}

int main() {
  int a[2];
  return takes_int_ptr(a) - 1;
}
