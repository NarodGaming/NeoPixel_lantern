/*
Virtual Ports:
V0 = White Candle Effect
V1 = Green Candle Effect
V2 = Blue Candle Effect
V3 = Fire Candle Effect
V4 = Random Candle Effect
V5 = Yellow Candle Effect
V10 = OFF
V11 = Reset
V15 = COOLING
V16 = SPARKING
V18 = FRAMES_PER_SECOND
V20 = Terminal Widget
V21 = UpTime
V22 = WiFi Signal Strength
*/

#define BLYNK_PRINT Serial

#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <BlynkSimpleEsp8266.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <SimpleTimer.h>

#define LED_PIN     D3
#define COLOR_ORDER GRB
#define CHIPSET     WS2812
#define NUM_LEDS    24

#define BRIGHTNESS	200
int FRAMES_PER_SECOND = 60;

#define HOSTNAME "NeoPixel_lanten"

//WS2812 Direction
bool gReverseDirection = false;

CRGB leds[NUM_LEDS];
CRGBPalette16 gPal;

// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 55, suggested range 20-100 
int COOLING = 55;

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
int SPARKING = 120;

#define EEPROM_SALT 12661
typedef struct {
	char  bootState[4] = "on";
	char  blynkToken[33] = "BLYNK_API_KEY";
	char  blynkServer[33] = "personal.blynk.server";
	char  blynkPort[6] = "8442";
	int   salt = EEPROM_SALT;
} WMSettings;

WMSettings settings;

//for LED status
Ticker ticker;

//For Simple timer
SimpleTimer timer;

//flag for saving data
bool shouldSaveConfig = false;
//static bool BLYNK_ENABLED = true;

WidgetTerminal terminal(V20);

int trigger;

void setup() {
  Serial.begin(115200);

  Serial.print(F("Heap: ")); Serial.println(system_get_free_heap_size());
  Serial.print(F("Boot Vers: ")); Serial.println(system_get_boot_version());
  Serial.print(F("CPU: ")); Serial.println(system_get_cpu_freq());
  Serial.print(F("SDK: ")); Serial.println(system_get_sdk_version());
  Serial.print(F("Chip ID: ")); Serial.println(system_get_chip_id());
  Serial.print(F("Flash ID: ")); Serial.println(spi_flash_get_id());
  Serial.print(F("Flash Size: ")); Serial.println(ESP.getFlashChipRealSize());
  Serial.print(F("Vcc: ")); Serial.println(ESP.getVcc());
  Serial.println();

  const char *hostname = HOSTNAME;

  delay(3000); // sanity delay
  WiFiManager wifiManager;

  //reset known SSID - for testing
  //wifiManager.resetSettings();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //timeout - this will quit WiFiManager if it's not configured in 6 minutes, causing a restart
  wifiManager.setConfigPortalTimeout(360);

  //custom params
  EEPROM.begin(512);
  EEPROM.get(0, settings);
  EEPROM.end();

  if (settings.salt != EEPROM_SALT) {
	  Serial.println("Invalid settings in EEPROM, trying with defaults");
	  WMSettings defaults;
	  settings = defaults;
  }

  WiFiManagerParameter custom_boot_state("boot-state", "on/off on boot", settings.bootState, 33);
  wifiManager.addParameter(&custom_boot_state);

  Serial.println(settings.bootState);

  WiFiManagerParameter custom_blynk_text("<br/>Blynk config.<br/>");
  wifiManager.addParameter(&custom_blynk_text);

  WiFiManagerParameter custom_blynk_token("blynk-token", "blynk token", settings.blynkToken, 33);
  wifiManager.addParameter(&custom_blynk_token);

  WiFiManagerParameter custom_blynk_server("blynk-server", "blynk server", settings.blynkServer, 33);
  wifiManager.addParameter(&custom_blynk_server);

  WiFiManagerParameter custom_blynk_port("blynk-port", "port", settings.blynkPort, 6);
  wifiManager.addParameter(&custom_blynk_port);

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  if (!wifiManager.autoConnect(hostname)) {
	  Serial.println("failed to connect and hit timeout");
	  //reset and try again, or maybe put it to deep sleep
	  ESP.reset();
	  delay(1000);
  }

  //Serial.println(custom_blynk_token.getValue());
  //save the custom parameters to FS
  if (shouldSaveConfig) {
	  Serial.println("Saving config");

	  strcpy(settings.bootState, custom_boot_state.getValue());
	  strcpy(settings.blynkToken, custom_blynk_token.getValue());
	  strcpy(settings.blynkServer, custom_blynk_server.getValue());
	  strcpy(settings.blynkPort, custom_blynk_port.getValue());

	  Serial.println(settings.bootState);
	  Serial.println(settings.blynkToken);
	  Serial.println(settings.blynkServer);
	  Serial.println(settings.blynkPort);

	  EEPROM.begin(512);
	  EEPROM.put(0, settings);
	  EEPROM.end();
  }

  //config blynk
  Blynk.config(settings.blynkToken, settings.blynkServer, atoi(settings.blynkPort));
  
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( BRIGHTNESS );

  Serial.print("Setup Finished !!!");
  Serial.println();

  timer.setTimeout(100, updateui);
  timer.setInterval(1000, sendUptime);
}

