#include <SPI.h>

/*  Install the "TFT_eSPI" library by Bodmer to interface with the TFT Display - https://github.com/Bodmer/TFT_eSPI
    *** IMPORTANT: User_Setup.h available on the internet will probably NOT work with the examples available at Random Nerd Tutorials ***
    *** YOU MUST USE THE User_Setup.h FILE PROVIDED IN THE LINK BELOW IN ORDER TO USE THE EXAMPLES FROM RANDOM NERD TUTORIALS ***
    FULL INSTRUCTIONS AVAILABLE ON HOW CONFIGURE THE LIBRARY: https://RandomNerdTutorials.com/cyd/ or https://RandomNerdTutorials.com/esp32-tft/   */
#include <TFT_eSPI.h>

// Install the "XPT2046_Touchscreen" library by Paul Stoffregen to use the Touchscreen - https://github.com/PaulStoffregen/XPT2046_Touchscreen
// Note: this library doesn't require further configuration
#include <XPT2046_Touchscreen.h>

// NimBLE library for advanced BLE scanning (install from: https://github.com/h2zero/NimBLE-Arduino)
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

TFT_eSPI tft = TFT_eSPI();

// Touchscreen pins
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define FONT_SIZE 2

// ===== STATE MANAGEMENT =====
enum ScreenState {
  SCREEN_MAIN_MENU = 0,
  SCREEN_BT_SCANNER,
  SCREEN_BMS,
  SCREEN_SETTINGS,
  SCREEN_OTHER,
  SCREEN_COUNT
};

ScreenState currentScreen = SCREEN_MAIN_MENU;

// ===== BT SCANNER STATE =====
enum BtScanState {
  BT_IDLE = 0,
  BT_READY,          // Scanner screen ready, waiting for user to press Start
  BT_SCANNING,
  BT_RESOLVING,      // Connecting to devices to resolve names
  BT_SHOWING_RESULTS,
  BT_SHOWING_DEVICE
};

BtScanState btState = BT_IDLE;
int currentDeviceIndex = 0;
int totalDevices = 0;

// ===== Device Data Structure =====
struct DeviceInfo {
  String name;
  String macAddress;
  int rssi;
  String manufacturer;
  String manufacturerId;
  String serviceUuids;
  String txPower;
  String appearance;
  String advInterval;
  String completeLocalName;
  bool hasScanRsp;
};

#define MAX_DEVICES 50
DeviceInfo devices[MAX_DEVICES];
int deviceCount = 0;

// ===== Manufacturer ID Lookup Table =====
struct ManufacturerInfo {
  uint16_t id;
  const char* name;
};

#define MANUFACTURER_COUNT 50
const ManufacturerInfo manufacturers[MANUFACTURER_COUNT] = {
  {0x000C, "Silicon Wave"},
  {0x000E, "Texas Instruments"},
  {0x0010, "Broadcom"},
  {0x0011, "Garmin"},
  {0x0014, "Bluegiga"},
  {0x0015, "Marvell"},
  {0x0025, "Samsung"},
  {0x0031, "CSR"},
  {0x0036, "Atheros"},
  {0x0037, "MediaTek"},
  {0x0046, "NXP"},
  {0x004C, "Apple"},
  {0x0055, "Free2move"},
  {0x005B, "Sony"},
  {0x005E, "GN Netcom"},
  {0x005F, "General Motors"},
  {0x005A, "Hewlett-Packard"},
  {0x0062, "Bluetooth SIG"},
  {0x0075, "Realtek"},
  {0x0078, "ASUSTek"},
  {0x0079, "Marantz"},
  {0x007B, "Bose"},
  {0x007C, "Suunto"},
  {0x007E, "Polar"},
  {0x0086, "Huawei"},
  {0x008C, "Saftronics"},
  {0x008E, "HF Technologies"},
  {0x0095, "LEGO"},
  {0x0097, "Inficon"},
  {0x0099, "Teledyne Lecroy"},
  {0x009B, "Valve"},
  {0x00A0, "OMRON"},
  {0x00C3, "i-SENS"},
  {0x00CD, "Amazon"},
  {0x00CF, "Xiaomi"},
  {0x00D1, "BEGA"},
  {0x00D7, "Apogee Instruments"},
  {0x00E6, "Amazon Lab126"},
  {0x00F0, "Zipcar"},
  {0x0103, "OnePlus"},
  {0x0105, "AIAIAI"},
  {0x0107, "mela"},
  {0x0111, "Sena"},
  {0x0146, "Apple"},
  {0x014A, "Volkswagen"},
  {0x014B, "Skoda"},
  {0x014C, "Ford"},
  {0x014E, "Honda"},
  {0x014F, "Hyundai"},
  {0x0153, "BMW"},
};

