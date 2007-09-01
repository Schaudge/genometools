/*
  Copyright (c) 2007 Gordon Gremme <gremme@zbh.uni-hamburg.de>
  Copyright (c) 2007 Center for Bioinformatics, University of Hamburg

  Permission to use, copy, modify, and distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef MA_H
#define MA_H

#include <stdio.h>
#include <stdlib.h>

/* the memory allocator class */
typedef struct MA MA;

MA*           ma_new(void);
void          ma_init(MA*, Env*);
#define       ma_malloc(ma, size)\
              ma_malloc_mem(ma, size, __FILE__, __LINE__)
void*         ma_malloc_mem(MA*, size_t size, const char*, int);
#define       ma_calloc(ma, nmemb, size)\
              ma_calloc_mem(ma, nmemb, size, __FILE__, __LINE__)
void*         ma_calloc_mem(MA*, size_t nmemb, size_t size, const char*, int);
#define       ma_realloc(ma, ptr, size)\
              ma_realloc_mem(ma, ptr, size, __FILE, __LINE__)
void*         ma_realloc_mem(MA*, void *ptr, size_t size, const char*, int);
#define       ma_free(ptr, ma)\
              ma_free_mem(ptr, ma, __FILE__, __LINE__)
void          ma_free_mem(void *ptr, MA*, const char*, int);
unsigned long ma_get_space_peak(const MA*); /* in bytes */
void          ma_show_space_peak(MA*, FILE*);
/* check if all allocated memory has been freed, prints to stderr */
int           ma_check_space_leak(MA*, Env*);
void          ma_clean(MA*, Env*);
void          ma_delete(MA*);

#endif
