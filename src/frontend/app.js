'use strict';
/* UNIFY CONTROL frontend. Talks to the native engine with plain-string commands:
   "scan" · "settings:get" · "settings:set:<key>:<int>" · "settings:restore". */
const $ = s => document.querySelector(s);
const inHost = !!(window.chrome && window.chrome.webview);
let devices = [], groups = [], current = 'dashboard';
let databaseInfo = { status: 'NOT LOADED', manufacturerCount: 0, verifiedDeviceCount: 0 };
let databaseHealth = { rejectedVerifiedRecords: 0, conflictingModelKeys: 0 };
let protocolResearch = { status: 'NOT LOADED', itemCount: 0, verifiedCount: 0, items: [] };
let sys = { values: null, originals: null };
let viaResults = {};
const PROFILE_STORAGE_KEY = 'unify-control-profiles-v1';
let profiles = loadProfiles();

/* ---------- helpers ---------- */
function esc(s){return String(s==null?'':s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));}
function summarizeFindings(inventory){const findings=inventory.flatMap(d=>Array.isArray(d.diagnostics)?d.diagnostics:[]);return {total:findings.length,errors:findings.filter(x=>x.severity==='ERROR').length,warnings:findings.filter(x=>x.severity==='WARNING').length};}
function groupInterfaces(list){const map=new Map();let n=0;(Array.isArray(list)?list:[]).forEach(d=>{const key=d&&d.containerId?[d.vid,d.pid,d.product||'',d.containerId,d.deviceType||''].join('|'):'single-'+(n++);const g=map.get(key);if(g){g.interfaces.push(d);}else{map.set(key,{device:d,interfaces:[d]});}});return Array.from(map.values());}
function send(command){try{if(inHost)window.chrome.webview.postMessage(command);}catch(e){console.error(e);}}
let toastTimer;
function toast(message, ok){const t=$('#toast');if(!t)return;t.textContent=message;t.className='show '+(ok?'ok':'err');clearTimeout(toastTimer);toastTimer=setTimeout(()=>{t.className='';},3200);}
const iconFor=t=>t==='Keyboard'?'⌨':t==='Mouse'?'⌖':'▦';
const kindFor=t=>t==='Keyboard'?'keyboard':t==='Mouse'?'mouse':'';

/* ---------- UI building blocks ---------- */
const head=(eyebrow,title,sub)=>`<div class="eyebrow">${eyebrow}</div><h1>${title}</h1>${sub?`<p class="lead">${sub}</p>`:''}`;
const stat=(label,value,sub)=>`<article class="card stat-card"><span>${label}</span><strong>${value}</strong><small>${sub||''}</small></article>`;
const stats=items=>`<div class="stats">${items.map(i=>stat(...i)).join('')}</div>`;
const notice=(title,text)=>`<div class="notice"><div class="notice-icon">i</div><div><b>${title}</b><p>${text}</p></div></div>`;
const empty=(title,text)=>`<div class="empty-state"><b>${title}</b><span>${text||''}</span></div>`;
const section=(eyebrow,title,right)=>`<div class="section-head"><div><div class="eyebrow">${eyebrow}</div><h2>${title}</h2></div>${right||''}</div>`;


/* ---------- open standards (layer 2) ---------- */
const STD={
  WINDOWS_HID:{name:'HID estándar',desc:'Los ajustes de Windows (capa 1) se aplican a este dispositivo.'},
  VIA_RAW_HID:{name:'QMK / VIA',desc:'Teclado con firmware QMK: protocolo abierto para remapear teclas y capas.'},
  HID_LAMPARRAY:{name:'Dynamic Lighting (LampArray)',desc:'RGB mediante el estándar de iluminación de Windows.'},
  LOGITECH_HIDPP:{name:'Logitech HID++',desc:'Protocolo de Logitech documentado por la comunidad.'},
  VENDOR_SPECIFIC:{name:'Canal propietario',desc:'Canal privado del fabricante: hace falta su documentación o capturar el protocolo.'}
};
const SUPPORT={APPLIES_NOW:['Activo','ok'],READ_ONLY_PROBE:['Verificación de solo lectura','ok'],DETECTED_NOT_IMPLEMENTED:['Detectado · aún sin implementar','lock'],NEEDS_DOCUMENTATION:['Requiere documentación','lock']};
const VIA_ERRORS={NO_REPLY:'El dispositivo no respondió.',NOT_A_VIA_KEYBOARD:'No habla VIA (¿firmware sin VIA activado?).',OPEN_FAILED:'No se pudo abrir la interfaz (¿otra app la usa, por ejemplo VIA?).',UNEXPECTED_REPORT_SIZE:'Tamaño de reporte inesperado.',NOT_VIA_INTERFACE:'Interfaz no válida.'};
function collectStandards(interfaces){
  const seen=new Map();
  interfaces.forEach(i=>(i.standards||[]).forEach(st=>{if(!seen.has(st.code))seen.set(st.code,{code:st.code,support:st.support,index:devices.indexOf(i)});}));
  return Array.from(seen.values());
}
function viaResultHtml(index){
  const r=viaResults[index];if(!r)return '';
  if(r.pending)return '<small class="muted-label">Consultando el dispositivo…</small>';
  return r.ok?`<small class="sev-INFO">✔ Responde a VIA · protocolo v${esc(r.protocolVersion)} · ${esc(r.layerCount)} capa(s). Este teclado se puede configurar con el protocolo abierto.</small>`:`<small class="sev-WARNING">${esc(VIA_ERRORS[r.error]||('Error: '+r.error))}</small>`;
}
function standardsHtml(interfaces){
  const list=collectStandards(interfaces);
  if(!list.length)return '<div class="std-list"><div class="std"><b>Sin estándar reconocido</b><small>Solo se aplican los ajustes de Windows si es un teclado o ratón.</small></div></div>';
  return `<div class="std-list">${list.map(x=>{const m=STD[x.code]||{name:x.code,desc:''},sp=SUPPORT[x.support]||[x.support,''];
    const btn=x.code==='VIA_RAW_HID'&&inHost&&x.index>=0?`<button class="btn" data-via-probe="${x.index}">Verificar VIA</button>`:'';
    return `<div class="std"><div class="std-top"><b>${esc(m.name)}</b><span class="badge ${sp[1]}">${esc(sp[0])}</span>${btn}</div><small>${esc(m.desc)}</small>${x.code==='VIA_RAW_HID'?viaResultHtml(x.index):''}</div>`}).join('')}</div>`;
}

