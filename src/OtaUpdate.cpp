#include "OtaUpdate.h"
#include "LiveData.h"
#include <Update.h>
#include "AppConfig.h"

namespace {
bool busy = false;
bool rebootPending = false;
bool uploadOk = false;
bool factoryPending = false;
char errBuf[96] = "";
size_t written = 0;

const char OTA_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="de"><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<meta name="color-scheme" content="dark"/><title>Firmware und Reset · HIT Proxy</title>
<link rel="stylesheet" href="/a.css"/>
<style>input[type=file]{width:100%;margin:14px 0;color:var(--txt)}.card{max-width:720px}.card+.card{margin-top:16px}.danger{background:var(--fault);color:#2a0d0a}</style>
</head><body><div class="app">
<aside class="side">
  <div class="logo"><svg viewBox="0 0 32 32" aria-hidden="true"><rect width="32" height="32" rx="9" fill="currentColor"/><path d="M18 6 10 18h6l-2 8 8-12h-6z" style="fill:var(--bg)"/></svg><div><b>HIT-Proxy</b><span>SunSpec für Consolinno</span></div></div>
  <nav>
    <a href="/"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 12h4l2-6 4 12 2-6h6"/></svg> Live</a>
    <div class="nav-grp">Datenquellen</div>
    <a href="/config#hit"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="4" y="4" width="16" height="16" rx="1"/><path d="M4 20 20 4M7 8.5h4M13 16c.9-1.1 1.8-1.1 2.7 0s1.8 1.1 2.7 0"/></svg> HIT-Wechselrichter</a>
    <a href="/config#tasmota"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M9 3v5M15 3v5M6 8h12v3a6 6 0 0 1-12 0zM12 17v4"/></svg> Tasmota-Zähler</a>
    <a href="/config#cloud"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M7 18h10a4 4 0 0 0 .4-8A6 6 0 0 0 6 9.6 4.2 4.2 0 0 0 7 18z"/></svg> Hoymiles-Cloud</a>
    <div class="nav-grp">Ausgänge</div>
    <a href="/config#sunspec"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 5h14M5 10h14M5 15h14M5 20h8"/></svg> Consolinno</a>
    <a href="/config#mqtt"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 11 12 4l8 7M6 9.5V20h12V9.5M10 20v-5h4v5"/></svg> Home Assistant</a>
    <a href="/config#gateway"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 7h16M4 17h16M8 7v10M16 7v10"/><circle cx="12" cy="12" r="2"/></svg> Modbus-Gateway</a>
    <a href="/config#knx"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 9h18M3 15h18"/><circle cx="8" cy="9" r="1.6"/><circle cx="16" cy="15" r="1.6"/></svg> KNX</a>
    <div class="nav-grp">Gerät</div>
    <a href="/config#netz"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 9h14v10H5zM8.5 19v-3M12 19v-3M15.5 19v-3M9 9V6h6v3"/></svg> Netzwerk und Zugang</a>
    <a href="/config#display"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="5" width="18" height="12" rx="2.5"/><path d="M9 20h6"/></svg> Display</a>
    <a href="/config#ctrl"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="7" width="16" height="10" rx="2.5"/><path d="M21 10.5v3M7 10.5v3M11 10.5v3"/></svg> Akku-Steuerung</a>
    <a href="/debug"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M9 6h11M9 12h11M9 18h11M4.5 6h.01M4.5 12h.01M4.5 18h.01"/></svg> Protokoll</a>
    <a class="on" aria-current="page" href="/ota"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 15V4M7.5 8.5 12 4l4.5 4.5M5 14v5h14v-5"/></svg> Firmware und Reset</a>
  </nav>
  <div class="foot">
    <div class="online"><span class="dot ok"></span> Online</div>
    <div class="meta" id="sideVer">—</div>
    <div class="prefs">
      <select id="uiLang" onchange="saveUiPrefs()">
        <option value="de">Deutsch</option>
        <option value="en">English</option>
      </select>
      <select id="uiTheme" onchange="saveUiPrefs()">
        <option value="mint">Mint</option>
        <option value="ocean">Ocean</option>
        <option value="ember">Ember</option>
      </select>
    </div>
  </div>
</aside>
<main class="main">
<div class="page-head">
  <div>
    <h1>Firmware und Reset</h1>
    <p class="sub">Installiert ist Version <b id="ver">…</b>.</p>
  </div>
</div>
<div class="card">
<h2>Firmware-Update</h2>
<label>Firmware-Datei (firmware.bin)</label>
<input type="file" id="f" accept=".bin,application/octet-stream"/>
<button type="button" onclick="go()">Hochladen</button>
<div id="bar"><div id="fill"></div></div>
<div id="msg" class="muted" style="margin-top:10px"></div>
<p class="hint">Einstellungen bleiben erhalten. Nach dem Hochladen startet das Gerät neu, dabei den Strom nicht trennen.
Schlägt das neue Image fehl, startet das Gerät mit der vorherigen Firmware.</p>
</div>
<div class="card">
<h2>Werkseinstellungen</h2>
<p class="hint" style="margin-top:0">Löscht alle Einstellungen: HIT-Adresse, WLAN, Passwörter, MQTT, KNX und Steuerung.
Danach startet das Gerät wie neu, mit eigenem WLAN und einem neuen Zugangscode auf dem Display.</p>
<label>Zum Bestätigen RESET eintippen</label>
<input id="rc" autocomplete="off" placeholder="RESET"/>
<div class="row"><button type="button" class="danger" onclick="factory()">Auf Werkseinstellungen zurücksetzen</button>
<span class="meta" id="rmsg"></span></div>
</div>
</main></div>

<script>
const I18N={
 de:{live:'Live',settings:'Einstellungen',proto:'Protokoll',energy:'Energiefluss',batSoc:'Batterie SoC',poll:'Jetzt pollen',
     pv:'PV',bat:'Batterie',grid:'Netz',soc:'SoC',enHit:'Energie HIT',yield:'PV Ertrag',imp:'Bezug',exp:'Einspeisung',
     lpp:'Consolinno LPP',limit:'Limit',on:'aktiv',off:'aus',hems:'HEMS',conn:'verbunden',wait:'wartet',tas:'Tasmota',
     online:'online',offline:'offline',ph:'Phasen Netz',freq:'Frequenz',sys:'System'},
 en:{live:'Live',settings:'Settings',proto:'Log',energy:'Energy flow',batSoc:'Battery SoC',poll:'Poll now',
     pv:'PV',bat:'Battery',grid:'Grid',soc:'SoC',enHit:'HIT Energy',yield:'PV yield',imp:'Import',exp:'Export',
     lpp:'Consolinno LPP',limit:'Limit',on:'on',off:'off',hems:'HEMS',conn:'connected',wait:'waiting',tas:'Tasmota',
     online:'online',offline:'offline',ph:'Grid phases',freq:'Frequency',sys:'System'}
};
let LANG='de';
function t(k){return (I18N[LANG]&&I18N[LANG][k])||k;}
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
<script>
async function boot(){const s=await(await fetch('/api/status')).json();applyThemeLang(s);document.getElementById('ver').textContent=s.version||'?';}
async function go(){
  const file=document.getElementById('f').files[0];
  if(!file){alert('Bitte firmware.bin wählen');return;}
  if(!file.name.endsWith('.bin')){if(!confirm('Keine .bin — trotzdem?'))return;}
  const msg=document.getElementById('msg'); const fill=document.getElementById('fill');
  msg.textContent='Upload läuft… Gerät nicht trennen.'; fill.style.width='0%';
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
      xhr.onerror=()=>reject(new Error('Netzwerkfehler'));
      const fd=new FormData(); fd.append('firmware',file,file.name); xhr.send(fd);
    });
    fill.style.width='100%';
    msg.textContent='Flash OK — Neustart. In ~5 s Seite neu laden (gleiche IP/Login).';
    setTimeout(()=>location.reload(),5000);
  }catch(e){msg.textContent='Fehler: '+e.message; fill.style.width='0%';}
}
async function factory(){
  if(document.getElementById('rc').value.trim()!=='RESET'){document.getElementById('rmsg').textContent='Bitte RESET eintippen.';return;}
  const r=await fetch('/api/factory-reset',{method:'POST',body:new URLSearchParams({confirm:'RESET'})});
  document.getElementById('rmsg').textContent=r.ok?'Wird zurückgesetzt. Danach mit dem WLAN vom Display verbinden.':'Fehler '+r.status;
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
    req->send_P(200, "text/html", OTA_HTML);
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
        rebootPending = true;
      },
      [requireAuth](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (index == 0) {
          if (!requireAuth(req)) return;
          errBuf[0] = '\0';
          uploadOk = false;
          written = 0;
          busy = true;
          strlcpy(gLive.status, "OTA flash…", sizeof(gLive.status));

          // Multipart: Content-Length ≠ Bin-Größe → UNKNOWN
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            snprintf(errBuf, sizeof(errBuf), "begin: %s", Update.errorString());
            busy = false;
            return;
          }
          Serial.printf("OTA begin %s\n", filename.c_str());
        }

        if (errBuf[0] || !busy) return;

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
        }
      });
}

void loop() {
  if (factoryPending) {
    delay(500);
    Serial.println("Werksreset");
    ConfigStore::factoryReset();
    ESP.restart();
  }
  if (!rebootPending) return;
  static uint32_t t0 = 0;
  if (!t0) t0 = millis();
  if (millis() - t0 < 1000) return;
  Serial.println("OTA reboot now");
  ESP.restart();
}

} // namespace OtaUpdate