#define MANUFACTURER_COUNT (800)

// ===== BLE Callbacks =====
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // During scan, only collect data
    // During name resolution phase, allow name updates from SCAN_RSP
    bool isScanning = (btState == BT_SCANNING);
    bool isResolving = (btState == BT_SHOWING_RESULTS);
    if (!isScanning && !isResolving) return;
    
    // Check if we already have MAX_DEVICES
    if (deviceCount >= MAX_DEVICES) return;
    
    // Get MAC address (normalize format to "XX:XX:XX:XX:XX:XX")
    String mac = advertisedDevice.getAddress().toString();
    // Ensure consistent format
    if (mac.length() == 12) {
      // Convert "AABBCCDDEEFF" to "AA:BB:CC:DD:EE:FF"
      String normalized = "";
      for (int i = 0; i < 12; i += 2) {
        if (i > 0) normalized += ":";
        normalized += mac.substring(i, i+2);
      }
      mac = normalized;
    }
    
    // Get name from this packet (SCAN_RSP often has names when advertising doesn't)
    String name = advertisedDevice.getName();
    if (name.length() == 0) name = "Unknown";
    
    // Serial debug
    Serial.print("[BLE] ");
    Serial.print(isResolving ? "RESOLVE" : "SCAN");
    Serial.print(" ");
    Serial.print(mac);
    Serial.print(" name=\"");
    Serial.print(name.c_str());
    Serial.print("\" rssi=");
    Serial.println(advertisedDevice.getRSSI());
    
    // Find or add device (dedup by MAC)
    bool found = false;
    for (int i = 0; i < deviceCount; i++) {
      if (devices[i].macAddress == mac) {
        found = true;
        int rssi = advertisedDevice.getRSSI();
        // Update with stronger RSSI (more negative is stronger)
        if (rssi < devices[i].rssi) {
          devices[i].rssi = rssi;
        }
        // If this device had no name yet, take the name from this packet
        // SCAN_RSP packets often carry the name when the initial advertising packet doesn't
        if (name != "Unknown" && devices[i].name == "Unknown") {
          Serial.print("[BLE] NAME RESOLVED: ");
          Serial.println(name);
          devices[i].name = name;
        } else if (name != "Unknown" && devices[i].name != "Unknown") {
          // Update with potentially better name if different
          Serial.print("[BLE] NAME UPDATE: ");
          Serial.print(devices[i].name);
          Serial.print(" -> ");
          Serial.println(name);
          devices[i].name = name;
        }
        break;
      }
    }
    
    if (!found) {
      devices[deviceCount].name = name;
      devices[deviceCount].macAddress = mac;
      devices[deviceCount].rssi = advertisedDevice.getRSSI();
      devices[deviceCount].manufacturer = "";
      devices[deviceCount].manufacturerId = "";
      devices[deviceCount].serviceUuids = "";
      devices[deviceCount].txPower = "";
      devices[deviceCount].appearance = "";
      devices[deviceCount].advInterval = "";
      devices[deviceCount].hasScanRsp = false;
      Serial.print("[BLE] NEW: ");
      Serial.println(name);
      deviceCount++;
    }
  }
};

// ===== Post-Scan Name Resolution =====
// Phase 1: Passive SCAN_RSP scan for names in scan response packets
// Phase 2: Connect to "Unknown" devices to read their GAP Device Name

