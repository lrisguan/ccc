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
