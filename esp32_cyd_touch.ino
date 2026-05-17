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
    if (btState != BT_SCANNING) return;
    
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
    
    // Get name (default to "Unknown" if empty)
    String name = advertisedDevice.getName();
    if (name.length() == 0) name = "Unknown";
    
    // Get RSSI
    int rssi = advertisedDevice.getRSSI();
    
    // Get manufacturer data safely (extract ID only, don't store raw data)
    String mfrData = advertisedDevice.getManufacturerData();
    String manufacturer = "Unknown";
    String mfrId = "N/A";
    if (mfrData.length() >= 2) {
      // Extract first 2 bytes as manufacturer ID (little-endian)
      uint16_t id = (uint8_t)mfrData[1] | ((uint8_t)mfrData[0] << 8);
      mfrId = "0x" + String(id, HEX);
      
      // Lookup manufacturer name from table
      for (int i = 0; i < MANUFACTURER_COUNT; i++) {
        if (manufacturers[i].id == id) {
          manufacturer = manufacturers[i].name;
          break;
        }
      }
    }
    
    // Find or add device (dedup by MAC)
    bool found = false;
    for (int i = 0; i < deviceCount; i++) {
      if (devices[i].macAddress == mac) {
        found = true;
        // Update with stronger RSSI (more negative is stronger, so use <)
        if (rssi < devices[i].rssi) {
          devices[i].rssi = rssi;
        }
        // Update name if we got one
        if (name != "Unknown" && devices[i].name == "Unknown") {
          devices[i].name = name;
        }
        // Update manufacturer info
        if (mfrId != "N/A") {
          devices[i].manufacturerId = mfrId;
          devices[i].manufacturer = manufacturer;
        }
        break;
      }
    }
    
    if (!found) {
      devices[deviceCount].name = name;
      devices[deviceCount].macAddress = mac;
      devices[deviceCount].rssi = rssi;
      devices[deviceCount].manufacturer = manufacturer;
      devices[deviceCount].manufacturerId = mfrId;
      devices[deviceCount].serviceUuids = "";
      devices[deviceCount].txPower = "";
      devices[deviceCount].appearance = "";
      devices[deviceCount].advInterval = "";
      devices[deviceCount].hasScanRsp = false;
      deviceCount++;
    }
  }
};

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

// Draw BT scanner - device list
void drawBtResults() {
  tft.fillScreen(0x000000);
  tft.setTextColor(0xFFFFFF);
  tft.setTextDatum(TC_DATUM);
  tft.drawCentreString("Devices Found", SCREEN_WIDTH/2, 8, 2);
  tft.drawCentreString(String(deviceCount) + " devices", SCREEN_WIDTH/2, 25, 2);
  drawBackButton();
  
  if (deviceCount == 0) {
    tft.drawCentreString("No devices found.", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 10, 2);
    tft.drawCentreString("Try again later.", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 10, 2);
  } else {
    // Draw device list with name, MAC, manufacturer + RSSI info
    int startY = 40;
    int lineHeight = 22;
    int visibleLines = (SCREEN_HEIGHT - startY - 55) / lineHeight;
    
    for (int i = 0; i < deviceCount && i < visibleLines; i++) {
      int y = startY + i * lineHeight;
      
      // Device name (top line)
      String name = devices[i].name;
      if (name.length() == 0 || name == "Unknown" || name == "Unknown Device") {
        name = "Unknown";
      } else if (name.length() > 18) {
        name = name.substring(0, 18);
      }
      
      // MAC address + manufacturer (bottom line)
      String mac = devices[i].macAddress;
      if (mac.length() > 12) {
        mac = mac.substring(mac.length() - 12);  // Show last 12 chars (xx:xx:xx)
      }
      String mfr = devices[i].manufacturerId;
      if (mfr != "N/A" && mfr != "") {
        mfr = mfr.substring(4);  // Remove "0x" prefix
      }
      
      tft.setTextDatum(ML_DATUM);
      tft.drawString(name, 10, y, 1);
      tft.drawString(mac + " " + mfr, 10, y + 11, 1);
      
      // RSSI with bar
      int rssiVal = devices[i].rssi;
      String rssiStr = String(rssiVal) + "dB";
      int barX = SCREEN_WIDTH - 90;
      int barW = 70;
      int barH = 6;
      int rssiAbs = abs(rssiVal);
      int filledW = map(rssiAbs, 30, 90, 0, barW);
      
      tft.drawString(rssiStr, barX - 45, y, 1);
      tft.fillRoundRect(barX, y + 1, barW, barH, 1, 0x303030);
      tft.fillRoundRect(barX, y + 1, filledW, barH, 1, 0x00E5FF);
    }
  }
  
  // Draw start scan button at bottom
  tft.fillRoundRect(50, SCREEN_HEIGHT - 50, SCREEN_WIDTH - 100, 40, 5, 0x00E5FF);
  tft.setTextColor(0x000000);
  tft.drawCentreString("Scan Again", SCREEN_WIDTH/2, SCREEN_HEIGHT - 30, 2);
}

