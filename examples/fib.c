/**
 * ccc
 * Copyright (c) 2026 lrisguan <lrisguan@outlook.com>
 *
 * This program is released under the terms of the  GNU General Public License
 * version 2(GPLv2). See https://opensource.org/licenses/GPL-2.0 for more
 * information.
 *
 * Project homepage: https://github.com/lrisguan/ccc
 * Description: A frontend of C
 */

int fib(int n) {
  if (n <= 1) {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}

int main() {
  printf("%d\n", fib(10));
  return 0;
}
