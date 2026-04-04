#include "web_server.h"

#include <QTimer>
#include <QUrl>

namespace {

QByteArray jsonEscape(const QString& s) {
    QString r = s;
    r.replace('\\', "\\\\").replace('"', "\\\"")
     .replace('\n', "\\n").replace('\r', "\\r");
    return r.toUtf8();
}

QString queryParam(const QString& query, const QString& key) {
    const QString prefix = key + '=';
    for (const QString& part : query.split('&', Qt::SkipEmptyParts)) {
        if (part.startsWith(prefix)) {
            return QUrl::fromPercentEncoding(part.mid(prefix.size()).toUtf8());
        }
    }
    return {};
}

static const char* kIndexHtml = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>InfinityAudio Remote</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#111;color:#bbb;font-family:'Segoe UI',sans-serif;min-width:430px;overflow:auto;padding:10px}
#hdr{width:420px;margin:0 auto;background:#1c1c1e;border:1px solid #2a2a2a;height:36px;padding:0 12px;display:flex;align-items:center;gap:8px}
.logo{font-size:15px;font-weight:700;color:#7CFC00;letter-spacing:.3px;font-family:'Courier New',monospace}
#dot{width:7px;height:7px;border-radius:50%;background:#333;flex-shrink:0;transition:background .3s}
#dot.rec{background:#ff3b30;animation:blink 1s infinite}
#dot.live{background:#34c759}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.3}}
#stxt{font-size:12px;color:#777}
#elapsed{margin-left:auto;font-size:12px;color:#666;font-variant-numeric:tabular-nums;min-width:54px;text-align:right}
#main{width:420px;margin:0 auto}
#bar{width:420px;background:#1c1c1e;border:1px solid #252525;border-top:none;display:flex;flex-direction:column}
.pnl{padding:9px 11px;border-bottom:1px solid #222}
.pnl-t{font-size:10px;font-weight:700;color:#555;text-transform:uppercase;letter-spacing:.7px;margin-bottom:9px}
.src-grid{display:grid;grid-template-columns:60px 1fr;gap:6px;margin-bottom:8px;align-items:center}
.src-lbl{font-size:11px;color:#666}
select{width:100%;background:#111;border:1px solid #2e2e2e;color:#bbb;padding:4px 7px;border-radius:5px;font-size:11px;cursor:pointer;appearance:none}
select:focus{outline:none;border-color:#555}
.row{display:grid;grid-template-columns:120px 120px;justify-content:space-between;gap:8px}
.btn{width:100%;padding:6px;border-radius:5px;border:none;font-size:11px;font-weight:600;cursor:pointer;transition:opacity .12s}
.btn:hover:not(:disabled){opacity:.78}.btn:disabled{opacity:.32;cursor:not-allowed}
.b-sm{background:#2c2c2e;color:#bbb}
.b-rec{background:#ff3b30;color:#fff;font-size:12px;padding:8px;letter-spacing:.3px}
.b-stop{background:#2c2c2e;color:#aaa;font-size:12px;padding:8px}
.b-mon{background:#2c2c2e;color:#bbb}
.b-mon.on{background:#ff8a1f;color:#111}
.kv{display:flex;justify-content:space-between;font-size:11px;padding:4px 0;border-bottom:1px solid #1e1e1e}
.kv:last-child{border:0}
.kk{color:#555}.kv_{color:#aaa;text-align:right;max-width:180px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
#vuWrap{padding:9px 11px 12px}
#vu{display:block;width:170px;height:280px;margin:0 auto;background:#111;border:1px solid #202020}
</style>
</head>
<body>
<div id="hdr">
  <div class="logo">Infinity Audio</div>
  <div id="dot"></div>
  <div id="stxt">Idle</div>
  <div id="elapsed"></div>
</div>
<div id="main">
  <div id="bar">
    <div class="pnl">
      <div class="pnl-t">Input</div>
      <div class="src-grid">
        <span class="src-lbl">Device</span>
        <select id="inSel"></select>
      </div>
      <div class="row">
        <button class="btn b-sm" onclick="loadInputs()">Refresh</button>
        <button class="btn b-mon" id="mBtn" onclick="toggleMonitor()">Monitor</button>
      </div>
    </div>
    <div class="pnl">
      <div class="pnl-t">Recording</div>
      <div class="row">
        <button class="btn b-rec" id="rBtn" onclick="doRec()">REC</button>
        <button class="btn b-stop" id="sBtn" onclick="doStop()" disabled>STOP</button>
      </div>
    </div>
    <div class="pnl">
      <div class="pnl-t">Settings</div>
      <div class="src-grid">
        <span class="src-lbl">Prefix</span>
        <input id="prefix" type="text" placeholder="ex: StudioA" style="width:100%;background:#111;border:1px solid #2e2e2e;color:#bbb;padding:4px 7px;border-radius:5px;font-size:11px" />
      </div>
      <div class="src-grid">
        <span class="src-lbl">Chunk</span>
        <select id="chunkSel">
          <option value="15">15 min</option>
          <option value="30">30 min</option>
          <option value="45">45 min</option>
          <option value="60">60 min</option>
        </select>
      </div>
      <div class="row">
        <button class="btn b-sm" onclick="loadSettings()">Reload</button>
        <button class="btn b-sm" onclick="saveSettings()">Save</button>
      </div>
    </div>
    <div class="pnl">
      <div class="pnl-t">Info</div>
      <div class="kv"><span class="kk">Format</span><span class="kv_" id="iFmt">—</span></div>
      <div class="kv"><span class="kk">Status</span><span class="kv_" id="iStatus">—</span></div>
      <div class="kv"><span class="kk">Input</span><span class="kv_" id="iInput">—</span></div>
      <div class="kv"><span class="kk">File</span><span class="kv_" id="iFile">—</span></div>
    </div>
    <div id="vuWrap">
      <div class="pnl-t">Audio Levels</div>
      <canvas id="vu"></canvas>
    </div>
  </div>
</div>
<script>
async function loadInputs(){
  try {
    const d = await (await fetch('/inputs')).json();
    const s = document.getElementById('inSel');
    const cur = d.current || '';
    s.innerHTML = '';
    (d.inputs || []).forEach(n => {
      const o = document.createElement('option');
      o.value = o.textContent = n;
      if (n === cur) o.selected = true;
      s.appendChild(o);
    });
    s.onchange = async () => { await fetch('/set-input?device=' + encodeURIComponent(s.value)); };
  } catch(e) {}
}
async function doRec(){ try { await fetch('/rec'); } catch(e) {} }
async function doStop(){ try { await fetch('/stop'); } catch(e) {} }
async function loadSettings(){
  try {
    const d = await (await fetch('/settings')).json();
    const prefix = document.getElementById('prefix');
    const chunkSel = document.getElementById('chunkSel');
    prefix.value = d.prefix || '';
    const chunk = String(d.chunkMinutes || 60);
    if ([...chunkSel.options].some(o => o.value === chunk)) {
      chunkSel.value = chunk;
    }
  } catch(e) {}
}
async function saveSettings(){
  try {
    const prefix = document.getElementById('prefix').value || '';
    const chunk = document.getElementById('chunkSel').value || '60';
    await fetch('/set-settings?prefix=' + encodeURIComponent(prefix) + '&chunkMinutes=' + encodeURIComponent(chunk));
    await loadSettings();
  } catch(e) {}
}
async function toggleMonitor(){
  const btn = document.getElementById('mBtn');
  const enabled = !btn.classList.contains('on');
  try { await fetch('/web-monitor?enabled=' + (enabled ? '1' : '0')); } catch(e) {}
  monitorOn = enabled;
  if (monitorOn) {
    startAudioLoop();
  }
}
let recStart = null;
let monitorOn = false;
let audioCtx = null;
let nextPlayTime = 0;
let monitorLoopRunning = false;

async function startAudioLoop(){
  if (monitorLoopRunning) return;
  monitorLoopRunning = true;
  if (!audioCtx) {
    const Ctx = window.AudioContext || window.webkitAudioContext;
    audioCtx = Ctx ? new Ctx() : null;
  }
  while (monitorOn) {
    try {
      if (!audioCtx) break;
      if (audioCtx.state === 'suspended') await audioCtx.resume();
      const resp = await fetch('/audio.wav?t=' + Date.now(), { cache: 'no-store' });
      if (resp.ok) {
        const arr = await resp.arrayBuffer();
        if (arr.byteLength > 44) {
          const buf = await audioCtx.decodeAudioData(arr.slice(0));
          const src = audioCtx.createBufferSource();
          src.buffer = buf;
          src.connect(audioCtx.destination);
          const now = audioCtx.currentTime;
          if ((nextPlayTime - now) > 1.0 || nextPlayTime < now) {
            nextPlayTime = now + 0.05;
          }
          src.start(nextPlayTime);
          nextPlayTime += buf.duration;
        }
      }
    } catch(e) {}
    await new Promise(r => setTimeout(r, 90));
  }
  monitorLoopRunning = false;
}

async function pollStatus(){
  try {
    const d = await (await fetch('/status')).json();
    const dot = document.getElementById('dot');
    const stxt = document.getElementById('stxt');
    const rBtn = document.getElementById('rBtn');
    const sBtn = document.getElementById('sBtn');
    const mBtn = document.getElementById('mBtn');
    if (d.recording) {
      dot.className='rec'; stxt.textContent='Recording'; rBtn.disabled=true; sBtn.disabled=false;
      if (!recStart) recStart = d.segmentStartMs || Date.now();
    } else {
      dot.className = d.input ? 'live' : '';
      stxt.textContent = d.status || (d.input ? 'Live' : 'Idle');
      rBtn.disabled=false; sBtn.disabled=true; recStart=null;
    }
    mBtn.classList.toggle('on', !!d.webMonitor);
    if (!d.webMonitor) {
      monitorOn = false;
      nextPlayTime = 0;
    }
    document.getElementById('iFmt').textContent = d.format || '—';
    document.getElementById('iStatus').textContent = d.status || '—';
    document.getElementById('iInput').textContent = d.input || '—';
    document.getElementById('iFile').textContent = d.file || '—';
    const inSel = document.getElementById('inSel');
    if (d.input && inSel.value !== d.input) {
      for (const o of inSel.options) if (o.value === d.input) { inSel.value = d.input; break; }
    }
    if (recStart) {
      const s = Math.floor((Date.now() - recStart) / 1000);
      document.getElementById('elapsed').textContent = [s/3600|0,(s%3600)/60|0,s%60].map(n=>String(n).padStart(2,'0')).join(':');
    } else {
      document.getElementById('elapsed').textContent = '';
    }
    drawVu(d.vuL ?? -60, d.vuR ?? -60);
  } catch(e) {}
}
const vc = document.getElementById('vu');
let pkL=-60, pkR=-60, pkLt=0, pkRt=0;
const HOLD_MS=1800, DECAY=12;
function dbToR(db){ return Math.max(0,Math.min(1,(db+60)/60)); }
function drawVu(dbL,dbR){
  const now=Date.now(), dt=0.2;
  if(dbL>=pkL){pkL=dbL;pkLt=now;}else if(now-pkLt>HOLD_MS)pkL=Math.max(-60,pkL-DECAY*dt);
  if(dbR>=pkR){pkR=dbR;pkRt=now;}else if(now-pkRt>HOLD_MS)pkR=Math.max(-60,pkR-DECAY*dt);
  const W=170, H=280;
  vc.width=W; vc.height=H;
  const ctx=vc.getContext('2d');
  ctx.clearRect(0,0,W,H);
  ctx.fillStyle='#111'; ctx.fillRect(0,0,W,H);
  const scaleW=26,gap=5,bW=Math.floor((W-scaleW-gap*3)/2);
  const bTop=4,bBot=H-14,bH=bBot-bTop,xL=scaleW+gap,xR=xL+bW+gap;
  const marks=[0,-3,-6,-12,-20,-40,-60];
  ctx.font='9px monospace'; ctx.textAlign='right';
  marks.forEach(db=>{ const y=bBot-dbToR(db)*bH; ctx.fillStyle='#2a2a2a'; ctx.fillRect(scaleW,y,W-scaleW,1); ctx.fillStyle='#555'; ctx.fillText(db,scaleW-2,y+3); });
  function bar(x,db,pk){
    ctx.fillStyle='#1a1a1a'; ctx.fillRect(x,bTop,bW,bH);
    const fillH=dbToR(db)*bH;
    if(fillH>0){ const g=ctx.createLinearGradient(0,bTop,0,bBot); g.addColorStop(0,'#ff3333'); g.addColorStop(3/60,'#ff3333'); g.addColorStop(3/60+.001,'#ffaa00'); g.addColorStop(20/60,'#ffe000'); g.addColorStop(20/60+.001,'#00cc55'); g.addColorStop(1,'#007a33'); ctx.fillStyle=g; ctx.fillRect(x,bBot-fillH,bW,fillH); }
    if(pk>-60){ const py=bBot-dbToR(pk)*bH; ctx.fillStyle=pk>-3?'#ff6666':pk>-20?'#ffe066':'#66ffaa'; ctx.fillRect(x,py-1,bW,2); }
  }
  bar(xL,dbL,pkL); bar(xR,dbR,pkR);
  ctx.fillStyle='#555'; ctx.font='10px -apple-system,sans-serif'; ctx.textAlign='center';
  ctx.fillText('L',xL+bW/2,H-2); ctx.fillText('R',xR+bW/2,H-2);
}
loadInputs();
loadSettings();
setInterval(pollStatus, 200); pollStatus();
</script>
</body>
</html>)HTML";

} // namespace

WebServer::WebServer(QObject* parent) : QObject(parent) {}

WebServer::~WebServer() { stop(); }

void WebServer::setPassword(const QString& password) { m_password = password; }

bool WebServer::start(quint16 port) {
    if (m_server) return true;
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &WebServer::onNewConnection);
    if (!m_server->listen(QHostAddress::Any, port)) {
        delete m_server;
        m_server = nullptr;
        return false;
    }
    return true;
}

void WebServer::stop() {
    if (!m_server) return;
    m_server->close();
    delete m_server;
    m_server = nullptr;
}

bool WebServer::isRunning() const { return m_server && m_server->isListening(); }

void WebServer::setInputHandlers(std::function<QStringList()> listFn,
                                 std::function<QString()> currentFn,
                                 std::function<bool(const QString&)> setFn) {
    m_listInputsFn = std::move(listFn);
    m_currentInputFn = std::move(currentFn);
    m_setInputFn = std::move(setFn);
}

void WebServer::setRecorderHandlers(std::function<void()> startFn,
                                    std::function<void()> stopFn) {
    m_startRecordingFn = std::move(startFn);
    m_stopRecordingFn = std::move(stopFn);
}

void WebServer::setMonitorHandlers(std::function<bool()> monitorEnabledFn,
                   std::function<bool(bool)> setMonitorFn) {
  m_isMonitorEnabledFn = std::move(monitorEnabledFn);
  m_setMonitorFn = std::move(setMonitorFn);
}

void WebServer::setStateHandlers(std::function<bool()> recordingFn,
                                 std::function<QString()> statusFn,
                                 std::function<QString()> fileFn,
                                 std::function<QString()> formatFn,
                                 std::function<qint64()> segmentStartFn) {
    m_isRecordingFn = std::move(recordingFn);
    m_statusFn = std::move(statusFn);
    m_fileFn = std::move(fileFn);
    m_formatFn = std::move(formatFn);
    m_segmentStartFn = std::move(segmentStartFn);
}

  void WebServer::setSettingsHandlers(std::function<QString()> filePrefixFn,
                    std::function<int()> chunkMinutesFn,
                    std::function<bool(const QString&)> setFilePrefixFn,
                    std::function<bool(int)> setChunkMinutesFn) {
    m_filePrefixFn = std::move(filePrefixFn);
    m_chunkMinutesFn = std::move(chunkMinutesFn);
    m_setFilePrefixFn = std::move(setFilePrefixFn);
    m_setChunkMinutesFn = std::move(setChunkMinutesFn);
  }

void WebServer::onVuLevels(float leftDb, float rightDb) {
    m_vuL = leftDb;
    m_vuR = rightDb;
}

void WebServer::onAudioPcm16(const QByteArray& pcm16le, int sampleRate, int channels) {
  if (!m_webMonitorEnabled || pcm16le.isEmpty()) return;
  if (sampleRate <= 0 || channels <= 0) return;

  m_audioRate = sampleRate;
  m_audioChannels = channels;
  m_audioPcm16.append(pcm16le);

  const int maxBytes = std::max(1, sampleRate * channels * 2 * 2); // ~2s
  if (m_audioPcm16.size() > maxBytes) {
    m_audioPcm16.remove(0, m_audioPcm16.size() - maxBytes);
  }
}

void WebServer::onNewConnection() {
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &WebServer::onClientReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &WebServer::onClientDisconnected);
    }
}

void WebServer::onClientReadyRead() {
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    m_clientBuffers[socket].append(socket->readAll());
    const QByteArray& buf = m_clientBuffers[socket];
    if (!buf.contains("\r\n\r\n")) return;

    const int lineEnd = buf.indexOf("\r\n");
    const QList<QByteArray> parts = buf.left(lineEnd).split(' ');
    QString authHeader;
    const QByteArray headerBlock = buf.left(buf.indexOf("\r\n\r\n"));
    for (const QByteArray& line : headerBlock.split('\n')) {
      const QByteArray t = line.trimmed();
      if (t.toLower().startsWith("authorization:")) {
        authHeader = QString::fromUtf8(t.mid(14).trimmed());
        break;
      }
    }
    m_clientBuffers.remove(socket);

    if (parts.size() < 2) {
        sendError(socket, 400, "Bad Request");
        return;
    }

    const QString full = QString::fromUtf8(parts[1]);
    const int queryIndex = full.indexOf('?');
    const QString path = (queryIndex < 0) ? full : full.left(queryIndex);
    const QString query = (queryIndex < 0) ? QString() : full.mid(queryIndex + 1);
    handleRequest(socket, path, query, authHeader);
}

void WebServer::onClientDisconnected() {
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    m_clientBuffers.remove(socket);
    socket->deleteLater();
}

void WebServer::handleRequest(QTcpSocket* socket, const QString& path, const QString& query,
                const QString& authHeader) {
  if (!m_password.isEmpty()) {
    bool ok = false;
    if (authHeader.startsWith("Basic ", Qt::CaseInsensitive)) {
      const QByteArray decoded = QByteArray::fromBase64(authHeader.mid(6).toUtf8());
      const int colon = decoded.indexOf(':');
      const QString pwd = QString::fromUtf8(colon >= 0 ? decoded.mid(colon + 1) : decoded);
      ok = (pwd == m_password);
    }
    if (!ok) {
      QByteArray r;
      r += "HTTP/1.1 401 Unauthorized\r\n";
      r += "WWW-Authenticate: Basic realm=\"InfinityAudio Remote\"\r\n";
      r += "Content-Length: 0\r\nConnection: close\r\n\r\n";
      socket->write(r);
      socket->disconnectFromHost();
      return;
    }
  }

    if (path == "/" || path == "/index.html") return serveIndex(socket);
    if (path == "/status") return serveStatus(socket);
    if (path == "/inputs") return serveInputs(socket);
    if (path == "/rec") return serveRec(socket);
    if (path == "/stop") return serveStop(socket);
    if (path == "/monitor") return serveMonitor(socket, query);
    if (path == "/web-monitor") return serveWebMonitor(socket, query);
    if (path == "/audio.wav") return serveAudioWav(socket);
    if (path == "/set-input") return serveSetInput(socket, query);
    if (path == "/settings") return serveSettings(socket);
    if (path == "/set-settings") return serveSetSettings(socket, query);
    sendError(socket, 404, "Not Found");
}

void WebServer::serveIndex(QTcpSocket* socket) { sendHtml(socket, QByteArray(kIndexHtml)); }

void WebServer::serveStatus(QTcpSocket* socket) {
    const bool recording = m_isRecordingFn ? m_isRecordingFn() : false;
  const bool monitor = m_isMonitorEnabledFn ? m_isMonitorEnabledFn() : false;
  const bool webMonitor = m_webMonitorEnabled;
    const QString input = m_currentInputFn ? m_currentInputFn() : QString{};
    const QString status = m_statusFn ? m_statusFn() : QString{};
    const QString file = m_fileFn ? m_fileFn() : QString{};
    const QString format = m_formatFn ? m_formatFn() : QString{};
    const qint64 segmentStartMs = m_segmentStartFn ? m_segmentStartFn() : 0;

    QByteArray json;
    json += "{";
    json += "\"recording\":" + QByteArray(recording ? "true" : "false") + ',';
    json += "\"monitor\":" + QByteArray(monitor ? "true" : "false") + ',';
    json += "\"webMonitor\":" + QByteArray(webMonitor ? "true" : "false") + ',';
    json += "\"input\":\"" + jsonEscape(input) + "\",";
    json += "\"status\":\"" + jsonEscape(status) + "\",";
    json += "\"file\":\"" + jsonEscape(file) + "\",";
    json += "\"format\":\"" + jsonEscape(format) + "\",";
    json += "\"segmentStartMs\":" + QByteArray::number(segmentStartMs) + ',';
    json += "\"vuL\":" + QByteArray::number(static_cast<double>(m_vuL), 'f', 1) + ',';
    json += "\"vuR\":" + QByteArray::number(static_cast<double>(m_vuR), 'f', 1);
    json += "}";
    sendJson(socket, json);
}

void WebServer::serveInputs(QTcpSocket* socket) {
    const QStringList inputs = m_listInputsFn ? m_listInputsFn() : QStringList{};
    const QString current = m_currentInputFn ? m_currentInputFn() : QString{};
    QByteArray json = "{\"inputs\":[";
    for (int i = 0; i < inputs.size(); ++i) {
        if (i) json += ',';
        json += '"' + jsonEscape(inputs[i]) + '"';
    }
    json += "],\"current\":\"" + jsonEscape(current) + "\"}";
    sendJson(socket, json);
}

void WebServer::serveRec(QTcpSocket* socket) {
    sendJson(socket, "{\"ok\":true}");
    QTimer::singleShot(0, this, [this] { if (m_startRecordingFn) m_startRecordingFn(); });
}

void WebServer::serveStop(QTcpSocket* socket) {
    sendJson(socket, "{\"ok\":true}");
    QTimer::singleShot(0, this, [this] { if (m_stopRecordingFn) m_stopRecordingFn(); });
}

void WebServer::serveMonitor(QTcpSocket* socket, const QString& query) {
    const QString enabledParam = queryParam(query, "enabled");
    const bool enabled = (enabledParam == "1" || enabledParam.compare("true", Qt::CaseInsensitive) == 0);

    bool ok = true;
    if (m_setMonitorFn) {
        ok = m_setMonitorFn(enabled);
    }

    sendJson(socket, ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

  void WebServer::serveWebMonitor(QTcpSocket* socket, const QString& query) {
    const QString enabledParam = queryParam(query, "enabled");
    const bool enabled = (enabledParam == "1" || enabledParam.compare("true", Qt::CaseInsensitive) == 0);
    m_webMonitorEnabled = enabled;
    if (!m_webMonitorEnabled) {
      m_audioPcm16.clear();
    }
    sendJson(socket, "{\"ok\":true}");
  }

void WebServer::serveAudioWav(QTcpSocket* socket) {
  QByteArray pcm;
  if (m_audioPcm16.isEmpty()) {
    QByteArray r;
    r += "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n";
    socket->write(r);
    socket->disconnectFromHost();
    return;
  }

  const int bytesPerSecond = std::max(1, m_audioRate * m_audioChannels * 2);
  const int maxChunk = std::max(1, bytesPerSecond / 3); // ~333ms
  const int take = std::min(maxChunk, static_cast<int>(m_audioPcm16.size()));
  pcm = m_audioPcm16.left(take);
  m_audioPcm16.remove(0, take);

  QByteArray wav;
  wav.reserve(44 + pcm.size());
  auto appendLE16 = [&wav](quint16 v) {
    wav.append(char(v & 0xFF));
    wav.append(char((v >> 8) & 0xFF));
  };
  auto appendLE32 = [&wav](quint32 v) {
    wav.append(char(v & 0xFF));
    wav.append(char((v >> 8) & 0xFF));
    wav.append(char((v >> 16) & 0xFF));
    wav.append(char((v >> 24) & 0xFF));
  };

  const quint16 channels = static_cast<quint16>(std::max(1, m_audioChannels));
  const quint32 sampleRate = static_cast<quint32>(std::max(1, m_audioRate));
  const quint16 bitsPerSample = 16;
  const quint16 blockAlign = channels * (bitsPerSample / 8);
  const quint32 byteRate = sampleRate * blockAlign;
  const quint32 dataSize = static_cast<quint32>(pcm.size());

  wav.append("RIFF", 4);
  appendLE32(36 + dataSize);
  wav.append("WAVE", 4);
  wav.append("fmt ", 4);
  appendLE32(16);
  appendLE16(1);
  appendLE16(channels);
  appendLE32(sampleRate);
  appendLE32(byteRate);
  appendLE16(blockAlign);
  appendLE16(bitsPerSample);
  wav.append("data", 4);
  appendLE32(dataSize);
  wav.append(pcm);

  QByteArray r;
  r += "HTTP/1.1 200 OK\r\nContent-Type: audio/wav\r\n";
  r += "Content-Length: " + QByteArray::number(wav.size()) + "\r\n";
  r += "Cache-Control: no-store\r\nConnection: close\r\n\r\n";
  r += wav;
  socket->write(r);
  socket->disconnectFromHost();
}

void WebServer::serveSetInput(QTcpSocket* socket, const QString& query) {
    const QString device = queryParam(query, "device");
    const bool ok = !device.isEmpty() && m_setInputFn ? m_setInputFn(device) : false;
    sendJson(socket, ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

void WebServer::serveSettings(QTcpSocket* socket) {
  const QString prefix = m_filePrefixFn ? m_filePrefixFn() : QString{};
  const int chunkMinutes = m_chunkMinutesFn ? m_chunkMinutesFn() : 60;
  QByteArray json;
  json += "{";
  json += "\"prefix\":\"" + jsonEscape(prefix) + "\",";
  json += "\"chunkMinutes\":" + QByteArray::number(chunkMinutes);
  json += "}";
  sendJson(socket, json);
}

void WebServer::serveSetSettings(QTcpSocket* socket, const QString& query) {
  const QString prefix = queryParam(query, "prefix");
  const QString chunkParam = queryParam(query, "chunkMinutes");

  bool ok = true;
  if (m_setFilePrefixFn) {
    ok = m_setFilePrefixFn(prefix) && ok;
  }

  if (!chunkParam.isEmpty()) {
    bool isNumber = false;
    const int minutes = chunkParam.toInt(&isNumber);
    if (!isNumber || !m_setChunkMinutesFn) {
      ok = false;
    } else {
      ok = m_setChunkMinutesFn(minutes) && ok;
    }
  }

  sendJson(socket, ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

static void writeAndClose(QTcpSocket* socket, const QByteArray& data) {
    socket->write(data);
    socket->disconnectFromHost();
}

void WebServer::sendJson(QTcpSocket* socket, const QByteArray& json) {
    QByteArray r;
    r += "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n";
    r += "Content-Length: " + QByteArray::number(json.size()) + "\r\n";
    r += "Access-Control-Allow-Origin: *\r\n";
    r += "Cache-Control: no-cache\r\nConnection: close\r\n\r\n";
    r += json;
    writeAndClose(socket, r);
}

void WebServer::sendHtml(QTcpSocket* socket, const QByteArray& html) {
    QByteArray r;
    r += "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n";
    r += "Content-Length: " + QByteArray::number(html.size()) + "\r\nConnection: close\r\n\r\n";
    r += html;
    writeAndClose(socket, r);
}

void WebServer::sendError(QTcpSocket* socket, int code, const QByteArray& msg) {
    QByteArray r;
    r += "HTTP/1.1 " + QByteArray::number(code) + ' ' + msg + "\r\n";
    r += "Content-Type: text/plain\r\nContent-Length: " + QByteArray::number(msg.size()) + "\r\nConnection: close\r\n\r\n";
    r += msg;
    writeAndClose(socket, r);
}