void loop()
{
	Blynk.run(); // Initiates SimpleTimer
	timer.run(); // Initiates SimpleTimer
	startthefire();
}

void startthefire() {
	Fire2012WithPalette(); // run simulation frame, using palette colors
	FastLED.show(); // display this frame
	FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void sendUptime() {
	Blynk.virtualWrite(V21, millis() / 1000);
	long rssi = WiFi.RSSI();
	Blynk.virtualWrite(V22, rssi);
}

//gets called when WiFiManager enters configuration mode
void configModeCallback(WiFiManager *myWiFiManager) {
	Serial.println("Entered config mode");
	Serial.println(WiFi.softAPIP());
	//if you used auto generated SSID, print it
	Serial.println(myWiFiManager->getConfigPortalSSID());
	//entered config mode, make led toggle faster
	ticker.attach(0.2, tick);
}

void tick()
{
	//toggle state
	int state = digitalRead(LED_BUILTIN);  // get the current state of GPIO1 pin
	digitalWrite(LED_BUILTIN, !state);     // set pin to the opposite state
}

//callback notifying us of the need to save config
void saveConfigCallback() {
	Serial.println("Should save config");
	shouldSaveConfig = true;
}

void Fire2012WithPalette()
{
// Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
    for( int i = 0; i < NUM_LEDS; i++) {
      heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
    }
  
    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for( int k= NUM_LEDS - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
    }
    
    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if( random8() < SPARKING ) {
      int y = random8(7);
      heat[y] = qadd8( heat[y], random8(160,255) );
    }

    // Step 4.  Map from heat cells to LED colors
    for( int j = 0; j < NUM_LEDS; j++) {
      // Scale the heat value from 0-255 down to 0-240
      // for best results with color palettes.
      byte colorindex = scale8( heat[j], 240);
      CRGB color = ColorFromPalette( gPal, colorindex);
      int pixelnumber;
      if( gReverseDirection ) {
        pixelnumber = (NUM_LEDS-1) - j;
      } else {
        pixelnumber = j;
      }
      leds[pixelnumber] = color;
    }
}

void updateui() {
	Blynk.virtualWrite(V15, COOLING);
	Blynk.virtualWrite(V16, SPARKING);
	Blynk.virtualWrite(V18, FRAMES_PER_SECOND);
}

BLYNK_WRITE(V0) { //White Candle Effect
	trigger = param.asInt();
	if (trigger == 1) {
		gPal = CRGBPalette16(CRGB::Black, CRGB::MistyRose, CRGB::FloralWhite, CRGB::MintCream);
	}
}

BLYNK_WRITE(V1) { //Green Candle Effect
	trigger = param.asInt();
	if (trigger == 1) {
		gPal = CRGBPalette16(CRGB::Black, CRGB::Green, CRGB::LimeGreen, CRGB::LawnGreen);
	}
}

BLYNK_WRITE(V2) { //Blue Candle Effect
	trigger = param.asInt();
	if (trigger == 1) {
		gPal = CRGBPalette16(CRGB::Black, CRGB::Navy, CRGB::DeepSkyBlue, CRGB::LightBlue);
	}
}

BLYNK_WRITE(V3) { //Fire Candle Effect
	trigger = param.asInt();
	if (trigger == 1) {
		gPal = CRGBPalette16(CRGB::Black, CRGB::Red, CRGB::Yellow, CRGB::White);
	}
}

BLYNK_WRITE(V4) { //Random Candle Effect
	trigger = param.asInt();
	if (trigger == 1) {
		static uint8_t hue = 0;
		hue++;
		CRGB darkcolor = CHSV(hue, 255, 192); // pure hue, three-quarters brightness
		CRGB lightcolor = CHSV(hue, 128, 255); // half 'whitened', full brightness
		gPal = CRGBPalette16(CRGB::Black, darkcolor, lightcolor, CRGB::White);
	}
}

BLYNK_WRITE(V5) { //Yellow Candle Effect
	trigger = param.asInt();
	if (trigger == 1) {
		gPal = CRGBPalette16(CRGB::Black, CRGB::Yellow, CRGB::Gold, CRGB::LightYellow);
	}
}

BLYNK_WRITE(V10) { //OFF
	trigger = param.asInt();
	if (trigger == 1) {
		gPal = CRGBPalette16(CRGB::Black, CRGB::Black, CRGB::Black);
	}
}

BLYNK_WRITE(V11) { //Reset Values
	trigger = param.asInt();
	COOLING = 55;
	SPARKING = 120;
	FRAMES_PER_SECOND = 60;
	if (trigger == 1) {
		updateui();
	}
}

BLYNK_WRITE(V15)
{
	COOLING = param.asInt();
	terminal.print("COOLING: ");
	terminal.println(COOLING);
	terminal.flush();
}

BLYNK_WRITE(V16)
{
	SPARKING = param.asInt();
	terminal.print("SPARKING: ");
	terminal.println(SPARKING);
	terminal.flush();
}

BLYNK_WRITE(V18)
{
	FRAMES_PER_SECOND = param.asInt();
	terminal.print("FRAMES_PER_SECOND: ");
	terminal.println(FRAMES_PER_SECOND);
	terminal.flush();
}