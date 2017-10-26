#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stropts.h>
#include <netdb.h>
#include <time.h>
#include <err.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <net/if.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/filter.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/sockios.h>
#include <arpa/inet.h>

#include <dynamic.h>
#include <reactor.h>

#include "reactor_packet.h"

static void reactor_packet_error(reactor_packet *p, char *reason)
{
  p->state = REACTOR_PACKET_STATE_ERROR;
  reactor_user_dispatch(&p->user, REACTOR_PACKET_EVENT_ERROR, reason);
}

static void reactor_packet_frame_ip(reactor_packet_frame *f)
{
  struct iphdr *ip;

  ip = f->layer[1].begin;
  fprintf(stderr, "[ip proto %d]\n", ip->protocol);

}

static void reactor_packet_frame_ether(reactor_packet_frame *f)
{
  struct ethhdr *ether;
  void *begin;

  ether = f->layer[0].begin;
  begin = (uint8_t *) f->layer[0].begin + sizeof *ether;
  if (begin > f->layer[0].end)
    return;
  f->layer[0].end = begin;
  f->layer[1].begin = begin;
  f->layer[1].end = f->layer[0].end;

  switch (ntohs(ether->h_proto))
    {
    case ETH_P_IP:
      f->layer[1].type = ETH_P_IP;
      reactor_packet_frame_ip(f);
      break;
    }
}

static void reactor_packet_receive_frame(reactor_packet *p, uint8_t *begin, uint8_t *end)
{
  reactor_packet_frame f = {.layer[0] = {.type = p->link_type, .begin = begin, .end = end}};

  switch (p->link_type)
    {
    case ARPHRD_ETHER:
      reactor_packet_frame_ether(&f);
      break;
    }

  reactor_user_dispatch(&p->user, REACTOR_PACKET_EVENT_FRAME, &f);
}

static void reactor_packet_receive(reactor_packet *p)
{
  struct tpacket_block_desc *bh;
  struct tpacket3_hdr *tp;
  uint8_t *begin;
  size_t i;

  while (1)
    {
      begin = p->map + (p->block_current * p->block_size);
      bh = (struct tpacket_block_desc *) begin;
      if (!bh->hdr.bh1.block_status & TP_STATUS_USER)
        break;

      begin += bh->hdr.bh1.offset_to_first_pkt;
      for (i = 0; i < bh->hdr.bh1.num_pkts; i ++)
        {
          tp = (struct tpacket3_hdr *) begin;
          if (tp->tp_len != tp->tp_snaplen)
            continue;
          reactor_packet_receive_frame(p, begin + tp->tp_mac, begin + tp->tp_mac + tp->tp_len);
          begin += tp->tp_next_offset;
        }

      bh->hdr.bh1.block_status = TP_STATUS_KERNEL;
      p->block_current = (p->block_current + 1) % p->block_count;
    }
}

static void reactor_packet_event(void *state, int type, void *data)
{
  reactor_packet *p = state;

  (void) data;
  switch (type)
    {
    case REACTOR_CORE_FD_EVENT_READ:
      reactor_packet_receive(p);
      break;
    default:
      reactor_packet_error(p, "socket event");
      break;
    }
}

static int reactor_packet_interface_type(reactor_packet *p)
{
  struct ifreq ifr = {0};
  int e;

  (void) strncpy(ifr.ifr_name, p->interface, sizeof(ifr.ifr_name));
  e = ioctl(p->fd, SIOCGIFHWADDR, &ifr);
  if (e == -1)
    return -1;

  return ifr.ifr_hwaddr.sa_family;
}

static int reactor_packet_socket(reactor_packet *p)
{
  int e;

  p->fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (p->fd == -1)
    return -1;

  e = setsockopt(p->fd, SOL_PACKET, PACKET_VERSION, (int []) {TPACKET_V3}, sizeof (int));
  if (e == -1)
    return -1;

  e = setsockopt(p->fd, SOL_PACKET, PACKET_RX_RING, (struct tpacket_req3 []) {{
        .tp_frame_size = p->frame_size,
        .tp_frame_nr = p->block_count * (p->block_size / p->frame_size),
        .tp_block_size = p->block_size,
        .tp_block_nr = p->block_count,
        .tp_retire_blk_tov = 100
        }}, sizeof (struct tpacket_req3));

  p->map = mmap(NULL, p->block_size * p->block_count,
                PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, p->fd, 0);
  if (!p->map)
    return -1;

  e = bind(p->fd, (struct sockaddr *) (struct sockaddr_ll []){{
        .sll_family = PF_PACKET,
        .sll_protocol = htons(ETH_P_ALL),
        .sll_ifindex = if_nametoindex(p->interface)
      }}, sizeof (struct sockaddr_ll));
  if (e == -1)
    return -1;

  p->link_type = reactor_packet_interface_type(p);
  if (p->link_type == -1)
    return -1;

  reactor_core_fd_register(p->fd, reactor_packet_event, p, REACTOR_CORE_FD_MASK_READ);
  return 0;
}

void reactor_packet_open(reactor_packet *p, reactor_user_callback *callback, void *state, char *interface)
{
  *p = (struct reactor_packet) {.fd = -1, .frame_size = 2048, .block_size = 128 * 2048, .block_count = 4};
  p->state = REACTOR_PACKET_STATE_OPEN;
  p->interface = strdup(interface);
  if (!p->interface)
    abort();
  reactor_user_construct(&p->user, callback, state);
}

void reactor_packet_start(reactor_packet *p)
{
  int e;

  if (p->state != REACTOR_PACKET_STATE_OPEN)
    {
      reactor_packet_error(p, "unable to start packet capture");
      return;
    }

  e = reactor_packet_socket(p);
  if (e == -1)
    {
      reactor_packet_error(p, "unable to create socket");
      return;
    }

  p->state = REACTOR_PACKET_STATE_STARTED;
}

void reactor_packet_close(reactor_packet *p)
{
  if (p->state > REACTOR_PACKET_STATE_CLOSED)
    p->state = REACTOR_PACKET_STATE_CLOSED;
}
