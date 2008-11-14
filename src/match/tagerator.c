/*
  Copyright (c) 2008 Stefan Kurtz <kurtz@zbh.uni-hamburg.de>
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

#include <limits.h>
#include "core/unused_api.h"
#include "core/str_array.h"
#include "core/ma.h"
#include "core/error.h"
#include "core/fileutils.h"
#include "core/seqiterator.h"
#include "core/arraydef.h"
#include "revcompl.h"
#include "sarr-def.h"
#include "intbits.h"
#include "alphadef.h"
#include "myersapm.h"
#include "eis-voiditf.h"
#include "format64.h"
#include "idx-limdfs.h"
#include "mssufpat.h"
#include "apmoveridx.h"
#include "dist-short.h"
#include "stamp.h"
#include "tagerator.h"

#include "echoseq.pr"
#include "esa-map.pr"

#define MAXTAGSIZE INTWORDSIZE

typedef struct
{
  Seqpos dbstartpos,
         matchlength;
  bool rcmatch;
} Simplematch;

typedef struct
{
  Uchar transformedtag[MAXTAGSIZE];
  unsigned long taglen;
  bool rcdir;
} Tagwithlength;

typedef struct
{
  const TageratorOptions *tageratoroptions;
  unsigned int alphasize;
  const Uchar *tagptr;
  const Alphabet *alpha;
  unsigned long *eqsvector;
  bool *rcdirptr;
} Showmatchinfo;

#define ADDTABULATOR\
        if (firstitem)\
        {\
          firstitem = false;\
        } else\
        {\
          (void) putchar('\t');\
        }

static void showmatch(void *processinfo,
                      Seqpos dbstartpos,
                      Seqpos dblen,
                      const Uchar *dbsubstring,
                      unsigned long pprefixlen,
                      GT_UNUSED unsigned long distance)
{
  Showmatchinfo *showmatchinfo = (Showmatchinfo *) processinfo;
  bool firstitem = true;

  gt_assert(showmatchinfo->tageratoroptions != NULL);
  if (showmatchinfo->tageratoroptions->outputmode & TAGOUT_DBLENGTH)
  {
    printf(FormatSeqpos,PRINTSeqposcast(dblen));
    firstitem = false;
  }
  if (showmatchinfo->tageratoroptions->outputmode & TAGOUT_STRAND)
  {
    ADDTABULATOR;
    printf("%c",(*showmatchinfo->rcdirptr) ? '-' : '+');
  }
  if (showmatchinfo->tageratoroptions->outputmode & TAGOUT_DBSTARTPOS)
  {
    ADDTABULATOR;
    printf(FormatSeqpos,PRINTSeqposcast(dbstartpos));
  }
  if (showmatchinfo->tageratoroptions->outputmode & TAGOUT_DBSEQUENCE)
  {
    ADDTABULATOR;
    gt_assert(dbsubstring != NULL);
    printfsymbolstring(showmatchinfo->alpha,dbsubstring,(unsigned long) dblen);
  }
  if (showmatchinfo->tageratoroptions->maxintervalwidth > 0)
  {
    if (showmatchinfo->tageratoroptions->skpp)
    {
      if (showmatchinfo->tageratoroptions->outputmode &
          (TAGOUT_TAGSTARTPOS | TAGOUT_TAGLENGTH))
      {
        unsigned long suffixlength
          = reversesuffixmatch(showmatchinfo->eqsvector,
                               showmatchinfo->alphasize,
                               dbsubstring,
                               (unsigned long) dblen,
                               showmatchinfo->tagptr,
                             pprefixlen,
                             (unsigned long) showmatchinfo->tageratoroptions->
                                                        userdefinedmaxdistance);
        gt_assert(pprefixlen >= suffixlength);
        if (showmatchinfo->tageratoroptions->outputmode & TAGOUT_TAGSTARTPOS)
        {
          ADDTABULATOR;
          printf("%lu",pprefixlen - suffixlength);
        }
        if (showmatchinfo->tageratoroptions->outputmode & TAGOUT_TAGLENGTH)
        {
          ADDTABULATOR;
          printf("%lu",suffixlength);
        }
        if (showmatchinfo->tageratoroptions->outputmode & TAGOUT_TAGSUFFIXSEQ)
        {
          printfsymbolstring(NULL,showmatchinfo->tagptr +
                                  (pprefixlen - suffixlength),
                                  suffixlength);
        }
      }
    } else
    {
      printf(" %lu 0 ",pprefixlen);
      printfsymbolstring(NULL,showmatchinfo->tagptr, pprefixlen);
    }
  }
  printf("\n");
}

typedef struct
{
  Simplematch *spaceSimplematch;
  unsigned long nextfreeSimplematch, allocatedSimplematch;
  bool *rcdirptr;
} ArraySimplematch;

static void storematch(void *processinfo,
                       Seqpos dbstartpos,
                       Seqpos dblen,
                       GT_UNUSED const Uchar *dbsubstring,
                       GT_UNUSED unsigned long pprefixlen,
                       GT_UNUSED unsigned long distance)
{
  ArraySimplematch *storetab = (ArraySimplematch *) processinfo;
  Simplematch *match;

  GETNEXTFREEINARRAY(match,storetab,Simplematch,32);
  match->dbstartpos = dbstartpos;
  match->matchlength = dblen;
  match->rcmatch = *storetab->rcdirptr;
}

static void checkmstats(void *processinfo,
                        const void *patterninfo,
                        unsigned long patternstartpos,
                        unsigned long mstatlength,
                        Seqpos leftbound,
                        Seqpos rightbound)
{
  unsigned long realmstatlength;
  Tagwithlength *twl = (Tagwithlength *) patterninfo;

  realmstatlength = genericmstats((const Limdfsresources *) processinfo,
                                  &twl->transformedtag[patternstartpos],
                                  &twl->transformedtag[twl->taglen]);
  if (mstatlength != realmstatlength)
  {
    fprintf(stderr,"patternstartpos = %lu: mstatlength = %lu != %lu "
                   " = realmstatlength\n",
                    patternstartpos,mstatlength,realmstatlength);
    exit(EXIT_FAILURE);
  }
  if (intervalwidthleq((const Limdfsresources *) processinfo,leftbound,
                       rightbound))
  {
    Uchar cc;
    Seqpos *sptr, witnessposition;
    unsigned long idx;
    ArraySeqpos *mstatspos = fromitv2sortedmatchpositions(
                                  (Limdfsresources *) processinfo,
                                  leftbound,
                                  rightbound,
                                  mstatlength);
    for (sptr = mstatspos->spaceSeqpos; sptr < mstatspos->spaceSeqpos +
                                               mstatspos->nextfreeSeqpos;
         sptr++)
    {
      witnessposition = *sptr;
      for (idx = patternstartpos; idx < patternstartpos + mstatlength; idx++)
      {
        cc = limdfsgetencodedchar((const Limdfsresources *) processinfo,
                                  witnessposition + idx - patternstartpos,
                                  Forwardmode);
        if (twl->transformedtag[idx] != cc)
        {
          fprintf(stderr,"patternstartpos = %lu: pattern[%lu] = %u != %u = "
                         "sequence[%lu]\n",
                          patternstartpos,
                          idx,
                          (unsigned int) twl->transformedtag[idx],
                          (unsigned int) cc,
                          (unsigned long)
                          (witnessposition+idx-patternstartpos));
          exit(EXIT_FAILURE);
        }
      }
    }
  }
}

static void showmstats(void *processinfo,
                       const void *patterninfo,
                       GT_UNUSED unsigned long patternstartpos,
                       unsigned long mstatlength,
                       Seqpos leftbound,
                       Seqpos rightbound)
{
  Tagwithlength *twl = (Tagwithlength *) patterninfo;

  printf("%lu %c",mstatlength,twl->rcdir ? '-' : '+');
  if (intervalwidthleq((const Limdfsresources *) processinfo,leftbound,
                       rightbound))
  {
    unsigned long idx;
    ArraySeqpos *mstatspos = fromitv2sortedmatchpositions(
                                  (Limdfsresources *) processinfo,
                                  leftbound,
                                  rightbound,
                                  mstatlength);
    for (idx = 0; idx<mstatspos->nextfreeSeqpos; idx++)
    {
      printf(" " FormatSeqpos,PRINTSeqposcast(mstatspos->spaceSeqpos[idx]));
    }
  }
  printf("\n");
}

static int cmpdescend(const void *a,const void *b)
{
  Simplematch *valuea = (Simplematch *) a;
  Simplematch *valueb = (Simplematch *) b;

  if (!valuea->rcmatch && valueb->rcmatch)
  {
    return -1;
  }
  if (valuea->rcmatch && !valueb->rcmatch)
  {
    return 1;
  }
  if (valuea->dbstartpos < valueb->dbstartpos)
  {
    return 1;
  }
  if (valuea->dbstartpos > valueb->dbstartpos)
  {
    return -1;
  }
  return 0;
}

static int dotransformtag(Uchar *transformedtag,
                          const Uchar *symbolmap,
                          const Uchar *currenttag,
                          unsigned long taglen,
                          uint64_t tagnumber,
                          bool replacewildcard,
                          GtError *err)
{
  unsigned long idx;
  Uchar charcode;

  if (taglen > (unsigned long) MAXTAGSIZE)
  {
    gt_error_set(err,"tag \"%*.*s\" of length %lu; "
                  "tags must not be longer than %lu",
                   (int) taglen,(int) taglen,currenttag,taglen,
                   (unsigned long) MAXTAGSIZE);
    return -1;
  }
  for (idx = 0; idx < taglen; idx++)
  {
    charcode = symbolmap[currenttag[idx]];
    if (charcode == (Uchar) UNDEFCHAR)
    {
      gt_error_set(err,"undefined character '%c' in tag number " Formatuint64_t,
                currenttag[idx],
                PRINTuint64_tcast(tagnumber));
      return -1;
    }
    if (charcode == (Uchar) WILDCARD)
    {
      if (replacewildcard)
      {
        charcode = 0; /* (Uchar) (drand48() * (mapsize-1)); */
      } else
      {
        gt_error_set(err,"wildcard in tag number " Formatuint64_t,
                  PRINTuint64_tcast(tagnumber));
        return -1;
      }
    }
    transformedtag[idx] = charcode;
  }
  return 0;
}

