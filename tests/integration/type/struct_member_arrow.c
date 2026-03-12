int main() {
  struct Pair {
    int a;
    int b;
  };

  struct Pair *p;
  p = (struct Pair *)0;

  if (0) {
    return p->a;
  }
  return 0;
}
