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
