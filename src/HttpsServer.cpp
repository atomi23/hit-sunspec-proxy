#include "HttpsServer.h"
#include "HttpChunked.h"
#include "EventLog.h"
#include "LiveData.h"
#include "LocalCertificates.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <atomic>
#include <esp_heap_caps.h>
#include <esp_https_server.h>
#include <esp_log.h>
#include <freertos/semphr.h>
#include <mbedtls/asn1write.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <mbedtls/oid.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <mbedtls/x509_crt.h>
#include <time.h>

namespace {
constexpr const char *OWN_CRT = "/tls/own.crt";
constexpr const char *OWN_KEY = "/tls/own.key";
// Apple-Geräte lehnen Serverzertifikate mit mehr als 825 Tagen Laufzeit ab, auch selbst signierte
constexpr uint32_t SELF_DAYS = 820;

AppConfig local;
httpd_handle_t server = nullptr;
String certPem, keyPem;
String caPem;
bool ownCert = false;
bool lowRam = false;
uint32_t startAfterMs = 0;
uint32_t restartAt = 0;
std::atomic<uint8_t> pending{0};
enum : uint8_t { P_NONE = 0, P_RESTART = 1, P_REGENERATE = 2 };

SemaphoreHandle_t infoLock = nullptr;
String info = "{}";

// Jede TLS-Sitzung belegt rund 38 KB internen RAM. Ruhende Sitzungen schließen wir deshalb nach kurzer Zeit;
// Browser bauen dann bei Bedarf neu auf. Sitzungen mit laufender Anfrage bleiben unangetastet.
constexpr int MAX_SESSIONS = 2;
// mbedTLS reserviert je Sitzung zwei Puffer von je rund 17 KB, jeder am Stück im internen RAM
constexpr size_t TLS_BUF = 17408;
// Daneben brauchen Server-Task, Handshakes und die Weboberfläche selbst Platz. Gemessen am WT32-ETH01:
// mit 43 KB Rest (WLAN aus) scheitern parallele Verbindungen eines Browsers fast immer
constexpr size_t TLS_SPARE = 60000;
constexpr uint32_t IDLE_CLOSE_MS = 4000;
// Zwei Seiten, die alle 2 s abfragen, würden die Plätze sonst dauerhaft belegen und neue Verbindungen aussperren
constexpr uint32_t MAX_AGE_MS = 15000;
struct Sess {
  int fd = -1;
  uint32_t opened = 0;
  uint32_t last = 0;
  bool busy = false;
};
Sess sess[8];
portMUX_TYPE sessMux = portMUX_INITIALIZER_UNLOCKED;

Sess *findSess(int fd, bool create) {
  for (Sess &s : sess)
    if (s.fd == fd) return &s;
  if (!create) return nullptr;
  for (Sess &s : sess)
    if (s.fd < 0) {
      s.fd = fd;
      s.opened = millis();
      s.last = millis();
      s.busy = false;
      return &s;
    }
  return nullptr;
}

void markBusy(int fd, bool busy) {
  portENTER_CRITICAL(&sessMux);
  if (Sess *s = findSess(fd, true)) {
    s->busy = busy;
    s->last = millis();
  }
  portEXIT_CRITICAL(&sessMux);
}

// ---- Zertifikat erzeugen -----------------------------------------------------------------------

struct Rng {
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context drbg;
  bool ok = false;
  Rng() {
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&drbg);
    static const char pers[] = "hit-proxy-tls";
    ok = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)pers, sizeof(pers)) == 0;
  }
  ~Rng() {
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&entropy);
  }
};

void fmtTime(char *out, time_t t) {
  struct tm tmv;
  gmtime_r(&t, &tmv);
  strftime(out, 15, "%Y%m%d%H%M%S", &tmv);
}

time_t nowOrBuild() {
  const time_t now = time(nullptr);
  if (now > 1700000000) return now;
  // ohne NTP: Erstellungsdatum der Firmware, damit das Zertifikat nicht "aus der Zukunft" kommt
  struct tm b = {};
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = {};
  int day = 1, year = 2026;
  sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
  b.tm_mon = (strstr(months, mon) - months) / 3;
  b.tm_mday = day;
  b.tm_year = year - 1900;
  return mktime(&b);
}

