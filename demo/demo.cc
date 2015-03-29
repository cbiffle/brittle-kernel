namespace demo {

static unsigned volatile counter;

struct Message { unsigned data[4]; };
struct ReceivedMessage {
  unsigned gate_brand;
  unsigned sender_brand;
  Message m;
};

__attribute__((naked))
static unsigned call(unsigned index, Message const * m, ReceivedMessage * out) {
  asm volatile (
      "svc #1\n"
      "bx lr\n"
      );
}

void main() {
  counter = 0;
  while (true) {
    auto m = Message { 1, counter, 0, 0 };
    ReceivedMessage rm;
    call(4, &m, &rm);
    ++counter;
  }
}

}  // namespace demo
