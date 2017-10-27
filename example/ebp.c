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
#include <linux/ip.h>
#include <linux/udp.h>
#include <err.h>

#include <dynamic.h>
#include <reactor.h>
#include <reactor_packet.h>

typedef struct app_stream app_stream;
struct app_stream
{
  uint32_t ip;
  uint16_t port;
  size_t   packets;
  size_t   bytes;
};

typedef struct app app;
struct app
{
  reactor_packet packet;
  reactor_timer  timer;
  list           streams;
};

static void record(app *app, uint32_t ip, uint16_t port, uint8_t *data, size_t size)
{
  if (size - 12 % 188)
    return;

  fprintf(stderr, "%02x %02x\n", data[0], data[1]);
}


static void receive(app *app, reactor_packet_frame *f)
{
  struct iphdr *ip = f->layer[1].begin;
  struct udphdr *udp = f->layer[2].begin;

  if (f->layer[0].type == REACTOR_PACKET_PROTOCOL_ETHER &&
      f->layer[1].type == REACTOR_PACKET_PROTOCOL_IP &&
      f->layer[2].type == REACTOR_PACKET_PROTOCOL_UDP &&
      f->layer[3].type == REACTOR_PACKET_PROTOCOL_DATA)
    record(app, ntohl(ip->daddr), ntohs(udp->dest), f->layer[3].begin, (uint8_t *) f->layer[3].end - (uint8_t *) f->layer[3].begin);
}

static void report(app *app)
{
  //(void) fprintf(stderr, "[stats] packets %lu, udp data %lu\n", app->packets, app->udp_bytes);
}

static void event(void *state, int type, void *data)
{
  app *app = state;
  reactor_packet_frame *f = data;

  switch (type)
    {
    case REACTOR_PACKET_EVENT_FRAME:
      receive(app, f);
      break;
    case REACTOR_PACKET_EVENT_INVALID_FRAME:
      warnx("invalid frame received");
      break;
    default:
    case REACTOR_PACKET_EVENT_ERROR:
      errx(1, "reactor packet error: %s", (char *) data);
    }
}

static void timer(void *state, int type, void *data)
{
  app *app = state;

  (void) data;
  switch (type)
    {
    case REACTOR_TIMER_EVENT_CALL:
      report(app);
      break;
    default:
      errx(1, "invalid timer event: %d", type);
    }
}

int main(int argc, char **argv)
{
  app app = {0};
  int e;

  if (argc != 2)
    errx(1, "usage: ebp <interface>");

  reactor_core_construct();
  list_construct(&app.streams);
  reactor_packet_open(&app.packet, event, &app, argv[1]);
  reactor_timer_open(&app.timer, timer, &app, 1, 1000000000);

  reactor_packet_start(&app.packet);
  e = reactor_core_run();
  if (e == -1)
    err(1, "reactor_core_run");

  reactor_core_destruct();
}