function deviceCard(g){
  const d=g.device,name=d.product||d.manufacturer||'Dispositivo HID desconocido';
  const exact=d.identificationStatus==='EXACT MODEL MATCH';
  const up=d.universalProfile||{},rl=d.reportLengths||{};
  const ifaces=g.interfaces.map(i=>i.interfaceNumber<0?'—':i.interfaceNumber).join(', ');
  return `<article class="card device"><div class="dev-icon ${kindFor(d.deviceType)}">${iconFor(d.deviceType)}</div><div class="dev-main">
    <div class="dev-title"><b>${esc(name)}</b><span class="badge ${exact?'ok':''}">${exact?'MODELO VERIFICADO':'SOLO DETECTADO'}</span></div>
    <div class="chips"><span class="chip">${esc(d.deviceType||'HID')}</span><span class="chip">VID ${esc(d.vid)}</span><span class="chip">PID ${esc(d.pid)}</span><span class="chip">${esc(d.connection||'HID')}</span><span class="chip">${g.interfaces.length} interfaz(ces)</span></div>
    ${standardsHtml(g.interfaces)}
    <details><summary>Detalles técnicos</summary><dl>
      <dt>Fabricante</dt><dd>${esc(d.manufacturer||'No disponible')}</dd>
      <dt>Identificación</dt><dd>${esc(d.identifiedModel||d.identifiedManufacturer||'Sin coincidencia en catálogo')} · ${esc(d.identificationStatus||'UNKNOWN')}</dd>
      <dt>Container ID</dt><dd>${esc(d.containerId||'No disponible')}</dd>
      <dt>Interfaces HID</dt><dd>${esc(ifaces)}</dd>
      <dt>Descriptor HID</dt><dd>${d.hidDescriptorReadable?'Legible':'No disponible'} · IN ${esc(rl.input??'—')} / OUT ${esc(rl.output??'—')} / FEATURE ${esc(rl.feature??'—')} bytes</dd>
      <dt>Perfil universal</dt><dd>${esc(up.function||'UNKNOWN')} · ${esc(up.transport||'UNKNOWN')} · escritura ${up.hardwareWritesAllowed?'elegible':'bloqueada'}</dd>
    </dl></details></div></article>`;
}
function deviceList(list,limit){const gs=groupInterfaces(list);return gs.length?`<div class="device-list">${gs.slice(0,limit||gs.length).map(deviceCard).join('')}</div>`:empty('No se detectó ningún dispositivo HID','Conecta un teclado o ratón y pulsa ↻ para buscar de nuevo.');}