void resolveDeviceNames() {
  // Phase 1: Passive SCAN_RSP scan
  Serial.println("[BLE] Starting name resolve scan...");
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(99);
  pScan->start(1, false);
  delay(1100);
  pScan->stop();
  Serial.println("[BLE] Passive scan complete");
  
  // Count unknown devices
  int unknownCount = 0;
  for (int i = 0; i < deviceCount; i++) {
    if (devices[i].name == "Unknown") {
      unknownCount++;
    }
  }
  
  if (unknownCount == 0) return;
  
  // Phase 2: Connect to each unknown device to resolve its name
  btState = BT_RESOLVING;
  int resolved = 0;
  int attempts = 0;
  int maxResolve = min(unknownCount, 10);  // Limit to 10 devices
  
  Serial.print("[BLE] Connecting to ");
  Serial.print(maxResolve);
  Serial.println(" device(s) to resolve names...");
  
  for (int i = 0; i < deviceCount && attempts < maxResolve; i++) {
    if (devices[i].name != "Unknown") continue;
    
    attempts++;
    
    // Show progress
    tft.fillScreen(0x000000);
    tft.setTextColor(0xFFFFFF);
    tft.setTextDatum(TC_DATUM);
    tft.drawCentreString("Resolving names...", SCREEN_WIDTH/2, 60, 2);
    tft.setTextColor(0x00E5FF);
    String progress = String(attempts) + "/" + String(maxResolve);
    tft.drawCentreString(progress, SCREEN_WIDTH/2, 90, 2);
    tft.setTextColor(0xAAAAAA);
    String macLabel = devices[i].macAddress;
    if (macLabel.length() > 17) macLabel = macLabel.substring(0, 17) + "..";
    tft.drawCentreString(macLabel, SCREEN_WIDTH/2, 120, 1);
    
    // Draw progress bar
    int barW = 200;
    int filledW = map(attempts, 1, maxResolve, 0, barW);
    tft.fillRoundRect(SCREEN_WIDTH/2 - barW/2, 145, barW, 8, 4, 0x303030);
    tft.fillRoundRect(SCREEN_WIDTH/2 - barW/2, 145, filledW, 8, 4, 0x00E5FF);
    
    // Create client and connect (try public, then random)
    BLEClient* pClient = BLEDevice::createClient();
    if (!pClient) {
      Serial.print("[BLE] Failed to create client for: ");
      Serial.println(devices[i].macAddress);
      continue;
    }
    
    BLEAddress addr(devices[i].macAddress.c_str(), BLE_ADDR_PUBLIC);
    bool connected = pClient->connect(addr, false, false, false);
    
    if (!connected) {
      // Try random address type
      BLEAddress addrRandom(devices[i].macAddress.c_str(), BLE_ADDR_RANDOM);
      connected = pClient->connect(addrRandom, false, false, false);
    }
    
    if (!connected) {
      Serial.print("[BLE] Connect failed: ");
      Serial.println(devices[i].macAddress);
      BLEDevice::deleteClient(pClient);
      delay(200);
      continue;
    }
    
    // Find GAP service (0x1800) and Device Name characteristic (0x2A00)
    BLERemoteService* pSvc = pClient->getService(BLEUUID("0x1800"));
    if (pSvc) {
      BLERemoteCharacteristic* pChar = pSvc->getCharacteristic(BLEUUID("0x2A00"));
      if (pChar && pChar->canRead()) {
        BLEAttValue value = pChar->readValue();
        if (value.size() > 0) {
          std::string nameStr(reinterpret_cast<const char*>(value.data()), value.size());
          String newName = String(nameStr.c_str());
          Serial.print("[BLE] CONNECT RESOLVED: ");
          Serial.println(newName);
          devices[i].name = newName;
          resolved++;
        }
      }
    }
    
    // Disconnect and cleanup
    pClient->disconnect();
    BLEDevice::deleteClient(pClient);
    delay(200);
  }
  
  Serial.print("[BLE] Resolved ");
  Serial.print(resolved);
  Serial.println(" device name(s) via connection");
  
  // Update display
  btState = BT_SHOWING_RESULTS;
}

// ===== UI Drawing Functions =====

// Helper: get menu item bounding box
void getMenuBounds(int index, int &x1, int &y1, int &x2, int &y2) {
  int cols = 2;
  int rows = 2;
  int marginX = 15;
  int marginY = 15;
  int itemW = (SCREEN_WIDTH - 2 * marginX) / cols;
  int itemH = (SCREEN_HEIGHT - 2 * marginY) / rows;
  
  int col = index % cols;
  int row = index / cols;
  x1 = marginX + col * itemW;
  y1 = marginY + row * itemH;
  x2 = x1 + itemW;
  y2 = y1 + itemH;
}

