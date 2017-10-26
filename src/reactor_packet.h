#ifndef REACTOR_PACKET_H_INCLUDED
#define REACTOR_PACKET_H_INCLUDED

enum reactor_packet_state
{
  REACTOR_PACKET_STATE_CLOSED = 0,
  REACTOR_PACKET_STATE_OPEN,
  REACTOR_PACKET_STATE_STARTED,
  REACTOR_PACKET_STATE_ERROR
};

enum reactor_packet_event
{
  REACTOR_PACKET_EVENT_ERROR,
  REACTOR_PACKET_EVENT_FRAME,
};

typedef struct reactor_packet reactor_packet;
typedef struct reactor_packet_layer reactor_packet_layer;
typedef struct reactor_packet_frame reactor_packet_frame;

struct reactor_packet
{
  short                 ref;
  short                 state;
  reactor_user          user;

  char                 *interface;
  int                   link_type;

  int                   fd;
  size_t                frame_size;
  size_t                block_size;
  size_t                block_count;
  size_t                block_current;
  uint8_t              *map;
};

struct reactor_packet_layer
{
  int                   type;
  void                 *begin;
  void                 *end;
};

struct reactor_packet_frame
{
  reactor_packet_layer  layer[4];
};

void reactor_packet_open(reactor_packet *, reactor_user_callback *, void *, char *);
void reactor_packet_start(reactor_packet *);
void reactor_packet_close(reactor_packet *);

#endif /* REACTOR_PACKET_H_INCLUDED */