/* ---------- Windows-level (layer 1) controls ---------- */
const SETTINGS={
  mouse:[
    {key:'mouseSpeed',label:'Velocidad del puntero',type:'range',min:1,max:20,fmt:v=>`${v} / 20`,hint:'Sensibilidad base del cursor en Windows (10 es el valor estándar).'},
    {key:'mouseAccel',label:'Mejorar precisión del puntero',type:'toggle',hint:'Aceleración del cursor. Desactívala para un movimiento 1:1 (gaming).'},
    {key:'swapButtons',label:'Invertir botones principal y secundario',type:'toggle',hint:'Para uso con la mano izquierda.'},
    {key:'doubleClickMs',label:'Velocidad de doble clic',type:'range',min:200,max:900,step:50,fmt:v=>`${v} ms`,hint:'Tiempo máximo entre dos clics.'},
    {key:'wheelLines',label:'Líneas por giro de rueda',type:'range',min:1,max:30,fmt:v=>`${v}`,hint:'Cuánto se desplaza la página con cada paso de la rueda.'}
  ],
  keyboard:[
    {key:'keyDelay',label:'Retardo antes de repetir tecla',type:'range',min:0,max:3,fmt:v=>`${(v+1)*250} ms`,hint:'Tiempo que mantienes una tecla antes de que se repita.'},
    {key:'keyRate',label:'Velocidad de repetición',type:'range',min:0,max:31,fmt:v=>`${(2.5+v*27.5/31).toFixed(1)} car/s`,hint:'Cuántos caracteres por segundo al mantener una tecla.'}
  ]
};
const ALL_SETTING_KEYS=[...SETTINGS.mouse,...SETTINGS.keyboard].map(s=>s.key);
function controlHtml(def){
  const have=sys.values&&sys.values[def.key]!=null,v=have?Number(sys.values[def.key]):null,dis=have?'':'disabled';
  if(def.type==='toggle')return `<article class="card control"><div class="control-top"><b>${def.label}</b><label class="switch"><input type="checkbox" data-setting="${def.key}" ${v?'checked':''} ${dis}><span></span></label></div><small>${def.hint}</small></article>`;
  const min=def.min,max=def.max,val=have?Math.min(max,Math.max(min,v)):min,p=((val-min)/(max-min))*100;
  return `<article class="card control"><div class="control-top"><b>${def.label}</b><span class="control-val" data-val-for="${def.key}">${have?def.fmt(val):'—'}</span></div><input type="range" data-setting="${def.key}" min="${min}" max="${max}" step="${def.step||1}" value="${val}" style="--p:${p}%" ${dis}><small>${def.hint}</small></article>`;
}
function windowsPanel(group){
  const note=inHost?'Estos ajustes usan APIs documentadas de Windows: funcionan con <b>cualquier</b> teclado o ratón y se pueden revertir en un clic. Afectan a todo el sistema, no a un dispositivo concreto.':'Abre la aplicación de Windows para aplicar ajustes. Esta es solo una vista previa.';
  return section('CAPA 1 · AJUSTES DE WINDOWS','Control universal (activo)',`<button class="btn" data-action="restore" ${inHost&&sys.originals?'':'disabled'}>Restaurar valores originales</button>`)+
    `<p class="lead">${note}</p><div class="controls">${SETTINGS[group].map(controlHtml).join('')}</div>`;
}
function lockedPanel(label){
  return section('CAPA 2 y 3 · CONTROL ESPECÍFICO DEL DISPOSITIVO',`${label} del hardware <span class="badge lock">BLOQUEADO</span>`)+
    notice('Por qué está bloqueado',`Cambiar ${label.toLowerCase()} dentro del dispositivo requiere el protocolo exacto de ese modelo (estándar abierto como QMK/VIA/OpenRGB, o un adaptador verificado). UNIFY CONTROL no adivina comandos ni envía reportes arbitrarios: un comando incorrecto puede dejar el dispositivo inutilizable.`);
}

