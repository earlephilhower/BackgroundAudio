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

# Licenses
cp ../../lib/espeak-ng-arduino/COPYING* .

# espeak-ng files
for i in common.c common.h compiledict.h config.h dictionary.c dictionary.h \
  encoding.c error.c error.h espeak_api.c espeak_command.h \
  event.h fifo.c fifo.h ieee80.c ieee80.h intonation.c intonation.h \
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
    echo "#pragma once" > dict/$(basename $i).h
    echo "const unsigned char __espeakng_dict[] __attribute__((aligned((4)))) = {" >> dict/$(basename $i).h
    xxd -i $i | grep -v "unsigned char" >> dict/$(basename $i).h
    sed -i 's/unsigned int.* =/size_t __espeakng_dictlen =/' dict/$(basename $i).h
done

# The phoneme database, also needs alignment
mkdir -p phoneme
for i in phondata phonindex phontab intonations; do
    echo "#pragma once" > phoneme/$i.h
    xxd -i /tmp/espeak/share/espeak-ng-data/$i | sed 's/unsigned/const unsigned/' | sed 's/_tmp_espeak_share_espeak_ng_data//' | sed 's/\]/] __attribute__((aligned((4))))/' | grep -v "const unsigned int" >> phoneme/$i.h
done

# All voices (languages).  No need to align, they are stringly processed
rm -f ../../LANGUAGES.md
mkdir -p voice
for i in `find /tmp/espeak/share/espeak-ng-data/lang/ -type f -print`; do
    dos2unix -q $i
    n=$(echo $i | cut -f8- -d/ | sed 's/\//_/')
    if [ "$n" == "" ]; then n=$(echo $i | cut -f7- -d/ | sed 's/\//_/'); fi
    out=$(echo $n | sed s/-/_/g | tr /A-Z/ /a-z/)
    m=$(grep ^name $i | head -1 | cut -f2- -d" " | sed 's/\/\/.*//')
    echo "// Auto-generated, do not edit!  See lib/update_lib.sh" > voice/$out.h
    echo "#pragma once" >> voice/$out.h
    echo "#include <BackgroundAudioSpeech.h>" >> voice/$out.h
    xxd -i $i | sed 's/unsigned char.*/const unsigned char __rawdata_'$out'[] = {/' | grep -v 'unsigned int' >> voice/$out.h 
    echo "" >> voice/$out.h
    echo "BackgroundAudioVoice voice_$out = {" >> voice/$out.h
    echo "    \"$m\"," >> voice/$out.h
    echo "    $(stat -c %s $i)," >> voice/$out.h
    echo "    __rawdata_$out" >> voice/$out.h
    echo "};"  >> voice/$out.h
    langs=$(grep "^language" $i | cut -f2 -d" " | paste -s -d, | sed 's/,/, /g')
    assocdict=$(grep "^language" $i | cut -f2 -d" " | head -1 | cut -f1 -d-)
    echo "#include <libespeak-ng/dict/$assocdict""_dict.h>" >> voice/$out.h
    echo "| $m | \`#include <libespeak-ng/voice/$out.h>\` | \`BackgroundAudioSpeech::setVoice(voice_$out)\` |" >> ../../LANGUAGES.md
done
sort < ../../LANGUAGES.md > ../../LANGUAGES.md1
echo "# ESpeak-NG Language Headers" > ../../LANGUAGES.md
echo "| Language Name | Include File | Voice Variable |" >> ../../LANGUAGES.md
echo "| --- | --- | -- | ">> ../../LANGUAGES.md
cat ../../LANGUAGES.md1 >> ../../LANGUAGES.md
rm -f ../../LANGUAGES.md1
