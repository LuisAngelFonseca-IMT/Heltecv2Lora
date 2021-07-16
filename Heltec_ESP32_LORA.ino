//Include libraries 
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <ssd1306.h>

// Sensors pin 
#define DHTPIN 13
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define PIN_SDA 4
#define PIN_SCL 15
#define PHOTO 12
#define SOIL 36

// Output strings from sensors
String humd;
String temp;
String lig;
String humsoil;

// OLED pin map for Heltec Board
#ifndef OLED_RST
  #define OLED_RST 16
#endif
#ifndef OLED_SDA
  #define OLED_SDA 4
#endif
#ifndef OLED_SCL
  #define OLED_SCL 15
#endif

SSD1306 display(OLED_RST);

#if (SSD1306_LCDHEIGHT != 64)
 #error("Height incorrect, please fix ssd1306.h!");
#endif

// Little-indian format APPEUI
static const u1_t PROGMEM APPEUI[8] = {"INSERT"};
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}
// Little endian format, DEVEUI
static const u1_t PROGMEM DEVEUI[8] = {"INSERT"};
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}
// big indian format APPKEY
static const u1_t PROGMEM APPKEY[16] = {"INSERT"};
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}

// payload to send to TTN gateway
static uint8_t payload[7];
static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 1200;

// Pin mapping for Heltec WiFi LoRa Board
const lmic_pinmap lmic_pins = {
    .nss = 18,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 14,
    .dio = {26, 35, 34},
    .rxtx_rx_active = 0,
    .rssi_cal = 0,              // This may not be correct for Heltec board
    .spi_freq = 8000000,
};

// init. DHT
DHT dht(DHTPIN, DHTTYPE);

void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            display.print("TTN: Joining...");
            display.display();
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            display.clearDisplay();
            display.display();
            display.setCursor(0, 0);
            display.println("TTN: Connected");
            display.display();
            Serial.println(F("EV_JOINED"));
            {
              u4_t netid = 0;
              devaddr_t devaddr = 0;
              u1_t nwkKey[16];
              u1_t artKey[16];
              LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
              Serial.print("netid: ");
              Serial.println(netid, DEC);
              Serial.print("devaddr: ");
              Serial.println(devaddr, HEX);
              Serial.print("artKey: ");
              for (int i=0; i<sizeof(artKey); ++i) {
                if (i != 0)
                  Serial.print("-");
                Serial.print(artKey[i], HEX);
              }
              Serial.println("");
              Serial.print("nwkKey: ");
              for (int i=0; i<sizeof(nwkKey); ++i) {
                      if (i != 0)
                              Serial.print("-");
                      Serial.print(nwkKey[i], HEX);
              }
              Serial.println("");
            }
            // Disable link check validation (automatically enabled
            // during join, but because slow data rates change max TX
      // size, we don't use it in this example.
            LMIC_setLinkCheckMode(0);
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
            break;
        case EV_TXCOMPLETE:
            display.clearDisplay();
            display.display();
            display.setCursor(0, 0);
            display.println("TTN: Connected");
            display.display();
            display.setCursor(0, 10);
            display.println("* Sent!");
            display.setCursor(0, 20);
            display.print(lig);
            display.setCursor(0, 30);
            display.println(humsoil);
            display.setCursor(0, 40);
            display.println(humd);
            display.setCursor(0, 50);
            display.println(temp); 
            display.display();
            Serial.println(F("EV_TXSTART"));
            display.display();   
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.println(F("Received "));
              Serial.println(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            // Schedule next transmission
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
        /*
        || This event is defined but not used in the code. No
        || point in wasting codespace on it.
        ||
        || case EV_SCAN_FOUND:
        ||    Serial.println(F("EV_SCAN_FOUND"));
        ||    break;
        */
        case EV_TXSTART:
            display.clearDisplay();
            display.display();
            display.setCursor(0, 0);
            display.println("TTN: Connected");
            display.setCursor(0, 10);
            display.println("* Sending");
            display.setCursor(0, 20);
            display.print(lig);
            display.setCursor(0, 30);
            display.println(humsoil);
            display.setCursor(0, 40);
            display.println(humd);
            display.setCursor(0, 50);
            display.println(temp); 
            display.display();
            Serial.println(F("EV_TXSTART"));
            break;
        default:
            Serial.print(F("Unknown event: "));
            Serial.println((unsigned) ev);
            break;
    }
}

void do_send(osjob_t* j){
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {

      // Read Brightness
      float b = map(analogRead(PHOTO), 0, 4000, 0, 100);
      lig = String("Brightnes: ") + (b) + ("%");
      b = b/100;
      uint16_t payloadBright = LMIC_f2sflt16(b);
      byte brightLow = lowByte(payloadBright);
      byte brightHigh = highByte(payloadBright);
      payload[0] = brightLow;
      payload[1] = brightHigh;


      // Read Soil Moisture
      float sm = map(analogRead(SOIL), 4000, 300, 0, 100);
      humsoil = String("Soil M: ") + (sm) + ("%"); 
      sm = sm/100;
      uint16_t payloadSM = LMIC_f2sflt16(sm);
      byte SMLow = lowByte(payloadSM);
      byte SMHigh = highByte(payloadSM);
      payload[2] = SMLow;
      payload[3] = SMHigh;


      // Read humidity
      float h = dht.readHumidity();
      humd = String("Humidity: ") + (h) + ("%");
      h = h/100;
      uint16_t payloadHumd = LMIC_f2sflt16(h);
      byte humLow = lowByte(payloadHumd);
      byte humHigh = highByte(payloadHumd);
      payload[4] = humLow;
      payload[5] = humHigh;


      // Read Temperature
      float t = dht.readTemperature();
      temp = String("Temp: ") + (t) + ("C");
      t = t/100;
      uint16_t payloadTemp = LMIC_f2sflt16(t);
      byte tempLow = lowByte(payloadTemp);
      byte tempHigh = highByte(payloadTemp);
      payload[6] = tempLow;
      payload[7] = tempHigh;

      // Upstream data
      LMIC_setTxData2(1, payload, sizeof(payload)-1, 0);
      Serial.println(F("EV_TXSTART"));
    }
    // Next TX is scheduled after TX_COMPLETE event.
}

void setup() {
    delay(5000);
    while (! Serial);
    Serial.begin(9600);
    Serial.println(F("Starting"));

    dht.begin();
    // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
    Wire.begin(OLED_SDA,OLED_SCL,100000);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x64)
    Serial.println("OLED and DHT init'd");

    // Show image buffer on the display hardware.
    // Since the buffer is intialized with an Adafruit splashscreen
    // internally, this will display the splashscreen.
    display.display();
    delay(1000);
   
    // Clear the buffer.
    display.clearDisplay();
    display.display();

    // set text display size/location
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    
    // LMIC init
    os_init();
    LMIC_reset();
    LMIC_setLinkCheckMode(0);
    LMIC_setDrTxpow(DR_SF7,14);
    #if defined(CFG_eu868)
    LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      // g-band
    LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK,  DR_FSK),  BAND_MILLI);      // g2-band
    #elif defined(CFG_us915)
    LMIC_selectSubBand(1);
    #endif
    do_send(&sendjob);
}

void loop() {
  os_runloop_once();
}
