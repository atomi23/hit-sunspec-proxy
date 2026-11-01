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
<style>.wrap{max-width:640px}input[type=file]{width:100%;margin:14px 0;color:var(--txt)}</style>
</head><body><div class="wrap">
<div class="top">
  <div>
    <p class="brand">HIT SunSpec Proxy</p>
    <h1>Firmware OTA</h1>
    <p class="sub">Aktuell <b id="ver">…</b> · NVS-Config bleibt · <code>firmware.bin</code></p>
  </div>
  <nav><a href="/">Live</a><a href="/config">Config</a><a href="/debug">Debug</a><a class="on" href="/ota">OTA</a></nav>
</div>
<div class="card">
<input type="file" id="f" accept=".bin,application/octet-stream"/>
<button type="button" onclick="go()">Hochladen &amp; flashen</button>
<div id="bar"><div id="fill"></div></div>
<div id="msg" class="muted" style="margin-top:10px"></div>
<p class="hint">Partition: OTA dual-slot (16 MB)</p>
</div>
<script>
async function boot(){const s=await(await fetch('/api/status')).json();document.getElementById('ver').textContent=s.version||'?';}
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
