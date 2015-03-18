#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "protocol.h"

static unsigned send_x(TaskMessageType t, Message const & m) {
  write(1, &t, sizeof(t));
  write(1, &m, sizeof(m));

  SysMessageType r;
  read(0, &r, sizeof(r));
  if (r != SysMessageType::done_basic) {
    fprintf(stderr, "CHILD: uh-oh.\n");
    while (true);
  }
  BasicResponse b;
  read(0, &b, sizeof(b));
  return b.status;
}

/*
static unsigned send(Message const & m) {
  return send_x(TaskMessageType::send, m);
}
*/

static void send_nb(Message const & m) {
  auto r = send_x(TaskMessageType::send_nb, m);
  assert(r == 0);
}

static MessageResponse receive() {
  auto t = TaskMessageType::open_receive;
  write(1, &t, sizeof(t));

  SysMessageType r;
  read(0, &r, sizeof(r));
  if (r != SysMessageType::done_message) {
    fprintf(stderr, "CHILD: uh-oh.\n");
    while (true);
  }
  MessageResponse mr;
  read(0, &mr, sizeof(mr));
  return mr;
}

/*
static MessageResponse call(Message const & m) {
  auto t = TaskMessageType::call;
  write(1, &t, sizeof(t));
  write(1, &m, sizeof(m));

  SysMessageType r;
  read(0, &r, sizeof(r));
  if (r != SysMessageType::done_message) {
    fprintf(stderr, "CHILD: uh-oh.\n");
    while (true);
  }
  MessageResponse mr;
  read(0, &mr, sizeof(mr));
  return mr;
}
*/

static void handle_mon(MessageResponse const & req) {
  switch (req.mdata[0]) {
    case 0:  // heartbeat
      {
        Message reply {
          .target = 0,
          .mdata = { req.mdata[1], req.mdata[2], req.mdata[3], 0 },
        };
        send_nb(reply);
      }
      break;
  }
}

int main() {
  fprintf(stderr, "CHILD: starting message loop\n");
  while (true) {
    fprintf(stderr, "CHILD: entering open receive\n");
    MessageResponse mr = receive();

    switch (mr.brand) {
      case 0: handle_mon(mr); break;
    }
  }
}
