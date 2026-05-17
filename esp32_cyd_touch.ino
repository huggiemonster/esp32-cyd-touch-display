#include <SPI.h>

/*  Install the "TFT_eSPI" library by Bodmer to interface with the TFT Display - https://github.com/Bodmer/TFT_eSPI
    *** IMPORTANT: User_Setup.h available on the internet will probably NOT work with the examples available at Random Nerd Tutorials ***
    *** YOU MUST USE THE User_Setup.h FILE PROVIDED IN THE LINK BELOW IN ORDER TO USE THE EXAMPLES FROM RANDOM NERD TUTORIALS ***
    FULL INSTRUCTIONS AVAILABLE ON HOW CONFIGURE THE LIBRARY: https://RandomNerdTutorials.com/cyd/ or https://RandomNerdTutorials.com/esp32-tft/   */
#include <TFT_eSPI.h>

// Install the "XPT2046_Touchscreen" library by Paul Stoffregen to use the Touchscreen - https://github.com/PaulStoffregen/XPT2046_Touchscreen
// Note: this library doesn't require further configuration
#include <XPT2046_Touchscreen.h>

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

// Menu options
enum MenuOption {
  BT_SCANNER = 0,
  BMS,
  SETTINGS,
  OTHER,
  MENU_COUNT
};

MenuOption currentScreen = BT_SCANNER; // 0 = main menu

// Touchscreen coordinates
int x, y;

// Touch calibration
#define TOUCH_MIN_X 200
#define TOUCH_MAX_X 3700
#define TOUCH_MIN_Y 240
#define TOUCH_MAX_Y 3800

// Menu layout
#define MENU_COLS 2
#define MENU_ROWS 2
#define MENU_MARGIN_X 15
#define MENU_MARGIN_Y 15
#define MENU_ITEM_PAD 10

// Calculate menu item dimensions
int menuItemWidth = (SCREEN_WIDTH - 2 * MENU_MARGIN_X) / MENU_COLS;
int menuItemHeight = (SCREEN_HEIGHT - 2 * MENU_MARGIN_Y) / MENU_ROWS;

// Color definitions
#define COLOR_BT 0x00E5FF    // Cyan/Blue for Bluetooth
#define COLOR_BMS 0x00E676   // Green for BMS
#define COLOR_SETTINGS 0xFF9800  // Orange for Settings
#define COLOR_OTHER 0xE040FB // Purple for Other
#define COLOR_BG 0x000000    // Black background
#define COLOR_TEXT 0xFFFFFF  // White text
#define COLOR_HIGHLIGHT 0x404040 // Highlight color

// Helper: get menu item bounding box
void getMenuBounds(int index, int &x1, int &y1, int &x2, int &y2) {
  int col = index % MENU_COLS;
  int row = index / MENU_COLS;
  x1 = MENU_MARGIN_X + col * menuItemWidth;
  y1 = MENU_MARGIN_Y + row * menuItemHeight;
  x2 = x1 + menuItemWidth;
  y2 = y1 + menuItemHeight;
}

// Helper: check if touch is within menu item
bool isTouchInItem(int tx, int ty, int index) {
  int x1, y1, x2, y2;
  getMenuBounds(index, x1, y1, x2, y2);
  int pad = MENU_ITEM_PAD;
  return (tx >= x1 + pad && tx <= x2 - pad && ty >= y1 + pad && ty <= y2 - pad);
}

// Helper: draw icon for menu option
void drawMenuIcon(int index, int cx, int cy, bool highlighted) {
  int iconSize = min(menuItemWidth, menuItemHeight) / 3;
  
  if (highlighted) {
    // Draw highlight background
    int x1, y1, x2, y2;
    getMenuBounds(index, x1, y1, x2, y2);
    tft.fillRoundRect(x1 + 5, y1 + 5, x2 - x1 - 10, y2 - y1 - 10, 10, COLOR_HIGHLIGHT);
  }
  
  tft.setTextDatum(MC_DATUM);
  
  switch (index) {
    case BT_SCANNER: {
      // Bluetooth symbol - simplified using lines
      int color = COLOR_BT;
      int s = iconSize / 2;
      
      // Draw vertical line
      tft.drawLine(cx, cy - s, cx, cy - s/2, color, 3);
      tft.drawLine(cx, cy + s/2, cx, cy + s, color, 3);
      
      // Draw upper triangle (pointing up-right)
      tft.fillTriangle(cx - 5, cy - s, cx + s, cy, cx - 5, cy, color);
      
      // Draw lower triangle (pointing down-right)
      tft.fillTriangle(cx - 5, cy + s, cx + s, cy, cx - 5, cy, color);
      break;
    }
    
    case BMS: {
      // Battery symbol
      int color = COLOR_BMS;
      int w = iconSize * 2/3;
      int h = iconSize * 2/3;
      
      // Battery body (rectangle)
      tft.fillRoundRect(cx - w/2, cy - h/2, w, h, 3, color);
      
      // Battery terminal (small rectangle on top)
      tft.fillRect(cx - w/6, cy - h/2 - h/6, w/3, h/6, color);
      
      // Battery fill indicator (inner rectangle)
      tft.fillRoundRect(cx - w/2 + 3, cy - h/2 + 3, w - 6, h - 6, 2, COLOR_BG);
      tft.fillRoundRect(cx - w/2 + 5, cy - h/2 + 5, w - 10, h/2 - 5, 2, color);
      break;
    }
    
    case SETTINGS: {
      // Settings gear/cog symbol
      int color = COLOR_SETTINGS;
      int r = iconSize / 2;
      
      // Outer circle (gear outline)
      tft.drawCircle(cx, cy, r, color);
      tft.drawCircle(cx, cy, r - 4, color);
      
      // Fill between circles
      tft.fillCircle(cx, cy, r, color);
      tft.fillCircle(cx, cy, r - 4, COLOR_BG);
      
      // Gear teeth (8 lines radiating outward)
      for (int i = 0; i < 8; i++) {
        float angle = i * 45.0 * 3.14159 / 180.0;
        int x1 = cx + (int)(r * 1.2 * cos(angle));
        int y1 = cy + (int)(r * 1.2 * sin(angle));
        int x2 = cx + (int)(r * 1.5 * cos(angle));
        int y2 = cy + (int)(r * 1.5 * sin(angle));
        tft.drawLine(x1, y1, x2, y2, color, 3);
      }
      
      // Center hole
      tft.fillCircle(cx, cy, r/3, COLOR_BG);
      break;
    }
    
    case OTHER: {
      // Question mark symbol
      int color = COLOR_OTHER;
      int r = iconSize / 2;
      
      // Circle background
      tft.fillCircle(cx, cy, r, color);
      
      // Question mark
      tft.setTextColor(COLOR_BG);
      tft.setTextDatum(MC_DATUM);
      tft.drawCentreString("?", cx, cy + 2, FONT_SIZE);
      break;
    }
  }
}