/* ---------- pages ---------- */
function pageDashboard(){
  const kb=devices.filter(d=>d.deviceType==='Keyboard').length,ms=devices.filter(d=>d.deviceType==='Mouse').length;
  const exact=devices.filter(d=>d.identificationStatus==='EXACT MODEL MATCH').length;
  return `<div class="hero"><div><div class="eyebrow">ECOSISTEMA DE DISPOSITIVOS</div><h1>Tu hardware.<br><em>Un solo centro de control.</em></h1><p class="lead" style="margin:0">Detecta tus periféricos, entiende qué expone Windows y configúralos por capas: de lo universal a lo específico de cada marca.</p><div class="hero-actions"><button class="btn primary" data-goto="mouse">Ajustar ratón</button><button class="btn" data-goto="keyboard">Ajustar teclado</button><button class="btn" data-action="scan">↻ Buscar dispositivos</button></div></div><div class="hero-orb"><div>U</div></div></div>
  ${stats([['DISPOSITIVOS',devices.length||'—','Interfaces HID detectadas'],['TECLADOS',kb,'Interfaces detectadas'],['RATONES',ms,'Interfaces detectadas'],['MODELOS VERIFICADOS',exact,'Con evidencia en catálogo']])}
  ${section('CÓMO FUNCIONA','Control universal por capas')}
  <div class="layers">
    <article class="card layer active"><span class="tag">CAPA 1 · ACTIVA</span><b>Ajustes de Windows</b><p>Velocidad, aceleración, botones, rueda y repetición de teclas. Funciona con cualquier marca.</p></article>
    <article class="card layer"><span class="tag">CAPA 2 · ${devices.reduce((n,d)=>n+(d.standards||[]).filter(x=>x.support==='READ_ONLY_PROBE'||x.support==='DETECTED_NOT_IMPLEMENTED').length,0)} DETECTADOS</span><b>Estándares abiertos</b><p>QMK/VIA, Dynamic Lighting, Logitech HID++. Un solo protocolo cubre muchos modelos. VIA ya se puede verificar.</p></article>
    <article class="card layer"><span class="tag">CAPA 3 · ${databaseInfo.verifiedDeviceCount||0} ADAPTADORES</span><b>Adaptadores por modelo</b><p>Plugins verificados con lectura previa, confirmación y reversión.</p></article>
  </div>
  ${section('HARDWARE','Dispositivos conectados',`<button class="btn" data-goto="devices">Ver todos →</button>`)}
  ${deviceList(devices,6)}`;
}
function pageDevices(){
  const gl=groups.length?`<div class="device-list">${groups.map(g=>`<article class="card device"><div class="dev-icon">▣</div><div class="dev-main"><div class="dev-title"><b>${esc(g.displayName||'Dispositivo físico desconocido')}</b><span class="badge">GRUPO</span></div><div class="chips"><span class="chip">${esc(g.interfaceCount)} interfaz(ces)</span><span class="chip">${esc(g.receiverHint||'Receptor desconocido')}</span></div><small class="muted-label">${esc(g.containerId||'Sin Container ID compartido')}</small></div></article>`).join('')}</div>`:empty('Sin grupos físicos','Windows no expone un Container ID compartido para todos los dispositivos.');
  return head('HARDWARE','Mis dispositivos','Todas las interfaces que Windows expone, agrupadas por dispositivo físico cuando es posible.')+deviceList(devices)+section('TOPOLOGÍA USB','Grupos físicos',`<span class="muted-label">Agrupados por Container ID de Windows</span>`)+gl;
}
function candidates(page){
  const rel=devices.filter(d=>page==='keyboard'?d.deviceType==='Keyboard':page==='mouse'?d.deviceType==='Mouse':true);
  return section('SELECCIÓN',`Dispositivos candidatos (${groupInterfaces(rel).length})`)+deviceList(rel);
}
function pageControl(page){
  const t={keyboard:['HARDWARE · TECLADO','Teclado','teclas, capas y memoria interna'],mouse:['HARDWARE · RATÓN','Ratón','DPI, polling rate y botones'],rgb:['ILUMINACIÓN','RGB','zonas, efectos y brillo'],macros:['AUTOMATIZACIÓN','Macros','grabación y almacenamiento en el dispositivo']}[page];
  const top=SETTINGS[page]?windowsPanel(page):'';
  return head(t[0],t[1],page==='rgb'||page==='macros'?`Los ajustes de ${t[2]} dependen del protocolo de cada modelo.`:'Configura lo que Windows permite para cualquier dispositivo y consulta qué requiere soporte específico.')+top+lockedPanel(t[2])+candidates(page);
}
function pageDiagnostics(){
  const s=summarizeFindings(devices);
  const rows=devices.map(d=>{const f=d.diagnostics||[],ev=d.evidenceAssessment||{};
    return `<article class="card device"><div class="dev-icon">⌕</div><div class="dev-main"><div class="dev-title"><b>${esc(d.product||d.manufacturer||'Dispositivo desconocido')}</b><span class="badge">${f.length} hallazgo(s)</span></div><div class="chips"><span class="chip">${esc(d.connection||'Transporte desconocido')}</span><span class="chip">${esc(ev.identityLevel||'UNKNOWN')}</span><span class="chip">Evidencia ${Number(ev.score||0)}</span><span class="chip">${esc(ev.confidence||'INSUFFICIENT')}</span></div>
    <details><summary>Evidencia y hallazgos</summary><dl><dt>Estado</dt><dd>${esc(ev.state||'DETECTED_ONLY')}</dd><dt>Descriptor HID</dt><dd>${esc(d.hidDescriptorAnalysis||'Sin metadatos')}</dd>${(d.reportIds||[]).length?`<dt>Report IDs</dt><dd>${d.reportIds.map(esc).join(', ')}</dd>`:''}<dt>WinUSB</dt><dd>${esc(d.winUsbStatus||'NOT_ATTEMPTED')} · ${(d.usbEndpoints||[]).length} endpoint(s)</dd><dt>BLE GATT</dt><dd>${esc(d.gattStatus||'NOT_ATTEMPTED')} · ${(d.gattServices||[]).length} servicio(s)</dd></dl>
    ${(d.usageDetails||[]).slice(0,8).map(x=>`<small class="muted-label" style="display:block">${esc(x)}</small>`).join('')}
    ${(d.usbEndpoints||[]).slice(0,12).map(ep=>`<small class="muted-label" style="display:block">USB EP 0x${Number(ep.address).toString(16).toUpperCase()} · ${esc(ep.transferType)} · IF ${esc(ep.interface)} ALT ${esc(ep.alternateSetting)} · ${esc(ep.maxPacketSize)} B</small>`).join('')}
    ${(d.gattServices||[]).slice(0,8).map(sv=>`<small class="muted-label" style="display:block">GATT ${esc(sv.uuid)} · ${(sv.characteristics||[]).length} característica(s)</small>`).join('')}
    ${f.map(x=>`<div class="finding"><b class="sev-${esc(x.severity)}">${esc(x.severity)} · ${esc(x.title)}</b><small>${esc(x.detail)}</small><small>${esc(x.recommendation)}</small></div>`).join('')}</details></div></article>`}).join('');
  return head('INTELIGENCIA DEL SISTEMA · SOLO LECTURA','Diagnóstico de dispositivos','Análisis basado en evidencia de cada dispositivo que expone Windows. Los hallazgos nunca autorizan escrituras al hardware.')+
    stats([['ANALIZADOS',devices.length,'Inventario PnP/HID actual'],['ERRORES',s.errors,'Problemas estructurales o de identidad'],['ADVERTENCIAS',s.warnings,'Evidencia incompleta'],['ESCRITURA HARDWARE','BLOQUEADA','El diagnóstico es de solo lectura']])+
    section('ANÁLISIS POR DISPOSITIVO','Hallazgos y evidencia',`<button class="btn" data-action="scan">Volver a escanear</button>`)+
    (rows?`<div class="device-list">${rows}</div>`:empty('Sin dispositivos','Conecta un dispositivo y busca de nuevo.'))+
    notice('Cómo leer la evidencia','La puntuación es un indicador de triaje, no una probabilidad ni una garantía de compatibilidad. Un canal de reportes HID legible no revela comandos del fabricante.');
}
function pageCompatibility(){
  const exact=devices.filter(d=>d.identificationStatus==='EXACT MODEL MATCH').length,readable=devices.filter(d=>d.hidDescriptorReadable).length;
  const rows=devices.map(d=>`<article class="card device"><div class="dev-icon">◎</div><div class="dev-main"><div class="dev-title"><b>${esc(d.product||d.manufacturer||'HID desconocido')}</b><span class="badge ${d.identificationStatus==='EXACT MODEL MATCH'?'ok':''}">${esc(d.identificationStatus||'UNKNOWN')}</span></div><div class="chips"><span class="chip">VID ${esc(d.vid)}</span><span class="chip">PID ${esc(d.pid)}</span><span class="chip">Descriptor ${d.hidDescriptorReadable?'legible':'no disponible'}</span><span class="chip">Capa 1 disponible</span></div></div></article>`).join('');
  return head('COMPATIBILIDAD','Centro de compatibilidad','El soporte específico se concede por modelo exacto, revisión de hardware y protocolo documentado. Los ajustes de Windows (capa 1) valen para todos.')+
    stats([['INTERFACES',devices.length,'Inventario HID de Windows'],['MODELOS EXACTOS',exact,'Catálogo con evidencia'],['DESCRIPTORES LEGIBLES',readable,'Datos HID preparsed'],['ESCRITURA HARDWARE','DESHABILITADA','Límite de seguridad']])+
    section('INVENTARIO ACTUAL','Estado de compatibilidad',`<button class="btn" data-action="scan">Volver a escanear</button>`)+(rows?`<div class="device-list">${rows}</div>`:empty('Sin dispositivos detectados','Conecta un teclado o ratón y busca de nuevo.'))+
    notice('Qué significa “compatible”','Para activar un control específico de un dispositivo hace falta un registro de modelo verificado y un adaptador de protocolo probado. Los ajustes de Windows no dependen de eso.');
}
function pageDatabase(){
  const rows=devices.map(d=>`<article class="card device"><div class="dev-icon">▧</div><div class="dev-main"><div class="dev-title"><b>${esc(d.identifiedModel||d.identifiedManufacturer||'Desconocido / sin coincidencia')}</b><span class="badge">${esc(d.identificationStatus||'UNKNOWN')}</span></div><div class="chips"><span class="chip">VID ${esc(d.vid)}</span><span class="chip">PID ${esc(d.pid)}</span></div><small class="muted-label">${esc(d.manufacturer||'Sin cadena de fabricante')} · ${esc(d.product||'Sin cadena de producto')}</small>${(d.capabilities||[]).map(c=>`<small class="muted-label" style="display:block">Capacidad: ${esc(c)}</small>`).join('')}</div></article>`).join('');
  return head('IDENTIDAD · EVIDENCIA','Base de datos de dispositivos','Una coincidencia de fabricante no demuestra un modelo concreto ni un protocolo configurable.')+
    stats([['BASE DE DATOS',esc(databaseInfo.status),'Catálogo JSON local'],['FABRICANTES',databaseInfo.manufacturerCount,'Nombres y alias'],['MODELOS VERIFICADOS',databaseInfo.verifiedDeviceCount,'Registros VID/PID exactos'],['INTERFACES',devices.length,'Inventario actual'],['REGISTROS RECHAZADOS',databaseHealth.rejectedVerifiedRecords||0,'VERIFIED inválidos'],['CLAVES EN CONFLICTO',databaseHealth.conflictingModelKeys||0,'Identidades duplicadas']])+
    section('RESULTADOS','Identidades detectadas',`<button class="btn" data-action="scan">Actualizar ↻</button>`)+(rows?`<div class="device-list">${rows}</div>`:empty('Sin interfaces escaneadas','Conecta un dispositivo HID y busca de nuevo.'))+
    notice('Reglas de evidencia','Solo los registros VERIFIED con VID/PID coincidentes (y revisión si se especifica) identifican un modelo exacto. Los alias en conflicto se marcan como ambiguos.');
}
function pageProtocols(){
  const items=protocolResearch.items||[];
  const rows=items.map(x=>`<article class="card device"><div class="dev-icon">⌁</div><div class="dev-main"><div class="dev-title"><b>${esc(x.manufacturer)} · ${esc(x.modelFamily||'Familia pendiente')}</b><span class="badge">${esc(x.status)}</span></div><div class="chips"><span class="chip">${esc(x.transport||'Transporte desconocido')}</span><span class="chip">Evidencia: ${esc(x.evidenceLevel||'NONE')}</span></div><small class="muted-label" style="display:block;margin-top:6px">${esc(x.notes||'Sin notas')}</small>${(x.sources||[]).map(u=>/^https:\/\//i.test(String(u))?`<small style="display:block"><a href="${esc(u)}" target="_blank" rel="noreferrer noopener">${esc(u)}</a></small>`:`<small class="muted-label" style="display:block">${esc(u)}</small>`).join('')}</div></article>`).join('');
  return head('INTELIGENCIA DE PROTOCOLOS','Centro de investigación de protocolos','Espacio controlado para investigar protocolos por fabricante y modelo. Son objetivos de investigación, no promesas de compatibilidad.')+
    stats([['CATÁLOGO',esc(protocolResearch.status),'protocol_research.json'],['OBJETIVOS',protocolResearch.itemCount||items.length,'Familias fabricante/modelo'],['PROTOCOLOS VERIFICADOS',protocolResearch.verifiedCount||0,'Requieren evidencia citada'],['ESCRITURA HARDWARE','DESHABILITADA','No se envían comandos HID']])+
    section('COLA DE INVESTIGACIÓN','Objetivos por fabricante')+(rows?`<div class="device-list">${rows}</div>`:empty('Sin objetivos cargados','Revisa database/protocol_research.json.'))+
    notice('Estándar de evidencia','Para marcar un protocolo como VERIFIED hacen falta evidencia específica del modelo, alcance de revisión, fuentes fiables y pruebas reproducibles.');
}
function pagePlugins(){return head('EXTENSIONES · LÍMITE DE CONFIANZA','Gestor de plugins','Los manifiestos son solo metadatos declarativos. La carga dinámica de código está deshabilitada.')+stats([['EJECUCIÓN DE PLUGINS','DESHABILITADA','No se carga código de terceros'],['VALIDACIÓN DE FIRMA','NO IMPLEMENTADA','Sin afirmaciones de confianza']])+notice('Política de extensión segura','Los plugins futuros usarán una API firmada y versionada con permisos aislados. Esquema: plugins/manifest.schema.json.');}
function pageSettings(){
  const row=(i,t,s,x)=>`<article class="card device"><div class="dev-icon">${i}</div><div class="dev-main"><div class="dev-title"><b>${t}</b></div><small class="muted-label">${s}</small>${x||''}</div></article>`;
  return head('APLICACIÓN · AJUSTES LOCALES','Ajustes')+`<div class="device-list">${row('◉','Versión','UNIFY CONTROL 1.8.0 · Windows 10/11 x64 · C++20 · WebView2')}${row('⌁','Escaneo de dispositivos','Manual y por notificaciones de llegada/salida de Windows. Sin transferencia de valores a dispositivos.')}${row('▤','Perfiles locales',`${profiles.length} perfil(es) guardado(s) en el perfil de WebView2.`)}${row('⚙','Ajustes de Windows','Se guardan los valores originales al abrir la app; puedes volver a ellos en las pestañas Ratón y Teclado.',`<div style="margin-top:10px"><button class="btn" data-action="restore" ${inHost&&sys.originals?'':'disabled'}>Restaurar valores originales</button></div>`)}${row('⛨','Protección de escritura','Las escrituras a firmware, HID, WinUSB y GATT permanecen deshabilitadas.')}</div>`;
}

