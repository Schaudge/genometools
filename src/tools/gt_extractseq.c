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

#include "libgtcore/bioseq.h"
#include "libgtcore/fasta.h"
#include "libgtcore/grep.h"
#include "libgtcore/ma.h"
#include "libgtcore/option.h"
#include "libgtcore/outputfile.h"
#include "libgtmatch/giextract.pr"
#include "tools/gt_extractseq.h"

#define FROMPOS_OPTION_STR  "frompos"
#define TOPOS_OPTION_STR    "topos"

typedef struct {
  Str *pattern;
  unsigned long frompos,
                topos,
                width;
  OutputFileInfo *ofi;
  GenFile *outfp;
  Str *str_ginumberfile;
} ExtractSeqArguments;

static void* gt_extractseq_arguments_new(void)
{
  ExtractSeqArguments *arguments = ma_calloc(1, sizeof *arguments);
  arguments->pattern = str_new();
  arguments->str_ginumberfile = str_new();
  arguments->ofi = outputfileinfo_new();
  return arguments;
}

static void gt_extractseq_arguments_delete(void *tool_arguments)
{
  ExtractSeqArguments *arguments = tool_arguments;
  if (!tool_arguments) return;
  genfile_close(arguments->outfp);
  outputfileinfo_delete(arguments->ofi);
  str_delete(arguments->pattern);
  str_delete(arguments->str_ginumberfile);
  ma_free(arguments);
}

static OptionParser* gt_extractseq_option_parser_new(void *tool_arguments)
{
  ExtractSeqArguments *arguments = tool_arguments;
  OptionParser *op;
  Option *frompos_option, *topos_option, *match_option, *width_option,
         *ginumberoption;
  assert(arguments);

  /* init */
  op = option_parser_new("[option ...] [sequence_file ...]",
                         "Extract sequences from given sequence file(s).");

  /* -frompos */
  frompos_option = option_new_ulong_min(FROMPOS_OPTION_STR,
                                        "extract sequence from this position\n"
                                        "counting from 1 on",
                                        &arguments->frompos, 0, 1);
  option_parser_add_option(op, frompos_option);

  /* -topos */
  topos_option = option_new_ulong_min(TOPOS_OPTION_STR,
                                      "extract sequence up to this position\n"
                                      "counting from 1 on",
                                      &arguments->topos, 0, 1);
  option_parser_add_option(op, topos_option);

  /* -match */
  match_option = option_new_string("match", "extract all sequences whose "
                                   "description matches the given pattern.\n"
                                   "The given pattern must be a valid extended "
                                   "regular expression.", arguments->pattern,
                                   NULL);
  option_parser_add_option(op, match_option);

  /* -ginum */
  ginumberoption = option_new_filename("ginum",
                                       "extract subsequences for gi numbers "
                                       "in specified file",
                                       arguments->str_ginumberfile);
  option_parser_add_option(op, ginumberoption);

  /* -width */
  width_option = option_new_ulong("width", "set output width for showing of "
                                  "sequences (0 disables formatting)",
                                  &arguments->width, 0);
  option_parser_add_option(op, width_option);

  /* output file options */
  outputfile_register_options(op, &arguments->outfp, arguments->ofi);

  /* option implications */
  option_imply(frompos_option, topos_option);
  option_imply(topos_option, frompos_option);

  /* option exclusions */
  option_exclude(frompos_option, match_option);
  option_exclude(topos_option, match_option);
  option_exclude(frompos_option, ginumberoption);
  option_exclude(match_option, ginumberoption);

  return op;
}

static int gt_extractseq_arguments_check(int rest_argc, void *tool_arguments,
                                         Error *err)
{
  ExtractSeqArguments *arguments = tool_arguments;
  error_check(err);
  assert(arguments);
  if (arguments->frompos > arguments->topos) {
    error_set(err,
              "argument to option '-%s' must be <= argument to option '-%s'",
              FROMPOS_OPTION_STR, TOPOS_OPTION_STR);
    return -1;
  }
  return 0;
}

