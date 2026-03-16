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

int takes_struct_ptr(struct S *p) {
  return p != 0;
}

int main() {
  struct S {
    int x;
    int y;
  };
  struct S *p;
  p = (struct S *)0;
  return takes_struct_ptr(p);
}