bool parseIp(const char *s, uint8_t out[4]) {
  IPAddress ip;
  if (!s || !ip.fromString(s) || uint32_t(ip) == 0) return false;
  for (int i = 0; i < 4; i++) out[i] = ip[i];
  return true;
}

String currentIps() { return String(gLive.ipEth) + "|" + gLive.ipWifi; }

int parseCa(mbedtls_x509_crt *crt) {
  return LocalCertificates::parseCa(crt, reinterpret_cast<const unsigned char *>(caPem.c_str()));
}

String deviceSubject(bool ca) {
  String dev = local.deviceName[0] ? local.deviceName : "HIT-Proxy";
  dev.replace(",", " ");
  dev.replace("=", " ");
  return String("CN=") + (ca ? "HIT-SunSpec-Proxy CA " : "") + dev + ",O=HIT-SunSpec-Proxy";
}

bool ensureCa(bool &created) {
  created = false;
  Preferences p;
  if (!p.begin("tls", false)) return false;
  caPem = p.getString("cacrt", "");
  const String storedKey = p.getString("cakey", "");
  if (caPem.length() || storedKey.length()) {
    p.end();
    return caPem.length() && storedKey.length();
  }
  Rng rng;
  mbedtls_pk_context key;
  mbedtls_pk_init(&key);
  auto *pem = static_cast<unsigned char *>(calloc(1, 2048));
  auto *kpem = static_cast<unsigned char *>(calloc(1, 512));
  char nb[16], na[16];
  const time_t from = nowOrBuild() - 86400;
  fmtTime(nb, from);
  fmtTime(na, from + time_t(3650) * 86400);
  const String subject = deviceSubject(true);
  bool ok = pem && kpem && rng.ok && LocalCertificates::generate(true, subject.c_str(), subject.c_str(),
      nb, na, &key, nullptr, nullptr, &rng.drbg, pem, 2048, kpem, 512) == 0;
  if (ok) {
    caPem = reinterpret_cast<char *>(pem);
    ok = p.putString("cakey", reinterpret_cast<char *>(kpem)) > 0 && p.putString("cacrt", caPem) > 0;
    created = ok;
  }
  p.end();
  free(pem);
  if (kpem) { memset(kpem, 0, 512); free(kpem); }
  mbedtls_pk_free(&key);
  return ok;
}

