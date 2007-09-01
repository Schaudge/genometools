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

#include "libgtext/seqid2file.h"

void seqid2file_options(OptionParser *op, Str *seqfile, Str *regionmapping,
                       Env *env)
{
  Option *seqfile_option, *regionmapping_option;
  assert(op && seqfile && regionmapping);

  /* -seqfile */
  seqfile_option = option_new_string("seqfile", "set the sequence file from "
                                     "which to extract the features", seqfile,
                                     NULL, env);
  option_parser_add_option(op, seqfile_option, env);

  /* -regionmapping */
  regionmapping_option = option_new_string("regionmapping", "set file "
                                           "containing sequence-region to "
                                           "sequence file mapping",
                                           regionmapping, NULL, env);
  option_parser_add_option(op, regionmapping_option, env);

  /* either option -seqfile or -regionmapping is mandatory */
  option_is_mandatory_either(seqfile_option, regionmapping_option);

  /* the options -seqfile and -regionmapping exclude each other */
  option_exclude(seqfile_option, regionmapping_option, env);
}

RegionMapping* seqid2file_regionmapping_new(Str *seqfile, Str *regionmapping,
                                            Env *env)
{
  env_error_check(env);
  assert(seqfile && regionmapping);
  assert(str_length(seqfile) || str_length(regionmapping));
  assert(!(str_length(seqfile) && str_length(regionmapping)));
  /* create region mapping */
  if (str_length(seqfile))
    return regionmapping_new_seqfile(seqfile, env);
  else
    return regionmapping_new_mapping(regionmapping, env);
}
