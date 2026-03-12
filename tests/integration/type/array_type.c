int takes_int_ptr(int *p) {
  return p != 0;
}

int main() {
  int a[2];
  return takes_int_ptr(a) - 1;
}
