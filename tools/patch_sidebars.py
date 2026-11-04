# -*- coding: utf-8 -*-
from pathlib import Path

def side(active: str) -> str:
    def a(key, href, icon, label):
        cls = ' class="on"' if key == active else ""
        return f'    <a{cls} href="{href}"><i>{icon}</i> {label}</a>'

    return "\n".join(
        [
            '  <div class="logo"><div class="mark">HS</div><div><b>HIT SunSpec</b><span>Proxy</span></div></div>',
            "  <nav>",
            '    <div class="nav-grp">System</div>',
            a("live", "/", "◉", "Live"),
            '    <div class="nav-grp">Anbindung</div>',
            a("hit", "/config#hit", "⚡", "HIT"),
            a("netz", "/config#netz", "⊕", "Netzwerk"),
            a("sunspec", "/config#sunspec", "≈", "SunSpec"),
            a("knx", "/config#knx", "▦", "KNX"),
            '    <div class="nav-grp">Dienste</div>',
            a("mqtt", "/config#mqtt", "☁", "MQTT / Cloud"),
            a("tasmota", "/config#tasmota", "⌀", "Tasmota"),
            a("debug", "/debug", "≡", "Protokoll"),
            '    <div class="nav-grp">Gerät</div>',
            a("config", "/config", "⚙", "Einstellungen"),
            a("ota", "/ota", "↑", "OTA"),
            "  </nav>",
            '  <div class="foot">',
            '    <div class="online"><span class="dot ok"></span> Online</div>',
            '    <div class="meta" id="sideVer">—</div>',
            '    <div class="prefs">',
            '      <select id="uiLang" onchange="saveUiPrefs()">',
            '        <option value="de">Deutsch</option>',
            '        <option value="en">English</option>',
            "      </select>",
            '      <select id="uiTheme" onchange="saveUiPrefs()">',
            '        <option value="mint">Mint</option>',
            '        <option value="ocean">Ocean</option>',
            '        <option value="ember">Ember</option>',
            "      </select>",
            "    </div>",
            "  </div>",
        ]
    )


UI_BOOT = r"""
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
"""


def inject_ui_boot(html: str) -> str:
    if "function applyThemeLang" in html:
        return html
    # inject before first <script> that is not UI_BOOT
    idx = html.find("<script>")
    if idx < 0:
        return html
    return html[:idx] + UI_BOOT + html[idx:]


wc = Path(r"G:\HA-Gemini-MCP\hit-sunspec-proxy\src\WebConfig.cpp")
text = wc.read_text(encoding="utf-8")

for title, active in [
    ("Live · HIT Proxy", "live"),
    ("Config · HIT Proxy", "config"),
    ("Write-Debug · HIT Proxy", "debug"),
]:
    marker = f"<title>{title}</title>"
    i = text.find(marker)
    if i < 0:
        raise SystemExit(f"missing {title}")
    j = text.find('<aside class="side">', i)
    k = text.find("</aside>", j) + len("</aside>")
    text = text[:j] + f'<aside class="side">\n{side(active)}\n</aside>' + text[k:]

# ensure card ids
for a, b in [
    ('id="hit"', None),
]:
    pass

if 'id="hit"' not in text:
    raise SystemExit("card ids missing — run previous patch first or re-add")

# inject UI_BOOT into each HTML blob once — look for DASH/INDEX/DEBUG script starts after aside
# Inject before refresh/load scripts by unique markers
if "function applyThemeLang" not in text:
    text = text.replace(
        '<script>\nconst chip=',
        UI_BOOT + '<script>\nconst chip=',
        1,
    )
    # config page
    if "function applyThemeLang" in text and text.count("function applyThemeLang") < 2:
        text = text.replace(
            "<script>\nfunction dptOpts",
            UI_BOOT + "<script>\nfunction dptOpts",
            1,
        )
    if text.count("function applyThemeLang") < 3:
        text = text.replace(
            "<script>\nfunction age(",
            UI_BOOT + "<script>\nfunction age(",
            1,
        )

wc.write_text(text, encoding="utf-8")

ota = Path(r"G:\HA-Gemini-MCP\hit-sunspec-proxy\src\OtaUpdate.cpp")
ot = ota.read_text(encoding="utf-8")
j = ot.find('<aside class="side">')
k = ot.find("</aside>", j) + len("</aside>")
ot = ot[:j] + f'<aside class="side">\n{side("ota")}\n</aside>' + ot[k:]
if "function applyThemeLang" not in ot:
    ot = ot.replace("<script>\nasync function boot()", UI_BOOT + "<script>\nasync function boot()", 1)
    ot = ot.replace(
        "async function boot(){const s=await(await fetch('/api/status')).json();document.getElementById('ver').textContent=s.version||'?';document.getElementById('sideVer').textContent='v'+(s.version||'');}",
        "async function boot(){const s=await(await fetch('/api/status')).json();applyThemeLang(s);document.getElementById('ver').textContent=s.version||'?';}",
        1,
    )
ota.write_text(ot, encoding="utf-8")
print("sidebars+prefs+ui boot ok", text.count("function applyThemeLang"))
