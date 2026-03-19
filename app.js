/**
 * EcoReward | IoT Smart Bin Scanner Application
 * Handles UI state, QR Scanning, and mocked backend communication.
 */

class App {
    constructor() {
        // Use relative path for production (Render) or absolute for local testing
        this.baseBackendUrl = window.location.origin + '/api/claim-reward';
        this.html5QrcodeScanner = null;
        this.currentUser = null;
        this.totalPoints = 0;
        this.isProcessing = false;

        // Used QR tokens store to simulate backend validation
        this.usedTokens = JSON.parse(localStorage.getItem('eco_used_tokens')) || [];

        this.initDOM();
        this.bindEvents();
        this.checkAuth();
    }

    initDOM() {
        // Sections
        this.sections = {
            auth: document.getElementById('authSection'),
            scanner: document.getElementById('scannerSection'),
            result: document.getElementById('resultSection')
        };

        // UI Elements
        this.userStats = document.getElementById('userStats');
        this.totalPointsDisplay = document.getElementById('totalPointsDisplay');
        this.scannerOverlay = document.getElementById('scannerOverlay');
        this.resultContent = document.getElementById('resultContent');
        this.toastContainer = document.getElementById('toastContainer');

        // Forms and Buttons
        this.loginForm = document.getElementById('loginForm');
        this.uidInput = document.getElementById('uidInput');
        this.manualTokenInput = document.getElementById('manualTokenInput');
        this.manualTokenBtn = document.getElementById('manualTokenBtn');
        this.logoutBtn = document.getElementById('logoutBtn');
        this.scanAgainBtn = document.getElementById('scanAgainBtn');
        this.stopScanBtn = document.getElementById('stopScanBtn');
    }

    bindEvents() {
        this.loginForm.addEventListener('submit', (e) => this.handleLogin(e));
        this.manualTokenBtn.addEventListener('click', () => this.handleManualToken());
        this.logoutBtn.addEventListener('click', () => this.handleLogout());
        this.scanAgainBtn.addEventListener('click', () => this.showSection('scanner'));
        this.stopScanBtn.addEventListener('click', () => {
             // Basic re-init for camera change
             if(this.html5QrcodeScanner) {
                this.html5QrcodeScanner.clear().then(() => {
                    this.initScanner();
                });
             }
        });
    }

    /* --- Auth & User State --- */

    checkAuth() {
        const savedUser = localStorage.getItem('eco_user_id');
        const savedPoints = localStorage.getItem('eco_user_points');

        if (savedUser) {
            this.currentUser = savedUser;
            this.totalPoints = parseInt(savedPoints) || 0;
            this.updateStatsUI();
            this.showSection('scanner');
        } else {
            this.showSection('auth');
        }
    }

    handleLogin(e) {
        e.preventDefault();
        const uid = this.uidInput.value.trim().toUpperCase();
        if (uid) {
            this.currentUser = uid;
            this.totalPoints = 0; // Starts at 0 for demo
            
            localStorage.setItem('eco_user_id', this.currentUser);
            localStorage.setItem('eco_user_points', this.totalPoints);
            
            this.updateStatsUI();
            this.showToast('Logged in successfully', 'success');
            this.showSection('scanner');
        }
    }

    handleLogout() {
        localStorage.removeItem('eco_user_id');
        localStorage.removeItem('eco_user_points');
        this.currentUser = null;
        this.totalPoints = 0;
        this.userStats.style.display = 'none';
        
        if (this.html5QrcodeScanner) {
            this.html5QrcodeScanner.clear().catch(console.error);
        }
        
        this.showSection('auth');
    }

    updateStatsUI() {
        this.userStats.style.display = 'flex';
        this.totalPointsDisplay.textContent = this.totalPoints;
    }

    /* --- UI Navigation --- */

    showSection(sectionName) {
        // Hide all
        Object.values(this.sections).forEach(sec => sec.style.display = 'none');
        
        // Setup states specific to sections
        if (sectionName === 'scanner') {
            this.initScanner();
        } else if (this.html5QrcodeScanner) {
            // Cleanup scanner if leaving section
            try { this.html5QrcodeScanner.clear(); } catch(e) {}
        }

        // Show requested
        this.sections[sectionName].style.display = 'block';
    }

