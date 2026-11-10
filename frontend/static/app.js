const $ = id => document.getElementById(id);
let toastTimer;
function toast(msg){const t=$('toast');t.textContent=msg;t.classList.add('show');clearTimeout(toastTimer);toastTimer=setTimeout(()=>t.classList.remove('show'),2500)}
async function api(path, method='POST'){
  try{const r=await fetch(path,{method});const d=await r.json();if(!r.ok||d.ok===false)throw new Error(d.message||'Request failed');if(d.message)toast(d.message);return d}
  catch(e){toast(e.message);return null}
}
function setStatus(s){
  $('side-status').textContent=s.running?(s.paused?'Paused':'Running'):'Offline';
  $('connection-label').textContent=s.running?(s.paused?'PAUSED':'ENGINE LIVE'):'READY';
  $('readerOps').textContent=s.reader_ops;$('writerOps').textContent=s.writer_ops;
  $('activeReaders').textContent=s.readers;$('readerCount').textContent=s.readers;
  $('sharedValue').textContent=s.value;$('readerThreads').textContent=s.reader_threads;
  $('writerThreads').textContent=s.writer_threads;
  $('lockState').textContent=s.readers>0?'READ ACCESS':(s.running?'AVAILABLE':'OFFLINE');
  $('semLock').textContent=s.readers>0?'◉':'♙';
}
async function refresh(){try{const r=await fetch('/api/status');setStatus(await r.json())}catch(e){}}
function addEvent(e){
  if(e.type==='connected')return;
  const list=$('activityList');const empty=list.querySelector('.empty-state');if(empty)empty.remove();
  const row=document.createElement('div');row.className='event-row';
  const role=e.role==='R'?'Reader':e.role==='W'?'Writer':'System';
  const chip=e.role==='R'?'R':e.role==='W'?'W':'•';
  const label=(e.type||'event').replaceAll('_',' ');
  const status=['read_start','write_start'].includes(e.type)?'IN PROGRESS':(['read_end','write_end','finished'].includes(e.type)?'COMPLETED':'INFO');
  row.innerHTML=`<span class="event-type"></span><span><i class="role-chip ${e.role==='W'?'writer':''}">${chip}</i> <b></b></span><span class="detail"></span><span class="event-state"></span>`;
  row.children[0].textContent=label;row.children[1].querySelector('b').textContent=e.id?`${role} #${e.id}`:role;
  row.querySelector('.detail').textContent=e.message||'';row.querySelector('.event-state').textContent=status;
  list.prepend(row);while(list.children.length>80)list.lastChild.remove();
  refresh();
}
$('startBtn').onclick=async()=>{const d=await api('/api/start');if(d){await api('/api/add/reader');await api('/api/add/writer');refresh()}};
$('stopBtn').onclick=async()=>{await api('/api/stop');refresh()};
$('addReader').onclick=async()=>{await api('/api/add/reader');refresh()};
$('addWriter').onclick=async()=>{await api('/api/add/writer');refresh()};
$('pauseBtn').onclick=async()=>{await api('/api/pause');refresh()};
$('resumeBtn').onclick=async()=>{await api('/api/resume');refresh()};
$('resetBtn').onclick=async()=>{const d=await api('/api/reset');if(d){$('activityList').innerHTML='<div class="empty-state">Dashboard reset. Start the simulation to generate new events.</div>';refresh()}};
document.querySelectorAll('.nav-item').forEach(a=>a.addEventListener('click',()=>{document.querySelectorAll('.nav-item').forEach(x=>x.classList.remove('active'));a.classList.add('active')}));
const stream=new EventSource('/api/events');stream.onmessage=e=>{try{addEvent(JSON.parse(e.data))}catch(err){}};
stream.onerror=()=>{$('connection-label').textContent='RECONNECTING'};
refresh();setInterval(refresh,1800);