static bool performpatternsearch(const AbstractDfstransformer *dfst,
                                 bool domstats,
                                 unsigned long maxdistance,
                                 bool online,
                                 bool docompare,
                                 unsigned long maxintervalwidth,
                                 bool skpp,
                                 Myersonlineresources *mor,
                                 Limdfsresources *limdfsresources,
                                 const Uchar *transformedtag,
                                 unsigned long taglen)
{
  if (online || (!domstats && docompare))
  {
    gt_assert(mor != NULL);
    edistmyersbitvectorAPM(mor,transformedtag,taglen,maxdistance);
  }
  if (!online || docompare)
  {
    if (domstats)
    {
      indexbasedmstats(limdfsresources,transformedtag,taglen,dfst);
      return false;
    }
    if (maxdistance == 0)
    {
      return indexbasedexactpatternmatching(limdfsresources,
                                            transformedtag,taglen);
    } else
    {
      return indexbasedapproxpatternmatching(limdfsresources,
                                             transformedtag,
                                             taglen,
                                             maxdistance,
                                             maxintervalwidth,
                                             skpp,
                                             dfst);
    }
  }
  return false;
}

static void compareresults(const ArraySimplematch *storeonline,
                           const ArraySimplematch *storeoffline)
{
  unsigned long ss;

  if (storeonline->nextfreeSimplematch != storeoffline->nextfreeSimplematch)
  {
    fprintf(stderr,"nextfreeSimplematch: storeonline = %lu != %lu "
                   "storeoffline\n",
                   storeonline->nextfreeSimplematch,
                   storeoffline->nextfreeSimplematch);
    exit(EXIT_FAILURE);
  }
  gt_assert(storeonline->nextfreeSimplematch ==
            storeoffline->nextfreeSimplematch);
  if (storeoffline->nextfreeSimplematch > 1UL)
  {
    qsort(storeoffline->spaceSimplematch,(size_t)
          storeoffline->nextfreeSimplematch,
          sizeof (Simplematch),
          cmpdescend);
  }
  for (ss=0; ss < storeoffline->nextfreeSimplematch; ss++)
  {
    gt_assert(storeonline->spaceSimplematch != NULL &&
           storeoffline->spaceSimplematch != NULL);
    if (storeonline->spaceSimplematch[ss].rcmatch &&
        !storeoffline->spaceSimplematch[ss].rcmatch)
    {
      fprintf(stderr,"rcmatch: storeonline[%lu] = p != d "
                     "= storeoffline[%lu]\n",ss,ss);
      exit(EXIT_FAILURE);
    }
    if (!storeonline->spaceSimplematch[ss].rcmatch &&
        storeoffline->spaceSimplematch[ss].rcmatch)
    {
      fprintf(stderr,"rcmatch: storeonline[%lu] = d != p "
                     "= storeoffline[%lu]\n",ss,ss);
      exit(EXIT_FAILURE);
    }
    if (storeonline->spaceSimplematch[ss].matchlength !=
        storeoffline->spaceSimplematch[ss].matchlength)
    {
      fprintf(stderr,"matchlength: storeonline[%lu] = " FormatSeqpos
                     " != " FormatSeqpos "= storeoffline[%lu]\n",
                     ss,
                     PRINTSeqposcast(storeonline->spaceSimplematch[ss].
                                     matchlength),
                     PRINTSeqposcast(storeoffline->spaceSimplematch[ss].
                                     matchlength),
                     ss);
      exit(EXIT_FAILURE);
    }
    if (storeonline->spaceSimplematch[ss].dbstartpos !=
        storeoffline->spaceSimplematch[ss].dbstartpos)
    {
      fprintf(stderr,"dbstartpos: storeonline[%lu] = " FormatSeqpos
                     " != " FormatSeqpos "= storeoffline[%lu]\n",
                     ss,
                     PRINTSeqposcast(storeonline->spaceSimplematch[ss].
                                     dbstartpos),
                     PRINTSeqposcast(storeoffline->spaceSimplematch[ss].
                                     dbstartpos),
                     ss);
      exit(EXIT_FAILURE);
    }
  }
}

