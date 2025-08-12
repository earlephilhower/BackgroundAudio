// StreamMP3 - Earle F. Philhower, III <earlephilhower@yahoo.com>
// Released to the public domain, December 2024
//
// Connects to an HTTPS streaming MP3 radio station and decodes to PWMAudio.
// Connect earphone L/R to GPIO 0/1 and GND and configure your SSID and password.
// Provides basic Serial text and http web based control

#include <WiFi.h>
#include <HTTPClient.h>
#include <BackgroundAudio.h>
#include <WebServer.h>

#ifndef STASSID
#define STASSID "ssid"
#define STAPSK "password"
#endif

const char *ssid = STASSID;
const char *pass = STAPSK;

#ifdef ESP32
#include <ESP32I2SAudio.h>
ESP32I2SAudio audio(4, 5, 6); // BCLK, LRCLK, DOUT (, MCLK)
// Or for 1-pin PDM with external LPF
//#include <ESP32PDMAudio.h>
//ESP32PDMAudio audio(5);
#else
#include <I2S.h>
#include <PWMAudio.h>
// Uncomment either the PWM or I2S version, being sure to adjust the PWM pin or (BCLK,DATA) pins.
I2S audio(OUTPUT, 0, 2);
//PWMAudio audio(0);
#endif

#ifdef ESP32
// The ESP32 devices seem to have very variable HTTP performance.  Increase the buffer here to sort-of compensate
#define STREAMBUFF (32 * 1024)
#else
// Pico and PicoW work well with much smaller compressed/raw buffer...
#define STREAMBUFF (16 * 1024)
#endif

// Instantiate a MP3 player with the specified raw (compressed) data buffer
BackgroundAudioMP3Class<RawDataBuffer<STREAMBUFF>> mp3(audio);

#ifdef ESP32
NetworkClientSecure client;
#else
WiFiClientSecure client;  // Because URL is HTTPS, need a WiFiSecureClient.  Plain HTTP can use WiFiClient
#endif

String url = "https://ice.audionow.com/485BBCWorld.mp3"; // Check out https://fmstream.org/index.php?c=FT for others
HTTPClient http;
uint8_t buff[512]; // HTTP reads into this before sending to MP3
WebServer web(80); // The HTTP interface for remote control

int icyMetaInt = 0;
int icyDataLeft = 0;
int icyMetadataLeft = 0;
int gain = 100;
String status;
String title;

// C++11 multiline string constants are neato...
static const char PAGE[] = R"KEWL(
<head>
<title>BackgroundAudio Web Radio</title>
<script type="text/javascript">
  function updateTitle() {
    var x = new XMLHttpRequest();
    x.open("GET", "title");
    x.onload = function() { document.getElementById("titlespan").innerHTML=x.responseText; setTimeout(updateTitle, 1000); }
    x.onerror = function() { setTimeout(updateTitle, 1000); }
    x.send();
  }
  setTimeout(updateTitle, 1000);
  function showValue(n) {
    document.getElementById("volspan").innerHTML=n;
    var x = new XMLHttpRequest();
    x.open("POST", "setvol");
    x.send(n);
  }
  function updateStatus() {var x = new XMLHttpRequest();
    x.open("GET", "status");
    x.onload = function() { document.getElementById("statusspan").innerHTML=x.responseText; setTimeout(updateStatus, 1000); }
    x.onerror = function() { setTimeout(updateStatus, 1000); }
    x.send();
  }
  setTimeout(updateStatus, 1000);
  function updateURL() {var x = new XMLHttpRequest();
    x.open("GET", "url");
    x.onload = function() { document.getElementById("urlspan").innerHTML=x.responseText; setTimeout(updateURL, 1000); }
    x.onerror = function() { setTimeout(updateURL, 1000); }
    x.send();
  }
  setTimeout(updateURL, 1000);
</script>
</head>

<body>
BackgroundAudio Web Radio!
<hr>
Currently Playing: <span id="titlespan">--</span><br>
Volume: <input type="range" name="vol" min="0" max="200" steps="10" value="100" onchange="showValue(this.value)"/> <span id="volspan">100</span>%
<hr>
Status: <span id="statusspan">--</span>
<hr>
<iframe name="dummy" id="dummy" style="display: none;"></iframe>
Current URL: <span id="urlspan">--</span><br>
<form action="seturl" method="POST" target="dummy">
Change URL: <input type="text" name="url">
<input type="submit" size="60" value="Change"></form>
</body>)KEWL";


void ConnectWiFi() {
#ifndef ESP32
  WiFi.end();
#endif
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, pass);
  while (!WiFi.isConnected()) {
    Serial.print("..");
    delay(100);
  }
  Serial.print("http://");
  Serial.println(WiFi.localIP());
  web.begin();
}

void printHelp() {
  Serial.println("MP3 Streamer");
  Serial.println("+ = volume up");
  Serial.println("- = volume down");
  Serial.println("n<newurl> = change stream");
}

void handleVolume() {
  if ((web.method() != HTTP_POST) || !web.hasArg("plain")) {
    web.send(501, "text/plain", "Error");
  } else {
    gain = std::min(200, (int)web.arg("plain").toInt());
    gain = std::max(10, gain);
    mp3.setGain(gain / 100.0);
    web.send(200, "text/plain", "OK");
  }
}

