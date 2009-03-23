#!/bin/sh
set -e -x

if test $# -eq 0
then
  filenames=`find testdata/ -name '*.fna'`
else
  if test $1 == 'valgrind'
  then
    VALGRIND=valgrind.sh
    shift
  fi
  if test $# -eq 0
  then
    filenames=`find testdata/ -name '*.fna'`
  else
    filenames=$*
  fi
fi

suffixerator()
{
  out="-v -lcp -tis -suf -des -ssp"
  ${VALGRIND} gt suffixerator -v -lcp -tis -suf -des -ssp -db ${filename} $*
}

sfxmap()
{
  gt dev sfxmap -lcp -suf $*
}

for filename in ${filenames}
do
  suffixerator -indexname sfx-idx 
  sfxmap sfx-idx
  suffixerator -maxdepth -indexname sfx-idx 
  sfxmap sfx-idx
  maxdepth=`grep '^prefixlength=' sfx-idx.prj | sed -e 's/prefixlength=//'`
  maxdepth=`expr ${maxdepth} \* 2`
  suffixerator -maxdepth ${maxdepth} -indexname sfx-idx${maxdepth}
  sfxmap sfx-idx${maxdepth}
  rm -f sfx-idx.* sfx-idx${maxdepth}.*
done