/* ---------- profiles ---------- */
function loadProfiles(){try{const v=JSON.parse(localStorage.getItem(PROFILE_STORAGE_KEY)||'[]');return Array.isArray(v)?v.filter(p=>p&&typeof p.id==='string'&&typeof p.deviceKey==='string'&&typeof p.name==='string'):[]}catch(e){return []}}
function saveProfiles(){try{localStorage.setItem(PROFILE_STORAGE_KEY,JSON.stringify(profiles));return true}catch(e){toast('No se pudieron guardar los perfiles en WebView2.',false);return false}}
function profileDeviceKey(d){if(d.containerId)return 'container:'+d.containerId;return `interface:${d.vid}:${d.pid}:${d.interfaceNumber}`;}
function profileDeviceLabel(d){return `${d.product||d.manufacturer||'HID desconocido'} · VID ${d.vid} / PID ${d.pid}`;}
function pickSettings(values){const out={};ALL_SETTING_KEYS.forEach(k=>{if(values&&Number.isInteger(Number(values[k]))&&values[k]!==null)out[k]=Number(values[k]);});return out;}
function pageProfiles(){
  const pd=devices.filter((d,i,a)=>a.findIndex(x=>profileDeviceKey(x)===profileDeviceKey(d))===i);
  const choices=`<option value="">Ninguno (solo ajustes de Windows)</option>`+pd.map((d,i)=>`<option value="${i}">${esc(profileDeviceLabel(d))}</option>`).join('');
  const rows=profiles.map(p=>{const n=Object.keys(p.settings||{}).length;const present=p.deviceKey==='global'||devices.some(d=>profileDeviceKey(d)===p.deviceKey);
    return `<article class="card device"><div class="dev-icon">▤</div><div class="dev-main"><div class="dev-title"><b>${esc(p.name)}</b><span class="badge ${present?'ok':''}">${p.deviceKey==='global'?'GLOBAL':present?'DISPOSITIVO CONECTADO':'NO CONECTADO'}</span></div><div class="chips"><span class="chip">${n} ajuste(s) de Windows</span><span class="chip">${esc(p.deviceLabel||'Sin dispositivo')}</span></div><small class="muted-label">${esc(p.notes||'Sin notas')} · Guardado ${esc(new Date(p.updatedAt).toLocaleString())}</small></div><div class="row-actions"><button class="btn primary" data-apply-profile="${esc(p.id)}" ${n&&inHost?'':'disabled'}>Aplicar</button><button class="btn danger" data-delete-profile="${esc(p.id)}">Eliminar</button></div></article>`}).join('');
  return head('PRESETS LOCALES','Perfiles','Guarda tus ajustes de Windows actuales como un perfil y vuelve a aplicarlos con un clic. Los perfiles no envían nada al firmware del dispositivo.')+
    stats([['PERFILES',profiles.length,'Guardados en WebView2'],['DISPOSITIVOS',devices.length,'Inventario actual'],['AJUSTES DISPONIBLES',sys.values?Object.keys(pickSettings(sys.values)).length:0,'Capa 1 · Windows']])+
    section('NUEVO PERFIL','Guardar configuración actual')+
    `<form id="profileForm" class="card profile-form"><label>Dispositivo asociado (opcional)<select id="profileDevice">${choices}</select></label><label>Nombre del perfil<input id="profileName" maxlength="48" required placeholder="p. ej. Competitivo"></label><label>Notas<textarea id="profileNotes" maxlength="500" rows="2" placeholder="Para qué lo usas, interruptores, etc."></textarea></label><label class="check"><input type="checkbox" id="profileInclude" ${sys.values?'checked':'disabled'}> Incluir ajustes actuales de Windows</label><button class="btn primary" type="submit">Guardar perfil</button></form>`+
    section('BIBLIOTECA','Perfiles guardados')+(rows?`<div class="device-list">${rows}</div>`:empty('Aún no hay perfiles','Ajusta tu ratón o teclado, ponle nombre y guárdalo aquí.'))+
    notice('Alcance','Aplicar un perfil cambia los ajustes de Windows (velocidad, rueda, repetición de teclas…). Los controles internos del dispositivo (DPI, RGB, macros) llegarán con los adaptadores verificados.');
}
function saveProfileFromForm(pdAll){
  const idx=$('#profileDevice').value,d=idx===''?null:pdAll[Number(idx)],name=$('#profileName').value.trim();if(!name)return;
  const key=d?profileDeviceKey(d):'global',existing=profiles.find(p=>p.deviceKey===key&&p.name.toLowerCase()===name.toLowerCase());
  const settings=$('#profileInclude').checked?pickSettings(sys.values):{};
  const rec={id:existing?existing.id:`profile-${Date.now()}-${Math.random().toString(16).slice(2,8)}`,deviceKey:key,deviceLabel:d?profileDeviceLabel(d):'',name,notes:$('#profileNotes').value.trim(),settings,hardwareWritesEnabled:false,updatedAt:new Date().toISOString()};
  profiles=existing?profiles.map(p=>p.id===existing.id?rec:p):[...profiles,rec];
  if(saveProfiles()){toast('Perfil guardado',true);render();}
}