bool makeSelfSigned() {
  Rng rng;
  mbedtls_pk_context key;
  mbedtls_pk_context issuerKey;
  mbedtls_pk_init(&issuerKey);
  mbedtls_pk_init(&key);
  auto *pem = static_cast<unsigned char *>(calloc(1, 2048));
  auto *kpem = static_cast<unsigned char *>(calloc(1, 512));

  char name[96];
  String dev = local.deviceName[0] ? local.deviceName : "HIT-Proxy";
  dev.replace(",", " ");
  dev.replace("=", " ");
  snprintf(name, sizeof(name), "CN=%s,O=HIT-SunSpec-Proxy", dev.c_str());

  // SAN: Gerätename.local plus aktuelle IP-Adressen; Browser prüfen nur noch den SAN
  String host = dev;
  host.replace(" ", "-");
  host.toLowerCase();
  host += ".local";
  uint8_t ipEth[4] = {}, ipWifi[4] = {};
  mbedtls_x509_san_list san[3] = {};
  int nSan = 0;
  san[nSan].node.type = MBEDTLS_X509_SAN_DNS_NAME;
  san[nSan].node.san.unstructured_name.p = (unsigned char *)host.c_str();
  san[nSan].node.san.unstructured_name.len = host.length();
  nSan++;
  if (parseIp(gLive.ipEth, ipEth)) {
    san[nSan].node.type = MBEDTLS_X509_SAN_IP_ADDRESS;
    san[nSan].node.san.unstructured_name.p = ipEth;
    san[nSan].node.san.unstructured_name.len = 4;
    nSan++;
  }
  if (parseIp(gLive.ipWifi, ipWifi) && memcmp(ipEth, ipWifi, 4) != 0) {
    san[nSan].node.type = MBEDTLS_X509_SAN_IP_ADDRESS;
    san[nSan].node.san.unstructured_name.p = ipWifi;
    san[nSan].node.san.unstructured_name.len = 4;
    nSan++;
  }
  for (int i = 0; i + 1 < nSan; i++) san[i].next = &san[i + 1];

  mbedtls_x509_crt ca;
  mbedtls_x509_crt_init(&ca);
  bool ok = false;
  do {
    if (!pem || !kpem || !rng.ok) break;
    Preferences p;
    p.begin("tls", true);
    String caKey = p.getString("cakey", "");
    p.end();
    if (mbedtls_pk_parse_key(&issuerKey, reinterpret_cast<const unsigned char *>(caKey.c_str()), caKey.length() + 1,
        nullptr, 0, mbedtls_ctr_drbg_random, &rng.drbg)) break;
    if (parseCa(&ca)) break;
    if (mbedtls_pk_check_pair(&ca.pk, &issuerKey, mbedtls_ctr_drbg_random, &rng.drbg)) break;
    char issuer[192];
    if (mbedtls_x509_dn_gets(issuer, sizeof(issuer), &ca.subject) < 0) break;
    const time_t from = nowOrBuild() - 86400;
    char nb[16], na[16];
    fmtTime(nb, from);
    fmtTime(na, from + time_t(SELF_DAYS) * 86400);
    if (LocalCertificates::generate(false, name, issuer, nb, na, &key, &issuerKey, san,
        &rng.drbg, pem, 2048, kpem, 512)) break;
    certPem = reinterpret_cast<char *>(pem);
    keyPem = reinterpret_cast<char *>(kpem);
    ok = true;
  } while (false);

  free(pem);
  if (kpem) {
    memset(kpem, 0, 512);
    free(kpem);
  }
  mbedtls_x509_crt_free(&ca);
  mbedtls_pk_free(&issuerKey);
  mbedtls_pk_free(&key);
  if (!ok) return false;

  Preferences p;
  if (!p.begin("tls", false)) return false;
  const bool saved = p.putString("crt", certPem) > 0 && p.putString("key", keyPem) > 0 &&
                     p.putString("ips", currentIps()) > 0 && p.putBool("caLeaf", true) > 0;
  p.end();
  if (!saved) return false;
  EventLog::add('I', "HTTPS", "von Geräte-CA signiertes Zertifikat erzeugt (%s, %s %s)", host.c_str(), gLive.ipEth,
                strcmp(gLive.ipWifi, "-") ? gLive.ipWifi : "");
  return true;
}

// ---- Laden, prüfen, Info ----------------------------------------------------------------------

String readFile(const char *path) {
  File f = LittleFS.open(path, "r");
  if (!f) return String();
  String s = f.readString();
  f.close();
  return s;
}

bool writeFile(const char *path, const char *data) {
  LittleFS.mkdir("/tls");
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  const size_t n = strlen(data);
  const bool ok = f.write(reinterpret_cast<const uint8_t *>(data), n) == n;
  f.close();
  return ok;
}

// Gültig bis als Unix-Zeit; 0 wenn nicht lesbar
time_t validTo(const mbedtls_x509_crt &c) {
  struct tm t = {};
  t.tm_year = c.valid_to.year - 1900;
  t.tm_mon = c.valid_to.mon - 1;
  t.tm_mday = c.valid_to.day;
  t.tm_hour = c.valid_to.hour;
  return mktime(&t);
}

