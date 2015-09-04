/*
  Copyright (C) 2015 Annika Seidel, annika.seidel@studium.uni-hamburg.de
  Copyright (c) 2007-2009 Gordon Gremme <gordon@gremme.org>
  Copyright (c) 2007-2008 Center for Bioinformatics, University of Hamburg

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

#include <limits.h>
#include "core/assert_api.h"
#include "core/array2dim_api.h"
#include "core/minmax.h"
#include "extended/affinealign.h"

typedef struct {
  GtUword Rdist,
                Ddist,
                Idist;
  AffineAlignEdge Redge,
                  Dedge,
                  Iedge;
} AffinealignDPentry;

static GtUword infadd(GtUword inf, GtUword s)
{
  if (inf == ULONG_MAX)
    return inf;
  return inf + s;
}

static void affinealign_fill_table(AffinealignDPentry **dptable,
                                   const GtUchar *u, GtUword ulen,
                                   const GtUchar *v, GtUword vlen,
                                   GtUword matchcost, GtUword mismatchcost,
                                   GtUword gap_opening, GtUword gap_extension)
{
  GtUword i, j, Rvalue, Dvalue, Ivalue, minvalue;
  int rcost;
  gt_assert(dptable && u && v);
  /*gt_assert(ulen && vlen);*/
  for (i = 0; i <= ulen; i++) {
    for (j = 0; j <= vlen; j++) {
      if (!i && !j) {
        dptable[0][0].Rdist = 0;
        dptable[0][0].Ddist = gap_opening;
        dptable[0][0].Idist = gap_opening;
      }
      else {
        /* compute A_affine(i,j,R) */
        if (!i || !j)
          dptable[i][j].Rdist = ULONG_MAX;
        else {
          rcost  = (u[i-1] == v[j-1]) ? matchcost : mismatchcost;
          Rvalue = infadd(dptable[i-1][j-1].Rdist, rcost);
          Dvalue = infadd(dptable[i-1][j-1].Ddist, rcost);
          Ivalue = infadd(dptable[i-1][j-1].Idist, rcost);
          minvalue = MIN3(Rvalue, Dvalue, Ivalue);
          gt_assert(minvalue != ULONG_MAX);
          dptable[i][j].Rdist = minvalue;
          /* set backtracing edge */
          if (Rvalue == minvalue)
            dptable[i][j].Redge = Affine_R;
          else if (Dvalue == minvalue)
            dptable[i][j].Redge = Affine_D;
          else /* Ivalue == minvalue */
            dptable[i][j].Redge = Affine_I;
        }
        /* compute A_affine(i,j,D) */
        if (!i)
          dptable[i][j].Ddist = ULONG_MAX;
        else {
          Rvalue = infadd(dptable[i-1][j].Rdist, gap_opening + gap_extension);
          Dvalue = infadd(dptable[i-1][j].Ddist, gap_extension);
          Ivalue = infadd(dptable[i-1][j].Idist, gap_opening + gap_extension);
          minvalue = MIN3(Rvalue, Dvalue, Ivalue);
          gt_assert(minvalue != ULONG_MAX);
          dptable[i][j].Ddist = minvalue;
          /* set backtracing edge */
          if (Rvalue == minvalue)
            dptable[i][j].Dedge = Affine_R;
          else if (Dvalue == minvalue)
            dptable[i][j].Dedge = Affine_D;
          else /* Ivalue == minvalue */
            dptable[i][j].Dedge = Affine_I;
        }
        /* compute A_affine(i,j,I) */
        if (!j)
          dptable[i][j].Idist = ULONG_MAX;
        else {
          Rvalue = infadd(dptable[i][j-1].Rdist, gap_opening + gap_extension);
          Dvalue = infadd(dptable[i][j-1].Ddist, gap_opening + gap_extension);
          Ivalue = infadd(dptable[i][j-1].Idist, gap_extension);
          minvalue = MIN3(Rvalue, Dvalue, Ivalue);
          gt_assert(minvalue != ULONG_MAX);
          dptable[i][j].Idist = minvalue;
          /* set backtracing edge */
          if (Rvalue == minvalue)
            dptable[i][j].Iedge = Affine_R;
          else if (Dvalue == minvalue)
            dptable[i][j].Iedge = Affine_D;
          else /* Ivalue == minvalue */
            dptable[i][j].Iedge = Affine_I;
        }
      }
    }
  }
}

static void affinealign_traceback(GtAlignment *a,
                                  AffinealignDPentry * const *dptable,
                                  GtUword i, GtUword j)
{
  GtUword minvalue;
  AffineAlignEdge edge;
  gt_assert(a && dptable);
  /* determine min{A_affine(m,n,x) | x in {R,D,I}} */
  minvalue = MIN3(dptable[i][j].Rdist, dptable[i][j].Ddist,
                  dptable[i][j].Idist);
  if (dptable[i][j].Rdist == minvalue)
    edge = Affine_R;
  else if (dptable[i][j].Ddist == minvalue)
    edge = Affine_D;
  else /* dptable[i][j].Idist == minvalue */
    edge = Affine_I;
  /* backtracing */
  while (i > 0 || j > 0) {
    switch (edge) {
      case Affine_R:
        gt_assert(dptable[i][j].Rdist != ULONG_MAX);
        gt_alignment_add_replacement(a);
        edge = dptable[i][j].Redge;
        gt_assert(i > 0 && j > 0);
        i--;
        j--;
        break;
      case Affine_D:
        gt_alignment_add_deletion(a);
        edge = dptable[i][j].Dedge;
        gt_assert(i);
        i--;
        break;
      case Affine_I:
        gt_alignment_add_insertion(a);
        edge = dptable[i][j].Iedge;
        gt_assert(j);
        j--;
        break;
      default:
        gt_assert(false);
    }
  }
}

GtAlignment* gt_affinealign(const GtUchar *u, GtUword ulen,
                            const GtUchar *v, GtUword vlen,
                            GtUword matchcost, GtUword mismatchcost,
                            GtUword gap_opening_cost,
                            GtUword gap_extension_cost)
{
  AffinealignDPentry **dptable;
  GtAlignment *align;

  gt_assert(u && v);
  gt_array2dim_malloc(dptable, ulen+1, vlen+1);
  affinealign_fill_table(dptable, u, ulen, v, vlen, matchcost, mismatchcost,
                         gap_opening_cost, gap_extension_cost);
  align = gt_alignment_new_with_seqs(u, ulen,  v, vlen);
  affinealign_traceback(align, dptable, ulen, vlen);
  gt_array2dim_delete(dptable);
  return align;
}
