
(()=>{
const base=window.applyThemeLang;let built=false;
const en=()=>document.documentElement.lang==='en';
window.uiText=(de,english)=>en()?english:de;
window.num=(n,d=0,max=d)=>(Number(n)||0).toLocaleString(en()?'en-US':'de-DE',{minimumFractionDigits:d,maximumFractionDigits:max});
let renderedLang='de';
const german=new WeakMap();
const translatedAttrs=['placeholder','title','aria-label'];
function translate(){
  const lang=document.documentElement.lang;
  if(renderedLang===lang)return;
  renderedLang=lang;
  document.querySelectorAll('[data-en],[data-en-placeholder],[data-en-title],[data-en-aria-label]').forEach(el=>{
    if(!german.has(el))german.set(el,{text:el.textContent,attrs:translatedAttrs.map(a=>el.getAttribute(a))});
    const original=german.get(el);
    if(el.hasAttribute('data-en'))el.textContent=en()?el.getAttribute('data-en'):original.text;
    translatedAttrs.forEach((attr,i)=>{
      const english=el.getAttribute('data-en-'+attr);
      if(english!==null)el.setAttribute(attr,en()?english:original.attrs[i]);
    });
  });
}
function addLogout(){
  const logout=document.createElement('button');logout.type='button';logout.className='sec';
  logout.textContent='Abmelden';logout.setAttribute('data-en','Sign out');
  logout.onclick=()=>{
    logout.disabled=true;
    const xhr=new XMLHttpRequest();
    xhr.open('POST','/api/logout',true);
    xhr.setRequestHeader('Content-Type','application/json');
    xhr.timeout=5000;
    xhr.onloadend=()=>{
      if(xhr.status>=200&&xhr.status<300)location.replace('/');
      else{
        logout.disabled=false;
        alert(uiText('Abmelden fehlgeschlagen. Bitte erneut versuchen.',
          'Sign out failed. Please try again.'));
      }
    };
    xhr.send();
  };
  const head=document.querySelector('.page-head');if(head)head.append(logout);
}
function build(){
  built=true;
  const side=document.querySelector('.side'),nav=side&&side.querySelector('nav');if(!nav)return;
  const menu=document.createElement('div');menu.className='bp-menu';
  let col=null;
  [...nav.children].forEach(el=>{
    if(el.classList.contains('nav-grp')){col=document.createElement('div');menu.append(col);col.append(el);}
    else if(col&&el.getAttribute('href')!=='debug.html')col.append(el);
  });
  const btn=document.createElement('a');btn.href='#';btn.className='bp-btn';
  btn.innerHTML='<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" aria-hidden="true"><path d="M4 7h10M18 7h2M4 17h2M10 17h10"/><circle cx="16" cy="7" r="2"/><circle cx="8" cy="17" r="2"/></svg> <span></span>';
  if(menu.querySelector('a.on'))btn.classList.add('on');
  btn.onclick=e=>{e.preventDefault();btn.setAttribute('aria-expanded',side.classList.toggle('open'));};
  nav.append(btn);
  const pf=side.querySelector('.prefs');if(pf)menu.append(pf);
  nav.append(menu);
  document.addEventListener('click',e=>{if(!side.contains(e.target)||(menu.contains(e.target)&&e.target.closest('a')))side.classList.remove('open');});
  document.addEventListener('keydown',e=>{if(e.key==='Escape')side.classList.remove('open');});
}
function live(s){
  const flow=document.querySelector('.flow');if(!flow)return;
  let g=document.getElementById('bpS');
  if(!g){flow.insertAdjacentHTML('beforeend','<svg id="bpS"></svg>');g=document.getElementById('bpS');}
  const nar=flow.clientWidth<600,VW=nar?480:960,XL=nar?160:300,XR=nar?306:646,XM=(XL+14+XR)/2;
  g.setAttribute('viewBox','0 0 '+VW+' 400');g.classList.toggle('nar',nar);
  const N=v=>Number(v)||0,E=en(),fmt=v=>Math.round(v).toLocaleString(E?'en-US':'de-DE');
  const pv=Math.max(0,N(s.pvW)),bat=N(s.batW),grid=(s.tasmotaOk&&s.tasmotaLiveGrid)?N(s.tasmotaPowerW):N(s.gridW);
  const dis=Math.max(0,bat),chg=Math.max(0,-bat),imp=Math.max(0,grid),exp=Math.max(0,-grid);
  const nd=(v,c,l)=>({v,r:v,c,l,o:0});
  const src=[nd(pv,'--sun',E?'Solar':'PV'),nd(dis,'--acc',E?'Battery discharging':'Akku entlädt'),nd(imp,'--grid',E?'Grid import':'Netzbezug')];
  const dst=[nd(Math.max(0,pv+bat+grid),'--home',E?'Home':'Haus'),nd(chg,'--acc',E?'Battery charging':'Akku lädt'),nd(exp,'--grid',E?'Feed-in':'Einspeisung')];
  const L=[];
  [[0,0],[0,1],[0,2],[1,0],[1,2],[2,0],[2,1]].forEach(([a,b])=>{const x=Math.min(src[a].r,dst[b].r);if(x>0){src[a].r-=x;dst[b].r-=x;L.push([src[a],dst[b],x]);}});
  const sc=Math.min(120/Math.max(pv+dis+imp,1),.03),GAP=66;
  const place=c=>{let y=(340-c.reduce((t,n)=>t+Math.max(2,n.v*sc),0)-2*GAP)/2+12;c.forEach(n=>{n.h=Math.max(2,n.v*sc);n.y=y;y+=n.h+GAP;});};
  place(src);place(dst);
  let h='<defs>'+src.map((n,i)=>`<pattern id="bpH${i}" patternUnits="userSpaceOnUse" width="7" height="7" patternTransform="rotate(45)"><path d="M0 0V7" style="stroke:var(${n.c});stroke-width:3"/></pattern>`).join('')+'</defs>';
  L.forEach(([a,b,x])=>{const t=x*sc,ya=a.y+a.o,yb=b.y+b.o;a.o+=t;b.o+=t;
    h+=`<path class="rib" style="fill:url(#bpH${src.indexOf(a)});stroke:var(${a.c})" d="M${XL+14} ${ya}C${XM} ${ya} ${XM} ${yb} ${XR} ${yb}V${yb+t}C${XM} ${yb+t} ${XM} ${ya+t} ${XL+14} ${ya+t}Z"/>`;});
  const col=(c,x,tx,anchor)=>c.forEach(n=>{const m=n.y+n.h/2,z=n.v<1?' class="zero"':'';
    h+=`<g${z}><rect x="${x}" y="${n.y}" width="14" height="${n.h}" style="fill:var(${n.c})"/>`+
      `<text x="${tx}" y="${m-10}" text-anchor="${anchor}" class="lbl">${n.l}</text>`+
      `<text x="${tx}" y="${m+22}" text-anchor="${anchor}" class="val">${fmt(n.v)}<tspan dx="6">W</tspan></text></g>`;});
  col(src,XL,XL-20,'end');col(dst,XR,XR+34,'start');
  const soc=Math.max(0,Math.min(100,N(s.soc)));
  h+=`<rect class="soc" x="${VW/2-70}" y="378" width="140" height="10"/><rect x="${VW/2-70}" y="378" width="${1.4*soc}" height="10" style="fill:var(${soc<15?'--fault':soc<30?'--warn':'--acc'})"/>`+
    `<text x="${VW/2}" y="368" text-anchor="middle" class="dir">${E?'Battery':'Akku'} ${fmt(soc)} %</text>`;
  g.innerHTML=h;
}
// Menüpunkt nur auf Boards mit Bluetooth; so muss er nicht in jeder Seitenleiste stehen
function bleNav(s){
  if(!s||!s.hasBleLed||document.getElementById('navBle'))return;
  const knx=document.querySelector('.side nav a[href="config.html#knx"]');if(!knx)return;
  const a=document.createElement('a');a.id='navBle';a.href='bluetooth.html';
  a.innerHTML='<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="m7 7 10 10-5 5V2l5 5L7 17"/></svg> Bluetooth';
  if(location.pathname.endsWith('/bluetooth.html')){a.className='on';a.setAttribute('aria-current','page');}
  knx.after(a);
}
function menu(){
  if(document.documentElement.dataset.theme!=='blueprint')return false;
  if(!built)build();
  const b=document.querySelector('.bp-btn span');if(b)b.textContent=en()?'Settings':'Einstellungen';
  return true;
}
window.applyThemeLang=s=>{base(s);bleNav(s);translate();if(menu())live(s);};
addLogout();
menu();
})();