void buildInfo() {
  mbedtls_x509_crt c;
  mbedtls_x509_crt_init(&c);
  String j = "{";
  if (mbedtls_x509_crt_parse(&c, reinterpret_cast<const unsigned char *>(certPem.c_str()), certPem.length() + 1) == 0) {
    char subj[160], iss[160];
    mbedtls_x509_dn_gets(subj, sizeof(subj), &c.subject);
    mbedtls_x509_dn_gets(iss, sizeof(iss), &c.issuer);
    unsigned char fp[32];
    mbedtls_sha256(c.raw.p, c.raw.len, fp, 0);
    char hex[32 * 3];
    for (int i = 0; i < 32; i++) snprintf(hex + i * 3, 4, i < 31 ? "%02X:" : "%02X", fp[i]);
    char until[20];
    snprintf(until, sizeof(until), "%04d-%02d-%02d", c.valid_to.year, c.valid_to.mon, c.valid_to.day);
    int chain = 0;
    for (mbedtls_x509_crt *p = &c; p && p->raw.len; p = p->next) chain++;
    auto esc = [](String s) { s.replace("\\", "\\\\"); s.replace("\"", "\\\""); return s; };
    j += "\"subject\":\"" + esc(subj) + "\",\"issuer\":\"" + esc(iss) + "\",\"validTo\":\"" + until +
         "\",\"sha256\":\"" + hex + "\",\"chain\":" + chain + ",";
  }
  mbedtls_x509_crt_free(&c);
  mbedtls_x509_crt_init(&c);
  if (caPem.length() && parseCa(&c) == 0) {
    unsigned char fp[32];
    mbedtls_sha256(c.raw.p, c.raw.len, fp, 0);
    char hex[96], until[20];
    for (int i = 0; i < 32; i++) snprintf(hex + i * 3, 4, i < 31 ? "%02X:" : "%02X", fp[i]);
    snprintf(until, sizeof(until), "%04d-%02d-%02d", c.valid_to.year, c.valid_to.mon, c.valid_to.day);
    j += String("\"caSha256\":\"") + hex + "\",\"caValidTo\":\"" + until + "\",";
  }
  mbedtls_x509_crt_free(&c);
  j += String("\"source\":\"") + (ownCert ? "own" : "self") + "\",\"running\":" + (server ? "true" : "false") +
       ",\"enabled\":" + (local.httpsEnabled ? "true" : "false") + ",\"lowRam\":" + (lowRam ? "true" : "false") + "}";
  if (infoLock) xSemaphoreTake(infoLock, portMAX_DELAY);
  info = j;
  if (infoLock) xSemaphoreGive(infoLock);
}

bool selfSignedExpired() {
  const time_t now = time(nullptr);
  if (now < 1700000000) return false; // Uhr noch nicht gestellt
  mbedtls_x509_crt c;
  mbedtls_x509_crt_init(&c);
  bool expired = false;
  if (mbedtls_x509_crt_parse(&c, reinterpret_cast<const unsigned char *>(certPem.c_str()), certPem.length() + 1) == 0)
    expired = validTo(c) < now + 7 * 86400; // eine Woche vor Ablauf erneuern
  mbedtls_x509_crt_free(&c);
  return expired;
}

bool loadCert(bool regenerate) {
  ownCert = false;
  Preferences caStore;
  caStore.begin("tls", true);
  caPem = caStore.getString("cacrt", "");
  caStore.end();
  if (!regenerate && LittleFS.exists(OWN_CRT) && LittleFS.exists(OWN_KEY)) {
    certPem = readFile(OWN_CRT);
    keyPem = readFile(OWN_KEY);
    if (certPem.length() && keyPem.length()) {
      ownCert = true;
      return true;
    }
    EventLog::add('W', "HTTPS", "eigenes Zertifikat nicht lesbar, nehme selbst signiertes");
  }
  bool created = false;
  if (!ensureCa(created)) return false;
  if (!regenerate && !created) {
    Preferences p;
    p.begin("tls", true);
    certPem = p.getString("crt", "");
    keyPem = p.getString("key", "");
    const String ips = p.getString("ips", "");
    const bool caLeaf = p.getBool("caLeaf", false);
    p.end();
    // neue Adresse (DHCP): Zertifikat neu, sonst passt es nicht mehr zur Adresszeile
    const bool moved = ips != currentIps();
    if (moved && certPem.length()) EventLog::add('I', "HTTPS", "IP-Adresse geändert, erneuere Zertifikat");
    if (caLeaf && certPem.length() && keyPem.length() && !moved && !selfSignedExpired()) return true;
  }
  return makeSelfSigned();
}

// ---- Proxy auf die Weboberfläche --------------------------------------------------------------

