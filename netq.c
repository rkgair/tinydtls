/* netq -- definition of a simple network packet queue with fixed length
 *
 * Copyright (C) 2011 Olaf Bergmann <bergmann@tzi.org>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#ifndef NDEBUG
#include <stdio.h>
#include <unistd.h>
#endif

#include "netq.h"

int 
nq_push(struct netq_t *q, struct packet_t *p) {
  assert(q);
  assert(p); 

  p->next = NULL;

  if (!q->pq_first) {
    q->pq_first = q->pq_last = p;
  } else {
    q->pq_last->next = p;
    q->pq_last = p;
  }
  
  return 1;
}

struct packet_t *
nq_pop(struct netq_t *q) {
  struct packet_t *p;

  assert(q);

  if (!q->pq_first) 
    return NULL;

  p = q->pq_first;
  q->pq_first = q->pq_first->next;

  if (q->pq_last == p)
    q->pq_last = NULL;

  return p;
}

struct netq_t *
nq_new(int bufsize) {
  netq_t *nq;
  nq = (struct netq_t *)malloc(bufsize + sizeof(struct netq_t));
  if (nq) {
    memset(nq, 0, bufsize + sizeof(struct netq_t));
    nq->packetbuf = (char *)nq + sizeof(struct netq_t);
    nq->bufsize = bufsize;
  }
  return nq;
}

#define nq_free(Q) free(Q)

struct packet_t *
nq_new_packet(struct netq_t *nq, struct sockaddr *raddr, socklen_t rlen,
	      int ifindex, char *buf, size_t len) {
  char *s_start;		/* start of available storage */
  size_t s_len;			/* max storage */
  size_t p_len = sizeof(struct packet_t) + rlen + len; /* actual length */
  
  assert(nq);

  if (nq->pq_last) {  /* there is at least one packet in our buffer */

    /* point s_start behind the last packet */
    s_start = nq->pq_last->buf + nq->pq_last->len; 
    
    /* and check for the rear boundary */
    if ((char *)nq->pq_first >= (char *)s_start)
      s_len = (char *)nq->pq_first - s_start; /* in the middle of the buffer */
    else {
      /* space after the last packet */
      s_len = nq->packetbuf + nq->bufsize - s_start;

      if (s_len < p_len) {
	/* We do not have sufficient space at the buffer's end.  Try
	 * at its front. */
	
	/* Check if there is sufficient space between buffer start and
	 * first packet. If not, just fall through to check behind
	 * this conditional. 
	 */
	s_len = (char *)nq->pq_first - (char *)nq->packetbuf;
	if (p_len <= s_len)
	  s_start = nq->packetbuf;
      }
    }
  } else {
    s_start = nq->packetbuf;
    s_len = nq->bufsize;
  }

  /* At this point, s_start points to free storage of size
   * s_len. There is nothing we can do if this is too small. */
  if (s_len < p_len) {
    printf("cannot store packet!\n");
    return NULL;
  }

#define PACKET(P) ((struct packet_t *)P)

  PACKET(s_start)->len = len;
  PACKET(s_start)->rlen = rlen;
  PACKET(s_start)->raddr = (struct sockaddr *)(s_start + sizeof(struct packet_t));
  PACKET(s_start)->ifindex = ifindex;
  PACKET(s_start)->buf = (char*)PACKET(s_start)->raddr + rlen;
  memcpy(PACKET(s_start)->raddr, raddr, rlen);
  memcpy(PACKET(s_start)->buf, buf, len);

  nq_push(nq, PACKET(s_start));

  return PACKET(s_start);

#undef PACKET
}

/* count list elements */
int 
nq_count(struct netq_t *nq) {
  int c = 0;
  struct packet_t *p;

  if (!nq)
    return 0;

  for (p = nq->pq_first; p; p = p->next) c++;
  return c;
}

#ifndef NDEBUG
/* dump contents of network queue */
void
nq_dump(struct netq_t *nq) {
  int cnt;
  static char buf[40];
  struct packet_t *p;
  
  if (!nq) {
    printf("no queue\n");
    return;
  }
  cnt = nq_count(nq);

  printf("========================================================================\n");
  printf("queue: %p (%d %s)\n", (void *)nq, cnt, cnt == 1 ? "element" : "elements");
  
  cnt = 0;
  for (p = nq->pq_first; p; p = p->next) {
#if 0 /* for some reason, %*s does not work here */
    printf("  %2d: %p: '%*s'\n" , ++cnt, p, p->len, p->buf);
#else
#  ifndef min
#    define min(A,B) ((A) <= (B) ? (A) : (B))
#  endif
    buf[snprintf(buf, min(39,p->len+1), "%s", p->buf)] = 0;
    printf("  %2d: %p: '%s'\n" , ++cnt, (void *)p, buf);
#endif
  }
}
#endif
