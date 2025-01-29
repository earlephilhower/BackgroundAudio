/*
    BackgroundAudio
    Plays an audio file using IRQ driven decompression.  Main loop() writes
    data to the buffer but isn't blocked while playing

    Copyright (c) 2024 Earle F. Philhower, III <earlephilhower@yahoo.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#include "BackgroundAudioAAC.h"
#include "BackgroundAudioMP3.h"
#include "BackgroundAudioWAV.h"
#include "BackgroundAudioMixer.h"

/**
    @mainpage
    Main Project Page
    =================
    https://github.com/earlephilhower/BackgroundAudio

    README
    ------
    https://earlephilhower.github.io/BackgroundAudio/md_README.html

    Class Listing
    -------------
    https://earlephilhower.github.io/BackgroundAudio/annotated.html

    ESpeak-NG Language Reference
    ----------------------------
    https://earlephilhower.github.io/BackgroundAudio/md_LANGUAGES.html
*/
