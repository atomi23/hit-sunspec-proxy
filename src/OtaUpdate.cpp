#include "OtaUpdate.h"
#include "LiveData.h"
#include <Update.h>
#include "AppConfig.h"
#include "EventLog.h"
#include "HttpsServer.h"

namespace {
bool busy = false;
bool rebootPending = false;
bool uploadOk = false;
bool factoryPending = false;
uint32_t rebootAt = 0;
char errBuf[96] = "";
size_t written = 0;
// Request, der gerade flasht. Andere Uploads (auch nicht angemeldete Folge-Stücke) dürfen nicht hineinschreiben.
AsyncWebServerRequest *owner = nullptr;

const char OTA_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="de"><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<meta name="color-scheme" content="dark"/><title data-en="Firmware and reset · HIT Proxy">Firmware und Reset · HIT Proxy</title>
<link rel="stylesheet" href="/a.css"/>
<style>input[type=file]{width:100%;margin:14px 0;color:var(--ink)}.card{max-width:720px}.card+.card{margin-top:16px}.danger{background:var(--fault);color:#2a0d0a}</style>
</head><body><div class="app">
<aside class="side">
  <div class="logo"><svg viewBox="0 0 32 32" aria-hidden="true"><rect width="32" height="32" rx="9" fill="currentColor"/><path d="M18 6 10 18h6l-2 8 8-12h-6z" style="fill:var(--bg)"/></svg><div><b>HIT-Proxy</b><span data-en="SunSpec for Consolinno">SunSpec für Consolinno</span></div></div>
  <nav>
    <a href="/"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 12h4l2-6 4 12 2-6h6"/></svg> Live</a>
    <a href="/history"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 19V5M4 19h16M8 15l3-4 3 2 4-6"/></svg> <span data-en="History">Verlauf</span></a>
    <div class="nav-grp" data-en="Data sources">Datenquellen</div>
    <a href="/config#hit"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="4" y="4" width="16" height="16" rx="1"/><path d="M4 20 20 4M7 8.5h4M13 16c.9-1.1 1.8-1.1 2.7 0s1.8 1.1 2.7 0"/></svg> <span data-en="HIT inverter">HIT-Wechselrichter</span></a>
    <a href="/config#tasmota"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M9 3v5M15 3v5M6 8h12v3a6 6 0 0 1-12 0zM12 17v4"/></svg> <span data-en="Tasmota meter">Tasmota-Zähler</span></a>
    <a href="/config#cloud"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M7 18h10a4 4 0 0 0 .4-8A6 6 0 0 0 6 9.6 4.2 4.2 0 0 0 7 18z"/></svg> <span data-en="Hoymiles Cloud">Hoymiles-Cloud</span></a>
    <a href="/config#akku"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="7" width="16" height="10" rx="2.5"/><path d="M21 10.5v3M7 12h8"/></svg> <span data-en="Battery">Akku</span></a>
    <div class="nav-grp" data-en="Outputs">Ausgänge</div>
    <a href="/config#sunspec"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 5h14M5 10h14M5 15h14M5 20h8"/></svg> Consolinno</a>
    <a href="/config#mqtt"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 11 12 4l8 7M6 9.5V20h12V9.5M10 20v-5h4v5"/></svg> Home Assistant</a>
    <a href="/config#gateway"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 7h16M4 17h16M8 7v10M16 7v10"/><circle cx="12" cy="12" r="2"/></svg> <span data-en="Modbus gateway">Modbus-Gateway</span></a>
    <a href="/config#knx"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 9h18M3 15h18"/><circle cx="8" cy="9" r="1.6"/><circle cx="16" cy="15" r="1.6"/></svg> KNX</a>
    <div class="nav-grp" data-en="Device">Gerät</div>
    <a href="/config#netz"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 9h14v10H5zM8.5 19v-3M12 19v-3M15.5 19v-3M9 9V6h6v3"/></svg> <span data-en="Network and access">Netzwerk und Zugang</span></a>
    <a href="/config#display"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="5" width="18" height="12" rx="2.5"/><path d="M9 20h6"/></svg> Display</a>
    <a href="/config#ctrl"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="7" width="16" height="10" rx="2.5"/><path d="M21 10.5v3M7 10.5v3M11 10.5v3"/></svg> <span data-en="Battery control">Akku-Steuerung</span></a>
    <a href="/debug"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M9 6h11M9 12h11M9 18h11M4.5 6h.01M4.5 12h.01M4.5 18h.01"/></svg> <span data-en="Log">Protokoll</span></a>
    <a class="on" aria-current="page" href="/ota"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 15V4M7.5 8.5 12 4l4.5 4.5M5 14v5h14v-5"/></svg> <span data-en="Firmware and reset">Firmware und Reset</span></a>
  </nav>
  <div class="foot">
    <div class="online"><span class="dot ok"></span> Online</div>
    <div class="meta" id="sideVer">—</div>
    <div class="prefs">
      <select id="uiLang" onchange="saveUiPrefs()" aria-label="Sprache" data-en-aria-label="Language">
        <option value="de">Deutsch</option>
        <option value="en">English</option>
      </select>
      <select id="uiTheme" onchange="saveUiPrefs()" aria-label="Design" data-en-aria-label="Theme">
        <option value="mint">Mint</option>
        <option value="ocean">Ocean</option>
        <option value="ember">Ember</option>
        <option value="light" data-en="Light">Hell</option>
        <option value="steel" data-en="Steel">Stahl</option>
        <option value="blueprint" data-en="Blueprint">Blaupause</option>
      </select>
    </div>
  </div>
</aside>
<main class="main">
<div class="page-head">
  <div>
    <h1 data-en="Firmware and reset">Firmware und Reset</h1>
    <p class="sub"><span data-en="Installed version:">Installiert ist Version</span> <b id="ver">…</b>.</p>
  </div>
</div>
<div class="card">
<h2 data-en="Firmware update">Firmware-Update</h2>
<label data-en="Firmware file (firmware.bin)">Firmware-Datei (firmware.bin)</label>
<input type="file" id="f" accept=".bin,application/octet-stream"/>
<button type="button" onclick="go()" data-en="Upload">Hochladen</button>
<div id="bar"><div id="fill"></div></div>
<div id="msg" class="muted" style="margin-top:10px"></div>
<p class="hint" data-en="Your settings are retained. The device restarts after the upload; do not disconnect power. If the new image fails, the device starts with the previous firmware.">Einstellungen bleiben erhalten. Nach dem Hochladen startet das Gerät neu, dabei den Strom nicht trennen.
Schlägt das neue Image fehl, startet das Gerät mit der vorherigen Firmware.</p>
</div>
<div class="card">
<h2 data-en="Restart">Neustart</h2>
<p class="hint" style="margin-top:0"><span data-en="Uptime:">Läuft seit</span> <b id="up">…</b><span data-en=". Last restart:">. Letzter Neustart:</span> <b id="rr">…</b><span data-en=". A hardware watchdog restarts the device after a 45-second hang. A second watchdog restarts it after 15 minutes without a data source.">.
Ein Hardware-Watchdog startet das Gerät nach 45 s Hänger selbst neu, ein zweiter nach 15 Minuten ohne Datenquelle.</span></p>
<div class="row"><button type="button" class="sec" onclick="restart()" data-en="Restart now">Jetzt neu starten</button>
<span class="meta" id="smsg"></span></div>
</div>
<div class="card">
<h2 data-en="Factory settings">Werkseinstellungen</h2>
<p class="hint" style="margin-top:0" data-en="Deletes all settings: HIT address, Wi-Fi, passwords, MQTT, KNX and control. The device then starts as new, with its own Wi-Fi network and a new access code shown on the display.">Löscht alle Einstellungen: HIT-Adresse, WLAN, Passwörter, MQTT, KNX und Steuerung.
Danach startet das Gerät wie neu, mit eigenem WLAN und einem neuen Zugangscode auf dem Display.</p>
<label data-en="Type RESET to confirm">Zum Bestätigen RESET eintippen</label>
<input id="rc" autocomplete="off" placeholder="RESET"/>
<div class="row"><button type="button" class="danger" onclick="factory()" data-en="Reset to factory settings">Auf Werkseinstellungen zurücksetzen</button>
<span class="meta" id="rmsg"></span></div>
</div>
</main></div>

<script>
let LANG='de';
function applyThemeLang(s){
  LANG=(s.uiLang==='en')?'en':'de';
  document.documentElement.lang=LANG;
  document.documentElement.dataset.theme=s.uiTheme||'mint';
  const sl=document.getElementById('uiLang'); if(sl)sl.value=LANG;
  const st=document.getElementById('uiTheme'); if(st)st.value=s.uiTheme||'mint';
  const sv=document.getElementById('sideVer'); if(sv)sv.textContent='v'+(s.version||'')+'\n'+(s.ipShown||'');
}
async function saveUiPrefs(){
  const body={uiLang:document.getElementById('uiLang').value,uiTheme:document.getElementById('uiTheme').value};
  await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  location.reload();
}
</script>
<script src="/b.js"></script>
<script>
function dur(s){s=Number(s)||0;const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);
  return (d?d+' d ':'')+(d||h?h+' h ':'')+m+' min';}
async function boot(){const s=await(await fetch('/api/status')).json();applyThemeLang(s);document.getElementById('ver').textContent=s.version||'?';
  document.getElementById('up').textContent=dur(s.uptimeS);document.getElementById('rr').textContent=s.resetReason||'?';}
async function restart(){
  const m=document.getElementById('smsg');
  const r=await fetch('/api/restart',{method:'POST'}).catch(()=>null);
  m.textContent=r&&r.ok?uiText('Startet neu, Seite lädt in 10 s neu.','Restarting. This page reloads in 10 s.'):uiText('Fehler','Error');
  if(r&&r.ok)setTimeout(()=>location.reload(),10000);
}
async function go(){
  const file=document.getElementById('f').files[0];
  if(!file){alert(uiText('Bitte firmware.bin wählen','Select firmware.bin'));return;}
  if(!file.name.endsWith('.bin')){if(!confirm(uiText('Keine .bin — trotzdem?','This is not a .bin file. Continue?')))return;}
  const msg=document.getElementById('msg'); const fill=document.getElementById('fill');
  msg.textContent=uiText('Upload läuft… Gerät nicht trennen.','Uploading… Do not disconnect the device.'); fill.style.width='0%';
  try{
    await new Promise((resolve,reject)=>{
      const xhr=new XMLHttpRequest();
      xhr.open('POST','/api/ota');
      xhr.upload.onprogress=e=>{if(e.lengthComputable){fill.style.width=Math.round(e.loaded/e.total*100)+'%';}};
      xhr.onload=()=>{
        let body=xhr.responseText||'';
        if(xhr.status>=200&&xhr.status<300) resolve(body);
        else reject(new Error(body||('HTTP '+xhr.status)));
      };
      xhr.onerror=()=>reject(new Error(uiText('Netzwerkfehler','Network error')));
      const fd=new FormData(); fd.append('firmware',file,file.name); xhr.send(fd);
    });
    fill.style.width='100%';
    msg.textContent=uiText('Flash OK — Neustart. In ~5 s Seite neu laden (gleiche IP/Login).','Firmware installed. Restarting and reloading in about 5 s (same IP and sign-in).');
    setTimeout(()=>location.reload(),5000);
  }catch(e){msg.textContent=uiText('Fehler: ','Error: ')+e.message; fill.style.width='0%';}
}
async function factory(){
  if(document.getElementById('rc').value.trim()!=='RESET'){document.getElementById('rmsg').textContent=uiText('Bitte RESET eintippen.','Type RESET to confirm.');return;}
  const r=await fetch('/api/factory-reset',{method:'POST',body:new URLSearchParams({confirm:'RESET'})});
  document.getElementById('rmsg').textContent=r.ok?uiText('Wird zurückgesetzt. Danach mit dem WLAN vom Display verbinden.','Resetting. Then connect to the Wi-Fi network shown on the display.'):uiText('Fehler ','Error ')+r.status;
}
boot();
</script></body></html>
)HTML";
} // namespace

