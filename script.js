let sessionId = null;
let currentPath = "/";
let selectedItem = { path: null, name: null, isDirectory: false };

// --- CORE UI AND API FUNCTIONS ---

async function apiCall(endpoint, method = 'GET', body = null) {
    const headers = { 'Content-Type': 'application/json' };
    if (sessionId) {
        headers['X-Session-ID'] = sessionId;
    }
    try {
        const options = { method, headers };
        if (body) {
            options.body = JSON.stringify({ parameters: body });
        }
        const response = await fetch(`http://localhost:8080${endpoint}`, options);
        const responseData = await response.json();
        
        if (responseData.status === 'success') {
            const successMsg = responseData.data?.message || `Operation ${endpoint} successful!`;
            showToast(successMsg, 'success');
        } else {
            const errorMsg = responseData.error_message || `Operation ${endpoint} failed.`;
            showToast(errorMsg, 'error');
        }
        
        return responseData;
    } catch (error) {
        console.error('Fetch error:', error);
        const errorResponse = { status: 'error', error_message: `Failed to connect to server at ${endpoint}` };
        showToast(errorResponse.error_message, 'error');
        return errorResponse;
    }
}

function showToast(message, type = 'success') {
    const container = document.getElementById('toast-container');
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.textContent = message;
    container.appendChild(toast);
    setTimeout(() => { toast.classList.add('show'); }, 10);
    setTimeout(() => {
        toast.classList.remove('show');
        setTimeout(() => { container.removeChild(toast); }, 300);
    }, 3000);
}

function showInContentArea(data) {
    document.getElementById('content-area').textContent = JSON.stringify(data, null, 2);
}

function updateFileBrowser(entries) {
    const browser = document.getElementById('fileBrowser').querySelector('ul');
    browser.innerHTML = '';
    document.getElementById('renameBtn').disabled = true;
    document.getElementById('deleteBtn').disabled = true;

    if (currentPath !== "/") {
        const li = document.createElement('li');
        li.textContent = '.. (Up a level)';
        li.className = 'directory';
        li.onclick = () => {
            const parts = currentPath.split('/').filter(p => p);
            parts.pop();
            const parentPath = '/' + parts.join('/');
            listDirectory(parentPath);
        };
        browser.appendChild(li);
    }
    
    entries.sort((a, b) => (a.is_directory === b.is_directory) ? a.name.localeCompare(b.name) : (a.is_directory ? -1 : 1));

    entries.forEach(entry => {
        const li = document.createElement('li');
        li.textContent = entry.name;
        li.className = entry.is_directory ? 'directory' : 'file';
        const fullPath = (currentPath === '/' ? '' : currentPath) + '/' + entry.name;

        li.onclick = () => {
            Array.from(browser.children).forEach(child => child.classList.remove('selected'));
            li.classList.add('selected');
            selectedItem = { path: fullPath, name: entry.name, isDirectory: entry.is_directory };
            document.getElementById('pathInput').value = entry.name;
            document.getElementById('renameBtn').disabled = false;
            document.getElementById('deleteBtn').disabled = false;
        };

        if (entry.is_directory) {
            li.ondblclick = () => listDirectory(fullPath);
        } else {
            li.ondblclick = () => readFile(fullPath);
        }
        browser.appendChild(li);
    });
}

// --- AUTHENTICATION FUNCTIONS ---
async function loginUser() {
    const username = document.getElementById('username').value;
    const password = document.getElementById('password').value;
    const response = await apiCall('/user_login', 'POST', { username, password });
    if (response.status === 'success') {
        sessionId = response.data.session_id;
        document.getElementById('login-view').style.display = 'none';
        document.getElementById('main-view').style.display = 'block';
        
        const sessionInfo = await apiCall('/get_session_info', 'GET');
        if (sessionInfo.status === 'success') {
            document.getElementById('currentUser').textContent = sessionInfo.data.username;
            const roleText = sessionInfo.data.role === 1 ? 'Admin' : 'User';
            document.getElementById('userRole').textContent = roleText;
            if (sessionInfo.data.role === 1) {
                document.getElementById('adminSection').style.display = 'block';
            }
        }
        listDirectory('/');
    }
}