/* ---------- routing & rendering ---------- */
const NAMES={dashboard:'Panel',devices:'Mis dispositivos',keyboard:'Teclado',mouse:'Ratón',rgb:'RGB',macros:'Macros',profiles:'Perfiles',compatibility:'Compatibilidad',diagnostics:'Diagnóstico',database:'Base de datos',protocols:'Protocolos',plugins:'Plugins',settings:'Ajustes'};
const PAGES={dashboard:pageDashboard,devices:pageDevices,keyboard:()=>pageControl('keyboard'),mouse:()=>pageControl('mouse'),rgb:()=>pageControl('rgb'),macros:()=>pageControl('macros'),profiles:pageProfiles,compatibility:pageCompatibility,diagnostics:pageDiagnostics,database:pageDatabase,protocols:pageProtocols,plugins:pagePlugins,settings:pageSettings};
function render(){
  const total=$('#navCount');if(total)total.textContent=groupInterfaces(devices).length;
  $('#content').innerHTML=(PAGES[current]||pageDashboard)();
}
function activate(page){
  current=PAGES[page]?page:'dashboard';
  document.querySelectorAll('.nav-item').forEach(b=>b.classList.toggle('active',b.dataset.page===current));
  $('#crumb').textContent=NAMES[current];render();
}
function setEngine(text,on){$('#engineStatus').textContent=text;$('#statusDot').classList.toggle('on',!!on);}

