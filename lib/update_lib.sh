#!/bin/bash

# Because Arduino can't really selectively compile files or update search paths, we
# just copy over the actual source files from the submodule.

# We also build and install a copy on the local machine in /tmp to get access to the
# matching /usr/share files espeak-ng needs.

# Rebuild the espeak-ng into a /tmp installation with all options predefined for clean ops
cd espeak-ng-arduino
./autogen.sh
./configure --prefix=/tmp/espeak CFLAGS=-m32 CXXFLAGS=-m32 --without-extdict-ru --without-extdict-cmn --without-extdict-yue --without-async --without-mbrola --without-speechplayer --without-klatt
make clean && make && make install
cd ..

# Start with completely clean subdir
rm -rf ../src/libespeak-ng
mkdir -p ../src/libespeak-ng
cd ../src/libespeak-ng

# UCD-tools
mkdir -p ucd-tools
for i in case.c categories.c ctype.c proplist.c scripts.c tostring.c; do
    cp ../../lib/espeak-ng-arduino/src/ucd-tools/src/$i ucd-tools/.
done
mkdir -p ucd-tools/ucd
cp ../../lib/espeak-ng-arduino/src/ucd-tools/src/include/ucd/ucd.h ucd-tools/ucd/.

# Includes
mkdir -p espeak-ng
for i in encoding.h espeak_ng.h speak_lib.h; do
    cp ../../lib/espeak-ng-arduino/src/include/espeak-ng/$i espeak-ng/.
done

mkdir -p ucd
cp ../../lib/espeak-ng-arduino/src/ucd-tools/src/include/ucd/ucd.h ucd/.

# espeak-ng files
for i in common.c common.h compiledict.h config.h dictionary.c dictionary.h \
  encoding.c error.c error.h espeak_api.c espeak_command.h \
  event.h fifo.c fifo.h ieee80.c ieee80.h intonation.c intonation.h klatt.c \
  klatt.h langopts.c langopts.h local_endian.h mbrola.h mnemonics.c \
  mnemonics.h numbers.c numbers.h phoneme.c phoneme.h phonemelist.c \
  phonemelist.h readclause.c readclause.h setlengths.c setlengths.h sintab.h \
  soundicon.c soundicon.h spect.c spect.h speech.c speech.h ssml.c \
  ssml.h synthdata.c synthdata.h synthesize.c synthesize.h \
  translate.c translate.h translateword.c translateword.h tr_languages.c \
  voice.h voices.c wavegen.c wavegen.h; do
    cp ../../lib/espeak-ng-arduino/src/libespeak-ng/$i .
done

# Build header for each dictionary, align on 4 byte boundary
mkdir -p dict
for i in /tmp/espeak/share/espeak-ng-data/*_dict; do
    xxd -i $i | sed 's/unsigned/const unsigned/' | sed 's/_tmp_espeak_share_espeak_ng_data//' | sed 's/\]/] __attribute__((aligned((4))))/' > dict/$(basename $i).h
done

# The phoneme database, also needs alignment
mkdir -p phoneme
for i in phondata phonindex phontab intonations; do
    xxd -i /tmp/espeak/share/espeak-ng-data/$i | sed 's/unsigned/const unsigned/' | sed 's/_tmp_espeak_share_espeak_ng_data//' | sed 's/\]/] __attribute__((aligned((4))))/' > phoneme/$i.h
done

# All voices (languages).  No need to align, they are stringly processed
mkdir -p voice
for i in `find /tmp/espeak/share/espeak-ng-data/lang/ -type f -print`; do
    n=$(echo $i | cut -f8- -d/ | sed 's/\//_/')
    if [ "$n" == "" ]; then n=$(echo $i | cut -f7- -d/ | sed 's/\//_/'); fi
    out=$(echo $n | sed s/-/_/g | tr /A-Z/ /a-z/)
    xxd -i $i | sed 's/unsigned char.*/const unsigned char _'$out'[] = {/' | grep -v 'unsigned int' > voice/$out.h 
    m=$(grep ^name $i | head -1 | cut -f2- -d" ")
    o=$(head -1 voice/$out.h | cut -f4 -d" " | sed s/lang/name/)
    echo "const char _name$o = \"$m\";" >> voice/$out.h
done
