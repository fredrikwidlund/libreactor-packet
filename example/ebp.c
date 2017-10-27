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
#include <arpa/inet.h>
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

app_stream *lookup(app *app, uint32_t ip, uint16_t port)
{
  app_stream *s;

  list_foreach(&app->streams, s)
    if (s->ip == ip && s->port == port)
      return s;

  list_foreach(&app->streams, s)
    if (ip < s->ip || (ip == s->ip && port < s->port))
      break;

  list_insert(s, (app_stream []) {{.ip = ip, .port = port}}, sizeof *s);
  return list_previous(s);
}

static void record(app *app, uint32_t ip, uint16_t port, uint8_t *data, size_t size)
{
  app_stream *s;

  if ((size - 12) % 188 || data[0] != 0x80 || data[1] != 0x21)
    return;

  s = lookup(app, ip, port);
  s->packets ++;
  s->bytes += size;
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
  app_stream *s;
  char ip_string[16];
  uint32_t ip;

  (void) printf("\033[H\033[J");
  list_foreach(&app->streams, s)
    {
      ip = htonl(s->ip);
      inet_ntop(AF_INET, &ip, ip_string, sizeof ip_string); 
      (void) printf("%s:%u - pps %lu, rate %.03f kbps\n", ip_string, s->port, s->packets, 8. * (double) s->bytes / 1000.);
      s->packets = 0;
      s->bytes = 0;
    }
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
  extern char *__progname;
  app app = {0};
  int e;

  if (argc != 2)
    errx(1, "Usage: %s <interface>", __progname);

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
