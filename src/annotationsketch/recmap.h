/*
  Copyright (c) 2008 Sascha Steinbiss <ssteinbiss@zbh.uni-hamburg.de>
  Copyright (c) 2008 Center for Bioinformatics, University of Hamburg

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

#ifndef RECMAP_H
#define RECMAP_H

#include "annotationsketch/recmap_api.h"

struct GT_RecMap {
  double nw_x,
         nw_y,
         se_x,
         se_y;
  GenomeFeature *gf;
  bool has_omitted_children;
};

GT_RecMap* gt_recmap_new(double nw_x, double nw_y, double se_x, double se_y,
                         GenomeFeature*);
int        gt_recmap_format_html_imagemap_coords(GT_RecMap*, char*, size_t);
void       gt_recmap_delete(GT_RecMap*);

#endif
