int takes_enum(enum Color c) {
  return c;
}

int main() {
  enum Color {
    RED,
    GREEN,
    BLUE
  };
  enum Color c;
  c = (enum Color)7;
  return takes_enum(c) - 7;
}
