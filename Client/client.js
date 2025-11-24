// http://www.websocket.org/echo.html
const button = document.querySelector("button");
const output = document.querySelector("#output");
const textarea = document.querySelector("textarea");
// NOTE: Change this URI to match your server IP if needed (e.g., 192.168.1.x)
const wsUri = "ws://127.0.0.1:8080/";
const websocket = new WebSocket(wsUri);

button.addEventListener("click", onClickButton);

websocket.onopen = (e) => {
    writeToScreen('<span style="color: green;">CONNECTED</span>');
};

websocket.onclose = (e) => {
    writeToScreen('<span style="color: red;">DISCONNECTED</span>');
};

websocket.onmessage = (e) => {
    // Check if the message is a text command or raw binary
    handleIncomingMessage(e.data);
};

websocket.onerror = (e) => {
    writeToScreen(`<span style="color: red;">ERROR:</span> ${e.message || 'Unknown Error'}`);
};

// --- CORE FUNCTIONS ---

/**
 * Parses the incoming message based on its first word prefix.
 * @param {string} fullMessage The message received from the server.
 */
function handleIncomingMessage(fullMessage) {
    const parts = fullMessage.split(/(\s+)/);
    // Split by whitespace to keep the delimiter
    const keyword = (parts[0] || '').toLowerCase();
    // First word is the command
    // The content is the rest of the string after the keyword and the first space
    const content = fullMessage.substring(keyword.length).trim();

    switch (keyword) {
        case "img":
            handleImageMessage(content);
            break;
        case "file":
            writeToScreen(`<span style="color: blue;">FILE command received. Processing...</span>`);
            handleFileMessage(content); 
            break;
        case "text":
            handleTextMessage(content);
            break;
        default:
            // MODIFIED: Treat unknown keywords as plain text from Server
            // This allows "Server: OK..." messages to be displayed correctly
            handleTextMessage(fullMessage);
            break;
    }
}

/**
 * Handles plain text messages
 */
function handleTextMessage(content) {
    output.insertAdjacentHTML("afterbegin", `<div class="text-block">
        <p><span style="color: blue;">SERVER RESPONSE:</span> ${content}</p>
    </div>`);
}

/**
 * Handles "img" command: Decodes base64 and appends an <img> tag.
 */
function handleImageMessage(base64Data) {
    const src = base64Data.startsWith('data:') ?
        base64Data : 'data:image/png;base64,' + base64Data;
    
    output.insertAdjacentHTML("afterbegin", `<div class="img-block">
        <p><span>IMAGE RECEIVED:</span></p>
        <img src="${src}" alt="Received Image" style="max-width: 100%; height: auto; border: 1px solid #ccc;">
    </div>`);
}

/**
 * Handles "file" command: Decodes base64 and prompts the user to download the file.
 */
function handleFileMessage(base64Data) {
    const byteString = atob(base64Data);
    const mimeType = 'application/octet-stream';
    const filename = 'downloaded_file_' + Date.now() + '.bin';
    
    const ab = new ArrayBuffer(byteString.length);
    const ia = new Uint8Array(ab);
    for (let i = 0; i < byteString.length; i++) {
        ia[i] = byteString.charCodeAt(i);
    }

    const blob = new Blob([ab], { type: mimeType });
    const url = URL.createObjectURL(blob);
    
    const link = document.createElement('a');
    link.href = url;
    link.download = filename;
    document.body.appendChild(link); 
    
    link.click();
    
    document.body.removeChild(link);
    URL.revokeObjectURL(url);
    writeToScreen(`<span style="color: green;">FILE DOWNLOAD INITIATED: **${filename}**</span>`);
}

// --- UTILITY FUNCTIONS ---

function doSend(message) {
    writeToScreen(`SENT: ${message}`);
    websocket.send(message);
}

function writeToScreen(message) {
    output.insertAdjacentHTML("afterbegin", `<p>${message}</p>`);
}

function onClickButton() {
    const text = textarea.value;
    text && doSend(text);
    textarea.value = "";
    textarea.focus();
}