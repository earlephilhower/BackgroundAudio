// SerialSpeak - Earle F. Philhower, III <earlephilhower@yahoo.com>
// Released to the public domain January 2025

// Reads from the serial port and plays what's typed over the output
// asynchronously.  Can queue up work while still speaking.
// Demonstrates dictionary and voice usage

#include <BackgroundAudioSpeech.h>

#define ENGLISH 0
#define FRENCH !ENGLISH // Many wars have been fought over this...

#if ENGLISH
#include <libespeak-ng/dict/en_dict.h>
#define DICT _en_dict
#include <libespeak-ng/voice/en_029.h>
#include <libespeak-ng/voice/en_gb_scotland.h>
#include <libespeak-ng/voice/en_gb_x_gbclan.h>
#include <libespeak-ng/voice/en_gb_x_gbcwmd.h>
#include <libespeak-ng/voice/en_gb_x_rp.h>
#include <libespeak-ng/voice/en.h>
#include <libespeak-ng/voice/en_shaw.h>
#include <libespeak-ng/voice/en_us.h>
#include <libespeak-ng/voice/en_us_nyc.h>
BackgroundAudioVoice v[] = {
  ADDVOICETOARRAY(en_029),
  ADDVOICETOARRAY(en_gb_scotland),
  ADDVOICETOARRAY(en_gb_x_gbclan),
  ADDVOICETOARRAY(en_gb_x_gbcwmd),
  ADDVOICETOARRAY(en),
  ADDVOICETOARRAY(en_shaw),
  ADDVOICETOARRAY(en_us),
  ADDVOICETOARRAY(en_us_nyc)
};
#else // If you're not English, you're French
#include <libespeak-ng/dict/fr_dict.h>
#define DICT _fr_dict
#include <libespeak-ng/voice/fr_be.h>
#include <libespeak-ng/voice/fr_ch.h>
#include <libespeak-ng/voice/fr.h>
BackgroundAudioVoice v[] = {
  ADDVOICETOARRAY(fr_be),
  ADDVOICETOARRAY(fr_ch),
  ADDVOICETOARRAY(fr)
};
#endif


#ifdef ESP32
#include <ESP32I2SAudio.h>
ESP32I2SAudio audio(4, 5, 6); // BCLK, LRCLK, DOUT (,MCLK)
#else
#include <PWMAudio.h>
PWMAudio audio(0);
#endif
BackgroundAudioSpeech BMP(audio);

void usage() {
  Serial.printf("Simple Speech Example\r\n");
  Serial.printf("Enter a line with *# to change voice (i.e. *3 to go to 3rd voice in list)\r\n");
  Serial.printf("Installed voices:\r\n");
  for (int i = 0; i < (int)sizeof(v) / (int)sizeof(v[0]); i++) {
    Serial.printf("*%d: %s\r\n", i, v[i].name);
  }
  Serial.printf("Otherwise, any typed words will be read aloud.\r\n\r\n");
}

void setup() {
  Serial.begin(115200);

  // We need to set up the dictionary and voice before any output
  BMP.setDict(DICT, sizeof(DICT));
  BMP.setVoice(v[0].data, v[0].len);

  delay(3000);
  Serial.printf("Ready!\r\n");
  BMP.begin();
  usage();
#if ENGLISH
  BMP.speak("I have awoken.  Tremble in fear!");
#else
  BMP.speak("Je me suis levé.  Ayez très peur!");
#endif
}

void loop() {
  if (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    if (s[0]  == '?') {
      usage();
    } else if (s[0] == '*') {
      int voice = s.substring(1, 2).toInt();
      if ((voice >= 0) && (voice < (int) sizeof(v) / (int) sizeof(v[0]))) {
        BMP.flush();
        while (!BMP.done()) {
          delay(1); // Busy-wait because it's not safe to change voices while producing samples...
        }
        BMP.setVoice(v[voice].data, v[voice].len);
        Serial.printf("Changed voice to '%s'\r\n", v[voice].name);
      } else {
        Serial.printf("Error: Voice number %d out of bounds\r\n", voice);
      }
    } else {
      Serial.printf("Speaking '%s'\r\n", s.c_str());
      BMP.write(s.c_str(), strlen(s.c_str()) + 1 /* Need to include \0 */);
    }
  }
  delay(10);
}