const char *methodName(int m) {
  switch (m) {
    case HTTP_GET: return "GET";
    case HTTP_POST: return "POST";
    default: return nullptr;
  }
}

// Antwort-Header, die durchgereicht werden
const char *const PASS_HEADERS[] = {"WWW-Authenticate", "Location", "Cache-Control", "X-Width", "X-Height",
                                    "Content-Disposition"};
constexpr size_t PASS_COUNT = sizeof(PASS_HEADERS) / sizeof(PASS_HEADERS[0]);

esp_err_t badGateway(httpd_req_t *r) {
  httpd_resp_set_status(r, "502 Bad Gateway");
  httpd_resp_set_type(r, "text/plain");
  return httpd_resp_sendstr(r, "Weboberflaeche nicht erreichbar");
}

esp_err_t proxyInner(httpd_req_t *r);

esp_err_t proxy(httpd_req_t *r) {
  const int fd = httpd_req_to_sockfd(r);
  markBusy(fd, true);
  const esp_err_t rc = proxyInner(r);
  markBusy(fd, false);
  return rc;
}

esp_err_t proxyInner(httpd_req_t *r) {
  const char *method = methodName(r->method);
  if (!method) {
    httpd_resp_set_status(r, "405 Method Not Allowed");
    return httpd_resp_send(r, nullptr, 0);
  }
  WiFiClient up;
  up.setTimeout(15000);
  if (!up.connect(IPAddress(127, 0, 0, 1), 80, 3000)) return badGateway(r);

  String head;
  head.reserve(512);
  head += method;
  head += ' ';
  head += r->uri;
  head += " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n";
  char val[256];
  for (const char *h : {"Authorization", "Content-Type", "Origin", "Sec-Fetch-Site"}) {
    if (httpd_req_get_hdr_value_str(r, h, val, sizeof(val)) == ESP_OK) {
      head += h;
      head += ": ";
      head += val;
      head += "\r\n";
    }
  }
  // für die Herkunftsprüfung (CSRF) der Weboberfläche, die hier nur 127.0.0.1 als Host sieht
  if (httpd_req_get_hdr_value_str(r, "Host", val, sizeof(val)) == ESP_OK) {
    head += "X-Forwarded-Host: ";
    head += val;
    head += "\r\n";
  }
  if (r->content_len) head += "Content-Length: " + String(r->content_len) + "\r\n";
  head += "\r\n";
  up.write(reinterpret_cast<const uint8_t *>(head.c_str()), head.length());

  static char buf[1460]; // httpd bearbeitet Anfragen nacheinander in einer Task
  size_t remaining = r->content_len;
  int timeouts = 0;
  while (remaining) {
    int n = httpd_req_recv(r, buf, min(remaining, sizeof(buf)));
    if (n == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < 3) continue;
    if (n <= 0) return ESP_FAIL;
    timeouts = 0;
    if (up.write(reinterpret_cast<const uint8_t *>(buf), n) != size_t(n)) return badGateway(r);
    remaining -= n;
  }

  String status = up.readStringUntil('\n');
  int sp = status.indexOf(' ');
  if (sp < 0) return badGateway(r);
  status = status.substring(sp + 1);
  status.trim();
  String type = "text/html";
  String passVal[PASS_COUNT];
  long bodyLen = -1; // ohne Länge bis zum Verbindungsende lesen
  bool chunked = false;
  while (true) {
    String line = up.readStringUntil('\n');
    line.trim();
    if (!line.length()) break;
    int c = line.indexOf(':');
    if (c <= 0) continue;
    String name = line.substring(0, c);
    String v = line.substring(c + 1);
    v.trim();
    if (name.equalsIgnoreCase("Content-Type")) type = v;
    if (name.equalsIgnoreCase("Content-Length")) bodyLen = v.toInt();
    if (name.equalsIgnoreCase("Transfer-Encoding")) {
      v.toLowerCase();
      chunked = v == "chunked" || v.endsWith(", chunked") || v.endsWith(",chunked");
    }
    for (size_t i = 0; i < PASS_COUNT; i++)
      if (name.equalsIgnoreCase(PASS_HEADERS[i])) passVal[i] = v;
  }
  httpd_resp_set_status(r, status.c_str());
  httpd_resp_set_type(r, type.c_str());
  for (size_t i = 0; i < PASS_COUNT; i++)
    if (passVal[i].length()) httpd_resp_set_hdr(r, PASS_HEADERS[i], passVal[i].c_str());

  uint32_t idleSince = millis();
  if (chunked) {
    const bool ok = decodeHttpChunks(
        [&](uint8_t *data, size_t size) -> int {
          while (up.connected() || up.available()) {
            const int n = up.read(data, size);
            if (n > 0) { idleSince = millis(); return n; }
            if (millis() - idleSince > 15000) return -1;
            delay(2);
          }
          return -1;
        },
        [&](const char *data, size_t size) {
          return httpd_resp_send_chunk(r, data, size) == ESP_OK;
        }, buf, sizeof(buf));
    return ok ? httpd_resp_send_chunk(r, nullptr, 0) : ESP_FAIL;
  }
  long got = 0;
  while ((bodyLen < 0 || got < bodyLen) && (up.connected() || up.available())) {
    size_t want = sizeof(buf);
    if (bodyLen >= 0 && size_t(bodyLen - got) < want) want = size_t(bodyLen - got);
    int n = up.read(reinterpret_cast<uint8_t *>(buf), want);
    if (n > 0) {
      if (httpd_resp_send_chunk(r, buf, n) != ESP_OK) return ESP_FAIL;
      got += n;
      idleSince = millis();
    } else {
      if (millis() - idleSince > 15000) break;
      delay(2);
    }
  }
  return httpd_resp_send_chunk(r, nullptr, 0);
}

