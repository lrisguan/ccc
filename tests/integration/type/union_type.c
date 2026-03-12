int takes_union_ptr(union U *p) {
  return p == 0;
}

int main() {
  union U {
    int i;
    float f;
  };
  union U *p;
  p = (union U *)0;
  return takes_union_ptr(p) - 1;
}
