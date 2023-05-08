#ifndef RING_BUFF_H_INCLUDED
#define RING_BUFF_H_INCLUDED

#include <stdlib.h>

struct s_ring_buff
{
	size_t size;  // size of buff
	size_t fill;  // filled content
	size_t tail;  // tail of buff
	char * buff;  // buffer array
};

void   rb_init (struct s_ring_buff * rb, void * b,   size_t len);
size_t rb_add  (struct s_ring_buff * rb, const void * src, size_t len);
size_t rb_adds (struct s_ring_buff * rb, const char * src);
size_t rb_get  (struct s_ring_buff * rb, void * dst, size_t len);
size_t rb_try  (struct s_ring_buff * rb, void * dst, size_t len);
size_t rb_skip (struct s_ring_buff * rb, size_t len);
size_t rb_back (struct s_ring_buff * rb, size_t len);
size_t rb_chr  (struct s_ring_buff * rb, char ch);
size_t rb_solid(struct s_ring_buff * rb);
void * rb_tailp(struct s_ring_buff * rb);
void   rb_clear(struct s_ring_buff * rb);

#endif /* RING_BUFF_H_INCLUDED */