// Draw BT scanner - single device detail
void drawBtDevice(int index) {
  tft.fillScreen(0x000000);
  tft.setTextColor(0xFFFFFF);
  tft.setTextDatum(TC_DATUM);
  
  // Header with device name
  tft.setTextSize(2);
  String name = devices[index].name;
  if (name.length() > 15) name = name.substring(0, 15) + "...";
  if (name.length() == 0) name = "Unknown Device";
  tft.drawCentreString(name, SCREEN_WIDTH/2, 10, 2);
  
  // Divider
  tft.drawLine(10, 35, SCREEN_WIDTH - 10, 35, 0xFFFFFF);
  
  // Device details
  int y = 50;
  int lineH = 18;
  
  // MAC
  tft.drawString("MAC:  ", 15, y, 2);
  tft.drawCentreString(devices[index].macAddress, SCREEN_WIDTH - 15, y, 2);
  y += lineH;
  
  // RSSI with bar
  tft.drawString("RSSI: ", 15, y, 2);
  int rssiVal = devices[index].rssi;
  tft.drawString(String(rssiVal) + " dBm", SCREEN_WIDTH - 120, y, 2);
  
  // RSSI bar indicator
  int barX = SCREEN_WIDTH - 110;
  int barY = y + 2;
  int barW = 60;
  int barH = 10;
  int filledW = map(abs(rssiVal), 30, 90, 0, barW);
  tft.fillRoundRect(barX, barY, barW, barH, 2, 0x404040);
  tft.fillRoundRect(barX, barY, filledW, barH, 2, 0x00E5FF);
  y += lineH;
  
  // Manufacturer
  tft.drawString("Manufacturer: ", 15, y, 2);
  String mfr = devices[index].manufacturer;
  if (mfr.length() > 12) mfr = mfr.substring(0, 12) + "...";
  tft.drawString(mfr, 170, y, 2);
  y += lineH;
  
  // Mfr ID
  tft.drawString("ID: ", 15, y, 2);
  tft.drawString(devices[index].manufacturerId, SCREEN_WIDTH - 15, y, 2);
  y += lineH;
  
  // Service UUIDs
  if (devices[index].serviceUuids.length() > 0) {
    tft.drawString("UUIDs: ", 15, y, 2);
    String uuids = devices[index].serviceUuids;
    if (uuids.length() > 15) uuids = uuids.substring(0, 15) + "...";
    tft.drawString(uuids, 100, y, 2);
    y += lineH;
  }
  
  // TX Power
  if (devices[index].txPower.length() > 0) {
    tft.drawString("TX Power: ", 15, y, 2);
    tft.drawString(devices[index].txPower + " dBm", SCREEN_WIDTH - 15, y, 2);
    y += lineH;
  }
  
  // Navigation bar
  tft.fillRoundRect(0, SCREEN_HEIGHT - 45, SCREEN_WIDTH, 45, 5, 0x202020);
  
  // Previous button
  bool canPrev = index > 0;
  int prevX = 10;
  tft.fillRoundRect(prevX, SCREEN_HEIGHT - 40, 70, 35, 5, canPrev ? 0x404040 : 0x202020);
  tft.setTextColor(canPrev ? 0xFFFFFF : 0x808080);
  tft.drawCentreString("< Prev", prevX + 35, SCREEN_HEIGHT - 22, 2);
  
  // Page indicator
  tft.setTextColor(0xFFFFFF);
  tft.drawCentreString(String(index + 1) + "/" + String(deviceCount), SCREEN_WIDTH/2, SCREEN_HEIGHT - 22, 2);
  
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
            // Check scan again button at bottom
            if (ty > SCREEN_HEIGHT - 50 && tx > 50 && tx < SCREEN_WIDTH - 50) {
              performBtScan();
            }
            break;
          }
          
          case BT_SHOWING_DEVICE: {
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