// Helper: get touch coordinates
bool getTouchCoords(int &tx, int &ty) {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    tx = map(p.x, 200, 3700, 0, SCREEN_WIDTH);
    ty = map(p.y, 240, 3800, 0, SCREEN_HEIGHT);
    return true;
  }
  return false;
}

// Helper: draw icon
void drawMenuIcon(int index, int cx, int cy, bool highlighted) {
  int iconSize = min((SCREEN_WIDTH - 30) / 2, (SCREEN_HEIGHT - 30) / 2) / 3;
  
  if (highlighted) {
    int x1, y1, x2, y2;
    getMenuBounds(index, x1, y1, x2, y2);
    tft.fillRoundRect(x1 + 5, y1 + 5, x2 - x1 - 10, y2 - y1 - 10, 10, 0x404040);
  }
  
  tft.setTextDatum(MC_DATUM);
  
  switch (index) {
    case 0: { // Bluetooth
      int color = 0x00E5FF;
      int s = iconSize / 2;
      tft.drawLine(cx, cy - s, cx, cy - s/2, color);
      tft.drawLine(cx, cy + s/2, cx, cy + s, color);
      tft.fillTriangle(cx - 5, cy - s, cx + s, cy, cx - 5, cy, color);
      tft.fillTriangle(cx - 5, cy + s, cx + s, cy, cx - 5, cy, color);
      break;
    }
    case 1: { // Battery
      int color = 0x00E676;
      int w = iconSize * 2/3;
      int h = iconSize * 2/3;
      tft.fillRoundRect(cx - w/2, cy - h/2, w, h, 3, color);
      tft.fillRect(cx - w/6, cy - h/2 - h/6, w/3, h/6, color);
      tft.fillRoundRect(cx - w/2 + 3, cy - h/2 + 3, w - 6, h - 6, 2, 0x000000);
      tft.fillRoundRect(cx - w/2 + 5, cy - h/2 + 5, w - 10, h/2 - 5, 2, color);
      break;
    }
    case 2: { // Gear
      int color = 0xFF9800;
      int r = iconSize / 2;
      tft.fillCircle(cx, cy, r, color);
      tft.fillCircle(cx, cy, r - 4, 0x000000);
      for (int i = 0; i < 8; i++) {
        float angle = i * 45.0 * 3.14159 / 180.0;
        int x1 = cx + (int)(r * 1.2 * cos(angle));
        int y1 = cy + (int)(r * 1.2 * sin(angle));
        int x2 = cx + (int)(r * 1.5 * cos(angle));
        int y2 = cy + (int)(r * 1.5 * sin(angle));
        tft.drawLine(x1, y1, x2, y2, color);
      }
      tft.fillCircle(cx, cy, r/3, 0x000000);
      break;
    }
    case 3: { // Question mark
      int color = 0xE040FB;
      tft.fillCircle(cx, cy, iconSize/2, color);
      tft.setTextColor(0x000000);
      tft.drawCentreString("?", cx, cy + 2, 2);
      break;
    }
  }
}

// Draw main menu
void drawMainMenu() {
  tft.fillScreen(0x000000);
  tft.setTextColor(0xFFFFFF);
  tft.setTextDatum(TC_DATUM);
  tft.drawCentreString("ESP32 CYD Menu", SCREEN_WIDTH/2, 8, 2);
  
  for (int i = 0; i < 4; i++) {
    int cx, cy;
    int x1, y1, x2, y2;
    getMenuBounds(i, x1, y1, x2, y2);
    cx = (x1 + x2) / 2;
    cy = (y1 + y2) / 2;
    
    drawMenuIcon(i, cx, cy - 15, false);
    
    const char* labels[] = {"BT Scanner", "BMS", "Settings", "Other"};
    tft.setTextDatum(BC_DATUM);
    tft.drawCentreString(labels[i], cx, y2 - 20, 2);
  }
}

// Draw back button area
bool drawBackButton() {
  // Draw back button
  tft.fillRoundRect(5, 35, 50, 30, 5, 0x404040);
  tft.setTextColor(0xFFFFFF);
  tft.setTextDatum(MC_DATUM);
  tft.drawCentreString("< Back", 30, 50, 2);
  
  return true;
}

// Check if touch is on back button
bool isTouchOnBackButton(int tx, int ty) {
  return (tx >= 5 && tx <= 55 && ty >= 35 && ty <= 65);
}