namespace OtaUpdate {

bool inProgress() { return busy; }
const char *lastError() { return errBuf; }

void registerRoutes(AsyncWebServer &server, bool (*requireAuth)(AsyncWebServerRequest *)) {
  server.on("/ota", HTTP_GET, [requireAuth](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    req->send(200, "text/html", (const uint8_t *)OTA_HTML, strlen(OTA_HTML));
  });

  server.on("/api/restart", HTTP_POST, [requireAuth](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    strlcpy(gLive.status, "Neustart", sizeof(gLive.status));
    rebootAt = millis() + 1000;
    rebootPending = true; // loop() startet nach 1 s neu, die Antwort geht vorher raus
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/factory-reset", HTTP_POST, [requireAuth](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    const AsyncWebParameter *p = req->getParam("confirm", true);
    if (!p || p->value() != "RESET") {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"confirm\"}");
      return;
    }
    factoryPending = true;
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on(
      "/api/ota", HTTP_POST,
      [requireAuth](AsyncWebServerRequest *req) {
        if (!requireAuth(req)) return;
        if (req != owner) {
          req->send(409, "application/json", "{\"ok\":false,\"error\":\"anderes Update läuft oder nicht angemeldet\"}");
          return;
        }
        owner = nullptr;
        busy = false;
        if (!uploadOk || Update.hasError() || errBuf[0]) {
          String e = errBuf[0] ? String(errBuf) : String(Update.errorString());
          if (!e.length()) e = "Update fehlgeschlagen";
          // JSON escape grob
          e.replace("\"", "'");
          req->send(500, "application/json", String("{\"ok\":false,\"error\":\"") + e + "\"}");
          return;
        }
        strlcpy(gLive.status, "OTA OK reboot", sizeof(gLive.status));
        req->send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
        rebootAt = millis() + 1000;
        rebootPending = true;
      },
      [requireAuth](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (index == 0) {
          if (!requireAuth(req)) return;
          if (busy && owner && owner != req) return; // anderer Upload läuft schon
          owner = req;
          // Verbindung weg, bevor das Bild fertig ist: Flash-Vorgang verwerfen und freigeben
          req->onDisconnect([req]() {
            if (owner != req) return;
            owner = nullptr;
            if (Update.isRunning()) Update.abort();
            busy = false;
          });
          errBuf[0] = '\0';
          uploadOk = false;
          written = 0;
          busy = true;
          strlcpy(gLive.status, "OTA flash…", sizeof(gLive.status));

          // Abgebrochener Upload (VPN/Timeout) lässt Update sonst mit „No Error“ hängen
          if (Update.isRunning()) Update.abort();

          // Multipart: Content-Length ≠ Bin-Größe → UNKNOWN
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            const char *why = Update.errorString();
            if (!why || !why[0] || strcmp(why, "No Error") == 0) why = "OTA-Slot belegt — Gerät neu starten und erneut versuchen";
            snprintf(errBuf, sizeof(errBuf), "begin: %s", why);
            busy = false;
            return;
          }
          Serial.printf("OTA begin %s freeSketch=%u\n", filename.c_str(), (unsigned)ESP.getFreeSketchSpace());
        }

        if (req != owner || errBuf[0] || !busy) return;

        if (len) {
          if (Update.write(data, len) != len) {
            snprintf(errBuf, sizeof(errBuf), "write@%u: %s", (unsigned)written, Update.errorString());
            busy = false;
            return;
          }
          written += len;
        }

        if (final) {
          if (errBuf[0]) {
            Update.abort();
            busy = false;
            return;
          }
          if (!Update.end(true)) {
            snprintf(errBuf, sizeof(errBuf), "end: %s", Update.errorString());
            busy = false;
            uploadOk = false;
            return;
          }
          uploadOk = true;
          Serial.printf("OTA OK %u bytes\n", (unsigned)written);
          EventLog::add('I', "OTA", "Firmware übertragen (%u Bytes), Neustart folgt", (unsigned)written);
        }
      });
}

void loop() {
  if (factoryPending) {
    delay(500);
    Serial.println("Werksreset");
    ConfigStore::factoryReset();
    HttpsServer::forget();
    ESP.restart();
  }
  if (!rebootPending) return;
  if (int32_t(millis() - rebootAt) < 0) return;
  Serial.println("OTA reboot now");
  ESP.restart();
}

} // namespace OtaUpdate
