# -*- coding: utf-8 -*-
from pathlib import Path

ICONS = {
    "live": '<path d="M3 12h4l2-6 4 12 2-6h6"/>',
    "hit": '<rect x="4" y="4" width="16" height="16" rx="1"/><path d="M4 20 20 4M7 8.5h4M13 16c.9-1.1 1.8-1.1 2.7 0s1.8 1.1 2.7 0"/>',
    "netz": '<path d="M5 9h14v10H5zM8.5 19v-3M12 19v-3M15.5 19v-3M9 9V6h6v3"/>',
    "sunspec": '<path d="M5 5h14M5 10h14M5 15h14M5 20h8"/>',
    "knx": '<path d="M3 9h18M3 15h18"/><circle cx="8" cy="9" r="1.6"/><circle cx="16" cy="15" r="1.6"/>',
    "mqtt": '<path d="M4 11 12 4l8 7M6 9.5V20h12V9.5M10 20v-5h4v5"/>',
    "gateway": '<path d="M4 7h16M4 17h16M8 7v10M16 7v10"/><circle cx="12" cy="12" r="2"/>',
    "cloud": '<path d="M7 18h10a4 4 0 0 0 .4-8A6 6 0 0 0 6 9.6 4.2 4.2 0 0 0 7 18z"/>',
    "tasmota": '<path d="M9 3v5M15 3v5M6 8h12v3a6 6 0 0 1-12 0zM12 17v4"/>',
    "display": '<rect x="3" y="5" width="18" height="12" rx="2.5"/><path d="M9 20h6"/>',
    "ctrl": '<rect x="3" y="7" width="16" height="10" rx="2.5"/><path d="M21 10.5v3M7 10.5v3M11 10.5v3"/>',
    "debug": '<path d="M9 6h11M9 12h11M9 18h11M4.5 6h.01M4.5 12h.01M4.5 18h.01"/>',
    "ota": '<path d="M12 15V4M7.5 8.5 12 4l4.5 4.5M5 14v5h14v-5"/>',
}
SVG = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">{}</svg>'
LOGO = (
    '<svg viewBox="0 0 32 32" aria-hidden="true"><rect width="32" height="32" rx="9" fill="currentColor"/>'
    '<path d="M18 6 10 18h6l-2 8 8-12h-6z" style="fill:var(--bg)"/></svg>'
)


def side(active: str) -> str:
    def a(key, href, label):
        cls = ' class="on" aria-current="page"' if key == active else ""
        return f'    <a{cls} href="{href}">{SVG.format(ICONS[key])} {label}</a>'

    return "\n".join(
        [
            f'  <div class="logo">{LOGO}<div><b>HIT-Proxy</b><span>SunSpec für Consolinno</span></div></div>',
            "  <nav>",
            a("live", "/", "Live"),
            '    <div class="nav-grp">Datenquellen</div>',
            a("hit", "/config#hit", "HIT-Wechselrichter"),
            a("tasmota", "/config#tasmota", "Tasmota-Zähler"),
            a("cloud", "/config#cloud", "Hoymiles-Cloud"),
            '    <div class="nav-grp">Ausgänge</div>',
            a("sunspec", "/config#sunspec", "Consolinno"),
            a("mqtt", "/config#mqtt", "Home Assistant"),
            a("gateway", "/config#gateway", "Modbus-Gateway"),
            a("knx", "/config#knx", "KNX"),
            '    <div class="nav-grp">Gerät</div>',
            a("netz", "/config#netz", "Netzwerk und Zugang"),
            a("display", "/config#display", "Display"),
            a("ctrl", "/config#ctrl", "Akku-Steuerung"),
            a("debug", "/debug", "Protokoll"),
            a("ota", "/ota", "Firmware und Reset"),
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
  const sv=document.getElementById('sideVer'); if(sv)sv.textContent='v'+(s.version||'')+'\n'+(s.ipShown||'');
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