// ===== SUB-PROGRAM SCREENS =====

// Draw BT scanner - ready state (waiting for user to press Start)
void drawBtReady() {
  tft.fillScreen(0x000000);
  tft.setTextColor(0xFFFFFF);
  tft.setTextDatum(TC_DATUM);
  tft.drawCentreString("BLE Scanner", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 50, 2);
  tft.drawCentreString("Tap Start to begin scan", SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 2);
  drawBackButton();
  
  // Draw Start button
  tft.fillRoundRect(SCREEN_WIDTH/2 - 70, SCREEN_HEIGHT/2 + 30, 140, 45, 8, 0x00E5FF);
  tft.setTextColor(0x000000);
  tft.drawCentreString("START", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 52, 2);
}

// Draw BT scanner - scanning state
void drawBtScanning() {
  tft.fillScreen(0x000000);
  tft.setTextColor(0xFFFFFF);
  tft.setTextDatum(TC_DATUM);
  tft.drawCentreString("Scanning...", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 30, 2);
  tft.drawCentreString(String(deviceCount) + " devices found", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 10, 2);
  tft.drawCentreString("Tap to stop", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 30, 2);
  drawBackButton();
}

// Draw BT scanner - single device card (one item per page)
void drawBtResults() {
  tft.fillScreen(0x000000);
  tft.setTextColor(0xFFFFFF);
  tft.setTextDatum(TC_DATUM);
  tft.drawCentreString("Devices Found", SCREEN_WIDTH/2, 8, 2);
  drawBackButton();
  
  if (deviceCount == 0) {
    tft.drawCentreString("No devices found.", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 10, 2);
    tft.drawCentreString("Try again later.", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 10, 2);
    return;
  }
  
  // Current device card
  int idx = currentDeviceIndex;
  String name = devices[idx].name;
  String mac = devices[idx].macAddress;
  int rssiVal = devices[idx].rssi;
  
  if (name.length() == 0 || name == "Unknown" || name == "Unknown Device") {
    name = "Unknown";
  }
  
  // Device icon (centered)
  tft.fillCircle(SCREEN_WIDTH/2, 55, 25, 0x00E5FF);
  tft.fillTriangle(SCREEN_WIDTH/2, 40, SCREEN_WIDTH/2 + 12, 60, SCREEN_WIDTH/2 - 12, 60, 0x000000);
  tft.fillTriangle(SCREEN_WIDTH/2, 70, SCREEN_WIDTH/2 + 12, 50, SCREEN_WIDTH/2 - 12, 50, 0x000000);
  
  // Device name
  if (name.length() > 22) name = name.substring(0, 22) + "..";
  tft.setTextColor(0x00E5FF);
  tft.drawCentreString(name, SCREEN_WIDTH/2, 95, 2);
  
  // MAC address
  tft.setTextColor(0xAAAAAA);
  tft.drawCentreString(mac, SCREEN_WIDTH/2, 125, 1);
  
  // RSSI display
  tft.setTextColor(0xFFFFFF);
  tft.drawCentreString(String(rssiVal) + " dBm", SCREEN_WIDTH/2, 150, 2);
  
  // RSSI bar
  int rssiAbs = abs(rssiVal);
  int barW = 200;
  int filledW = map(rssiAbs, 30, 90, 0, barW);
  tft.fillRoundRect(SCREEN_WIDTH/2 - barW/2, 175, barW, 8, 4, 0x303030);
  tft.fillRoundRect(SCREEN_WIDTH/2 - barW/2, 175, filledW, 8, 4, 0x00E5FF);
  
  // Page indicator
  tft.setTextColor(0x808080);
  tft.drawCentreString(String(idx + 1) + "/" + String(deviceCount), SCREEN_WIDTH/2, 195, 1);
  
  // Navigation bar
  tft.fillRoundRect(0, SCREEN_HEIGHT - 45, SCREEN_WIDTH, 45, 5, 0x202020);
  
  // Previous button
  bool canPrev = idx > 0;
  int prevX = 10;
  tft.fillRoundRect(prevX, SCREEN_HEIGHT - 40, 70, 35, 5, canPrev ? 0x404040 : 0x202020);
  tft.setTextColor(canPrev ? 0xFFFFFF : 0x808080);
  tft.drawCentreString("< Prev", prevX + 35, SCREEN_HEIGHT - 22, 2);
  
  // Next button
  bool canNext = idx < deviceCount - 1;
  int nextX = SCREEN_WIDTH - 80;
  tft.fillRoundRect(nextX, SCREEN_HEIGHT - 40, 70, 35, 5, canNext ? 0x404040 : 0x202020);
  tft.setTextColor(canNext ? 0xFFFFFF : 0x808080);
  tft.drawCentreString("Next >", nextX + 35, SCREEN_HEIGHT - 22, 2);
  
  // Tap device card area to see details
  tft.drawCentreString("Tap device for details", SCREEN_WIDTH/2, 215, 1);
}

