const API_BASE = "http://localhost:8080/api";
const state = { services: [], incidents: [], dependencies: [], metrics: {}, activePage: "overview", connected: false, refreshing: false };
const $ = (id) => document.getElementById(id);
const esc = (value) => String(value ?? "").replace(/[&<>"']/g, c => ({ "&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;" }[c]));
const num = (value, digits=0) => Number.isFinite(Number(value)) ? Number(value).toLocaleString(undefined,{maximumFractionDigits:digits,minimumFractionDigits:digits}) : "—";
const statusClass = (s) => "status-" + String(s || "unknown").toLowerCase().replaceAll("_","-");
const severityClass = (s) => "severity-" + String(s || "low").toLowerCase();
const isOpen = (i) => ["OPEN","RECOVERING","RECOVERY_FAILED"].includes(i.status);
const serviceGlyph = (name) => ({ "API Gateway":"AG","Auth":"AU","Order":"OR","Payment":"PY","Inventory":"IN","Database":"DB","Cache":"CA" }[name] || String(name).slice(0,2).toUpperCase());
const serviceByName = (name) => state.services.find(s => s.name === name);
const fmtTime = (value) => { if(!value) return "—"; const d = new Date(value); return Number.isNaN(d.getTime()) ? value : d.toLocaleString([], {month:"short",day:"2-digit",hour:"2-digit",minute:"2-digit"}); };
const fmtAgo = (value) => { if(!value) return "—"; const delta = Math.max(0,Date.now()-new Date(value).getTime()); if(!Number.isFinite(delta)) return fmtTime(value); if(delta<60000) return "just now"; if(delta<3600000) return Math.floor(delta/60000)+"m ago"; if(delta<86400000) return Math.floor(delta/3600000)+"h ago"; return Math.floor(delta/86400000)+"d ago"; };
async function api(path, options={}) {
  const controller = new AbortController();
  const timeout = setTimeout(()=>controller.abort(), 6500);
  try {
    const response = await fetch(API_BASE+path,{...options,signal:controller.signal,headers:{"Content-Type":"application/json",...(options.headers||{})}});
    const text = await response.text();
    let data; try { data = text ? JSON.parse(text) : {}; } catch { throw new Error("The API returned an invalid response."); }
    if(!response.ok) throw new Error(data.error || `Request failed (${response.status})`);
    if(data && data.error) throw new Error(data.error);
    return data;
  } catch(error) {
    if(error.name === "AbortError") throw new Error("API request timed out. Is ResiliX running?");
    if(error instanceof TypeError) throw new Error("Cannot reach the API. Start ResiliX.exe api and check localhost:8080.");
    throw error;
  } finally { clearTimeout(timeout); }
}
function toast(message,type="info") {
  const node=document.createElement("div"); node.className=`toast ${type}`; node.textContent=message; $("toastRegion").appendChild(node);
  setTimeout(()=>{node.style.opacity="0";node.style.transform="translateY(4px)";setTimeout(()=>node.remove(),200)},4000);
}
function setConnection(ok,message="") {
  state.connected=ok; const pill=$("connectionPill"); pill.classList.toggle("connected",ok);pill.classList.toggle("error",!ok);
  $("connectionText").textContent=ok?"API connected":(message||"API unavailable"); $("footerApi").textContent=ok?"API: localhost:8080":"API: disconnected";
}
function statusBadge(status) { return `<span class="status-badge ${statusClass(status)}">${esc(status||"UNKNOWN")}</span>`; }
function severityBadge(severity) { return `<span class="severity-badge ${severityClass(severity)}">${esc(severity||"LOW")}</span>`; }
function serviceRows(services) {
  if(!services.length) return `<tr><td colspan="5" class="empty-table">No services match this filter.</td></tr>`;
  return services.map(s=>`<tr><td><div class="service-name-cell"><span class="service-glyph">${esc(serviceGlyph(s.name))}</span>${esc(s.name)}</div></td><td>${statusBadge(s.status)}</td><td><span class="metric-cell">${num(s.latencyMs,1)} ms</span></td><td><span class="metric-cell">${num(s.errorRatePct,2)}%</span></td><td><span class="metric-cell">${num(s.cpuPct,0)}%</span></td></tr>`).join("");
}
function renderOverview() {
  const services=state.services, healthy=services.filter(s=>s.status==="HEALTHY").length, open=state.incidents.filter(isOpen).length;
  $("statServices").textContent=services.length||"—"; $("statHealthy").textContent=healthy;
  $("healthyPercent").textContent=services.length?`${Math.round(healthy/services.length*100)}% of fleet`:"—";
  $("statOpen").textContent=open; $("openSub").textContent=open?"Requires triage":"No open incidents";
  $("statRecovery").innerHTML=`${num(state.metrics.meanRecoveryMs)}<small> ms</small>`;
  $("navServiceCount").textContent=services.length; $("navIncidentCount").textContent=open;
  $("serviceTableBody").innerHTML=serviceRows(services.slice(0,6));
  $("serviceTableFooter").textContent=`Showing ${Math.min(services.length,6)} of ${services.length} services`;
  const recent=[...state.incidents].sort((a,b)=>String(b.createdAt||"").localeCompare(String(a.createdAt||""))).slice(0,4);
  $("recentIncidents").innerHTML=recent.length?recent.map(i=>`<div class="incident-item"><span class="incident-icon ${i.status==="RESOLVED"?"resolved":""}">${i.status==="RESOLVED"?"✓":"!"}</span><div class="incident-copy"><strong>${esc(i.service)} · ${esc(i.description||"Service incident")}</strong><small>${esc(i.status)} · ${esc(i.severity)} · ${fmtAgo(i.createdAt)}</small></div><span class="incident-time">#${esc(i.id)}</span></div>`).join(""):`<div class="empty-state compact"><span class="empty-icon">✓</span><strong>No incidents recorded</strong><p>Inject a controlled failure to see impact and recovery activity here.</p></div>`;
  renderTopologyPreview(); renderServiceCards(); renderIncidentTable(); renderDependencies(); renderRecoveryControls(); renderMetrics();
}
function renderTopologyPreview() {
  const path=["Database","Payment","Order","API Gateway"];
  $("topologyPreview").innerHTML=path.map((n,i)=>`${i?'<span class="topology-arrow">→</span>':""}<div class="topology-node ${i===0?"core-node":""}">${esc(n)}<small>${i===0?"UPSTREAM":i===3?"EDGE":"DEPENDENT"}</small></div>`).join("");
}
function renderServiceCards() {
  const q=($("serviceSearch")?.value||"").toLowerCase(), filter=$("serviceStatusFilter")?.value||"all";
  const list=state.services.filter(s=>(s.name||"").toLowerCase().includes(q)&&(filter==="all"||s.status===filter));
  if($("serviceFilterCount")) $("serviceFilterCount").textContent=`${list.length} service${list.length===1?"":"s"}`;
  $("serviceCards").innerHTML=list.length?list.map(s=>{
    const cpu=Math.max(0,Math.min(100,Number(s.cpuPct)||0)),mem=Math.max(0,Math.min(100,Number(s.memoryPct)||0));
    const barClass=v=>v>=90?"danger":v>=75?"warning":"";
    return `<article class="service-card"><div class="service-card-top"><div class="service-card-name"><span class="service-glyph">${esc(serviceGlyph(s.name))}</span>${esc(s.name)}</div>${statusBadge(s.status)}</div><p>Service health snapshot from the current simulator state.</p><div class="service-card-stats"><div class="mini-metric"><small>LATENCY</small><strong>${num(s.latencyMs,1)} ms</strong></div><div class="mini-metric"><small>ERROR RATE</small><strong>${num(s.errorRatePct,2)}%</strong></div></div><div class="resource-row"><span class="resource-label">CPU</span><div class="progress-track"><div class="progress-fill ${barClass(cpu)}" style="width:${cpu}%"></div></div><span class="resource-value">${num(cpu)}%</span></div><div class="resource-row"><span class="resource-label">Memory</span><div class="progress-track"><div class="progress-fill ${barClass(mem)}" style="width:${mem}%"></div></div><span class="resource-value">${num(mem)}%</span></div><div class="card-actions"><button class="btn-mini" data-fail="${esc(s.name)}">Inject failure</button><button class="btn-mini" data-metrics="${esc(s.name)}">Update metrics</button><button class="btn-mini" data-recover="${esc(s.name)}">Recover</button></div></article>`;
  }).join(""):`<div class="panel loading-panel">No services match the selected filters.</div>`;
}
function renderIncidentTable() {
  const sf=$("incidentStatusFilter")?.value||"all", vf=$("incidentSeverityFilter")?.value||"all";
  const list=[...state.incidents].sort((a,b)=>Number(b.riskScore||0)-Number(a.riskScore||0)).filter(i=>(sf==="all"||i.status===sf)&&(vf==="all"||i.severity===vf));
  $("incTotal").textContent=state.incidents.length; $("incOpen").textContent=state.incidents.filter(isOpen).length;
  $("incResolved").textContent=state.incidents.filter(i=>i.status==="RESOLVED").length;
  $("incCritical").textContent=state.incidents.filter(i=>i.severity==="CRITICAL").length;
  $("incidentTableBody").innerHTML=list.length?list.map(i=>{
    const risk=Math.max(0,Math.min(100,Number(i.riskScore)||0)),impact=(i.directImpact?.length||0)+(i.indirectImpact?.length||0);
    const fill=risk>=80?"critical":risk>=60?"high":"";
    const action=isOpen(i)?`<button class="btn-mini" data-recover="${esc(i.service)}">Recover</button>`:`<span class="footer-muted">—</span>`;
    return `<tr><td><div class="service-name-cell"><span class="service-glyph">${esc(serviceGlyph(i.service))}</span><span><strong>#${esc(i.id)} ${esc(i.service)}</strong><small style="display:block;color:#9aa4b3;font-weight:400;margin-top:4px">${esc(i.description||"Incident")}</small></span></div></td><td>${severityBadge(i.severity)}</td><td class="risk-cell"><div class="risk-label"><span>${num(risk)}</span><span>/ 100</span></div><div class="risk-track"><div class="risk-fill ${fill}" style="width:${risk}%"></div></div></td><td><span class="impact-count">${impact} services</span></td><td>${statusBadge(i.status)}</td><td><span class="metric-cell">${fmtTime(i.createdAt)}</span></td><td>${action}</td></tr>`;
  }).join(""):`<tr><td colspan="7" class="empty-table">No incidents match these filters.</td></tr>`;
  $("recoveryIncidentBody").innerHTML=state.incidents.filter(isOpen).length?state.incidents.filter(isOpen).map(i=>`<tr><td>${esc(i.service)}</td><td>${severityBadge(i.severity)}</td><td>${statusBadge(i.status)}</td><td>${esc(i.strategy||"—")}</td><td><button class="btn-mini" data-recover="${esc(i.service)}">Run recovery →</button></td></tr>`).join(""):`<tr><td colspan="5" class="empty-table">No open incidents require recovery.</td></tr>`;
}
function renderDependencies() {
  const edges=state.dependencies; $("graphSummary").textContent=`${edges.length} configured dependency relationships`;
  $("dependencyTableBody").innerHTML=edges.length?edges.map(e=>`<tr><td><div class="service-name-cell"><span class="service-glyph">${esc(serviceGlyph(e.upstream))}</span>${esc(e.upstream)}</div></td><td>→</td><td><div class="service-name-cell"><span class="service-glyph">${esc(serviceGlyph(e.dependent))}</span>${esc(e.dependent)}</div></td></tr>`).join(""):`<tr><td colspan="3" class="empty-table">No dependency relationships returned by the API.</td></tr>`;
  // Layer graph based on upstream-to-dependent edges; cycle-safe and readable without a chart library.
  const names=[...new Set([...state.services.map(s=>s.name),...edges.flatMap(e=>[e.upstream,e.dependent])])];
  const indegree=Object.fromEntries(names.map(n=>[n,0])), outgoing=Object.fromEntries(names.map(n=>[n,[]]));
  edges.forEach(e=>{if(outgoing[e.upstream]&&!outgoing[e.upstream].includes(e.dependent)){outgoing[e.upstream].push(e.dependent);indegree[e.dependent]=(indegree[e.dependent]||0)+1;}});
  const layers={},queue=names.filter(n=>indegree[n]===0).map(n=>[n,0]); let seen=new Set();
  while(queue.length){const [n,l]=queue.shift();if(seen.has(n))continue;seen.add(n);layers[n]=Math.max(layers[n]||0,l);for(const d of outgoing[n]||[]){indegree[d]--;queue.push([d,l+1]);}}
  names.filter(n=>!seen.has(n)).forEach(n=>layers[n]=0);
  const maxLayer=Math.max(0,...Object.values(layers));const cols=Array.from({length:maxLayer+1},()=>[]);
  names.forEach(n=>cols[Math.min(maxLayer,layers[n]||0)].push(n));
  const labels=["UPSTREAM","DEPENDENCY LAYER 02","DEPENDENCY LAYER 03","EDGE / CONSUMER"];
  $("dependencyGraph").innerHTML=`<div class="graph-columns">${cols.map((col,i)=>`<div class="graph-column"><div class="graph-level-label">${esc(labels[i]||`LAYER ${String(i+1).padStart(2,"0")}`)}</div>${col.map(n=>{const s=serviceByName(n),status=s?.status||"UNKNOWN";const dot=status==="FAILED"||status==="OPEN_CIRCUIT"?"failed-dot":status==="DEGRADED"?"degraded-dot":"";return `<div class="graph-node"><div class="node-name">${esc(n)}</div><div class="node-status"><span class="${dot}"></span>${esc(status)}</div></div>`;}).join("")}</div>`).join("")}</div><div class="graph-caption">Layer placement is inferred from the configured dependency edges; the graph is a topology view, not a live traffic map.</div>`;
}
function renderRecoveryControls() {
  const names=state.services.map(s=>s.name);
  for(const id of ["recoveryService","failureService"]) {
    const el=$(id);if(!el)continue;const old=el.value;
    el.innerHTML=`<option value="">Select a service…</option>`+names.map(n=>`<option value="${esc(n)}">${esc(n)}</option>`).join("");
    if(names.includes(old))el.value=old;
  }
}
function renderMetrics() {
  const m=state.metrics||{};
  $("metricSamples").textContent=num(m.samples); $("metricAvailability").innerHTML=`${num(m.availabilityPct,2)}<small>%</small>`;
  $("metricPrecision").innerHTML=`${num(Number(m.detectionPrecision||0)*100,1)}<small>%</small>`;
  $("metricRecall").innerHTML=`${num(Number(m.detectionRecall||0)*100,1)}<small>%</small>`;
  const signals=state.services;
  $("signalList").innerHTML=signals.length?signals.map(s=>{
    const items=[["Latency",Number(s.latencyMs)||0,500,"ms"],["Error rate",Number(s.errorRatePct)||0,5,"%"],["CPU",Number(s.cpuPct)||0,100,"%"],["Memory",Number(s.memoryPct)||0,100,"%"]];
    const item=items.reduce((a,b)=>a[1]/a[2]>b[1]/b[2]?a:b);
    const ratio=Math.min(100,item[1]/item[2]*100),cls=ratio>=100?"danger":ratio>=75?"warn":"";
    return `<div class="signal-row"><span class="signal-name">${esc(s.name)}</span><div class="signal-bar"><small>${esc(item[0])} pressure</small><div class="signal-track"><div class="signal-fill ${cls}" style="width:${ratio}%"></div></div></div><span class="signal-number">${num(item[1],1)}${item[3]}</span></div>`;
  }).join(""):`<div class="empty-table">No service metrics available.</div>`;
  $("outcomeSummary").innerHTML=`<div class="outcome-row"><span>Total incidents</span><strong>${num(m.incidents)}</strong></div><div class="outcome-row"><span>Resolved incidents</span><strong>${num(m.resolved)}</strong></div><div class="outcome-row"><span>Failed recoveries</span><strong>${num(m.failedRecoveries)}</strong></div><div class="outcome-row"><span>Mean recovery time</span><strong>${num(m.meanRecoveryMs)} ms</strong></div><div class="outcome-row"><span>TP / FP / TN / FN</span><strong>${num(m.tp)} / ${num(m.fp)} / ${num(m.tn)} / ${num(m.fn)}</strong></div>`;
}
async function refreshAll(showToast=false) {
  if(state.refreshing)return;state.refreshing=true;$("refreshButton").style.transform="rotate(90deg)";
  try {
    const [health,services,deps,incidents,metrics]=await Promise.all([
      api("/health"),api("/services"),api("/dependencies"),api("/incidents"),api("/metrics")
    ]);
    state.services=Array.isArray(services.services)?services.services:[];
    state.dependencies=Array.isArray(deps.edges)?deps.edges:[];
    state.incidents=Array.isArray(incidents.incidents)?incidents.incidents:[];
    state.metrics=metrics||{};
    setConnection(true); $("lastSync").textContent=new Date().toLocaleTimeString([],{hour:"2-digit",minute:"2-digit",second:"2-digit"});
    renderOverview(); if(showToast)toast("Dashboard data refreshed.","success");
  } catch(error) {
    setConnection(false,error.message);
    if(!state.services.length) {
      $("serviceTableBody").innerHTML=`<tr><td colspan="5" class="empty-table">${esc(error.message)}</td></tr>`;
      $("recentIncidents").innerHTML=`<div class="empty-state compact"><span class="empty-icon" style="background:#fff0f0;color:#d9535d">!</span><strong>API connection unavailable</strong><p>Start ResiliX.exe api, then refresh this dashboard.</p></div>`;
    }
    if(showToast)toast(error.message,"error");
  } finally { state.refreshing=false;$("refreshButton").style.transform=""; }
}
function navigate(page) {
  const section=$(page); if(!section)return;
  state.activePage=page;
  document.querySelectorAll(".page-section").forEach(s=>s.classList.toggle("active-page",s.id===page));
  document.querySelectorAll(".nav-link").forEach(a=>a.classList.toggle("active",a.dataset.page===page));
  const title=({overview:"Overview",services:"Services",incidents:"Incidents",dependencies:"Dependencies",recovery:"Recovery center",metrics:"Metrics & reports"})[page]||"Overview";
  $("breadcrumbPage").textContent=title;document.title=`ResiliX | ${title}`;
  $("sidebar").classList.remove("open");
  if(page==="services")renderServiceCards();if(page==="incidents")renderIncidentTable();if(page==="dependencies")renderDependencies();if(page==="recovery")renderIncidentTable();if(page==="metrics")renderMetrics();
}
function openFailureModal(service="") {
  renderRecoveryControls(); $("failureService").value=service; $("modalBackdrop").hidden=false; $("failureDescription").focus();
}
function closeFailureModal(){ $("modalBackdrop").hidden=true; }
async function injectFailure(service,description) {
  const result=await api("/failure",{method:"POST",body:JSON.stringify({service,description})});
  if(result.ok===false||result.error)throw new Error(result.error||"Failure injection failed");
  toast(`Failure injected for ${service}. Risk score: ${result.riskScore ?? "—"}.`,"success");await refreshAll();navigate("incidents");
}
function openRecovery(service="") {
  renderRecoveryControls();if(service)$("recoveryService").value=service;navigate("recovery");
  $("recoveryForm").scrollIntoView({behavior:"smooth",block:"center"});
}
async function runRecovery(service,strategy) {
  const result=await api("/recovery",{method:"POST",body:JSON.stringify({service,strategy})});
  $("recoveryResult").hidden=false;$("recoveryResultBody").textContent=JSON.stringify(result,null,2);
  if(result.ok)toast(`Recovery simulation completed for ${service}.`,"success");else toast(result.message||"Recovery did not complete.","error");
  await refreshAll();navigate("recovery");
}
async function evaluateHealth() {
  const result=await api("/evaluate",{method:"POST",body:"{}"});
  await refreshAll();toast(`Health evaluation completed for ${result.evaluated?.length||0} services.`,"success");
}
function openMetricsEditor(service) {
  const s=serviceByName(service);if(!s)return;
  const latency=prompt(`New latency in ms for ${service}`,s.latencyMs);if(latency===null)return;
  const error=prompt("New error rate (%)",s.errorRatePct);if(error===null)return;
  const cpu=prompt("New CPU usage (%)",s.cpuPct);if(cpu===null)return;
  const memory=prompt("New memory usage (%)",s.memoryPct);if(memory===null)return;
  const values=[latency,error,cpu,memory].map(Number);
  if(values.some(v=>!Number.isFinite(v))||values[0]<0||values.slice(1).some(v=>v<0||v>100)){toast("Enter valid metrics: latency ≥ 0 and percentages between 0 and 100.","error");return;}
  api("/metrics",{method:"POST",body:JSON.stringify({service,latencyMs:values[0],errorRatePct:values[1],cpuPct:values[2],memoryPct:values[3]})}).then(async r=>{toast(`${service} metrics updated (${r.status}).`,"success");await refreshAll();navigate("services")}).catch(e=>toast(e.message,"error"));
}
function updateStrategyExplainer() {
  const map={
    restart:["Restart simulation","Models a bounded restart attempt and checks the resulting service health."],
    retry:["Retry with backoff","Models retry attempts with configured exponential backoff and a maximum delay."],
    failover:["Failover simulation","Models recovery through an alternate target. No real traffic is moved."],
    fallback:["Fallback simulation","Models a fallback path while preserving the simulation-only boundary."]
  };
  const item=map[$("recoveryStrategy").value]||map.restart;
  $("strategyExplainer").innerHTML=`<strong>${item[0]}</strong><p>${item[1]}</p>`;
}
document.addEventListener("click",e=>{
  const nav=e.target.closest("[data-page]");if(nav){e.preventDefault();navigate(nav.dataset.page);history.replaceState(null,"",`#${nav.dataset.page}`);return;}
  const fail=e.target.closest("[data-fail]");if(fail){openFailureModal(fail.dataset.fail);return;}
  const recover=e.target.closest("[data-recover]");if(recover){openRecovery(recover.dataset.recover);return;}
  const metric=e.target.closest("[data-metrics]");if(metric){openMetricsEditor(metric.dataset.metrics);return;}
});
document.querySelectorAll(".nav-link").forEach(a=>a.addEventListener("click",()=>navigate(a.dataset.page)));
$("refreshButton").addEventListener("click",()=>refreshAll(true));
$("serviceRefresh").addEventListener("click",()=>refreshAll(true));
$("dependencyRefresh").addEventListener("click",()=>refreshAll(true));
$("metricsRefresh").addEventListener("click",()=>refreshAll(true));
$("evaluateButton").addEventListener("click",()=>evaluateHealth().catch(e=>toast(e.message,"error")));
$("quickEvaluate").addEventListener("click",()=>evaluateHealth().catch(e=>toast(e.message,"error")));
$("quickFailure").addEventListener("click",()=>openFailureModal());
$("quickRecovery").addEventListener("click",()=>openRecovery());
$("newIncidentButton").addEventListener("click",()=>openFailureModal());
$("dismissNotice").addEventListener("click",()=>document.querySelector(".notice-bar").remove());
$("mobileMenu").addEventListener("click",()=>$("sidebar").classList.toggle("open"));
$("settingsButton").addEventListener("click",()=>toast("API endpoint: "+API_BASE,"info"));
$("modalClose").addEventListener("click",closeFailureModal);$("modalCancel").addEventListener("click",closeFailureModal);
$("modalBackdrop").addEventListener("click",e=>{if(e.target===$("modalBackdrop"))closeFailureModal()});
document.addEventListener("keydown",e=>{if(e.key==="Escape")closeFailureModal()});
$("failureForm").addEventListener("submit",async e=>{e.preventDefault();const service=$("failureService").value;if(!service){toast("Choose a target service.","error");return;}const btn=e.submitter;btn.disabled=true;btn.textContent="Injecting…";try{await injectFailure(service,$("failureDescription").value.trim()||"Manual failure injection");closeFailureModal();}catch(err){toast(err.message,"error");}finally{btn.disabled=false;btn.textContent="Inject failure";}});
$("recoveryForm").addEventListener("submit",async e=>{e.preventDefault();const service=$("recoveryService").value,strategy=$("recoveryStrategy").value;if(!service){toast("Choose a target service.","error");return;}const matchingIncident=state.incidents.some(i=>i.service===service&&isOpen(i));if(!matchingIncident){toast(`No open incident for ${service}. Inject a failure first, then run recovery.`,"info");return;}const btn=e.submitter;btn.disabled=true;btn.textContent="Running…";try{await runRecovery(service,strategy);}catch(err){toast(err.message,"error");}finally{btn.disabled=false;btn.innerHTML='Run recovery <span>→</span>';}});
$("recoveryStrategy").addEventListener("change",updateStrategyExplainer);
$("serviceSearch").addEventListener("input",renderServiceCards);$("serviceStatusFilter").addEventListener("change",renderServiceCards);
$("incidentStatusFilter").addEventListener("change",renderIncidentTable);$("incidentSeverityFilter").addEventListener("change",renderIncidentTable);
$("runMetricEvaluation").addEventListener("click",async e=>{const btn=e.currentTarget;btn.disabled=true;btn.textContent="Evaluating…";try{const result=await api("/evaluate",{method:"POST",body:"{}"});await refreshAll();const count=Array.isArray(result.evaluated)?result.evaluated.length:state.services.length;toast(`Health evaluation completed for ${count} service(s).`,"success");}catch(err){toast(err.message,"error");}finally{btn.disabled=false;btn.textContent="Evaluate all services";}});
$("detectionEvaluationForm").addEventListener("submit",async e=>{e.preventDefault();const btn=$("recordDetectionEvaluation");const expectedFailure=$("expectedFailureInput").value==="true";const detectedFailure=$("detectedFailureInput").value==="true";btn.disabled=true;btn.textContent="Recording…";try{const result=await api("/evaluation",{method:"POST",body:JSON.stringify({expectedFailure,detectedFailure})});if(result.ok===false)throw new Error(result.error||"Could not record evaluation sample");await refreshAll();const outcome=expectedFailure?(detectedFailure?"TP":"FN"):(detectedFailure?"FP":"TN");toast(`Recorded ${outcome} sample. Detection counters updated.`,"success");}catch(err){toast(err.message,"error");}finally{btn.disabled=false;btn.textContent="Record evaluation sample";}});
$("clock").textContent="--:--:--";setInterval(()=>$("clock").textContent=new Date().toLocaleTimeString([],{hour12:false}),1000);
updateStrategyExplainer();
const initial=location.hash.replace("#","");if(initial&&$(initial))navigate(initial);
refreshAll();
setInterval(()=>{if(!document.hidden)refreshAll(false)},15000);
