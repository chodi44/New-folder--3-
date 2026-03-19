# EcoReward | IoT Smart Waste Reward System

This project contains a modern web application and a Node.js backend to bridge your physical IoT bin with the reward system.

## 🚀 Getting Started

### 1. Setup Backend
1. Open a terminal in this folder.
2. Install dependencies:
   ```bash
   npm install
   ```
3. Start the server:
   ```bash
   npm start
   ```
   *The server will run on `http://localhost:3000`.*

### 2. Run the Web App
1. Open `index.html` in your browser.
2. It is now configured to talk to your local backend.

### 3. Integrate IoT Bin
1. Use the code in `bin_code.cpp` for your ESP32/Arduino.
2. Update the `serverUrl` in the C++ code to your computer's IP address.
3. When the bin sends data to `/api/generate-qr`, it will get a token.
4. Display that token in a QR code for users to scan!

## 📂 File Structure
- `index.html` / `style.css` / `app.js`: The frontend web application.
- `server.js`: The central backend (Node.js/Express).
- `bin_code.cpp`: Reference code for your physical IoT bin hardware.
- `test_qr.png`: A sample QR code for testing the scanner.