// Draw BT scanner - single device detail
void drawBtDevice(int index) {
  tft.fillScreen(0x000000);
  tft.setTextColor(0xFFFFFF);
  tft.setTextDatum(TC_DATUM);
  
  // Header bar
  tft.fillRect(0, 0, SCREEN_WIDTH, 30, 0x00E5FF);
  tft.setTextColor(0x000000);
  String name = devices[index].name;
  if (name.length() > 18) name = name.substring(0, 18) + "..";
  if (name.length() == 0) name = "Unknown Device";
  tft.drawCentreString(name, SCREEN_WIDTH/2, 8, 2);
  
  // Back button
  tft.fillRoundRect(5, 35, 50, 30, 5, 0x404040);
  tft.setTextColor(0xFFFFFF);
  tft.setTextDatum(MC_DATUM);
  tft.drawCentreString("< Back", 30, 50, 2);
  
  // MAC address (below back button)
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(0xAAAAAA);
  String mac = devices[index].macAddress;
  if (mac.length() > 24) mac = mac.substring(0, 24);
  tft.drawString("MAC: " + mac, 15, 72, 2);
  
  // Divider
  tft.drawLine(10, 90, SCREEN_WIDTH - 10, 90, 0x404040);
  
  // RSSI display
  int y = 110;
  int lineH = 20;
  int rssiVal = devices[index].rssi;
  
  tft.setTextColor(0xFFFFFF);
  tft.drawString("RSSI:", 15, y, 2);
  tft.drawCentreString(String(rssiVal) + " dBm", SCREEN_WIDTH/2, y, 2);
  
  // RSSI bar
  int rssiAbs = abs(rssiVal);
  int barW = 220;
  int filledW = map(rssiAbs, 30, 90, 0, barW);
  tft.fillRoundRect(15, y + 18, barW, 8, 4, 0x303030);
  tft.fillRoundRect(15, y + 18, filledW, 8, 4, 0x00E5FF);
  y += 45;
  
  // Manufacturer (if known)
  if (devices[index].manufacturer.length() > 0) {
    tft.drawString("Manufacturer:", 15, y, 2);
    String mfr = devices[index].manufacturer;
    if (mfr.length() > 18) mfr = mfr.substring(0, 18);
    tft.drawCentreString(mfr, SCREEN_WIDTH/2, y, 2);
    y += lineH;
  }
  
  // Mfr ID (if known)
  if (devices[index].manufacturerId.length() > 0) {
    tft.drawString("Mfr ID:", 15, y, 2);
    tft.drawCentreString(devices[index].manufacturerId, SCREEN_WIDTH/2, y, 2);
    y += lineH;
  }
  
  // Service UUIDs (if known)
  if (devices[index].serviceUuids.length() > 0) {
    tft.drawString("UUIDs:", 15, y, 2);
    String uuids = devices[index].serviceUuids;
    if (uuids.length() > 18) uuids = uuids.substring(0, 18) + "..";
    tft.drawCentreString(uuids, SCREEN_WIDTH/2, y, 2);
    y += lineH;
  }
  
  // TX Power (if known)
  if (devices[index].txPower.length() > 0) {
    tft.drawString("TX Power:", 15, y, 2);
    tft.drawCentreString(devices[index].txPower + " dBm", SCREEN_WIDTH/2, y, 2);
    y += lineH;
  }
  
  // Page indicator (bottom bar)
  tft.fillRoundRect(0, SCREEN_HEIGHT - 45, SCREEN_WIDTH, 45, 5, 0x202020);
  tft.setTextColor(0x808080);
  tft.drawCentreString(String(index + 1) + "/" + String(deviceCount), SCREEN_WIDTH/2, SCREEN_HEIGHT - 22, 1);
  
  // Prev button
  bool canPrev = index > 0;
  int prevX = 10;
  tft.fillRoundRect(prevX, SCREEN_HEIGHT - 40, 70, 35, 5, canPrev ? 0x404040 : 0x202020);
  tft.setTextColor(canPrev ? 0xFFFFFF : 0x808080);
  tft.drawCentreString("< Prev", prevX + 35, SCREEN_HEIGHT - 22, 2);
  
  // Next button
  bool canNext = index < deviceCount - 1;
  int nextX = SCREEN_WIDTH - 80;
  tft.fillRoundRect(nextX, SCREEN_HEIGHT - 40, 70, 35, 5, canNext ? 0x404040 : 0x202020);
  tft.setTextColor(canNext ? 0xFFFFFF : 0x808080);
  tft.drawCentreString("Next >", nextX + 35, SCREEN_HEIGHT - 22, 2);
}