static int extractseq_pos(GenFile *outfp, Bioseq *bs, unsigned long frompos,
                          unsigned long topos, unsigned long width, Error *err)
{
  int had_err = 0;
  error_check(err);
  assert(bs);
  if (topos > bioseq_get_raw_sequence_length(bs)) {
    error_set(err,
              "argument %lu to option '-%s' is larger than sequence length %lu",
              topos, TOPOS_OPTION_STR, bioseq_get_raw_sequence_length(bs));
    had_err = -1;
  }
  if (!had_err) {
    fasta_show_entry_generic(NULL, bioseq_get_raw_sequence(bs) + frompos - 1,
                             topos - frompos + 1, width, outfp);
  }
  return had_err;
}

static int extractseq_match(GenFile *outfp, Bioseq *bs, const char *pattern,
                            unsigned long width, Error *err)
{
  const char *desc;
  unsigned long i;
  bool match;
  int had_err = 0;

  error_check(err);
  assert(bs && pattern);

  for (i = 0; !had_err && i < bioseq_number_of_sequences(bs); i++) {
    desc = bioseq_get_description(bs, i);
    assert(desc);
    had_err = grep(&match, pattern, desc, err);
    if (!had_err && match) {
      fasta_show_entry_generic(desc, bioseq_get_sequence(bs, i),
                               bioseq_get_sequence_length(bs, i), width, outfp);
    }
  }

  return had_err;
}

static int gt_extractseq_runner(int argc, const char **argv,
                                void *tool_arguments, Error *err)
{
  ExtractSeqArguments *arguments = tool_arguments;
  int had_err = 0;

  error_check(err);
  assert(arguments);
  if (str_length(arguments->str_ginumberfile) > 0)
  {
    if (argc == 0)
    {
      error_set(err,"option -ginum requires at least one file argument");
      had_err = -1;
    } else
    {
      StrArray *referencefiletab;
      int i;

      referencefiletab = strarray_new();
      for (i = 0; i < argc; i++)
      {
        strarray_add_cstr(referencefiletab, argv[i]);
      }
      if (extractginumbers(true,
                           arguments->outfp,
                           arguments->width,
                           arguments->str_ginumberfile,
                           referencefiletab,
                           err) != 1)
      {
        had_err = -1;
      }
      strarray_delete(referencefiletab);
    }
  } else
  {
    Bioseq *bs;
    int arg = 0;
    if (argc == 0) { /* no file given, use stdin */
      if (!(bs = bioseq_new("-", err)))
        had_err = -1;
      if (!had_err) {
        if (arguments->frompos) {
          had_err = extractseq_pos(arguments->outfp, bs, arguments->frompos,
                                   arguments->topos, arguments->width, err);
        }
        else {
          had_err = extractseq_match(arguments->outfp, bs,
                                     str_get(arguments->pattern),
                                     arguments->width, err);
        }
      }
      bioseq_delete(bs);
    }

    /* process all files */
    while (!had_err && arg < argc) {
      if (!(bs = bioseq_new(argv[arg], err)))
        had_err = -1;
      if (!had_err) {
        if (arguments->frompos) {
          had_err = extractseq_pos(arguments->outfp, bs, arguments->frompos,
                                   arguments->topos, arguments->width, err);
        }
        else {
          had_err = extractseq_match(arguments->outfp, bs,
                                     str_get(arguments->pattern),
                                     arguments->width, err);
        }
      }
      bioseq_delete(bs);
      arg++;
    }
  }
  return had_err;
}

Tool* gt_extractseq(void)
{
  return tool_new(gt_extractseq_arguments_new,
                  gt_extractseq_arguments_delete,
                  gt_extractseq_option_parser_new,
                  gt_extractseq_arguments_check,
                  gt_extractseq_runner);
}