void handleURL() {
  if ((web.method() != HTTP_POST) || !web.hasArg("url")) {
    web.send(501, "text/plain", "Error");
  } else {
    url = web.arg("url");
    http.end(); // Disconnect old, will connect on next loop()
    web.send(200, "text/plain", "OK");
  }
}

void setup() {
  Serial.begin(115200);
  delay(5000);
  printHelp();
  client.setInsecure(); // Don't worry about certs, just use encryption
  web.on("/", [] { web.send(200, "text/html", PAGE);});
  web.on("/status", [] { web.send(200, "text/plain", status.c_str());});
  web.on("/title", [] { web.send(200, "text/plain", title.c_str());});
  web.on("/url", [] { web.send(200, "text/plain", url.c_str());});
  web.on("/setvol", handleVolume);
  web.on("/seturl", handleURL);
  mp3.begin();
}


void loop() {
  static uint32_t last = 0;

  // Ensure WiFi is up.  If not, retry
  if (!WiFi.isConnected()) {
    ConnectWiFi();
    return;
  }

  web.handleClient();

  // If the HTTP stream drops, reconnect
  if (!http.connected()) {
    Serial.printf("(Re)connecting to '%s'...\n", url.c_str());
    http.end();
    http.begin(client, url);
    http.setReuse(true);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    const char *icyHdrs[] = { "icy-metaint" }; // What is the MD interval?
    http.collectHeaders(icyHdrs, 1);
    http.addHeader("Icy-MetaData", "1");
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
      http.end();
      Serial.printf("Can't GET: '%s'\n", url.c_str());
      delay(1000);
      return;
    }
    if (http.hasHeader("icy-metaint")) {
      icyMetaInt = http.header("icy-metaint").toInt();
      icyDataLeft = icyMetaInt;
    } else {
      icyMetaInt = 0;
    }
  }


  // Pump the MP3 player data.  Read what's available from the web and send to the MP3 object
  WiFiClient *stream = http.getStreamPtr();
  do {
    size_t httpavail = stream->available();
    httpavail = std::min(sizeof(buff), httpavail); // We can only read up to the buffer size
    size_t mp3avail = mp3.availableForWrite();
    if (!httpavail || !mp3avail) {
      break;
    }
    size_t toRead = std::min(mp3avail, httpavail); // Only read as much as we can send to MP3
    if (icyMetaInt) {
      toRead = std::min(toRead, (size_t)icyDataLeft);
    }
    int read = stream->read(buff, toRead);
    if (read < 0) {
      return; // Error in the read
    }
    mp3.write(buff, read);

    // If we drop too low, pause playback to let us catch up
    if (mp3.available() < 1024) {
      mp3.pause();
    } else if (mp3.paused() && mp3.available() > (STREAMBUFF / 2)) { // When paused wait until kind-of full before restarting
      mp3.unpause();
    }

    icyDataLeft -= read;
    if (icyMetaInt && !icyDataLeft) {
      while (!stream->available() && stream->connected()) {
        delay(1);
      }
      if (!stream->connected()) {
        return;
      }
      int totalCnt = stream->read() * 16;
      int cnt = totalCnt;

      // Read up to buff[] of metadata
      int buffCnt = std::min(sizeof(buff), (size_t)cnt); // The size of data to stuff in buff[]
      uint8_t *p = buff;
      while (buffCnt && stream->connected()) {
        read = stream->read(p, buffCnt);
        p += read;
        buffCnt -= read;
        cnt -= read;
      }

      // Throw out the rest
      while (cnt && stream->connected()) {
        stream->read(); // Throw out metadata larger than 512b buffer
        cnt--;
      }
      if (totalCnt) {
        buff[std::min(sizeof(buff) - 1, (size_t)totalCnt)] = 0;
        Serial.printf("md: '%s'\n", buff);
        char *titlestr = strstr((const char *)buff, "StreamTitle='");
        if (titlestr) {
          titlestr += 13; // Skip the StreamTitle=' bit
          // Really we should parse and check this isn't inside a ' section, but we're not that concerned right now
          char *end = strchr(titlestr, ';');
          if (end) {
            *(end - 1) = 0;
          }
          title = titlestr;
        }
      }
      icyDataLeft = icyMetaInt;
    }
  } while (true);

  // Can do UI processing, etc. at this point  Just be sure to run loop() often enough to get the 20-30KB/s of transfers needed for MP3 streaming
  if ((millis() - last) > 1000) {
    last = millis();
    sprintf((char *)buff, "buffer: %d, frames %lu, shifts %lu, underflows %lu, errors %lu, dumps %lu, uptime %lu", mp3.available(), mp3.frames(), mp3.shifts(), mp3.underflows(), mp3.errors(), mp3.dumps(), last);
    Serial.println((char*)buff);
    status = (char*)buff;
  }

  if (Serial.available()) {
    int chr = Serial.read();
    switch (chr) {
      case '\r': break;
      case '\n': break;
      case '?': printHelp(); break;
      case '+': gain = std::min(200, gain + 10); mp3.setGain(gain / 100.0); break;
      case '-': gain = std::max(10, gain - 10); mp3.setGain(gain / 100.0); break;
      case 'n': url = Serial.readString(); url.trim(); http.end(); break; // Will reconnect next pass
    }
  }
}