static void searchoverstrands(const TageratorOptions *tageratoroptions,
                              Tagwithlength *twl,
                              const AbstractDfstransformer *dfst,
                              Myersonlineresources *mor,
                              Limdfsresources *limdfsresources,
                              ArraySimplematch *storeonline,
                              ArraySimplematch *storeoffline)
{
  int try;
  bool domstats, matchfound;
  unsigned long maxdistance, mindistance, distance;

  if (tageratoroptions->userdefinedmaxdistance < 0)
  {
    domstats = true;
    mindistance = maxdistance = 0;
  } else
  {
    domstats = false;
    gt_assert(tageratoroptions->userdefinedmaxdistance >= 0);
    maxdistance = (unsigned long) tageratoroptions->userdefinedmaxdistance;
    if (tageratoroptions->best)
    {
      mindistance = 0;
    } else
    {
      mindistance = maxdistance;
    }
  }
  matchfound = false;
  for (distance = mindistance; distance <= maxdistance; distance++)
  {
    for (try=0 ; try < 2; try++)
    {
      if ((try == 0 && !tageratoroptions->nofwdmatch) ||
          (try == 1 && !tageratoroptions->norcmatch))
      {
        if (try == 1 && !tageratoroptions->norcmatch)
        {
          inplace_reversecomplement(twl->transformedtag,twl->taglen);
          twl->rcdir = true;
        }
        if (performpatternsearch(dfst,
                                 domstats,
                                 distance,
                                 tageratoroptions->online,
                                 tageratoroptions->docompare,
                                 tageratoroptions->maxintervalwidth,
                                 tageratoroptions->skpp,
                                 mor,
                                 limdfsresources,
                                 twl->transformedtag,
                                 twl->taglen) && !matchfound)
        {
          matchfound = true;
        }
        if (tageratoroptions->docompare)
        {
          compareresults(storeonline,storeoffline);
        }
      }
    }
    if (tageratoroptions->best && matchfound)
    {
      break;
    }
  }
}

