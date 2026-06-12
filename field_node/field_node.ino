#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(4, 5);
const byte address[6] = "00001";

String NODE_ID = "B"; 

const char* ssid = "EMERGENCY_NETWORK";
const byte DNS_PORT = 53;

DNSServer dnsServer;
WebServer server(80);

unsigned long lastPing = 0;
uint8_t msgCounter = 0;

#define MAX_SEEN 20
String seenMessages[MAX_SEEN];
int seenIndex = 0;

String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Emergency Rescue Node</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body {
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  text-align: center;
  margin: 0;
  padding: 40px 20px;
  background: #0b0f14;
  color: #e0e0e0;
}
h2 {
  color: #ffffff;
  text-transform: uppercase;
  letter-spacing: 2px;
  border-bottom: 2px solid #ff4d4d;
  display: inline-block;
  padding-bottom: 10px;
  margin-bottom: 30px;
}
p { color: #8b9bb4; font-size: 16px; margin-bottom: 30px; }
input {
  padding: 16px;
  width: 90%;
  max-width: 320px;
  margin-bottom: 20px;
  font-size: 16px;
  border-radius: 6px;
  border: 1px solid #1f2937;
  background: #16202c;
  color: white;
}
input:focus { outline: none; border-color: #ff4d4d; }
button {
  padding: 16px 40px;
  font-size: 18px;
  background: #ff4d4d;
  color: white;
  font-weight: bold;
  letter-spacing: 1px;
  border: none;
  border-radius: 6px;
  cursor: pointer;
  box-shadow: 0 4px 6px rgba(255, 77, 77, 0.2);
}
button:active { background: #cc0000; }
#status_msg {
  color: #00ff9f;
  font-weight: bold;
  margin-top: 25px;
  font-size: 18px;
  display: none;
}
</style>
</head>
<body>
<h2>Aero-Bridge Uplink</h2>
<p>Secure connection established. Transmit your critical status below.</p>
<input id="coord" maxlength="15" placeholder="E.g., Bleeding, Trapped"><br>
<button onclick="send()">TRANSMIT SOS</button>
<p id="status_msg">DATA TRANSMITTED SUCCESSFULLY</p>

<script>
function send(){
  let c = document.getElementById("coord").value;
  if(c === "") return;
  fetch("/send?c=" + encodeURIComponent(c)).then(() => {
    document.getElementById("status_msg").style.display = "block";
    document.getElementById("coord").value = "";
    setTimeout(() => { document.getElementById("status_msg").style.display = "none"; }, 4000);
  });
}
</script>
</body>
</html>
)rawliteral";

String generateMsgID() {
  msgCounter++;
  return NODE_ID + String(msgCounter); 
}

bool alreadySeen(String id) {
  if (id == "") return false;
  for (int i = 0; i < MAX_SEEN; i++) {
    if (seenMessages[i] == id) return true;
  }
  return false;
}

void storeMsg(String id) {
  if (id == "") return;
  seenMessages[seenIndex] = id;
  seenIndex = (seenIndex + 1) % MAX_SEEN;
}

void sendNRF(String msg) {
  if (msg.length() > 31) {
    msg = msg.substring(0, 31);
  }

  radio.stopListening();
  char text[32];
  msg.toCharArray(text, 32);

  bool ok = radio.write(&text, sizeof(text));
  if (!ok) {
    delay(random(10, 30)); 
    radio.write(&text, sizeof(text));
  }
  radio.startListening();
}

void handleSend() {
  String coord = server.arg("c");
  String msgID = generateMsgID();
  
  String message = "H|" + NODE_ID + "|" + msgID + "|5|" + coord;
  
  storeMsg(msgID);
  sendNRF(message);

  server.send(200, "text/plain", "Sent");
}

void setup() {
  Serial.begin(115200);

  WiFi.softAP(ssid);
  
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  radio.begin();
  radio.openWritingPipe(address);
  radio.openReadingPipe(0, address);

  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(115);

  radio.startListening();

  server.on("/", [](){ server.send(200, "text/html", page); });
  server.on("/send", handleSend);

  server.onNotFound([]() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();

  Serial.println("Mesh Relay Node Ready. Origin ID: " + NODE_ID);
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  if (radio.available()) {
    char text[32] = "";
    radio.read(&text, sizeof(text));

    String received = String(text);
    Serial.println("RX: " + received);

    int p1 = received.indexOf('|');
    int p2 = received.indexOf('|', p1 + 1);
    int p3 = received.indexOf('|', p2 + 1);
    int p4 = received.indexOf('|', p3 + 1);

    if (p1 == -1 || p2 == -1 || p3 == -1 || p4 == -1) return;

    String type = received.substring(0, p1);
    String origin = received.substring(p1 + 1, p2);
    String msgID = received.substring(p2 + 1, p3);
    int ttl = received.substring(p3 + 1, p4).toInt();
    String data = received.substring(p4 + 1);

    if (alreadySeen(msgID)) return;

    storeMsg(msgID);

    if (ttl > 0) {
      delay(random(20, 80)); 

      String forwardMsg = type + "|" + origin + "|" + msgID + "|" + String(ttl - 1) + "|" + data;
      sendNRF(forwardMsg);
      Serial.println("Forwarding packet. Remaining TTL: " + String(ttl - 1));
    }
  }

  if (millis() - lastPing > 2000) {
    String msgID = generateMsgID();
    
    String pingMsg = "P|" + NODE_ID + "|" + msgID + "|3|";
    
    storeMsg(msgID);
    sendNRF(pingMsg);
    lastPing = millis();
  }
}
