#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(4, 5);
const byte address[6] = "00001";
String NODE_ID = "A";

const char* ssid = "Drone_Hub";
const char* password = "12345678";
WebServer server(80);

#define MAX_SEEN_MSGS 20
String seenMsgIds[MAX_SEEN_MSGS];
int seenMsgIndex = 0;

bool isDuplicate(String id) {
  if (id == "") return false;
  for (int i = 0; i < MAX_SEEN_MSGS; i++) {
    if (seenMsgIds[i] == id) return true;
  }
  return false;
}

void markSeen(String id) {
  if (id == "") return;
  seenMsgIds[seenMsgIndex] = id;
  seenMsgIndex = (seenMsgIndex + 1) % MAX_SEEN_MSGS;
}

#define MAX_NODES 10
struct Node {
  String id;
  unsigned long lastSeen;
  String lastMsg;
  String path; 
};
Node nodes[MAX_NODES];
int nodeCount = 0;

struct LogEntry {
  String id;
  String text;
};
LogEntry logs[10];
int logIndex = 0;

String latestData = "SCANNING...";
String status = "Offline";
int activeNodes = 0;

String webpage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>AERO-BRIDGE | COMMAND</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
:root {
  --bg: #05080a;
  --panel: rgba(16, 22, 28, 0.85);
  --accent: #00f2ff; 
  --warn: #ffae00;   
  --danger: #ff0055; 
  --text: #c4d1d9;
}
body {
  font-family: 'Segoe UI', 'Courier New', monospace;
  background: var(--bg);
  background-image: 
    linear-gradient(rgba(0, 242, 255, 0.05) 1px, transparent 1px),
    linear-gradient(90deg, rgba(0, 242, 255, 0.05) 1px, transparent 1px);
  background-size: 40px 40px;
  color: var(--text);
  margin: 0; padding: 20px;
  text-transform: uppercase;
  transition: background-color 0.1s;
}
.alert-flash { background-color: rgba(255, 0, 85, 0.3) !important; }

.header-box { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }
h2 {
  color: var(--accent); text-shadow: 0 0 15px rgba(0,242,255,0.5); letter-spacing: 3px;
  border-left: 5px solid var(--accent); padding-left: 15px; margin: 0;
}
.clock { color: var(--accent); font-size: 18px; font-weight: bold; }

.main-grid { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 20px; margin-bottom: 20px; }
.two-col-grid { display: grid; grid-template-columns: 300px 1fr; gap: 20px; }