    async handleManualToken() {
        const token = this.manualTokenInput.value.trim().toUpperCase();
        if (!token) return;
        
        this.isProcessing = true;
        this.scannerOverlay.style.display = 'flex';
        
        try {
            const response = await fetch(this.baseBackendUrl, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    uid: this.currentUser,
                    token: token
                })
            });

            const data = await response.json();
            this.handleScanResponse(data);
            this.manualTokenInput.value = ''; // Clear input
        } catch (error) {
            this.renderErrorResult("Failed to verify token.");
            this.showSection('result');
        } finally {
            this.isProcessing = false;
            this.scannerOverlay.style.display = 'none';
        }
    }

    /* --- Scanner Logic --- */

    initScanner() {
        if (this.html5QrcodeScanner) {
            try { this.html5QrcodeScanner.clear(); } catch(e) {}
        }

        this.isProcessing = false;
        this.scannerOverlay.style.display = 'none';

        // Initialize Html5QrcodeScanner
        this.html5QrcodeScanner = new Html5QrcodeScanner(
            "reader", 
            { 
                fps: 10, 
                qrbox: { width: 250, height: 250 }, 
                aspectRatio: 1.0,
                // Prefer back camera but allow selection
                rememberLastUsedCamera: true
            },
            /* verbose= */ false
        );


        this.html5QrcodeScanner.render(
            (decodedText, decodedResult) => this.onScanSuccess(decodedText, decodedResult),
            (errorMessage) => { /* Ignore background scanning errors */ }
        );
    }

    async onScanSuccess(decodedText, decodedResult) {
        // Prevent multiple scans while processing
        if (this.isProcessing) return;
        this.isProcessing = true;
        
        this.scannerOverlay.style.display = 'flex';
        
        try {
            // Pause scanner
            this.html5QrcodeScanner.pause(true);
            
            // Expected URL: https://yourserver.com/reward?uid=USER123&pts=10&type=metal&token=456789
            const url = new URL(decodedText);
            const params = new URLSearchParams(url.search);
            
            const uidInQR = params.get('uid'); // May or may not match logged in user depending on flow
            const pts = params.get('pts');
            const type = params.get('type');
            const token = params.get('token');

            if (!pts || !type || !token) {
                throw new Error("Invalid Smart Bin QR Code format.");
            }

            // Real fetch call to backend API
            const response = await fetch(this.baseBackendUrl, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    uid: this.currentUser || uidInQR || "ANON",
                    token: token
                })
            });

            const data = await response.json();
            this.handleScanResponse(data);

        } catch (error) {
            console.error("Scan error:", error);
            this.renderErrorResult(error.message || "Failed to parse QR code. Make sure it's a valid EcoReward code.");
            this.showSection('result');
        } finally {
            this.isProcessing = false;
            this.scannerOverlay.style.display = 'none';
        }
    }

    /* --- API & Responses --- */

    // Simulated Backend
    async mockBackendFetch(url, data) {
        return new Promise((resolve) => {
            setTimeout(() => {
                // Simulate backend validation
                if (this.usedTokens.includes(data.token)) {
                    resolve({
                        success: false,
                        message: "This QR code has already been claimed."
                    });
                } else {
                    // Mark token as used
                    this.usedTokens.push(data.token);
                    localStorage.setItem('eco_used_tokens', JSON.stringify(this.usedTokens));
                    
                    resolve({
                        success: true,
                        pointsEarned: data.pts,
                        wasteType: data.type,
                        message: "Points added successfully!"
                    });
                }
            }, 1200); // Simulate network delay
        });
    }

    handleScanResponse(data) {
        if (data.success) {
            // Update local state
            this.totalPoints += data.pointsEarned;
            localStorage.setItem('eco_user_points', this.totalPoints);
            this.updateStatsUI();
            
            this.renderSuccessResult(data);
        } else {
            this.renderErrorResult(data.message);
        }
        
        this.showSection('result');
    }

    /* --- View Rendering --- */

    renderSuccessResult(data) {
        const typeIcons = {
            'metal': 'fa-box-archive',
            'plastic': 'fa-bottle-water',
            'paper': 'fa-newspaper',
            'glass': 'fa-wine-bottle',
            'electronic': 'fa-plug',
            'default': 'fa-recycle'
        };
        
        const typeUpper = data.wasteType.charAt(0).toUpperCase() + data.wasteType.slice(1);
        const iconClass = typeIcons[data.wasteType.toLowerCase()] || typeIcons['default'];

        this.resultContent.innerHTML = `
            <i class="fa-solid fa-circle-check result-icon success"></i>
            <h2 class="result-title">Scan Successful!</h2>
            <p class="result-desc">${data.message}</p>
            
            <div class="result-card">
                <div class="result-row">
                    <span class="result-label">Waste Type</span>
                    <span class="result-value"><i class="fa-solid ${iconClass}"></i> ${typeUpper}</span>
                </div>
                <div class="result-row">
                    <span class="result-label">Points Earned</span>
                    <span class="result-value points-earn">+${data.pointsEarned} pts</span>
                </div>
            </div>
            
            <p style="color: var(--text-muted); font-size: 0.9rem;">
                Your new balance is <strong>${this.totalPoints} pts</strong>
            </p>
        `;
    }

    renderErrorResult(message) {
        this.resultContent.innerHTML = `
            <i class="fa-solid fa-circle-xmark result-icon error"></i>
            <h2 class="result-title">Scan Failed</h2>
            <p class="result-desc" style="color: var(--error);">${message}</p>
            <p style="color: var(--text-muted); font-size: 0.9rem; margin-top: 24px;">
                Please try scanning another valid smart bin QR code.
            </p>
        `;
    }

    showToast(message, type = 'success') {
        const toast = document.createElement('div');
        toast.className = `toast ${type}`;
        
        const icon = type === 'success' ? 'fa-check' : 'fa-circle-exclamation';
        toast.innerHTML = `<i class="fa-solid ${icon}"></i> <span>${message}</span>`;
        
        this.toastContainer.appendChild(toast);
        
        setTimeout(() => {
            toast.classList.add('toast-hiding');
            setTimeout(() => toast.remove(), 300);
        }, 3000);
    }
}

// Initialize application when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    window.ecoApp = new App();
});