async function logoutUser() {
    await apiCall('/user_logout', 'POST');
    sessionId = null;
    window.location.reload(); // Simple way to reset the UI
}

// --- FILE & DIRECTORY FUNCTIONS ---
async function listDirectory(path = '/') {
    currentPath = path;
    document.getElementById('currentPath').value = currentPath;
    selectedItem = { path: null, name: null, isDirectory: false };
    document.getElementById('pathInput').value = '';

    const response = await apiCall('/dir_list', 'POST', { path });
    if (response.status === 'success') {
        updateFileBrowser(response.data);
    }
}

async function createDirectory() {
    const newDirName = document.getElementById('pathInput').value;
    if (!newDirName) { showToast('Please enter a directory name.', 'error'); return; }
    const path = (currentPath === '/' ? '' : currentPath) + '/' + newDirName;
    const response = await apiCall('/dir_create', 'POST', { path });
    if (response.status === 'success') listDirectory(currentPath);
}

async function createFile() {
    const newFileName = document.getElementById('pathInput').value;
    if (!newFileName) { showToast('Please enter a file name.', 'error'); return; }
    const content = prompt("Enter content for the new file:", "");
    if (content !== null) {
        const path = (currentPath === '/' ? '' : currentPath) + '/' + newFileName;
        const response = await apiCall('/file_create', 'POST', { path: path, data: content });
        if (response.status === 'success') listDirectory(currentPath);
    }
}

async function readFile(path) {
    const pathToRead = path || selectedItem.path;
    if (!pathToRead) { showToast('Please select a file to read.', 'error'); return; }
    const response = await apiCall('/file_read', 'POST', { path: pathToRead });
    if (response.status === 'success') {
        document.getElementById('content-area').textContent = response.data.content;
    } else {
        showInContentArea(response);
    }
}

async function deleteItem() {
    if (!selectedItem.path) { showToast('Please select an item to delete.', 'error'); return; }
    if (confirm(`Are you sure you want to delete "${selectedItem.name}"?`)) {
        const endpoint = selectedItem.isDirectory ? '/dir_delete' : '/file_delete';
        const response = await apiCall(endpoint, 'POST', { path: selectedItem.path });
        if (response.status === 'success') listDirectory(currentPath);
    }
}

async function renameItem() {
    if (!selectedItem.path) { showToast('Please select an item to rename.', 'error'); return; }
    const newName = prompt("Enter the new name for the item:", selectedItem.name);
    if (newName && newName !== selectedItem.name) {
        const parts = selectedItem.path.split('/');
        parts.pop();
        const newPath = (parts.join('/') || '/') + (parts.length > 1 ? '/' : '') + newName;
        const response = await apiCall('/file_rename', 'POST', { old_path: selectedItem.path, new_path: newPath });
        if (response.status === 'success') listDirectory(currentPath);
    }
}

// --- ADMIN FUNCTIONS ---
async function listUsers() {
    const response = await apiCall('/user_list', 'GET');
    if (response.status === 'success') {
        showInContentArea(response.data);
    }
}

async function createUser() {
    const username = document.getElementById('adminUserInput').value;
    if (!username) { showToast('Please enter a username in the admin panel.', 'error'); return; }
    const password = prompt(`Enter password for new user "${username}":`);
    const role = prompt(`Enter role for "${username}" (1 for Admin, 0 for User):`, "0");
    if (password) {
        await apiCall('/user_create', 'POST', { username, password, role: parseInt(role) });
    }
}

async function deleteUser() {
    const username = document.getElementById('adminUserInput').value;
    if (!username) { showToast('Please enter a username to delete.', 'error'); return; }
    if (confirm(`Are you sure you want to delete user "${username}"?`)) {
        await apiCall('/user_delete', 'POST', { username });
    }
}