.card {
  background: var(--panel); backdrop-filter: blur(10px); padding: 20px;
  border: 1px solid rgba(0, 242, 255, 0.2); box-shadow: 0 0 15px rgba(0,0,0,0.5);
  position: relative; border-radius: 4px;
}
.card::before {
  content: ""; position: absolute; top: 0; left: 0; width: 10px; height: 10px;
  border-top: 2px solid var(--accent); border-left: 2px solid var(--accent);
}
h3 { margin-top: 0; font-size: 11px; color: #6a808d; letter-spacing: 1px; }
.data-value { font-size: 24px; font-weight: bold; margin-top: 5px; }

.green { color: var(--accent); text-shadow: 0 0 10px rgba(0,242,255,0.5); }
.red { color: var(--danger); text-shadow: 0 0 10px rgba(255,0,85,0.5); }
.orange { color: var(--warn); text-shadow: 0 0 10px rgba(255,174,0,0.5); }

.drone-grid { display: flex; flex-wrap: wrap; gap: 15px; }
.drone-card { border-left: 4px solid var(--accent); width: 220px; padding: 15px; }
.bar-container { width: 100%; background: #111; height: 6px; margin: 5px 0 10px 0; border-radius: 3px; }
.bar-fill { height: 100%; background: var(--accent); border-radius: 3px; transition: width 0.5s; }
.drone-msg { font-size: 13px; color: var(--warn); background: rgba(255,174,0,0.1); padding: 8px; border: 1px dashed var(--warn); }

.radar {
  width: 250px; height: 250px; border-radius: 50%;
  background: 
    linear-gradient(90deg, rgba(0,242,255,0.2) 1px, transparent 1px) 50% 50% / 100% 100%,
    linear-gradient(rgba(0,242,255,0.2) 1px, transparent 1px) 50% 50% / 100% 100%,
    radial-gradient(circle, transparent 30%, rgba(0,242,255,0.1) 31%, transparent 32%),
    radial-gradient(circle, transparent 60%, rgba(0,242,255,0.1) 61%, transparent 62%);
  border: 2px solid rgba(0,242,255,0.4); margin: 0 auto; position: relative; overflow: hidden;
}
.radar::after {
  content: ""; position: absolute; top: 0; left: 0; width: 100%; height: 100%; border-radius: 50%;
  background: conic-gradient(from 0deg, transparent 75%, rgba(0,242,255,0.6) 100%);
  animation: spin 3s linear infinite;
}
.radar-node {
  position: absolute; width: 8px; height: 8px; background: var(--accent);
  border-radius: 50%; opacity: 0; 
  animation: fadeBlip 3s infinite; 
}
@keyframes spin { 100% { transform: rotate(360deg); } }
@keyframes fadeBlip {
  0% { opacity: 0; box-shadow: none; }
  2% { opacity: 1; box-shadow: 0 0 15px 5px var(--accent); }
  30% { opacity: 0; box-shadow: none; }
  100% { opacity: 0; }
}

.logbox { max-height: 200px; overflow-y: auto; font-size: 12px; background: rgba(0,0,0,0.4); padding: 15px; border: 1px solid rgba(0,242,255,0.1); }
.log-entry { padding: 8px 0; border-bottom: 1px solid rgba(0,242,255,0.05); color: #889; display: flex; justify-content: space-between; align-items: center; }
.log-entry.resolved { text-decoration: line-through; opacity: 0.5; border-left: 3px solid #4a5a63; padding-left: 10px; }
.ack-btn { background: rgba(0,242,255,0.1); border: 1px solid var(--accent); color: var(--accent); cursor: pointer; padding: 4px 10px; font-weight: bold; border-radius: 3px; }
.ack-btn:hover { background: var(--accent); color: #000; }
.ack-btn:disabled { background: transparent; border-color: #4a5a63; color: #4a5a63; cursor: not-allowed; }
</style>
</head>
<body>
<div style="max-width: 1200px; margin: auto;">
  
  <div class="header-box">
    <h2>AERO-BRIDGE // COMMAND</h2>
    <div class="clock" id="uptime">SYS_UPTIME: 00:00:00</div>
  </div>

  <div class="main-grid">
    <div class="card">
      <h3>SYSTEM_STATUS</h3>
      <div id="status" class="data-value red">LINK_OFFLINE</div>
    </div>
    <div class="card">
      <h3>ACTIVE_NODES</h3>
      <div id="nodes" class="data-value">00</div>
    </div>
    <div class="card" id="priority-card">
      <h3>PRIORITY_SIGNAL</h3>
      <div id="data" class="data-value orange">SCANNING...</div>
    </div>
  </div>

  <div class="two-col-grid">
    <div class="card">
      <h3 style="text-align: center;">TACTICAL_RADAR</h3>
      <div class="radar" id="radar-screen"></div>
    </div>
    <div class="card">
      <h3>DEPLOYED_UNITS</h3>
      <div id="drone-container" class="drone-grid"></div>
    </div>
  </div>

  <div class="card" style="margin-top: 20px;">
    <h3>ENCRYPTED_COMMS_LOG</h3>
    <div id="log" class="logbox">NO_DATA_STREAM</div>
  </div>
</div>

<script>
let startTime = Date.now();
setInterval(() => {
  let diff = Math.floor((Date.now() - startTime)/1000);
  let h = String(Math.floor(diff/3600)).padStart(2,'0');
  let m = String(Math.floor((diff%3600)/60)).padStart(2,'0');
  let s = String(diff%60).padStart(2,'0');
  document.getElementById("uptime").innerText = `SYS_UPTIME: ${h}:${m}:${s}`;
}, 1000);

let audioCtx = null;
let alarmInterval = null;

let knownIds = new Set();
let resolvedIds = new Set();
let activeAlarms = new Set();

function playSiren() {
  if(!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
  let osc = audioCtx.createOscillator();
  osc.type = 'square';
  osc.frequency.setValueAtTime(800, audioCtx.currentTime);
  osc.frequency.exponentialRampToValueAtTime(400, audioCtx.currentTime + 0.5);
  osc.connect(audioCtx.destination);
  osc.start();
  osc.stop(audioCtx.currentTime + 0.5);
}

function checkAlarms() {
  if(activeAlarms.size > 0) {
    document.body.classList.add("alert-flash");
    document.getElementById("priority-card").style.border = "2px solid #ff0055";
    if(!alarmInterval) {
      playSiren();
      alarmInterval = setInterval(playSiren, 1000);
    }
  } else {
    document.body.classList.remove("alert-flash");
    document.getElementById("priority-card").style.border = "1px solid rgba(0, 242, 255, 0.2)";
    clearInterval(alarmInterval);
    alarmInterval = null;
  }
}

function ack(msgId) {
  resolvedIds.add(msgId);
  activeAlarms.delete(msgId);
  
  let row = document.getElementById("log-" + msgId);
  if(row) {
    row.classList.add("resolved");
    let btn = row.querySelector("button");
    if(btn) { btn.innerText = "RESOLVED"; btn.disabled = true; }
  }
  checkAlarms();
}

setInterval(() => {
  fetch('/data')
  .then(res => res.json())
  .then(data => {
    document.getElementById("nodes").innerText = data.count.toString().padStart(2, '0');
    document.getElementById("data").innerText = data.msg;
    
    let s = document.getElementById("status");
    s.innerText = (data.status == "Online") ? "LINK_ESTABLISHED" : "LINK_OFFLINE";
    s.className = "data-value " + ((data.status == "Online") ? "green" : "red");
    
    let logHTML = "";
    if(data.logs.length === 0) {
      logHTML = "NO_DATA_STREAM";
    } else {
      data.logs.forEach(lg => {
        let isResolved = resolvedIds.has(lg.id);
        
        if(!knownIds.has(lg.id)) {
          knownIds.add(lg.id);
          if(!isResolved) activeAlarms.add(lg.id);
        }

        let resClass = isResolved ? " resolved" : "";
        let btnHtml = isResolved ? 
          `<button class='ack-btn' disabled>RESOLVED</button>` : 
          `<button onclick='ack("${lg.id}")' class='ack-btn'>RESOLVE</button>`;
          
        let textDisplay = lg.text.includes("[SEC_ALERT]") ? `<span style="color:var(--danger)">> ${lg.text}</span>` : `<span>> ${lg.text}</span>`;
          
        logHTML += `<div class='log-entry${resClass}' id='log-${lg.id}'>${textDisplay} ${btnHtml}</div>`;
      });
    }
    document.getElementById("log").innerHTML = logHTML;
    checkAlarms();

    let droneHTML = "";
    let radarHTML = "";
    if (data.drones.length === 0) {
      droneHTML = "<div style='color:#4a5a63; padding:10px;'>NO ACTIVE SIGNATURES</div>";
    } else {
      data.drones.forEach((d, index) => {
        let msgDisplay = (d.msg === "") ? "IDLE_STANDBY" : d.msg;
        
        let batt = 100 - (index * 12); 
        let rssi = Math.floor(Math.random() * 20) + 70;
        
        droneHTML += `
          <div class="card drone-card">
            <h4 style="margin:0; color:var(--accent);">UNIT_${d.id}</h4>
            <div style="font-size:10px; color:#6a808d;">BATT: ${batt}%</div>
            <div class="bar-container"><div class="bar-fill" style="width:${batt}%;"></div></div>
            <div style="font-size:10px; color:#6a808d;">RSSI: -${rssi} dBm</div>
            <div class="bar-container"><div class="bar-fill" style="width:${rssi}%; background:#00ff9f;"></div></div>
            <div class="drone-msg">${msgDisplay}</div>
          </div>`;

        let angleDegrees = index * 120; 
        let angleRads = (angleDegrees - 90) * (Math.PI/180); 
        let radius = 50 + (index * 25);
        
        let px = 125 + (radius * Math.cos(angleRads)) - 4; 
        let py = 125 + (radius * Math.sin(angleRads)) - 4;
        
        let delay = (angleDegrees / 360) * 3;
        
        radarHTML += `<div class="radar-node" style="left:${px}px; top:${py}px; animation-delay:${delay}s;"></div>`;
      });
    }
    document.getElementById("drone-container").innerHTML = droneHTML;
    document.getElementById("radar-screen").innerHTML = radarHTML;
  });
}, 1000);
</script>
</body>
</html>
)rawliteral";

void updateNode(String id, String data, String path) {
  for (int i = 0; i < nodeCount; i++) {
    if (nodes[i].id == id) {
      nodes[i].lastSeen = millis();
      if (data != "") nodes[i].lastMsg = data;
      if (path != "") nodes[i].path = path;
      return;
    }
  }
  if (nodeCount < MAX_NODES) {
    nodes[nodeCount].id = id;
    nodes[nodeCount].lastSeen = millis();
    nodes[nodeCount].lastMsg = (data != "") ? data : "";
    nodes[nodeCount].path = (path != "") ? path : "DIRECT";
    nodeCount++;
  }
}

void addLog(String id, String msg) {
  logs[logIndex].id = id;
  logs[logIndex].text = msg;
  logIndex = (logIndex + 1) % 10;
}

String getLogsJSON() {
  String out = "[";
  bool first = true;
  for (int i = 0; i < 10; i++) {
    int idx = (logIndex + i) % 10;
    if (logs[idx].text != "") {
      if (!first) out += ",";
      out += "{\"id\":\"" + logs[idx].id + "\",\"text\":\"" + logs[idx].text + "\"}";
      first = false;
    }
  }
  out += "]";
  return out;
}

String getDronesJSON() {
  String out = "[";
  bool first = true;
  for (int i = 0; i < nodeCount; i++) {
    if (millis() - nodes[i].lastSeen < 5000) {
      if (!first) out += ",";
      out += "{\"id\":\"" + nodes[i].id + "\",\"msg\":\"" + nodes[i].lastMsg + "\",\"path\":\"" + nodes[i].path + "\"}";
      first = false;
    }
  }
  out += "]";
  return out;
}

void handleData() {
  int active = 0;
  for (int i = 0; i < nodeCount; i++) {
    if (millis() - nodes[i].lastSeen < 5000) active++;
  }
  activeNodes = active;
  status = (activeNodes > 0) ? "Online" : "Offline";

  String json = "{";
  json += "\"msg\":\"" + latestData + "\",";
  json += "\"status\":\"" + status + "\",";
  json += "\"count\":\"" + String(activeNodes) + "\",";
  json += "\"logs\":" + getLogsJSON() + ",";
  json += "\"drones\":" + getDronesJSON();
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  radio.begin();
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(115);
  radio.startListening();

  server.on("/", [](){ server.send(200, "text/html", webpage); });
  server.on("/data", handleData);
  server.begin();
  Serial.println("Command Centre Ready (Node A)");
}

void loop() {
  server.handleClient();
  if (radio.available()) {
    char text[32] = "";
    radio.read(&text, sizeof(text));
    String received = String(text);
    
    int p1 = received.indexOf('|');
    int p2 = received.indexOf('|', p1 + 1);
    int p3 = received.indexOf('|', p2 + 1);
    int p4 = received.indexOf('|', p3 + 1);
    if (p1 == -1 || p2 == -1 || p3 == -1 || p4 == -1) return;

    String type = received.substring(0, p1);
    String origin = received.substring(p1 + 1, p2);
    String msgId = received.substring(p2 + 1, p3);
    String ttl = received.substring(p3 + 1, p4);
    String data = received.substring(p4 + 1);

    if (type == "H") {
      if (isDuplicate(msgId)) return;
      markSeen(msgId);
      updateNode(origin, data, ttl);
      latestData = origin + ": " + data;
      addLog(msgId, origin + " -> " + data); 
    } 
    else if (type == "P") {
      updateNode(origin, "", ttl);
    }
    else if (type == "S") {
      if (isDuplicate(msgId)) return;
      markSeen(msgId);
      addLog(msgId, "[SEC_ALERT] " + origin + " -> " + data);
      Serial.println("SECURITY WARNING: " + data);
    }
  }
}
