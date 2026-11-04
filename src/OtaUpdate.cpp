#include "OtaUpdate.h"
#include "LiveData.h"
#include <Update.h>

namespace {
bool busy = false;
bool rebootPending = false;
bool uploadOk = false;
char errBuf[96] = "";
size_t written = 0;

const char OTA_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="de"><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<meta name="color-scheme" content="dark"/><title>OTA Update · HIT Proxy</title>
<link rel="stylesheet" href="/a.css"/>
<style>input[type=file]{width:100%;margin:14px 0;color:var(--txt)}.card{max-width:720px}</style>
</head><body><div class="app">
<aside class="side">
  <div class="logo"><div class="mark">HS</div><div><b>HIT SunSpec</b><span>Proxy</span></div></div>
  <nav>
    <div class="nav-grp">System</div>
    <a href="/"><i>◉</i> Live</a>
    <div class="nav-grp">Anbindung</div>
    <a href="/config#hit"><i>⚡</i> HIT</a>
    <a href="/config#netz"><i>⊕</i> Netzwerk</a>
    <a href="/config#sunspec"><i>≈</i> SunSpec</a>
    <a href="/config#knx"><i>▦</i> KNX</a>
    <div class="nav-grp">Dienste</div>
    <a href="/config#mqtt"><i>☁</i> MQTT / Cloud</a>
    <a href="/config#tasmota"><i>⌀</i> Tasmota</a>
    <a href="/debug"><i>≡</i> Protokoll</a>
    <div class="nav-grp">Gerät</div>
    <a href="/config"><i>⚙</i> Einstellungen</a>
    <a class="on" href="/ota"><i>↑</i> OTA</a>
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
    <p class="brand">Update</p>
    <h1>Firmware OTA</h1>
    <p class="sub">Aktuell <b id="ver">…</b> · NVS-Config bleibt</p>
  </div>
  <span class="pill">Sicher</span>
</div>
<div class="card">
<label>Firmware-Datei</label>
<input type="file" id="f" accept=".bin,application/octet-stream"/>
<button type="button" onclick="go()">Hochladen</button>
<div id="bar"><div id="fill"></div></div>
<div id="msg" class="muted" style="margin-top:10px"></div>
<p class="hint">Nach Erfolg startet das Gerät neu. Strom nicht trennen. Partition: OTA dual-slot.</p>
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
  const sv=document.getElementById('sideVer'); if(sv)sv.textContent='v'+(s.version||'')+' · '+(s.ipShown||'');
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
  if (!rebootPending) return;
  static uint32_t t0 = 0;
  if (!t0) t0 = millis();
  if (millis() - t0 < 1000) return;
  Serial.println("OTA reboot now");
  ESP.restart();
}

} // namespace OtaUpdate
