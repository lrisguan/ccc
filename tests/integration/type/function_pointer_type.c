int add1(int x) {
  return x + 1;
}

int apply(int (*fn)(int x), int v) {
  return fn != 0;
}

int main() {
  int (*fp)(int x);
  fp = add1;
  return apply(fp, 41) - 1;
}