// Draw placeholder for other sub-programs
void drawPlaceholder(String title, int color) {
  tft.fillScreen(0x000000);
  
  // Header bar
  tft.fillRect(0, 0, SCREEN_WIDTH, 30, color);
  tft.setTextColor(0xFFFFFF);
  tft.setTextDatum(TC_DATUM);
  tft.drawCentreString(title, SCREEN_WIDTH/2, 8, 2);
  
  drawBackButton();
  
  tft.setTextColor(0xFFFFFF);
  tft.drawCentreString("Coming Soon", SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 2);
}

// ===== SCAN CONTROL =====

// Request SCAN_RSP for all devices
void requestScanRsp() {
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setActiveScan(true); // Active scan triggers SCAN_RSP
  // The scan callback in onResult already handles merging data
}

// Show results after scan completes
void showBtResults() {
  // Resolve device names from SCAN_RSP (many devices only broadcast name here)
  resolveDeviceNames();
  
  Serial.println("[BLE] === Scan complete ===");
  Serial.print("[BLE] Total devices: ");
  Serial.println(deviceCount);
  for (int i = 0; i < deviceCount; i++) {
    Serial.print("[BLE] ");
    Serial.print(i);
    Serial.print(": \"");
    Serial.print(devices[i].name);
    Serial.print("\" ");
    Serial.print(devices[i].macAddress);
    Serial.print(" rssi=");
    Serial.println(devices[i].rssi);
  }
  
  // Sort devices by RSSI (strongest first = most negative RSSI values first)
  for (int i = 0; i < deviceCount - 1; i++) {
    for (int j = i + 1; j < deviceCount; j++) {
      if (devices[j].rssi < devices[i].rssi) {  // More negative = stronger
        DeviceInfo temp = devices[i];
        devices[i] = devices[j];
        devices[j] = temp;
      }
    }
  }
  
  totalDevices = deviceCount;
  currentDeviceIndex = 0;
  
  // Always show results screen (even if 0 devices)
  btState = BT_SHOWING_RESULTS;
  drawBtResults();
}

// Perform a BLE scan
void performBtScan() {
  btState = BT_SCANNING;
  deviceCount = 0;
  
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  drawBtScanning();
  
  // Scan for 10 seconds
  BLEScanResults* results = pBLEScan->start(10, false);
  
  // Stop scanning
  pBLEScan->stop();
  
  showBtResults();
}

// Stop scanning and show results (called when user taps to stop)
void stopBtScan() {
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->stop();
  showBtResults();
}

// ===== MAIN LOOP =====

void setup() {
  Serial.begin(115200);
  
  // Touchscreen setup
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(1);
  
  // TFT setup
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(0x000000);
  tft.setTextColor(0xFFFFFF);
  
  drawMainMenu();
}

