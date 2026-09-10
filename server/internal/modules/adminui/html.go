package adminui

// adminHTML 简易 ACL 控制后台（内嵌）。
// CUSTOMIZE: 可换成独立前端。
const adminHTML = `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8"/>
<title>tray_demo Admin</title>
<style>
  body{font-family:Segoe UI,system-ui,sans-serif;margin:24px;max-width:960px;background:#f6f7f9;color:#222}
  h1{font-size:1.4rem;margin:0 0 8px}
  .card{background:#fff;border:1px solid #e2e4e8;border-radius:8px;padding:16px;margin:12px 0}
  input,textarea,button{font:inherit}
  input{width:100%;padding:8px;margin:4px 0 12px;box-sizing:border-box}
  textarea{width:100%;height:360px;padding:10px;box-sizing:border-box;font-family:Consolas,monospace;font-size:13px}
  button{padding:8px 14px;margin-right:8px;cursor:pointer}
  .muted{color:#666;font-size:.9rem}
  .err{color:#b00020} .ok{color:#0a7a32}
  code{background:#eee;padding:1px 4px;border-radius:3px}
</style>
</head>
<body>
  <h1>tray_demo API 权限后台</h1>
  <p class="muted">用 <code>admin/admin</code> 登录后编辑 <code>acl.yaml</code>。配置优先；此处写回文件。</p>

  <div class="card" id="loginBox">
    <label>用户名</label>
    <input id="user" value="admin"/>
    <label>密码</label>
    <input id="pass" type="password" value="admin"/>
    <button onclick="doLogin()">登录</button>
    <span id="loginMsg" class="err"></span>
  </div>

  <div class="card" id="editorBox" style="display:none">
    <p>当前用户：<strong id="who"></strong>　<button onclick="reloadAcl()">重新加载</button>
      <button onclick="saveAcl()">保存 ACL</button>
      <span id="saveMsg"></span></p>
    <textarea id="aclJson"></textarea>
    <p class="muted">编辑 JSON（与 acl.yaml 结构一致：version + routes[]）。保存后立即生效。</p>
  </div>

  <div class="card" id="extBox" style="display:none">
    <h2 style="font-size:1.1rem;margin:0 0 8px">业务扩展</h2>
    <p class="muted">由各 module 经 <code>Deps.Admin.RegisterNav</code> 注册；ACL 编辑仍属框架能力。</p>
    <ul id="extList"></ul>
    <pre id="extOut" style="background:#f0f1f3;padding:10px;overflow:auto;max-height:240px"></pre>
  </div>

<script>
let token = localStorage.getItem('tray_demo_admin_token') || '';
async function doLogin(){
  const username = document.getElementById('user').value;
  const password = document.getElementById('pass').value;
  const r = await fetch('/api/v1/login',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({username,password})});
  const j = await r.json();
  if(!r.ok){ document.getElementById('loginMsg').textContent = j.error||'登录失败'; return; }
  if(!(j.roles||[]).includes('admin')){
    document.getElementById('loginMsg').textContent = '需要 admin 角色';
    return;
  }
  token = j.token; localStorage.setItem('tray_demo_admin_token', token);
  document.getElementById('loginMsg').textContent = '';
  showEditor(j.username, j.roles);
  await reloadAcl();
  await loadExtensions();
}
function showEditor(u, roles){
  document.getElementById('loginBox').style.display='none';
  document.getElementById('editorBox').style.display='block';
  document.getElementById('extBox').style.display='block';
  document.getElementById('who').textContent = u + ' ['+(roles||[]).join(',')+']';
}
async function reloadAcl(){
  const r = await fetch('/api/v1/admin/acl',{headers:{Authorization:'Bearer '+token}});
  const j = await r.json();
  if(!r.ok){ document.getElementById('saveMsg').className='err'; document.getElementById('saveMsg').textContent=j.error||'加载失败'; return; }
  document.getElementById('aclJson').value = JSON.stringify(j,null,2);
  document.getElementById('saveMsg').className='ok'; document.getElementById('saveMsg').textContent='已加载';
}
async function saveAcl(){
  let doc;
  try{ doc = JSON.parse(document.getElementById('aclJson').value); }
  catch(e){ document.getElementById('saveMsg').className='err'; document.getElementById('saveMsg').textContent='JSON 无效'; return; }
  const r = await fetch('/api/v1/admin/acl',{method:'PUT',headers:{
    Authorization:'Bearer '+token,'Content-Type':'application/json'}, body:JSON.stringify(doc)});
  const j = await r.json();
  document.getElementById('saveMsg').className = r.ok?'ok':'err';
  document.getElementById('saveMsg').textContent = r.ok?'已保存并生效':(j.error||'保存失败');
}
async function loadExtensions(){
  const r = await fetch('/api/v1/admin/extensions',{headers:{Authorization:'Bearer '+token}});
  const j = await r.json();
  const ul = document.getElementById('extList');
  ul.innerHTML = '';
  (j.extensions||[]).forEach(function(ext){
    const li = document.createElement('li');
    const a = document.createElement('a');
    a.href = '#';
    a.textContent = ext.title + ' (' + ext.kind + ')';
    a.onclick = function(ev){ ev.preventDefault(); openExt(ext); };
    li.appendChild(a);
    ul.appendChild(li);
  });
  if(!(j.extensions||[]).length){
    ul.innerHTML = '<li class="muted">暂无业务扩展</li>';
  }
}
async function openExt(ext){
  if(ext.kind === 'page'){
    window.open(ext.path, '_blank');
    return;
  }
  const r = await fetch(ext.path,{headers:{Authorization:'Bearer '+token}});
  const text = await r.text();
  let pretty = text;
  try{ pretty = JSON.stringify(JSON.parse(text), null, 2); }catch(e){}
  document.getElementById('extOut').textContent = pretty;
}
(async function(){
  if(!token) return;
  const r = await fetch('/api/v1/admin/me',{headers:{Authorization:'Bearer '+token}});
  if(!r.ok){ localStorage.removeItem('tray_demo_admin_token'); token=''; return; }
  const j = await r.json();
  showEditor(j.username, j.roles);
  await reloadAcl();
  await loadExtensions();
})();
</script>
</body>
</html>
`