// ---- Start / Stopp ----------------------------------------------------------------------------

void stop() {
  if (!server) return;
  httpd_ssl_stop(server);
  server = nullptr;
  portENTER_CRITICAL(&sessMux);
  for (Sess &s : sess) s.fd = -1;
  portEXIT_CRITICAL(&sessMux);
}

void closeIdle() {
  static uint32_t lastCheck = 0;
  if (!server || millis() - lastCheck < 500) return;
  lastCheck = millis();
  int fds[8];
  size_t n = 8;
  if (httpd_get_client_list(server, &n, fds) != ESP_OK) return;
  int toClose[8];
  size_t nClose = 0;
  portENTER_CRITICAL(&sessMux);
  for (Sess &s : sess) { // Einträge geschlossener Sitzungen verwerfen
    bool open = false;
    for (size_t i = 0; i < n; i++) open |= fds[i] == s.fd;
    if (!open) s.fd = -1;
  }
  // bei knappem RAM ruhende Sitzungen sofort schließen
  const uint32_t idleMs = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < 50000 ? 0 : IDLE_CLOSE_MS;
  for (size_t i = 0; i < n; i++) {
    Sess *s = findSess(fds[i], true); // neu: ab jetzt zählen, auch wenn noch nichts kam
    if (s && !s->busy && (millis() - s->last > idleMs || millis() - s->opened > MAX_AGE_MS)) {
      toClose[nClose++] = fds[i];
      s->fd = -1;
    }
  }
  portEXIT_CRITICAL(&sessMux);
  for (size_t i = 0; i < nClose; i++) httpd_sess_trigger_close(server, toClose[i]);
}

// Passen die Puffer für alle Sitzungen? Probeweise reservieren, weil der freie RAM in Stücken liegt
bool tlsFits() {
  void *b[2 * MAX_SESSIONS] = {};
  bool ok = true;
  for (void *&p : b) ok = ok && (p = heap_caps_malloc(TLS_BUF, MALLOC_CAP_INTERNAL));
  ok = ok && heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >= TLS_SPARE;
  for (void *p : b) free(p);
  return ok;
}

