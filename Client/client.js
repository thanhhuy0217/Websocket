let websocket = null;

// BIẾN QUẢN LÝ THỜI GIAN (LOGIC HOME MỚI)
let intervalUptime = null;
let intervalSensors = null;
let intervalPing = null;
let systemStartTime = 0; 

document.addEventListener('DOMContentLoaded', () => {
    if(typeof lucide !== 'undefined') lucide.createIcons();
    
    const defaultNav = document.querySelector('.nav-item.active');
    if(defaultNav) switchTab('home', defaultNav);
    
    const btnConnect = document.getElementById("btnConnect");
    if(btnConnect) btnConnect.addEventListener("click", connectToServer);

    // Mặc định gọi hàm reset để hiện trạng thái chưa kết nối
    resetDashboardState();
});

// --- UI NAVIGATION ---
function switchTab(tabId, element) {
    document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.nav-item').forEach(el => el.classList.remove('active'));
    
    document.getElementById('tab-' + tabId).classList.add('active');
    element.classList.add('active');

    const titles = {
        'home': 'Home', 'apps': 'Application Manager', 'processes': 'System Processes',
        'media': 'Screenshot Viewer', 'webcam': 'Webcam Stream', 'keylogger': 'Keylogger Records', 'power': 'Power Control'
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

function updateHomeStatus(type, statusText) {
    const el = document.getElementById(`status-${type}`);
    if(el) {
        el.innerText = statusText;
        el.style.color = statusText === "Active" || statusText === "Recording" ? "#a78bfa" : "#A8AAB3";
    }
}

// --- CORE: WEBSOCKET ---
function connectToServer() {
    const ipInput = document.getElementById("ipInput");
    const ip = ipInput ? ipInput.value.trim() : '';
    if (!ip) { alert("Please enter IP!"); return; }

    const wsUri = "ws://" + ip + ":8080/";
    const btnText = document.getElementById("btn-text");
    if(btnText) btnText.innerText = "Connecting...";

    if (websocket) websocket.close();

    try {
        websocket = new WebSocket(wsUri);

        websocket.onopen = () => {
            setConnectedState(true);
            addActivityLog("Connected to Server: " + ip);
            // BẮT ĐẦU CẬP NHẬT DASHBOARD
            startDashboardUpdates();
        };
        websocket.onclose = () => {
            setConnectedState(false);
            addActivityLog("Connection Closed");
            // DỪNG CẬP NHẬT
            stopDashboardUpdates();
        };
        websocket.onerror = (e) => {
            setConnectedState(false);
            stopDashboardUpdates();
        };
        websocket.onmessage = (e) => {
            handleIncomingMessage(e.data);
        };
    } catch (e) {
        alert("Error: " + e.message);
        setConnectedState(false);
        stopDashboardUpdates();
    }
}

function setConnectedState(isConnected) {
    const dot = document.getElementById("connection-dot");
    const btnText = document.getElementById("btn-text");
    const sysNetwork = document.getElementById("sys-network"); // Ô Network trong Info

    if (isConnected) {
        if(dot) { dot.classList.remove("disconnected"); dot.classList.add("connected"); }
        if(btnText) btnText.innerText = "Connected";
        if(sysNetwork) {
            sysNetwork.innerText = "Connected";
            sysNetwork.style.color = "#34d399"; // Xanh
        }
    } else {
        if(dot) { dot.classList.remove("connected"); dot.classList.add("disconnected"); }
        if(btnText) btnText.innerText = "Connect";
        if(sysNetwork) {
            sysNetwork.innerText = "Disconnected";
            sysNetwork.style.color = "#fb7185"; // Đỏ
        }
    }
}

function sendCmd(command) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send(command);
        console.log("Sent:", command);
    } else {
        alert("Server Disconnected!");
    }
}

// --- LOGIC CẬP NHẬT HOME (THEO YÊU CẦU MỚI) ---

function resetDashboardState() {
    // Reset về --- khi chưa kết nối
    document.getElementById("sys-hostname").innerText = "---";
    document.getElementById("sys-os").innerText = "---";
    document.getElementById("sys-uptime").innerText = "--:--:--";
    document.getElementById("sys-network").innerText = "Disconnected";
    document.getElementById("sys-network").style.color = "#fb7185";
    
    document.getElementById("sensor-temp").innerText = "--°C";
    document.getElementById("sensor-ping").innerText = "-- ms";
    
    document.getElementById("chart-cpu").style.setProperty('--p', 0);
    document.getElementById("chart-cpu").querySelector('span').innerText = "0%";
    
    document.getElementById("chart-ram").style.setProperty('--p', 0);
    document.getElementById("chart-ram").querySelector('span').innerText = "0%";
}