// Helper: draw menu screen
void drawMainMenu() {
  // Fill black background
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  
  // Title
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(FONT_SIZE);
  tft.drawCentreString("ESP32 CYD Menu", SCREEN_WIDTH/2, 8, FONT_SIZE);
  
  // Draw 4 menu items
  for (int i = 0; i < MENU_COUNT; i++) {
    int cx, cy;
    int x1, y1, x2, y2;
    getMenuBounds(i, x1, y1, x2, y2);
    
    cx = (x1 + x2) / 2;
    cy = (y1 + y2) / 2;
    
    // Draw icon
    drawMenuIcon(i, cx, cy - 15, false);
    
    // Draw label
    tft.setTextDatum(BC_DATUM);
    tft.drawCentreString(getMenuLabel(i), cx, y2 - 20, FONT_SIZE);
  }
}

// Helper: get menu label
String getMenuLabel(int index) {
  switch (index) {
    case BT_SCANNER: return "BT Scanner";
    case BMS: return "BMS";
    case SETTINGS: return "Settings";
    case OTHER: return "Other";
    default: return "";
  }
}

// Helper: draw sub-program screen
void drawSubProgram(int option) {
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  
  String title;
  int color;
  
  switch (option) {
    case BT_SCANNER:
      title = "Bluetooth Scanner";
      color = COLOR_BT;
      break;
    case BMS:
      title = "BMS";
      color = COLOR_BMS;
      break;
    case SETTINGS:
      title = "Settings";
      color = COLOR_SETTINGS;
      break;
    case OTHER:
      title = "Other";
      color = COLOR_OTHER;
      break;
    default:
      return;
  }
  
  // Draw header bar
  tft.fillRect(0, 0, SCREEN_WIDTH, 30, color);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextDatum(TC_DATUM);
  tft.drawCentreString(title, SCREEN_WIDTH/2, 8, FONT_SIZE);
  
  // Draw back button
  tft.fillRoundRect(5, 40, 50, 30, 5, COLOR_HIGHLIGHT);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextDatum(MC_DATUM);
  tft.drawCentreString("< Back", 30, 55, FONT_SIZE);
  
  // Draw content area
  tft.setTextColor(COLOR_TEXT);
  tft.setTextDatum(MC_DATUM);
  tft.drawCentreString("Coming Soon", SCREEN_WIDTH/2, SCREEN_HEIGHT/2, FONT_SIZE);
}

// Get touch coordinates with calibration
bool getTouchCoords(int &tx, int &ty) {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    // Map raw touch values to screen coordinates
    tx = map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_WIDTH);
    ty = map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_HEIGHT);
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);

  // Start the SPI for the touchscreen and init the touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  // Set the Touchscreen rotation in landscape mode
  // Note: in some displays, the touchscreen might be upside down, so you might need to set the rotation to 3: touchscreen.setRotation(3);
  touchscreen.setRotation(1);

  // Start the tft display
  tft.init();
  // Set the TFT display rotation in landscape mode
  tft.setRotation(1);

  // Initialize with main menu
  drawMainMenu();
}

void loop() {
  int tx, ty;
  
  if (getTouchCoords(tx, ty)) {
    Serial.print("Touch at X = ");
    Serial.print(tx);
    Serial.print(", Y = ");
    Serial.println(ty);
    
    // Check if main menu is displayed and which item was touched
    // Simple approach: check touch against menu item bounds
    for (int i = 0; i < MENU_COUNT; i++) {
      int x1, y1, x2, y2;
      getMenuBounds(i, x1, y1, x2, y2);
      
      // Check if touch is in this menu item
      if (tx >= x1 && tx <= x2 && ty >= y1 && ty <= y2) {
        Serial.print("Menu option: ");
        Serial.println(getMenuLabel(i));
        
        // Draw the sub-program screen
        drawSubProgram(i);
        
        // Wait for touch to release (prevent double-tap)
        while (touchscreen.touched()) {
          touchscreen.getPoint();
          delay(10);
        }
        
        // Wait a bit before allowing next touch
        delay(200);
        
        break;
      }
    }
  }
}
