namespace demo {

static unsigned volatile counter;

void main() {
  counter = 0;
  while (true) {
    ++counter;
  }
}

}  // namespace demo