/* ---------- events ---------- */
document.querySelectorAll('.nav-item').forEach(b=>b.addEventListener('click',()=>activate(b.dataset.page)));
document.addEventListener('click',e=>{
  const t=e.target.closest('[data-action],[data-goto],[data-apply-profile],[data-delete-profile],[data-via-probe]');if(!t)return;
  if(t.dataset.viaProbe!==undefined){const i=Number(t.dataset.viaProbe);viaResults[i]={pending:true};send(`via:probe:${i}`);render();return;}
  if(t.dataset.goto)return activate(t.dataset.goto);
  if(t.dataset.action==='scan')return send('scan');
  if(t.dataset.action==='restore'){send('settings:restore');return;}
  if(t.dataset.applyProfile){const p=profiles.find(x=>x.id===t.dataset.applyProfile);if(p)Object.entries(p.settings||{}).forEach(([k,v])=>{if(ALL_SETTING_KEYS.includes(k)&&Number.isInteger(v))send(`settings:set:${k}:${v}`);});return;}
  if(t.dataset.deleteProfile){profiles=profiles.filter(p=>p.id!==t.dataset.deleteProfile);if(saveProfiles())render();}
});
document.addEventListener('input',e=>{ // live label while dragging; nothing is sent until release
  const el=e.target;if(!el.matches||!el.matches('input[type=range][data-setting]'))return;
  const def=[...SETTINGS.mouse,...SETTINGS.keyboard].find(s=>s.key===el.dataset.setting);if(!def)return;
  const v=Number(el.value),lab=document.querySelector(`[data-val-for="${el.dataset.setting}"]`);
  if(lab)lab.textContent=def.fmt(v);el.style.setProperty('--p',((v-el.min)/(el.max-el.min))*100+'%');
});
document.addEventListener('change',e=>{
  const el=e.target;if(!el.matches||!el.matches('[data-setting]'))return;
  const v=el.type==='checkbox'?(el.checked?1:0):Number(el.value);send(`settings:set:${el.dataset.setting}:${v}`);
});
document.addEventListener('submit',e=>{
  if(e.target.id!=='profileForm')return;e.preventDefault();
  saveProfileFromForm(devices.filter((d,i,a)=>a.findIndex(x=>profileDeviceKey(x)===profileDeviceKey(d))===i));
});