int runtagerator(const TageratorOptions *tageratoroptions,GtError *err)
{
  Suffixarray suffixarray;
  Seqpos totallength;
  GtSeqIterator *seqit = NULL;
  bool haserr = false;
  int retval;
  unsigned int demand;
  Myersonlineresources *mor = NULL;
  ArraySimplematch storeonline, storeoffline;
  void *packedindex = NULL;
  const AbstractDfstransformer *dfst;

  if (tageratoroptions->userdefinedmaxdistance >= 0)
  {
    dfst = apm_AbstractDfstransformer();
  } else
  {
    dfst = pms_AbstractDfstransformer();
  }
  if (tageratoroptions->withesa)
  {
    demand = SARR_ESQTAB;
    if (!tageratoroptions->online)
    {
      demand |= SARR_SUFTAB;
    }
  } else
  {
    if (tageratoroptions->docompare || tageratoroptions->online)
    {
      demand = SARR_ESQTAB;
    } else
    {
      demand = 0;
    }
  }
  if (mapsuffixarray(&suffixarray,
                     &totallength,
                     demand,
                     tageratoroptions->indexname,
                     NULL,
                     err) != 0)
  {
    haserr = true;
  }
  if (!haserr)
  {
    if (tageratoroptions->withesa && suffixarray.readmode != Forwardmode)
    {
      gt_error_set(err,"using option -esa you can only process index "
                    "in forward mode");
      haserr = true;
    } else
    {
      if (!tageratoroptions->withesa && suffixarray.readmode != Reversemode)
      {
        gt_error_set(err,"with option -pck you can only process index "
                      "in reverse mode");
        haserr = true;
      }
    }
  }
  if (!haserr && !tageratoroptions->withesa)
  {
    packedindex = loadvoidBWTSeqForSA(tageratoroptions->indexname,
                                      &suffixarray,
                                      totallength, true, err);
    if (packedindex == NULL)
    {
      haserr = true;
    }
  }
  INITARRAY(&storeonline,Simplematch);
  INITARRAY(&storeoffline,Simplematch);
  if (!haserr)
  {
    Tagwithlength twl;
    uint64_t tagnumber;
    unsigned int mapsize;
    const Uchar *symbolmap, *currenttag;
    char *desc = NULL;
    Processmatch processmatch;
    Showmatchinfo showmatchinfo;
    void *processmatchinfoonline, *processmatchinfooffline;
    Limdfsresources *limdfsresources = NULL;

    storeonline.rcdirptr = storeoffline.rcdirptr = &twl.rcdir;
    symbolmap = getsymbolmapAlphabet(suffixarray.alpha);
    mapsize = getmapsizeAlphabet(suffixarray.alpha);
    if (tageratoroptions->docompare)
    {
      processmatch = storematch;
      processmatchinfoonline = &storeonline;
      processmatchinfooffline = &storeoffline;
      showmatchinfo.eqsvector = NULL;
    } else
    {
      processmatch = showmatch;
      showmatchinfo.rcdirptr = &twl.rcdir;
      showmatchinfo.tageratoroptions = tageratoroptions;
      showmatchinfo.alphasize = (unsigned int) (mapsize-1);
      showmatchinfo.tagptr = &twl.transformedtag[0];
      showmatchinfo.alpha = suffixarray.alpha;
      showmatchinfo.eqsvector = gt_malloc(sizeof(*showmatchinfo.eqsvector) *
                                          showmatchinfo.alphasize);
      processmatchinfooffline = &showmatchinfo;
      processmatchinfoonline = &showmatchinfo;
    }
    if (tageratoroptions->online || tageratoroptions->docompare)
    {
      gt_assert(suffixarray.encseq != NULL);
      mor = newMyersonlineresources(mapsize,
                                    tageratoroptions->nowildcards,
                                    suffixarray.encseq,
                                    processmatch,
                                    processmatchinfoonline);
    }
    if (!tageratoroptions->online || tageratoroptions->docompare)
    {
      const Matchbound **mbtab;
      unsigned int maxdepth;
      unsigned long maxpathlength;

      if (tageratoroptions->withesa)
      {
        mbtab = NULL;
        maxdepth = 0;
      } else
      {
        mbtab = bwtseq2mbtab(packedindex);
        maxdepth = bwtseq2maxdepth(packedindex);
        if (tageratoroptions->userdefinedmaxdepth >= 0 &&
            maxdepth > (unsigned int) tageratoroptions->userdefinedmaxdepth)
        {
          maxdepth = (unsigned int) tageratoroptions->userdefinedmaxdepth;
        }
      }
      if (tageratoroptions->userdefinedmaxdistance >= 0)
      {
        maxpathlength = (unsigned long) (1+ MAXTAGSIZE +
                                         tageratoroptions->
                                         userdefinedmaxdistance);
      } else
      {
        maxpathlength = (unsigned long) (1+MAXTAGSIZE);
      }
      limdfsresources = newLimdfsresources(tageratoroptions->withesa
                                             ? &suffixarray : packedindex,
                                           mbtab,
                                           maxdepth,
                                           suffixarray.encseq,
                                           tageratoroptions->withesa,
                                           tageratoroptions->nowildcards,
                                           tageratoroptions->maxintervalwidth,
                                           mapsize,
                                           totallength,
                                           maxpathlength,
                                           processmatch,
                                           processmatchinfooffline,
                                           tageratoroptions->docompare
                                             ? checkmstats
                                             : showmstats,
                                           &twl, /* refer to uninit structure */
                                           dfst);
    }
    seqit = gt_seqiterator_new(tageratoroptions->tagfiles, NULL, true);
    for (tagnumber = 0; !haserr; tagnumber++)
    {
      retval = gt_seqiterator_next(seqit, &currenttag, &twl.taglen, &desc, err);
      if (retval != 1)
      {
        if (retval < 0)
        {
          gt_free(desc);
        }
        break;
      }
      if (dotransformtag(&twl.transformedtag[0],
                         symbolmap,
                         currenttag,
                         twl.taglen,
                         tagnumber,
                         tageratoroptions->replacewildcard,
                         err) != 0)
      {
        haserr = true;
        gt_free(desc);
        break;
      }
      twl.rcdir = false;
      printf("# %lu ",twl.taglen);
      fprintfsymbolstring(stdout,suffixarray.alpha,twl.transformedtag,
                          twl.taglen);
      printf("\n");
      storeoffline.nextfreeSimplematch = 0;
      storeonline.nextfreeSimplematch = 0;
      if (tageratoroptions->userdefinedmaxdistance > 0 &&
          twl.taglen <= (unsigned long)
                        tageratoroptions->userdefinedmaxdistance)
      {
        gt_error_set(err,"tag \"%*.*s\" of length %lu; "
                     "tags must be longer than the allowed number of errors "
                     "(which is %ld)",
                     (int) twl.taglen,
                     (int) twl.taglen,currenttag,
                     twl.taglen,
                     tageratoroptions->userdefinedmaxdistance);
        haserr = true;
        gt_free(desc);
        break;
      }
      gt_assert(tageratoroptions->userdefinedmaxdistance < 0 ||
                twl.taglen > (unsigned long)
                             tageratoroptions->userdefinedmaxdistance);
      searchoverstrands(tageratoroptions,
                        &twl,
                        dfst,
                        mor,
                        limdfsresources,
                        &storeonline,
                        &storeoffline);
      gt_free(desc);
    }
    gt_free(showmatchinfo.eqsvector);
    if (limdfsresources != NULL)
    {
      freeLimdfsresources(&limdfsresources,dfst);
    }
  }
  FREEARRAY(&storeonline,Simplematch);
  FREEARRAY(&storeoffline,Simplematch);
  if (mor != NULL)
  {
    freeMyersonlineresources(&mor);
  }
  gt_seqiterator_delete(seqit);
  freesuffixarray(&suffixarray);
  if (packedindex != NULL)
  {
    deletevoidBWTSeq(packedindex);
  }
  return haserr ? -1 : 0;
}
