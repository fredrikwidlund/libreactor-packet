#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <err.h>

#include <dynamic.h>
#include <reactor.h>
#include <reactor_packet.h>

void event(void *state, int type, void *data)
{
  reactor_packet *p = state;

  switch (type)
    {
    case REACTOR_PACKET_EVENT_FRAME:
      fprintf(stderr, "%p %d %p\n", (void *) p, type, data);
      break;
    case REACTOR_PACKET_EVENT_ERROR:
      errx(1, "%p %d %p\n", (void *) p, type, data);
    default:
      fprintf(stderr, "%p %d %p\n", (void *) p, type, data);
      break;
    }
}

int main(int argc, char **argv)
{
  reactor_packet p;
  int e;

  if (argc != 2)
    errx(1, "usage: ebp <interface>");

  reactor_core_construct();
  reactor_packet_open(&p, event, &p, argv[1]);
  reactor_packet_start(&p);

  e = reactor_core_run();
  if (e == -1)
    err(1, "reactor_core_run");

  reactor_core_destruct();
}