void loop() {
  int tx, ty;
  
  if (getTouchCoords(tx, ty)) {
    // Skip touch handling during name resolution (blocking operation)
    if (btState == BT_RESOLVING) {
      while (touchscreen.touched()) {
        touchscreen.getPoint();
        delay(10);
      }
      delay(200);
      return;
    }
    
    Serial.print("Touch: ");
    Serial.print(tx);
    Serial.print(", ");
    Serial.println(ty);
    
    switch (currentScreen) {
      case SCREEN_MAIN_MENU: {
        // Check menu item touches
        bool touched = false;
        for (int i = 0; i < 4; i++) {
          int x1, y1, x2, y2;
          getMenuBounds(i, x1, y1, x2, y2);
          if (tx >= x1 && tx <= x2 && ty >= y1 && ty <= y2) {
            Serial.print("Menu: ");
            Serial.println(i);
            
            switch (i) {
              case 0: // BT Scanner
                btState = BT_READY;
                currentScreen = SCREEN_BT_SCANNER;
                drawBtReady();
                break;
              case 1: // BMS
                currentScreen = SCREEN_BMS;
                drawPlaceholder("BMS", 0x00E676);
                break;
              case 2: // Settings
                currentScreen = SCREEN_SETTINGS;
                drawPlaceholder("Settings", 0xFF9800);
                break;
              case 3: // Other
                currentScreen = SCREEN_OTHER;
                drawPlaceholder("Other", 0xE040FB);
                break;
            }
            touched = true;
            break;
          }
        }
        break;
      }
      
      case SCREEN_BT_SCANNER: {
        // Check back button
        if (isTouchOnBackButton(tx, ty)) {
          currentScreen = SCREEN_MAIN_MENU;
          btState = BT_IDLE;
          deviceCount = 0;
          drawMainMenu();
          break;
        }
        
        switch (btState) {
          case BT_READY: {
            // Check if user tapped the START button
            int btnCenterX = SCREEN_WIDTH / 2;
            int btnCenterY = SCREEN_HEIGHT / 2 + 52;
            int btnH = 45;
            if (tx > btnCenterX - 75 && tx < btnCenterX + 75 && 
                ty > btnCenterY - btnH/2 && ty < btnCenterY + btnH/2) {
              performBtScan();
            }
            break;
          }
          
          case BT_SCANNING: {
            // Tap to stop scan early
            stopBtScan();
            break;
          }
          
          case BT_SHOWING_RESULTS: {
            // Check prev button
            if (currentDeviceIndex > 0 && tx >= 10 && tx <= 80 && 
                ty >= SCREEN_HEIGHT - 40 && ty <= SCREEN_HEIGHT - 5) {
              currentDeviceIndex--;
              drawBtResults();
              break;
            }
            
            // Check next button
            if (currentDeviceIndex < deviceCount - 1 && 
                tx >= SCREEN_WIDTH - 80 && tx <= SCREEN_WIDTH - 10 && 
                ty >= SCREEN_HEIGHT - 40 && ty <= SCREEN_HEIGHT - 5) {
              currentDeviceIndex++;
              drawBtResults();
              break;
            }
            
            // Tap on device card (center area) to see details
            if (tx > 30 && tx < SCREEN_WIDTH - 30 && ty > 30 && ty < 200) {
              btState = BT_SHOWING_DEVICE;
              drawBtDevice(currentDeviceIndex);
            }
            break;
          }
          
          case BT_SHOWING_DEVICE: {
            // Check back button (top-left, below header)
            if (isTouchOnBackButton(tx, ty)) {
              btState = BT_SHOWING_RESULTS;
              drawBtResults();
              break;
            }
            
            // Check prev button
            if (currentDeviceIndex > 0 && tx >= 10 && tx <= 80 && 
                ty >= SCREEN_HEIGHT - 40 && ty <= SCREEN_HEIGHT - 5) {
              currentDeviceIndex--;
              drawBtDevice(currentDeviceIndex);
              break;
            }
            
            // Check next button
            if (currentDeviceIndex < deviceCount - 1 && 
                tx >= SCREEN_WIDTH - 80 && tx <= SCREEN_WIDTH - 10 && 
                ty >= SCREEN_HEIGHT - 40 && ty <= SCREEN_HEIGHT - 5) {
              currentDeviceIndex++;
              drawBtDevice(currentDeviceIndex);
              break;
            }
            break;
          }
        }
        break;
      }
      
      case SCREEN_BMS:
      case SCREEN_SETTINGS:
      case SCREEN_OTHER: {
        // Check back button for all sub-programs
        if (isTouchOnBackButton(tx, ty)) {
          currentScreen = SCREEN_MAIN_MENU;
          drawMainMenu();
        }
        break;
      }
    }
    
    // Wait for touch release
    while (touchscreen.touched()) {
      touchscreen.getPoint();
      delay(10);
    }
    delay(200);
  }
}
