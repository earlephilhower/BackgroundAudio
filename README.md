# BackgroundAudio - Play MP3, AAC, and WAV on the Raspberry Pi Pico (RP2040) and Pico 2 (RP2350)

BackgroundAudio is an Arduino library that lets sketches play MP3s, AACs, and WAVs on the Raspberry Pi Pico
and Pico 2 with a very simple application level interface.  Data is written to the playback object when
available from the main application, and interrupts (IRQs) are used to generate data for I2S, PWMAudio,
or Bluetooth A2DP in the background.  The application only needs to provide data faster than it is
decompressed.  There is a source buffer that's managed by the playback routine, letting apps "fire and forget"
for playback.

This is based off of [ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio) but is
significantly simplified in terms of API and usage, as well as available options. This allows for a much
more efficient use of the processor and a better playback experience in most cases.  If you need the ultimate
in control and capabilities, see [ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio).  If you
just want to play back audio without blocking your main application, this library can help.

Because this library is much more optimized and efficient than ESP8266Audio, a standard Pico W can actually
easily stream an MP3 web radio station using HTTPS encryption (the standard).  A Pico 2 W can easily handle
a HE-AAC web station under the same conditions, too, because of it's much higher IPC core and higher clock
rate.

(As a point of comparison, the WebRadio example in the ESP8266Audio library is not very stable because the
audio processing is happening in the same context as the web interface and net streaming code leading to
dropouts and hangs in extended use.  The WebRadioMP3PlusWebUI sketch here is basically the same user application
but because of the interrupt-driver, buffer-based architecture playback is very smooth even on an original
Pico and even with HTTPS decryption in software.  The MP3 decoder and HTTPs encryption stack are the same
in both libraries, they're just used more efficiently here.)

## Compatibility

BackgroundAudio today builds and runs under [Arduino-Pico](https://github.com/earlephilhower/arduino-pico),
but ports to the ESP32 family would be welcomed.

## Adding BackgroundAudio to an Application

Simply include the appropriate header, or `BackgroundAudio.h` for all codecs.  Instantiate a global object of
the appropriate type, passing in an output device (`I2S`, `PWMAudio`, or even `A2DPSink`) to the constructor.
When the app is initialized, simply call `begin()` and start `write`ing in data as it is available.

Note that the app is responsible for feeding in data from whatever source to the decoder.  The decoder itself,
because it runs at interrupt level, cannot read the filesystem or network to collect samples (either operation
could block which is not allowed at interrupt time).  If all data can live in flash, there is a ROM-based
playback object which _can_ feed itself, but only from arrays in flash and not files in `LittleFS` or `SD.  See
the examples for more details on this mode.

In the case of underflow the playback object will play silence and should recover when new data arrives.  The
built-in source (raw, uncompressed) buffer should help reduce this possibility.

````
// Barnyard sounds for children sketch
#include <BackgroundAudio.h>
#include <LittleFS.h>
...
PWMAudio pwm(0);
BackgroundAudioMP3 mp3(pwm);
...
void setup() {
    LittleFS.begin();
    mp3.begin();
}

File f;
void loop() {
    // If there's a file open, stuff the playback buffer with it
    while (f && mp3.availableForWrite() > 512) {
        int len = f.read(filebuff, 512);
        // Write whatever was read out.
        // Even if we change files in the middle, the playback object will resynchronize automatically
        mp3.write(filebuff, len);
        if (len != 512) {
            f.close(); // Short reads == EOF
        }
    }
    ...
    // Display UI, check buttons, etc.
    ...
    if (RoosterButtonPressed) {
        f = LittleFS.open("cluck.mp3");
    } else if (CowButtonPressed) {
        f = LittleFS.open("moo.mp3");
    } else ...

}
````

## Performance

The SpeedTest example included provided allows for different optimization and CPUs to be tested.  I
recommend using `-O3` optimization mode, available in the IDE menus, as it can reduce CPU usage by
a very large amount on the Pico.  As is obvious from the numbers below, HE-AAC decoding is much more
computationally intensive than simple AAC.  (The original Pico can playback most normal AAC files without
issue, but many web radio streamers use HE-AAC to minimize bandwidth.)

### PICO 2, -O3  (ARM mode)
````
HE-AAC decode cycles: 112322331, frames 103, cycles/sample 1486.00
AAC decode cycles: 112322331, frames 206, cycles/sample 532.47
MP3 decode cycles: 111250252, frames 183, cycles/sample 527.71
````

### PICO, -O3
````
HE-AAC decode cycles: 310594598, frames 103, cycles/sample 6390.67
AAC decode cycles: 310594598, frames 206, cycles/sample 1472.40
MP3 decode cycles: 201450592, frames 183, cycles/sample 955.58
````

## Examples

* BeepWAV : Plays "beep" using PWM to a phono jack every time the BOOTSEL button is pressed.  Shows how the main app can continue doing work while playback is ongoing and how to start a new WAV file without needing to finish a currently playing one.
* PlayAACROM : Plays an AAC piano sample over PWM every time a character comes in over Serial.  Demonstrates the ROM playback objects and how they don't need any application interaction after they're fired off.
* SimpleMP3Shuffle : Convert your Pico into an SD-card MP3 player.  Continuously plays randomly selected MP3 files from the root folder of an attached SD card.  Demonstrates how an application can send raw data to the decoder while still doing other processing.
* SpeedTest : Calculates the number of CPU cycles per output sample for MP3, AAC, and HE-AAC using the current CPU and optimization settings.  Useful to see how different `-Ox` settings change performance,
* WebradioMP3PlusWebUI : Shows a more complete application that can play an MP3 web radio using HTTPS connectivity.  Includes a serial and HTTP WebServer interface to allow the user to change URLs, volumes, and see the ICY metadata.

## Codec Licensing

* AAC decode source is from the Helix project and licensed under RealNetwork's RSPL license.  For commercial use you're still going to need the usual AAC licensing from [Via Licensing](http://www.via-corp.com/us/en/licensing/aac/overview.html).
* MP3 decode source is from libMAD by Underbit Technologies, Inc., and licensed under the GPL.