void start(bool regenerate) {
  stop();
  lowRam = false;
  if (!local.httpsEnabled) {
    buildInfo();
    return;
  }
  // Ohne PSRAM reicht der interne RAM oft nicht; ein Port, der jede Verbindung abbricht, sperrt die Oberfläche aus
  lowRam = !tlsFits();
  if (lowRam) {
    EventLog::add('W', "HTTPS", "zu wenig RAM für HTTPS (frei %u, größter Block %u), nur HTTP",
                  unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                  unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
    buildInfo();
    return;
  }
  if (!loadCert(regenerate)) {
    EventLog::add('E', "HTTPS", "kein Zertifikat, nur HTTP");
    buildInfo();
    return;
  }
  httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
  conf.servercert = reinterpret_cast<const uint8_t *>(certPem.c_str());
  conf.servercert_len = certPem.length() + 1;
  conf.prvtkey_pem = reinterpret_cast<const uint8_t *>(keyPem.c_str());
  conf.prvtkey_len = keyPem.length() + 1;
  conf.httpd.uri_match_fn = httpd_uri_match_wildcard;
  // TLS liegt im internen RAM (Hardware-Krypto per DMA). Weitere Verbindungen warten in der Warteschlange,
  // bis eine ruhende geschlossen ist; LRU-Verdrängung würde laufende Handshakes abbrechen.
  conf.httpd.max_open_sockets = MAX_SESSIONS;
  conf.httpd.lru_purge_enable = false;
  conf.httpd.backlog_conn = 8;
  conf.httpd.stack_size = 10240;
  conf.httpd.max_uri_handlers = 4;
  conf.httpd.recv_wait_timeout = 10;
  conf.httpd.send_wait_timeout = 10;
  if (httpd_ssl_start(&server, &conf) != ESP_OK) {
    server = nullptr;
    EventLog::add('E', "HTTPS", "Start fehlgeschlagen (freier RAM %u), nur HTTP",
                  unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
    buildInfo();
    return;
  }
  for (httpd_method_t m : {HTTP_GET, HTTP_POST}) {
    httpd_uri_t u = {};
    u.uri = "/*";
    u.method = m;
    u.handler = proxy;
    httpd_register_uri_handler(server, &u);
  }
  EventLog::add('I', "HTTPS", "Port 443 aktiv, %s Zertifikat", ownCert ? "eigenes" : "von Geräte-CA signiertes");
  buildInfo();
}

// Mit Kabel auf dessen Adresse warten; die Adresse des Geräte-WLANs ist sofort da und reicht nicht
bool networkReady() {
  uint8_t ip[4];
  if (local.useEthernet) return gLive.ethOk && parseIp(gLive.ipEth, ip);
  return local.staSsid[0] ? WiFi.status() == WL_CONNECTED : parseIp(gLive.ipWifi, ip);
}
} // namespace

namespace HttpsServer {

void begin(const AppConfig &cfg) {
  local = cfg;
  if (!infoLock) infoLock = xSemaphoreCreateMutex();
  // abgelehnte Handshakes (Browser vor dem Bestätigen des Zertifikats) sind normal, nicht jedes Mal loggen
#ifndef TLS_DIAG
  esp_log_level_set("esp-tls-mbedtls", ESP_LOG_NONE);
  esp_log_level_set("esp-tls", ESP_LOG_NONE);
  esp_log_level_set("esp_https_server", ESP_LOG_NONE);
#endif
  esp_log_level_set("httpd", ESP_LOG_NONE);
  esp_log_level_set("httpd_txrx", ESP_LOG_NONE);
  // erst starten, wenn eine Adresse da ist, damit sie ins Zertifikat kommt; sonst nach 30 s trotzdem
  startAfterMs = millis() + 30000;
  buildInfo();
}

void applyConfig(const AppConfig &cfg) {
  const bool changed = cfg.httpsEnabled != local.httpsEnabled;
  local = cfg;
  if (changed) pending = P_RESTART;
}

void loop() {
  if (startAfterMs) {
    if (!networkReady() && int32_t(millis() - startAfterMs) < 0) return;
    startAfterMs = 0;
    start(false);
    return;
  }
  const uint8_t p = pending.exchange(P_NONE);
  if (p != P_NONE) restartAt = millis() + 1500; // Antwort auf die auslösende Anfrage erst zustellen lassen
  static bool regen = false;
  if (p == P_REGENERATE) regen = true;
  if (restartAt && int32_t(millis() - restartAt) >= 0) {
    restartAt = 0;
    start(regen);
    regen = false;
  }
  closeIdle();
}

bool running() { return server != nullptr; }

// Ohne PSRAM kann der interne RAM für eine TLS-Sitzung fehlen; dann darf HTTP nicht auf HTTPS umleiten,
// sonst ist die Oberfläche gar nicht mehr erreichbar
bool hasRoom() { return server && heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) >= 2 * TLS_BUF + 64; }

String infoJson() {
  if (infoLock) xSemaphoreTake(infoLock, portMAX_DELAY);
  String s = info;
  if (infoLock) xSemaphoreGive(infoLock);
  return s;
}

String caCertificate() {
  Preferences p;
  p.begin("tls", true);
  String s = p.getString("cacrt", "");
  p.end();
  return s;
}

bool installCustom(const char *crtPem, const char *kPem, String &err, bool english) {
  if (!crtPem || !kPem || !strstr(crtPem, "-----BEGIN CERTIFICATE-----") || !strstr(kPem, "PRIVATE KEY-----")) {
    err = english ? "Use PEM format for the certificate and key (-----BEGIN …)"
                  : "Zertifikat und Schlüssel bitte im PEM-Format (-----BEGIN …)";
    return false;
  }
  if (strstr(kPem, "ENCRYPTED")) {
    err = english ? "The key is encrypted. Export it without a password."
                  : "Schlüssel ist verschlüsselt; bitte ohne Passwort exportieren";
    return false;
  }
  Rng rng;
  mbedtls_x509_crt c;
  mbedtls_pk_context k;
  mbedtls_x509_crt_init(&c);
  mbedtls_pk_init(&k);
  bool ok = false;
  char why[96];
  int rc = mbedtls_x509_crt_parse(&c, reinterpret_cast<const unsigned char *>(crtPem), strlen(crtPem) + 1);
  if (rc != 0) {
    snprintf(why, sizeof(why), english ? "Cannot read certificate (-0x%04X)" : "Zertifikat nicht lesbar (-0x%04X)", unsigned(-rc));
  } else if ((rc = mbedtls_pk_parse_key(&k, reinterpret_cast<const unsigned char *>(kPem), strlen(kPem) + 1, nullptr, 0,
                                        mbedtls_ctr_drbg_random, &rng.drbg)) != 0) {
    snprintf(why, sizeof(why), english ? "Cannot read key (-0x%04X)" : "Schlüssel nicht lesbar (-0x%04X)", unsigned(-rc));
  } else if (mbedtls_pk_check_pair(&c.pk, &k, mbedtls_ctr_drbg_random, &rng.drbg) != 0) {
    snprintf(why, sizeof(why), "%s", english ? "The key does not match the certificate" : "Schlüssel passt nicht zum Zertifikat");
  } else if (time(nullptr) > 1700000000 && validTo(c) < time(nullptr)) {
    snprintf(why, sizeof(why), "%s", english ? "The certificate has expired" : "Zertifikat ist abgelaufen");
  } else {
    ok = true;
  }
  mbedtls_pk_free(&k);
  mbedtls_x509_crt_free(&c);
  if (!ok) {
    err = why;
    return false;
  }
  if (!writeFile(OWN_CRT, crtPem) || !writeFile(OWN_KEY, kPem)) {
    LittleFS.remove(OWN_CRT);
    LittleFS.remove(OWN_KEY);
    err = english ? "Could not save the certificate and key" : "Speichern fehlgeschlagen";
    return false;
  }
  EventLog::add('I', "HTTPS", "eigenes Zertifikat hochgeladen");
  pending = P_RESTART;
  return true;
}

void useSelfSigned() {
  LittleFS.remove(OWN_CRT);
  LittleFS.remove(OWN_KEY);
  pending = P_REGENERATE;
}

void forget() {
  LittleFS.remove(OWN_CRT);
  LittleFS.remove(OWN_KEY);
  Preferences p;
  p.begin("tls", false);
  p.clear();
  p.end();
}

} // namespace HttpsServer
