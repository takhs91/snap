/*
 * glue.{cc,hh} -- minimize portability headaches, and miscellany
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#include <click/config.h>

#include <click/glue.hh>
#include <click/error.hh>

#include <limits.h>
#ifndef CLICK_LINUXMODULE
# include <stdarg.h>
#endif

void
click_chatter(const char *fmt, ...)
{
  va_list val;
  va_start(val, fmt);

  ErrorHandler *errh = ErrorHandler::default_handler();
  if (errh)
    errh->verror(ErrorHandler::ERR_MESSAGE, "", fmt, val);
  else {
#ifdef CLICK_LINUXMODULE
#if __MTCLICK__
    static char buf[NR_CPUS][512];	// XXX
    int i = vsprintf(buf[current->processor], fmt, val);
    printk("<1>%s\n", buf[current->processor]);
#else
    static char buf[512];		// XXX
    int i = vsprintf(buf, fmt, val);
    printk("<1>%s\n", buf);
#endif
#else
    vfprintf(stderr, fmt, val);
    fprintf(stderr, "\n");
#endif
  }
  
  va_end(val);
}


#ifdef __KERNEL__

// Just for statistics.
unsigned click_new_count = 0;
unsigned click_outstanding_news = 0;

#define CHUNK_MAGIC -49281
struct Chunk {
  int magic;
  int size;
  int where;
  Chunk *prev;
  Chunk *next;
};

static Chunk *chunks = 0;
int click_dmalloc_where = 0x3F3F3F3F;

void *
operator new(unsigned int sz)
{
  click_new_count++;
  click_outstanding_news++;
#if CLICK_DMALLOC
  void *v = kmalloc(sz + sizeof(Chunk), GFP_ATOMIC);
  Chunk *c = (Chunk *)v;
  c->magic = CHUNK_MAGIC;
  c->size = sz;
  c->next = chunks;
  c->prev = 0;
  c->where = click_dmalloc_where;
  if (chunks) chunks->prev = c;
  chunks = c;
  return (void *)((unsigned char *)v + sizeof(Chunk));
#else
  return kmalloc(sz, GFP_ATOMIC);
#endif
}

void *
operator new [] (unsigned int sz)
{
  click_new_count++;
  click_outstanding_news++;
#if CLICK_DMALLOC
  void *v = kmalloc(sz + sizeof(Chunk), GFP_ATOMIC);
  Chunk *c = (Chunk *)v;
  c->magic = CHUNK_MAGIC;
  c->size = sz;
  c->next = chunks;
  c->prev = 0;
  c->where = click_dmalloc_where;
  if (chunks) chunks->prev = c;
  chunks = c;
  return (void *)((unsigned char *)v + sizeof(Chunk));
#else
  return kmalloc(sz, GFP_ATOMIC);
#endif
}

void
operator delete(void *addr)
{
  if (addr) {
    click_outstanding_news--;
#if CLICK_DMALLOC
    Chunk *c = (Chunk *)((unsigned char *)addr - sizeof(Chunk));
    if (c->magic != CHUNK_MAGIC) {
      printk("<1>click error: memory corruption on delete %p\n", addr);
      return;
    }
    if (c->prev) c->prev->next = c->next;
    else chunks = c->next;
    if (c->next) c->next->prev = c->prev;
    kfree((void *)c);
#else
    kfree(addr);
#endif
  }
}

void
operator delete [] (void *addr)
{
  if (addr) {
    click_outstanding_news--;
#if CLICK_DMALLOC
    Chunk *c = (Chunk *)((unsigned char *)addr - sizeof(Chunk));
    if (c->magic != CHUNK_MAGIC) {
      printk("<1>click error: memory corruption on delete[] %p\n", addr);
      return;
    }
    if (c->prev) c->prev->next = c->next;
    else chunks = c->next;
    if (c->next) c->next->prev = c->prev;
    kfree((void *)c);
#else
    kfree(addr);
#endif
  }
}

void
print_and_free_chunks()
{
#if CLICK_DMALLOC
  const char *hexstr = "0123456789ABCDEF";
  for (Chunk *c = chunks; c; ) {
    Chunk *n = c->next;

    // set where buffer
    char buf[13], *s = buf;
    for (int i = 24; i >= 0; i -= 8) {
      int ch = (c->where >> i) & 0xFF;
      if (ch >= 32 && ch < 127)
	*s++ = ch;
      else {
	*s++ = '%';
	*s++ = hexstr[(ch>>4) & 0xF];
	*s++ = hexstr[ch & 0xF];
      }
    }
    *s++ = 0;
    
    printk("<1>  chunk at %p size %d alloc[%s] data ",
	   (void *)(c + 1), c->size, buf);
    unsigned char *d = (unsigned char *)(c + 1);
    for (int i = 0; i < 20 && i < c->size; i++)
      printk("%02x", d[i]);
    printk("\n");
    kfree((void *)c);
    
    c = n;
  }
#endif
}

extern "C" {

void
__assert_fail(const char *__assertion,
	      const char *__file,
	      unsigned int __line,
	      const char *__function)
{
  printk("<1>assertion failed %s %s %d %s\n",
	 __assertion,
	 __file,
	 __line,
	 __function);
  panic("Click assertion failed");
}

/*
 * GCC generates calls to these run-time library routines.
 */

void
__pure_virtual()
{
  printk("<1>pure virtual method called\n");
}

void *
__rtti_si()
{
  printk("<1>rtti_si\n");
  return(0);
}

void *
__rtti_user()
{
  printk("<1>rtti_user\n");
  return(0);
}


/*
 * Convert a string to a long integer. use simple_strtoul, which is in the
 * kernel
 */

long
strtol(const char *nptr, char **endptr, int base)
{
  // XXX should check for overflow and underflow, but strtoul doesn't, so why
  // bother?
  if (*nptr == '-')
    return -simple_strtoul(nptr + 1, endptr, base);
  else if (*nptr == '+')
    return simple_strtoul(nptr + 1, endptr, base);
  else
    return simple_strtoul(nptr, endptr, base);
}

};

#else /* !__KERNEL__ */

int click_new_count; /* dummy */

unsigned
click_jiffies()
{
  struct timeval tv;
  click_gettimeofday(&tv);
  return (tv.tv_sec * 100) + (tv.tv_usec / 10000);
}

#endif /* __KERNEL__ */