/* ---------- host messages ---------- */
if(inHost){
  window.chrome.webview.addEventListener('message',e=>{
    const m=e.data||{};
    if(m.type==='devices'){
      devices=m.devices||[];groups=m.groups||[];viaResults={};databaseInfo=m.database||databaseInfo;databaseHealth=m.databaseHealth||databaseHealth;protocolResearch=m.protocolResearch||protocolResearch;
      setEngine('Conectado',true);render();
    }else if(m.type==='viaProbe'){
      viaResults[m.index]=m;toast(m.ok?'El teclado responde a VIA':'No se pudo verificar VIA',!!m.ok);render();
    }else if(m.type==='systemSettings'){
      sys.values=m.settings||{};sys.originals=m.originals||{};const r=m.result||{};
      if(r.key==='*')toast(r.ok?'Valores originales restaurados':'No se pudo restaurar todo: '+(r.error||'error'),r.ok);
      else if(r.key)toast(r.ok?'Ajuste aplicado':'No se pudo aplicar ('+(r.error||'error')+')',r.ok);
      if(['mouse','keyboard','profiles','settings'].includes(current))render();
    }
  });
  setEngine('Conectando…',false);send('scan');send('settings:get');
}else{setEngine('Vista previa (sin motor nativo)',false);}
activate('dashboard');
