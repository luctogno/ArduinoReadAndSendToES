
#include <SPI.h>
#include <Ethernet.h>

#include <avr/io.h>
#include <avr/wdt.h>

#define Reset_AVR() wdt_enable(WDTO_30MS); while(1) {}

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 205);

int val;
const int tempPin = 0;
const int ledPin = 8;

const String esHost = "192.168.0.25";
const char* esHostC = "192.168.0.25";
const int esPort = 9200;
const String esIndex = "temperature_monitor";
const String esType = "temp_entry";
const String esTempKey = "temperature";
const String esTimestampKey  = "ttimestamp";
const String esAuthorKey = "author";
const String esAuthorValue = "LTArduino";

EthernetClient ethClient;

void setup()
{
  pinMode(ledPin, OUTPUT);  //LED DI MONITORAGGIO

  digitalWrite(ledPin, HIGH);

  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip);
  }
  float cel = getTemperature();

  // give the Ethernet shield a second to initialize:
  delay(1000);
  Serial.println("connecting...");

  cel = getTemperature();

  // if you get a connection, report back via serial:
  if (ethClient.connect(esHostC, esPort)) {
    Serial.println("connected");
    // Make a HTTP request:
    ethClient.println("GET / HTTP/1.1");
    ethClient.println("Host: " + esHost);
    ethClient.println();
  } else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  }

  cel = getTemperature();
}

void loop()
{
  float cel = getTemperature();
  delay(900000);
  cel = getTemperature();
  // if there are incoming bytes available
  // from the server, read them and print them:
  if (ethClient.available()) {
    Serial.println("SONO CONNESSO");
    sendToElastic(cel);
  }

  // if the server's disconnected, stop the client:
  if (!ethClient.connected()) {
    Serial.println();
    Serial.println("disconnecting.");
    ethClient.stop();

    //WAIT A WHILE for check server availability
    delay(600000);
    // TRY TO RESET ARDUINO
    Reset_AVR()
  }

  // FINE CICLO
}

float getTemperature() {
  float val = analogRead(tempPin);
  float temp = (5 * val * 100) / 1024; //converte voltagem em temperatura

  Serial.print("TEMPERATURE = ");
  Serial.print(temp);
  Serial.print("*C");
  Serial.println();

  return temp;
}

void sendJson(char host[], int port, String path, String json) {
  digitalWrite(ledPin, HIGH);

  String request = "";
  request.concat("POST " + path + " HTTP/1.1\r\n");
  request.concat("Host: 192.168.0.25\r\n");
  request.concat("User-Agent: Arduino/1.0\r\n");
  //request.concat("Connection: close\r\n");
  request.concat("Content-Length: ");
  request.concat(json.length());
  request.concat("\r\n");
  request.concat("\r\n");
  request.concat(json + "\r\n");

  ethClient.println(request);
  ethClient.flush();
  Serial.println(request);

  delay(1000);
  digitalWrite(ledPin, LOW);
}

unsigned long inline ntpUnixTime (UDP &udp)
{
  static int udpInited = udp.begin(123); // open socket on arbitrary port

  const char timeServer[] = "pool.ntp.org";  // NTP server

  // Only the first four bytes of an outgoing NTP packet need to be set
  // appropriately, the rest can be whatever.
  const long ntpFirstFourBytes = 0xEC0600E3; // NTP request header

  // Fail if WiFiUdp.begin() could not init a socket
  if (! udpInited)
    return 0;

  // Clear received data from possible stray received packets
  udp.flush();

  // Send an NTP request
  if (! (udp.beginPacket(timeServer, 123) // 123 is the NTP port
         && udp.write((byte *)&ntpFirstFourBytes, 48) == 48
         && udp.endPacket()))
    return 0;       // sending request failed

  // Wait for response; check every pollIntv ms up to maxPoll times
  const int pollIntv = 150;   // poll every this many ms
  const byte maxPoll = 15;    // poll up to this many times
  int pktLen;       // received packet length
  for (byte i = 0; i < maxPoll; i++) {
    if ((pktLen = udp.parsePacket()) == 48)
      break;
    delay(pollIntv);
  }
  if (pktLen != 48)
    return 0;       // no correct packet received

  // Read and discard the first useless bytes
  // Set useless to 32 for speed; set to 40 for accuracy.
  const byte useless = 40;
  for (byte i = 0; i < useless; ++i)
    udp.read();

  // Read the integer part of sending time
  unsigned long time = udp.read();  // NTP time
  for (byte i = 1; i < 4; i++)
    time = time << 8 | udp.read();

  // Round to the nearest second if we want accuracy
  // The fractionary part is the next byte divided by 256: if it is
  // greater than 500ms we round to the next second; we also account
  // for an assumed network delay of 50ms, and (0.5-0.05)*256=115;
  // additionally, we account for how much we delayed reading the packet
  // since its arrival, which we assume on average to be pollIntv/2.
  time += (udp.read() > 115 - pollIntv / 8);

  // Discard the rest of the packet
  udp.flush();

  return time - 2208988800ul;   // convert NTP time to Unix time
}

void sendToElastic(float temp) {
  String path = "/" + esIndex + "/" + esType + "/";
  EthernetUDP udp;
  unsigned long unixTime = ntpUnixTime(udp);
  String json = "{";
  json.concat("\"" + esTempKey + "\":");
  json.concat(temp);
  json.concat(",");
  json.concat("\"" + esTimestampKey + "\":");
  json.concat(unixTime);
  json.concat(",");
  json.concat("\"" + esAuthorKey + "\":");
  json.concat("\"" + esAuthorValue + "\"");
  json.concat("}");

  sendJson(esHostC, esPort, path, json);
}

