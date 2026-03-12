/**
 * ccc
 * Copyright (c) 2026 lrisguan <lrisguan@outlook.com>
 *
 * This program is released under the terms of the GNU General Public License version 2 (GPLv2).
 * See https://opensource.org/licenses/GPL-2.0 for more information.
 *
 * Project homepage: https://github.com/lrisguan/ccc
 * Description: A frontend of C
 */

int main() {
  int sum = 0;
  int i = 0;
  int j = 0;

  for (i = 0; i < 5; i = i + 1) {
    for (j = 0; j < 5; j = j + 1) {
      if (((i + j) % 2) == 0) {
        sum = sum + 1;
      } else {
        sum = sum + 2;
      }
    }
  }

  return sum - 37;
}
