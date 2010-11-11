/*
  Copyright (c) 2007 Stefan Kurtz <kurtz@zbh.uni-hamburg.de>
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

#ifndef SFX_OPTDEF_H
#define SFX_OPTDEF_H

#include <stdbool.h>
#include "core/str.h"
#include "core/str_array.h"
#include "core/readmode.h"
#include "core/option.h"
#include "sfx-strategy.h"
#ifndef S_SPLINT_S /* splint reports too much errors for the following and so
                      we exclude it */
#include "eis-bwtseq-param.h"
#endif

#define PREFIXLENGTH_AUTOMATIC 0
#define MAXDEPTH_AUTOMATIC     0

typedef struct
{
  GtStr *indexname,
        *smap,
        *sat;
  GtStrArray *filenametab;
  bool isdna,
       isprotein,
       isplain,
       outtistab,
       outdestab,
       outsdstab,
       outssptab,
       outoistab;
} Filenames2encseqoptions;

typedef struct
{
  unsigned int numofparts,
               prefixlength;
  unsigned long maximumspace;
  GtStr *inputindex,
        *maxdepth;
  GtOption *optionalgboundsref, *optionpartsargvref;
  GtStrArray *algbounds, *partsargv;
  GtReadmode readmode;
  bool beverbose,
       showtime,
       showprogress,
       outsuftab,
       outlcptab,
       outbwttab,
       outbcktab,
       outkystab,
       outkyssort,
       iteratorbasedkmerscanning,
       suftabasulongarray;
  GtStr *optionkysargumentstring;
  Sfxstrategy sfxstrategy;
  Filenames2encseqoptions fn2encopt;
#ifndef S_SPLINT_S /* splint reports too much errors for the following and so
                      we exclude it */
  struct bwtOptions bwtIdxParams;
#endif
} Suffixeratoroptions;

#endif
