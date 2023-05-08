

#include "ring_buff.h"
#include "goods.h"
#include <string.h>



//--------------------------------------------------------------------------------------------------------------------------------------
void   rb_init (struct s_ring_buff * rb, void * b,   size_t len)
{
	if(!rb) return;
	rb->buff = b;
	rb->size = len;
	rb_clear(rb);
}
//--------------------------------------------------------------------------------------------------------------------------------------
size_t rb_add(struct s_ring_buff * rb, const void * src, size_t len)
{
	// добавляем в буфер несколько байт
	// возврат - сколько байт добавлено
	if(!rb) return 0;
	size_t remain = len = min(rb->size - rb->fill, len);
	const char * cp = src;

	while(remain)
	{
		size_t head = rb->tail + rb->fill;
		if(head >= rb->size) head -= rb->size;
		size_t to_copy = min(remain, rb->size - head);

		memcpy(rb->buff + head, cp, to_copy);

		disable_irq;
		rb->fill += to_copy;
		enable_irq;

		remain -= to_copy;
		cp += to_copy;

	}
	return len;
}
//--------------------------------------------------------------------------------------------------------------------------------------
size_t rb_adds  (struct s_ring_buff * rb, const char * src)
{
	return rb_add(rb, src, strlen(src));
}
//--------------------------------------------------------------------------------------------------------------------------------------
size_t rb_get(struct s_ring_buff * rb, void * dst, size_t len)
{
	// забираем из буфера несколько байт
	// буфер при этом опустошается на это количество
	// возврат - сколько байт получено
	len = rb_try(rb, dst, len);
	rb_skip(rb, len);

	return len;
}
//--------------------------------------------------------------------------------------------------------------------------------------
size_t rb_try(struct s_ring_buff * rb, void * dst, size_t len)
{
	// забираем из буфера несколько байт
	// буфер при этом не опустошается
	// возврат - сколько байт получено
	if(!rb) return 0;
	size_t remain = len = min(rb->fill, len);
	size_t tail = rb->tail;
	char * cp = dst;

	while(remain)
	{
		size_t to_copy = min(remain, rb->size - tail);
		memcpy(cp, rb->buff + tail, to_copy);

		tail += to_copy;
		if(tail >= rb->size) tail -= rb->size;
		remain -= to_copy;
		cp += to_copy;
	}
	return len;
}

//--------------------------------------------------------------------------------------------------------------------------------------
size_t rb_skip (struct s_ring_buff * rb, size_t len)
{
	// пропустить ("потерять") некоторое кол-во содержимого с начала буфера
	// возврат - сколько байт пропущено
	if(!rb) return 0;
	len = min(rb->fill, len);

	disable_irq;
	size_t tail = rb->tail + len;
	if(tail >= rb->size) tail -= rb->size;
	rb->fill -= len;
	rb->tail = tail;
	enable_irq;
	return len;
}
//--------------------------------------------------------------------------------------------------------------------------------------
size_t rb_back (struct s_ring_buff * rb, size_t len)
{
	// пропустить ("потерять") некоторое кол-во содержимого с конца буфера
	// возврат - на сколько байт возвращено
	if(!rb) return 0;
	len = min(rb->fill, len);
	disable_irq;
	rb->fill -= len;
	enable_irq;
	return len;
}
//--------------------------------------------------------------------------------------------------------------------------------------
size_t rb_chr  (struct s_ring_buff * rb, char ch)
{
	// ищем в буфере символ ch
	// возврат:
	// -1 - не найдено
	// >= 0 - смещение найденного относительно начала
	if(!rb) return -1;
	size_t offs = rb->tail;

	for(size_t remain = rb->fill; remain; remain--)
	{
		if(rb->buff[offs] == ch) return rb->fill - remain;
		offs++;
		if(offs >= rb->size) offs = 0;
	}
	return -1;
}
//--------------------------------------------------------------------------------------------------------------------------------------
size_t rb_solid(struct s_ring_buff * rb)
{
	// возвращаем длину непрерывного фрагмента,
	// которую можно считать из буфера
	return rb ? min(rb->fill, rb->size - rb->tail) : 0;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void * rb_tailp(struct s_ring_buff * rb)
{
	// возвращаем указатель на хвост буфера (откуда читать)
	return rb ? rb->buff + rb->tail : 0;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void rb_clear(struct s_ring_buff * rb)
{
	// делаем буфер "пустым"
	// обнуляем размер и хвост
	if(!rb) return;
	disable_irq;
	rb->fill = 0;
	rb->tail = 0;
	enable_irq;
}
//--------------------------------------------------------------------------------------------------------------------------------------
