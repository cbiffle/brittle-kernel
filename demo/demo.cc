namespace demo {

static unsigned volatile counter;

struct Message { unsigned data[4]; };

static unsigned send(unsigned index, Message const * m) {
  unsigned r0 asm ("r0") = index;
  Message const * r1 asm("r1") = m;
  asm volatile ("svc #0" : "+r"(r0) : "r"(r1), "m"(*m));
  return r0;
}

void main() {
  counter = 0;
  while (true) {
    auto m = Message { counter, 0, 0, 0 };
    send(0, &m);
    ++counter;
  }
}

}  // namespace demo
