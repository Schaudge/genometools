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

#include <string.h>
#include "libgtcore/hashtable.h"
#include "libgtcore/ma.h"
#include "libgtcore/xansi.h"
#include "libgtext/compare.h"
#include "libgtext/toolbox.h"

struct Toolbox {
  Hashtable *tools;
};

Toolbox* toolbox_new(Env *env)
{
  Toolbox *tb;
  env_error_check(env);
  tb = ma_malloc(sizeof (Toolbox));
  tb->tools = hashtable_new(HASH_STRING, NULL, NULL, env);
  return tb;
}

void toolbox_add(Toolbox *tb, const char *toolname, Tool tool, Env *env)
{
  env_error_check(env);
  assert(tb && tb->tools);
  hashtable_add(tb->tools, (char*) toolname, tool, env);
}

Tool toolbox_get(const Toolbox *tb, const char *toolname)
{
  assert(tb && tb->tools);
  return hashtable_get(tb->tools, toolname);
}

static int show_tool_name(void *key, void *value, void *data, Env *env)
{
  env_error_check(env);
  assert(key && value);
  if (strcmp(key, "dev"))
    xputs(key);
  return 0;
}

int toolbox_show(const char *progname, void *toolbox, Env *env)
{
  Toolbox *tb;
  env_error_check(env);
  assert(toolbox);
  tb = (Toolbox*) toolbox;
  printf("\nTools:\n\n");
  hashtable_foreach_ao(tb->tools, show_tool_name, NULL, env);
  return 0;
}

void toolbox_delete(Toolbox *tb, Env *env)
{
  if (!tb) return;
  hashtable_delete(tb->tools, env);
  ma_free(tb);
}
