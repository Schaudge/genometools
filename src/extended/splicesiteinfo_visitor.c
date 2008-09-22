/*
  Copyright (c) 2007-2008 Gordon Gremme <gremme@zbh.uni-hamburg.de>
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

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "core/fasta.h"
#include "core/string_distri.h"
#include "core/translate.h"
#include "core/unused_api.h"
#include "core/warning.h"
#include "core/xansi.h"
#include "extended/genome_node_iterator.h"
#include "extended/node_visitor_rep.h"
#include "extended/splicesiteinfo_visitor.h"
#include "extended/reverse.h"

struct SpliceSiteInfoVisitor {
  const GtNodeVisitor parent_instance;
  GtRegionMapping *region_mapping;
  StringDistri *splicesites,
               *donorsites,
               *acceptorsites;
  bool show,
       intron_processed;
};

#define splicesiteinfo_visitor_cast(GV)\
        gt_node_visitor_cast(splicesiteinfo_visitor_class(), GV)

static void splicesiteinfo_visitor_free(GtNodeVisitor *gv)
{
  SpliceSiteInfoVisitor *splicesiteinfo_visitor;
  assert(gv);
  splicesiteinfo_visitor = splicesiteinfo_visitor_cast(gv);
  gt_region_mapping_delete(splicesiteinfo_visitor->region_mapping);
  string_distri_delete(splicesiteinfo_visitor->splicesites);
  string_distri_delete(splicesiteinfo_visitor->donorsites);
  string_distri_delete(splicesiteinfo_visitor->acceptorsites);
}

static int process_intron(SpliceSiteInfoVisitor *ssiv, GtGenomeNode *intron,
                          GtError *err)
{
  const char *sequence;
  unsigned long seqlen;
  GtStrand strand;
  GtRange range;
  char site[5];
  GtStr *seqid;
  int had_err = 0;
  gt_error_check(err);
  assert(ssiv && intron);
  ssiv->intron_processed = true;
  range = gt_genome_node_get_range(intron);
  assert(range.start); /* 1-based coordinates */
  if (gt_range_length(range) >= 4) {
    seqid = gt_genome_node_get_seqid(intron);
    had_err = gt_region_mapping_get_raw_sequence(ssiv->region_mapping,
                                                 &sequence, seqid, err);
    if (!had_err) {
      had_err = gt_region_mapping_get_raw_sequence_length(ssiv->region_mapping,
                                                          &seqlen, seqid, err);
    }
    if (!had_err) {
      assert(range.end <= seqlen);
      strand = gt_feature_node_get_strand((GtFeatureNode*) intron);
      if (strand == GT_STRAND_FORWARD || strand == GT_STRAND_REVERSE) {
        /* fill site */
        site[0] = tolower(sequence[range.start-1]);
        site[1] = tolower(sequence[range.start]);
        site[2] = tolower(sequence[range.end-2]);
        site[3] = tolower(sequence[range.end-1]);
        site[4] = '\0';
        if (strand == GT_STRAND_REVERSE)
          had_err = reverse_complement(site, 4, err);
        if (!had_err) {
          /* add site to distributions */
          string_distri_add(ssiv->splicesites, site);
          string_distri_add(ssiv->acceptorsites, site + 2);
          site[2] = '\0';
          string_distri_add(ssiv->donorsites, site);
          ssiv->show = true;
        }
      }
      else {
        warning("skipping intron with unknown orientation "
                "(file '%s', line %u)", gt_genome_node_get_filename(intron),
                gt_genome_node_get_line_number(intron));
      }
    }
  }
  return had_err;
}

static int splicesiteinfo_visitor_genome_feature(GtNodeVisitor *gv,
                                                 GtFeatureNode *gf,
                                                 GtError *err)
{
  SpliceSiteInfoVisitor *ssiv;
  GtGenomeNodeIterator *gni;
  GtGenomeNode *node;
  int had_err = 0;
  gt_error_check(err);
  ssiv = splicesiteinfo_visitor_cast(gv);
  assert(ssiv->region_mapping);
  gni = gt_genome_node_iterator_new((GtGenomeNode*) gf);
  while (!had_err && (node = gt_genome_node_iterator_next(gni))) {
    if (gt_feature_node_has_type((GtFeatureNode*) node, gft_intron))
      had_err = process_intron(ssiv, node, err);
  }
  gt_genome_node_iterator_delete(gni);
  return had_err;
}

const GtNodeVisitorClass* splicesiteinfo_visitor_class()
{
  static const GtNodeVisitorClass *gvc = NULL;
  if (!gvc) {
   gvc = gt_node_visitor_class_new(sizeof (SpliceSiteInfoVisitor),
                                   splicesiteinfo_visitor_free,
                                   NULL,
                                   splicesiteinfo_visitor_genome_feature,
                                   NULL,
                                   NULL);
  }
  return gvc;
}

GtNodeVisitor* splicesiteinfo_visitor_new(GtRegionMapping *rm)
{
  GtNodeVisitor *gv;
  SpliceSiteInfoVisitor *ssiv;
  assert(rm);
  gv = gt_node_visitor_create(splicesiteinfo_visitor_class());
  ssiv = splicesiteinfo_visitor_cast(gv);
  ssiv->region_mapping = rm;
  ssiv->splicesites = string_distri_new();
  ssiv->acceptorsites = string_distri_new();
  ssiv->donorsites = string_distri_new();
  return gv;
}

static void showsplicesite(const char *string, unsigned long occurrences,
                           double probability, GT_UNUSED void *unused)
{
  assert(string && strlen(string) == 4);
  gt_xputchar(string[0]);
  gt_xputchar(string[1]);
  gt_xputchar('-');
  gt_xputchar(string[2]);
  gt_xputchar(string[3]);
  printf(": %6.2f%% (n=%lu)\n", probability * 100.0, occurrences);
}

static void showsinglesite(const char *string, unsigned long occurrences,
                           double probability, GT_UNUSED void *unused)
{
  assert(string && strlen(string) == 2);
  printf("%s: %6.2f%% (n=%lu)\n", string, probability * 100.0, occurrences);
}

bool splicesiteinfo_visitor_show(GtNodeVisitor *gv)
{
  SpliceSiteInfoVisitor *ssiv;
  assert(gv);
  ssiv = splicesiteinfo_visitor_cast(gv);

  if (ssiv->show) {
    /* show splice sites */
    printf("splice site distribution (for introns >= 4bp)\n");
    string_distri_foreach(ssiv->splicesites, showsplicesite, NULL);
    gt_xputchar('\n');

    /* show donor sites */
    printf("donor site distribution (for introns >= 4bp)\n");
    string_distri_foreach(ssiv->donorsites, showsinglesite, NULL);
    gt_xputchar('\n');

    /* show acceptor sites */
    printf("acceptor site distribution (for introns >= 4bp)\n");
    string_distri_foreach(ssiv->acceptorsites, showsinglesite, NULL);
  }
  return ssiv->intron_processed;
}
