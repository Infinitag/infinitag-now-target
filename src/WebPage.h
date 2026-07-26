// SoftAP page of the target (design by Tobias, 2026-07-12 – derived
// from the config box template, without its WLAN/image-store sections).
//
// Served via WebUpdateService::setCustomPage(); runUpdateMode() fills
// the placeholders %DEVICE_ID% and %VERSION% before starting the AP.

#pragma once

static const char WEB_PAGE_TEMPLATE[] = R"rawpage(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Target %DEVICE_ID%</title>
<style>
body{margin:0;background:#0c0e0f;font-family:Helvetica,Arial,sans-serif;color:#e8eaea;font-size:13px}
a{color:#4db3a2;text-decoration:none}
a:hover{color:#79C8B4}
.wrap{max-width:440px;margin:0 auto;padding:16px}
.card{background:#131516;border:1px solid #23282a;border-radius:8px;overflow:hidden}
.bar{height:5px;background:linear-gradient(90deg,#79C8B4,#03817D)}
.hdr{display:flex;align-items:center;gap:12px;padding:18px 28px;border-bottom:1px solid #23282a}
.wm{font-size:15px;font-weight:700;letter-spacing:3px}
.wm b{color:#fff}
.wm i{font-style:normal;color:#939598}
.wm em{font-style:normal;color:#4db3a2}
.sub{font-size:11px;color:#8a9092;letter-spacing:1px;margin-top:2px}
.nav{display:flex;border-bottom:1px solid #23282a;padding:0 16px;flex-wrap:wrap}
.nav a{padding:10px 14px;font-size:13px;color:#8a9092}
.nav a:hover{color:#e8eaea}
.nav a.on{color:#fff;font-weight:700;border-bottom:2px solid #03817D;margin-bottom:-1px}
.bd{padding:6px 28px 28px}
.sec{border-top:2px solid #333c3f;margin-top:28px;padding-top:22px}
.sec:first-child{border:0;margin-top:0;padding-top:16px}
h2{font-size:11px;font-weight:700;letter-spacing:2px;color:#4db3a2;margin:0 0 10px}
p{color:#8a9092;margin:0 0 10px;line-height:1.5}
.hint{font-size:12px;color:#6e7578;line-height:1.5;margin-top:10px}
.hint b,p b{color:#a9b0b2}
.col{display:flex;flex-direction:column;gap:10px}
.file{display:flex;align-items:center;gap:12px;background:#16191b;border:1px dashed #414a4d;border-radius:6px;padding:10px;cursor:pointer}
.file span{background:#24292b;color:#d7dbdb;border:1px solid #3a4144;border-radius:4px;padding:6px 14px;font-weight:700;white-space:nowrap}
.file i{font-style:normal;color:#6e7578;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.file input{display:none}
.file.drag{border-color:#79C8B4;background:#1b2422}
input[type=text],input[type=password]{background:#131516;border:1px solid #3a4144;border-radius:6px;color:#e8eaea;padding:9px 12px;font-size:13px}
button{align-self:flex-start;background:transparent;color:#79C8B4;border:1px solid #0d8c80;border-radius:4px;padding:8px 18px;font-size:13px;font-weight:700;cursor:pointer}
button:hover{background:#0d8c80;color:#fff}
.ft{font-size:11px;color:#565c5e;text-align:center;letter-spacing:1px;border-top:1px solid #24282a;margin-top:18px;padding-top:14px}
.ok{color:#79C8B4;font-weight:700}
</style>
</head>
<body>
<div class="wrap">
<div class="card">
<div class="bar"></div>
<div class="hdr" style="border:0">
<svg width="46" height="24" viewBox="0 0 64 32" fill="none"><defs><linearGradient id="g" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#79C8B4"/><stop offset="1" stop-color="#03817D"/></linearGradient></defs><polygon points="32,16 25,3.9 11,3.9 4,16 11,28.1 25,28.1" stroke="url(#g)" stroke-width="4"/><polygon points="60,16 53,3.9 39,3.9 32,16 39,28.1 53,28.1" stroke="url(#g)" stroke-width="4"/></svg>
<div>
<div class="wm"><i>INFINI</i><b>TAG</b> <em>NOW</em></div>
<div class="sub">TARGET &middot; %DEVICE_ID%</div>
</div>
</div>
<div class="nav"><a class="on" href="/">Update</a></div>
<div class="bd">

<form class="sec" method="POST" action="/update" enctype="multipart/form-data">
<h2>FIRMWARE-UPDATE</h2>
<p>Laufende Version: <span class="ok">%VERSION%</span> &middot; <a href="https://github.com/Infinitag/infinitag-now-target/releases" target="_blank">Neueste Version auf GitHub&nbsp;&#8599;</a></p>
<div class="col">
<label class="file"><span>Datei ausw&auml;hlen</span><i>Keine Datei ausgew&auml;hlt</i><input type="file" name="fw" accept=".bin"></label>
<button type="submit">Update starten</button>
</div>
<div class="hint">Erwartete Datei: <b>infinitag-target-vX.Y.Z.bin</b> &mdash; andere Dateinamen werden abgelehnt. Nach dem Upload startet das Ger&auml;t neu.</div>
</form>



<div class="ft">INFINITAG NOW &middot; TARGET &middot; <a href="https://github.com/Infinitag" target="_blank">GITHUB&nbsp;&#8599;</a></div>
</div>
</div>
</div>
<script>
document.querySelectorAll('label.file').forEach(function(l){
var f=l.querySelector('input');
function nm(){l.querySelector('i').textContent=f.files.length?f.files[0].name:'Keine Datei ausgew\u00e4hlt';}
f.addEventListener('change',nm);
var z=l.closest('form')||l;
['dragover','dragenter'].forEach(function(ev){z.addEventListener(ev,function(e){e.preventDefault();l.classList.add('drag');});});
z.addEventListener('dragleave',function(){l.classList.remove('drag');});
z.addEventListener('drop',function(e){e.preventDefault();l.classList.remove('drag');if(e.dataTransfer.files.length){f.files=e.dataTransfer.files;nm();}});
});
</script>
</body>
</html>
)rawpage";