function startDashboardUpdates() {
    // 1. Cập nhật thông tin tĩnh ngay lập tức
    document.getElementById("sys-hostname").innerText = "DESKTOP-CLIENT";
    document.getElementById("sys-os").innerText = "Windows 10 Pro";
    systemStartTime = Date.now(); 

    // 2. Uptime: Cập nhật mỗi 1 giây
    if(intervalUptime) clearInterval(intervalUptime);
    intervalUptime = setInterval(() => {
        const now = Date.now();
        const diff = Math.floor((now - systemStartTime) / 1000);
        
        const h = Math.floor(diff / 3600).toString().padStart(2, '0');
        const m = Math.floor((diff % 3600) / 60).toString().padStart(2, '0');
        const s = (diff % 60).toString().padStart(2, '0');
        
        document.getElementById("sys-uptime").innerText = `${h}:${m}:${s}`;
    }, 1000);

    // 3. Ping: Cập nhật mỗi 2 giây (trong khoảng 1-3s)
    if(intervalPing) clearInterval(intervalPing);
    intervalPing = setInterval(() => {
        const ping = Math.floor(Math.random() * 20) + 10; 
        document.getElementById("sensor-ping").innerText = `${ping} ms`;
    }, 2000);

    // 4. Sensors & Charts: Cập nhật mỗi 4 giây (trong khoảng 3-5s)
    if(intervalSensors) clearInterval(intervalSensors);
    intervalSensors = setInterval(() => {
        // CPU Temp
        const temp = Math.floor(Math.random() * 15) + 45; 
        document.getElementById("sensor-temp").innerText = `${temp}°C`;

        // CPU Usage
        const cpu = Math.floor(Math.random() * 30) + 20;
        const chartCpu = document.getElementById("chart-cpu");
        chartCpu.style.setProperty('--p', cpu);
        chartCpu.querySelector('span').innerText = cpu + "%";

        // RAM Usage
        const ram = Math.floor(Math.random() * 20) + 50;
        const chartRam = document.getElementById("chart-ram");
        chartRam.style.setProperty('--p', ram);
        chartRam.querySelector('span').innerText = ram + "%";

    }, 4000);
}

function stopDashboardUpdates() {
    if(intervalUptime) clearInterval(intervalUptime);
    if(intervalPing) clearInterval(intervalPing);
    if(intervalSensors) clearInterval(intervalSensors);
    resetDashboardState();
}

// --- XỬ LÝ DỮ LIỆU TỪ SERVER (GIỮ NGUYÊN) ---
function handleIncomingMessage(data) {
    if (data.startsWith("file ")) {
        const base64 = data.substring(5).trim();
        handleVideoFile(base64); 
        addActivityLog("Webcam video received");
        updateHomeStatus("webcam", "Idle");
    } 
    else if (data.startsWith("KEYLOG_DATA:")) {
        const box = document.getElementById("keylog-output");
        if(box) box.innerHTML = data.substring(12);
        addActivityLog("Fetched Keylogger records");
    }
    else if (data.includes("PID") && data.includes("RAM")) {
        renderProcessTable(data, "process-list-body");
        renderProcessTable(data, "app-list-body");
        addActivityLog("Process/App list updated");
    }
    else {
        console.log("Server:", data);
        if(data.includes("Keylogger da bat dau")) updateHomeStatus("keylog", "Active");
        if(data.includes("Keylogger da dung lai")) updateHomeStatus("keylog", "Idle");
        if(data.includes("Server:")) addActivityLog(data.replace("Server: ", ""));
    }
}

// --- APP & PROCESS ---
function handleAppAction(action) {
    const el = document.getElementById("appInput");
    if (!el || !el.value) return alert("Enter App Name!");
    sendCmd(`${action} ${el.value}`);
}

function handleProcessAction(action) {
    const el = document.getElementById("procInput");
    if (!el || !el.value) return alert("Enter PID or Name!");
    sendCmd(`${action} ${el.value}`);
}

function renderProcessTable(rawData, tableId) {
    const tbody = document.getElementById(tableId);
    if(!tbody) return;
    tbody.innerHTML = "";
    
    const rows = rawData.trim().split('\n');
    for (let i = 1; i < rows.length; i++) {
        const cols = rows[i].split('\t');
        if (cols.length >= 4) {
            const tr = document.createElement("tr");
            tr.innerHTML = `
                <td><strong>${cols[0]}</strong></td>
                <td>${cols[3]}</td> 
                <td>${cols[2]}</td>
                <td>${cols[1]}</td>
            `;
            tbody.appendChild(tr);
        }
    }
}

// --- WEBCAM ---
function requestWebcam() {
    const btn = document.getElementById("btn-record");
    if(btn) {
        btn.innerHTML = `<i data-lucide="loader" class="btn-icon-inside"></i> Recording...`;
        btn.disabled = true;
    }
    updateHomeStatus("webcam", "Recording");
    if(typeof lucide !== 'undefined') lucide.createIcons();
    
    sendCmd("capture"); 
}

function handleVideoFile(base64) {
    const btn = document.getElementById("btn-record");
    if(btn) {
        btn.innerHTML = `<i data-lucide="radio" class="btn-icon-inside"></i> Record 10s`;
        btn.disabled = false;
    }

    try {
        const byteChars = atob(base64);
        const byteNumbers = new Array(byteChars.length);
        for (let i = 0; i < byteChars.length; i++) {
            byteNumbers[i] = byteChars.charCodeAt(i);
        }
        const byteArray = new Uint8Array(byteNumbers);
        const blob = new Blob([byteArray], {type: "video/mp4"});
        const url = URL.createObjectURL(blob);

        const previewBox = document.getElementById("webcam-preview");
        if(previewBox) {
            previewBox.innerHTML = `<video src="${url}" controls autoplay style="width:100%; height:100%; border-radius:16px;"></video>`;
        }

        const downloadArea = document.getElementById("webcam-download-area");
        if(downloadArea) {
            downloadArea.innerHTML = `
                <a href="${url}" download="webcam_capture.mp4" class="btn-download-pill">
                    <i data-lucide="download"></i> Download Video
                </a>
            `;
            if(typeof lucide !== 'undefined') lucide.createIcons();
        }
    } catch (e) {
        console.error("Error decoding video:", e);
        alert("Lỗi khi tải video từ server!");
    }
}

// --- POWER ---
function confirmPower(action) {
    if(confirm(`Are you sure you want to ${action.toUpperCase()}?`)) {
        sendCmd(action);
    }
}