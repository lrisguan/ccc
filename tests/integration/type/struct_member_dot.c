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

int main() {
  struct Pair {
    int a;
    int b;
  };

  struct Pair p;
  p.a = 40;
  p.b = 2;
  return (p.a + p.b) - 42;
}
