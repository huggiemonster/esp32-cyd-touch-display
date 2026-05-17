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

ManufacturerInfo manufacturers[] = {
  {0x0001, "Ericsson Technology Licensing"},
  {0x0002, "Nokia Mobile Phones"},
  {0x0003, "Intel Corp."},
  {0x0004, "IBM Corp."},
  {0x0005, "Toshiba Corp."},
  {0x0006, "3Com"},
  {0x0007, "Microsoft"},
  {0x0008, "Lucent"},
  {0x0009, "Motorola"},
  {0x000A, "Infineon Technologies AG"},
  {0x000B, "Cambridge Silicon Radio"},
  {0x000C, "Silicon Wave"},
  {0x000D, "Digianswer A/S"},
  {0x000E, "Texas Instruments Inc."},
  {0x000F, "Ceva, Inc. (formerly Parthus Technologies, Inc.)"},
  {0x0010, "Broadcom Corporation"},
  {0x0011, "Garmin International, Inc."},
  {0x0012, "ContiTech"},
  {0x0013, "Visteon Corp."},
  {0x0014, "Bluegiga"},
  {0x0015, "Marvell Technology Group Ltd."},
  {0x0016, "3DSP Corporation"},
  {0x0017, "Acconet AB"},
  {0x0018, "Commil Ltd"},
  {0x0019, "ColorCrome Inc."},
  {0x001A, "Red-M (Manufacturing) Ltd."},
  {0x001B, "Value Technology, Inc."},
  {0x001C, "Syntronix Corporation"},
  {0x001D, "Integrated System Solution Corp."},
  {0x001E, "Mstar Semiconductor, Inc."},
  {0x001F, "Banoo Electronics"},
  {0x0020, "Kt Technologies"},
  {0x0021, "MSD Inc."},
  {0x0022, "ShangHai Super Smart Electronics Co. Ltd."},
  {0x0023, "Group Sense Ltd."},
  {0x0024, "Zomm Limited"},
  {0x0025, "Samsung Electronics Co. Ltd."},
  {0x0026, "Creative Technology Ltd."},
  {0x0027, "Luminary Micro Inc."},
  {0x0028, "Inventel"},
  {0x0029, "AVM Berlin"},
  {0x002A, "BandSpeed, Inc."},
  {0x002B, "Mansella Ltd"},
  {0x002C, "Neuberthan GmbH"},
  {0x002D, "CG Technology"},
  {0x002E, "Kawantech"},
  {0x002F, "A and R Cambridge"},
  {0x0030, "Standard Innovations Inc."},
  {0x0031, "CSR"},
  {0x0032, "GCT Semiconductor"},
  {0x0033, "Macronix International Co. Ltd."},
  {0x0034, "Geoforce Inc."},
  {0x0035, "Erli Cyber Systems"},
  {0x0036, "Atheros Communications"},
  {0x0037, "MediaTek, Inc."},
  {0x0038, "BlueGiga"},
  {0x0039, "Marvell Technology Group Ltd."},
  {0x003A, "3210.com"},
  {0x003B, "Accel Semiconductor Ltd."},
  {0x003C, "Continental Automotive Systems"},
  {0x003D, "Adventure Wireless"},
  {0x003E, "Tsurushi Technology"},
  {0x003F, "AVANTRON Inc."},
  {0x0040, "A & R Cambridge"},
  {0x0041, "Widcomm"},
  {0x0042, "Oki Electric Industry Co. Ltd."},
  {0x0043, "Signal 7 Systems"},
  {0x0044, "Transilica, Inc."},
  {0x0045, "Rohde & Schwarz GmbH & Co. KG"},
  {0x0046, "NXP B.V."},
  {0x0047, "Wincor-Nixdorf GmbH & Co. Ltd"},
  {0x0048, "Komusan Convergence Research Institute Institute"},
  {0x0049, "Inventel"},
  {0x004A, "AVM GmbH"},
  {0x004B, "BandSpeed Inc"},
  {0x004C, "Apple, Inc."},
  {0x004D, "Alps Electric Co., Ltd."},
  {0x004E, "Renesas Electronics Corporation"},
  {0x004F, "Qualcomm Technologies Inc."},
  {0x0050, "Qualcomm Innovation Group, Inc. (QuIC)"},
  {0x0051, "Aplux Communications Ltd."},
  {0x0052, "MewTel Technology Inc."},
  {0x0053, "Tzero Technologies, Inc."},
  {0x0054, "J&M Corporation"},
  {0x0055, "Free2move AB"},
  {0x0056, "Jolla Ltd"},
  {0x0057, "Lectronix, Inc."},
  {0x0058, "Caterpillar Inc."},
  {0x0059, "FleetSmart Incorporated"},
  {0x005A, "Hewlett-Packard Company"},
  {0x005B, "Sony Corporation"},
  {0x005C, "Sircum, Inc."},
  {0x005D, "9Solutions Oy"},
  {0x005E, "GN Netcom A/S"},
  {0x005F, "General Motors"},
  {0x0060, "A&D Engineering, Inc."},
  {0x0061, "Salvant Systems, Inc."},
  {0x0062, "Bluetooth SIG, Inc"},
  {0x0063, "Nihon Condenser Industries, Inc."},
  {0x0064, "Uplinks Technologies Corporation"},
  {0x0065, "Frame Technology A/S"},
  {0x0066, "WuXi Vimicro"},
  {0x0067, "Sennheiser Communications A/S"},
  {0x0068, "TimeKeeping Systems, Inc."},
  {0x0069, "Ludus Helsinki Ltd."},
  {0x006A, "TrueVision Corporation"},
  {0x006B, "Gimbal Inc."},
  {0x006C, "RDA Microelectronics"},
  {0x006D, "GeLo Inc."},
  {0x006E, "Essen Digital GmbH"},
  {0x006F, "Kohler Company"},
  {0x0070, "Emergn Technologies LLC"},
  {0x0071, "Simple Technology"},
  {0x0072, "Noventa AG"},
  {0x0073, "Convergence Systems Limited"},
  {0x0074, "MobiNoox Corporation"},
  {0x0075, "Realtek Semiconductor Corporation"},
  {0x0076, "SEMI Solutions Poland Sp. z o.o."},
  {0x0077, "I-O DATA DEVICE, INC."},
  {0x0078, "ASUSTek Computer, Inc."},
  {0x0079, "Marantz (SAFEMANZ Corporation)"},
  {0x007A, "Novidan, Inc."},
  {0x007B, "Bose Corporation"},
  {0x007C, "Suunto Oy"},
  {0x007D, "Adolf Wuerth GmbH & Co KG"},
  {0x007E, "Polar Electro Oy"},
  {0x007F, "Openbrain Technologies, Co., Ltd."},
  {0x0080, "Xensium"},
  {0x0081, "The Linux Foundation"},
  {0x0082, "Seeed Technology Co., Ltd."},
  {0x0083, "City Technologies Ltd"},
  {0x0084, "Nikon Corporation"},
  {0x0085, "Yardforce Dynamo Inc"},
  {0x0086, "HUAWEI Technologies Co., Ltd"},
  {0x0087, "Kobian Inc"},
  {0x0088, "Quintray"},
  {0x0089, "Dot Wave Inc"},
  {0x008A, "KDDI Corporation"},
  {0x008B, "ANP Electronics Co., LTD"},
  {0x008C, "Saftronics Co. Ltd."},
  {0x008D, "Foster Electric Co., Ltd."},
  {0x008E, "HF Technologies"},
  {0x008F, "ROCKWELL AUTOMATION"},
  {0x0090, "S-Power Electronics Limited"},
  {0x0091, "AceSensor Inc."},
  {0x0092, "Afero, Inc."},
  {0x0093, "Snapchat Inc"},
  {0x0094, "Devialet France SAS"},
  {0x0095, "LEGO System A/S"},
  {0x0096, "Thalmic Labs Inc."},
  {0x0097, "Inficon Switzerland GmbH"},
  {0x0098, "Rivet Networks"},
  {0x0099, "Teledyne Lecroy, Inc."},
  {0x009A, "Data Sciences, Inc."},
  {0x009B, "Valve Corporation"},
  {0x009C, "Hekler Corp"},
  {0x009D, "Tangerine, Inc."},
  {0x009E, "BHL International Ltd."},
  {0x009F, "PROXIMITY SA"},
  {0x00A0, "OMRON Corporation"},
  {0x00A1, "Wireless Cables Inc"},
  {0x00A2, "Eteq, Inc."},
  {0x00A3, "Accumulate AB"},
  {0x00A4, "NITTO KOGYO CORPORATION"},
  {0x00A5, "SmartMovt Technology Co., Ltd"},
  {0x00A6, "Cooler Master Co., Ltd."},
  {0x00A7, "AG Measurematics LLP"},
  {0x00A8, "Sphair Corporation"},
  {0x00A9, "Airoha Technology Corp."},
  {0x00AA, "T&A Laboratories LLC"},
  {0x00AB, "Renishaw PLC"},
  {0x00AC, "B&O Play A/S"},
  {0x00AD, "General Electric Technologies"},
  {0x00AE, "Avago Technologies"},
  {0x00AF, "SABIK"},
  {0x00B0, "Luminous BV"},
  {0x00B1, "Lumo Bodytech Inc."},
  {0x00B2, "Pepperl+Fuchs GmbH"},
  {0x00B3, "Draeger Inc."},
  {0x00B4, "DeLorme Publishing Company Inc."},
  {0x00B5, "G2o Corp"},
  {0x00B6, "Amtran International Co., Ltd."},
  {0x00B7, "Modul-System AG"},
  {0x00B8, "GIM-Sensor Alterungs-und Sicherheitsysteme GmbH"},
  {0x00B9, "Nymi Inc."},
  {0x00BA, "GIM Inc"},
  {0x00BB, "Gimbal Inc."},
  {0x00BC, "Orlan LLC"},
  {0x00BD, "Blue Clover Devices"},
  {0x00BE, "Netizens Sp. z o.o."},
  {0x00BF, "Gigagon Corporation"},
  {0x00C0, "Doppler Lab"},
  {0x00C1, "Doppler Labs"},
  {0x00C2, "JouleView"},
  {0x00C3, "i-SENS Co., Ltd."},
  {0x00C4, "Telecon Mobile Limited"},
  {0x00C5, "Esrille Inc."},
  {0x00C6, "AirTurn, Inc."},
  {0x00C7, "Sino Wealth Electronic Ltd."},
  {0x00C8, "Pacebloom Technologies"},
  {0x00C9, "Focus Systems Corporation"},
  {0x00CA, "NTEO Inc."},
  {0x00CB, "Emlid Limited"},
  {0x00CC, "Nest Labs Inc."},
  {0x00CD, "Amazon.com Services, Inc."},
  {0x00CE, "iVISTA High-Tech Co., Ltd."},
  {0x00CF, "Xiaomi Inc."},
  {0x00D0, "Qingdao Realtime Technology Co., Ltd."},
  {0x00D1, "BEGA Gantenbrink-Leuchten GmbH"},
  {0x00D2, "Podo Labs"},
  {0x00D3, "Savant Systems LLC"},
  {0x00D4, "Niru Corporation"},
  {0x00D5, "Paxton Access Ltd"},
  {0x00D6, "Vernier"},
  {0x00D7, "Apogee Instruments"},
  {0x00D8, "Shenzhen Qianfenyi Intelligent Technology Co., Ltd"},
  {0x00D9, "Metanote Inc."},
  {0x00DA, "SetPoint Medical"},
  {0x00DB, "BRControls Products BV"},
  {0x00DC, "Savant Systems LLC"},
  {0x00DD, "Nivona"},
  {0x00DE, "SmartWave Technology Inc."},
  {0x00DF, "Garadget (2015) Inc."},
  {0x00E0, "DMD Network Systems Inc"},
  {0x00E1, "HabitAware, LLC"},
  {0x00E2, "Kocomojo, LLC"},
  {0x00E3, "Applied Research Associates"},
  {0x00E4, "Rambus Inc."},
  {0x00E5, "Flextronics International USA"},
  {0x00E6, "Amazon Lab126"},
  {0x00E7, "Phonepe Pvt Ltd"},
  {0x00E8, "Sunplus Technology Co., Ltd."},
  {0x00E9, "ALCATEL TELEPHONES"},
  {0x00EA, "Gentle Energy Corp."},
  {0x00EB, "w5LAN, Inc."},
  {0x00EC, "Molex"},
  {0x00ED, "Iton Technology Corp."},
  {0x00EE, "OTL Labs"},
  {0x00EF, "AirBolt Pty Ltd"},
  {0x00F0, "Zipcar"},
  {0x00F1, "Arculus"},
  {0x00F2, "BradyWorld A/S"},
  {0x00F3, "SECURETECH"},
  {0x00F4, "Sirona Stairlifts Limited"},
  {0x00F5, "HDL Australia"},
  {0x00F6, "Kobian Technologies PTY LTD"},
  {0x00F7, "Shenzhen iMCOGas Technology Co., Ltd"},
  {0x00F8, "Elecs Industry Co., Ltd."},
  {0x00F9, "VERISIT"},
  {0x00FA, "CUE"},
  {0x00FB, "Alivecor, Inc."},
  {0x00FC, "Smart Solutions S.L."},
  {0x00FD, "HabitAware, LLC"},
  {0x00FE, "FUBA Automotive Electronics GmbH"},
  {0x00FF, "AW Company"},
  {0x0100, "Shanghai MiXamp Technology Co., Ltd"},
  {0x0101, "Lunare Sciences, Inc."},
  {0x0102, "Pura Scents, Inc"},
  {0x0103, "OnePlus Electronics (Shanghai) Co., Ltd."},
  {0x0104, "Ellisect AB"},
  {0x0105, "AIAIAI ApS"},
  {0x0106, "The L.S. Starrett Company"},
  {0x0107, "mela GmbH"},
  {0x0108, "Koki Technologies (Shenzhen) Co., Ltd."},
  {0x0109, "Musen Connect, Inc."},
  {0x010A, "BlueKitchen GmbH"},
  {0x010B, "Rigado LLC"},
  {0x010C, "CUBE TECHNOLOGIES"},
  {0x010D, "TRSystems GmbH"},
  {0x010E, "CUBETECH srl"},
  {0x010F, "DPTechnics"},
  {0x0110, "Ztoic Tech"},
  {0x0111, "Sena Technologies Inc."},
  {0x0112, "SmartSensor Labs Ltd"},
{0x0113, "Akciju sabiedriba SAFATECH"},
  {0x0114, "Kartographers Technologies Pvt. Ltd."},
  {0x0115, "Valeo Service"},
  {0x0116, "NIDEC MOTOR CORPORATION"},
  {0x0117, "EasySRing co.,llc"},
  {0x0118, "Shenzhen SuLong Technology Ltd"},
  {0x0119, "ViewStar, Co., Ltd"},
  {0x011A, "Reoqoo IoT Pte Ltd"},
  {0x011B, "Sencilion A/S"},
  {0x011C, "Avid Identification Systems, Inc."},
  {0x011D, "Littelfuse"},
  {0x011E, "Belparts d.o.o."},
  {0x011F, "NeST"},
  {0x0120, "Southco"},
  {0x0121, "LEGO System A/S"},
  {0x0122, "Thetatronics Ltd."},
  {0x0123, "Nanoleaf Canada Limited"},
  {0x0124, "Urban Compass, Inc"},
  {0x0125, "digma spa z o.o."},
  {0x0126, "Optek"},
  {0x0127, "Bluepeaks"},
  {0x0128, "TIVIA SA"},
  {0x0129, "Alango Technologies Ltd."},
  {0x012A, "Mist Systems, Inc."},
  {0x012B, "Startup Labs"},
  {0x012C, "Geopal system A/S"},
  {0x012D, "Openbrain Tech, Co., Ltd."},
  {0x012E, "Xicato Inc."},
  {0x012F, "Byton North America Corporation"},
  {0x0130, "GierLab Designs"},
  {0x0131, "S4C, Ltd"},
  {0x0132, "Matting GmbH"},
  {0x0133, "OJ Electronics A/S"},
  {0x0134, "Schrack Technik GmbH"},
  {0x0135, "Kiiroo BV"},
  {0x0136, "FlameCo"},
  {0x0137, "iRhythm Technologies, Inc."},
  {0x0138, "Ravenseft Ltd"},
  {0x0139, "SOLUX PTY LTD"},
  {0x013A, "Clover Network, Inc."},
  {0x013B, "Beflex Inc."},
  {0x013C, "Beijing Smartspace Technologies Co."},
  {0x013D, "SAVOY ELECTRONIC LIGHTING"},
  {0x013E, "Nordic Semiconductor AS"},
  {0x013F, "DASH ROBOT CO., LTD."},
  {0x0140, "Blincam, Inc."},
  {0x0141, "Luxer Corporation"},
  {0x0142, "RealTerm"},
  {0x0143, "Chargifi Limited"},
  {0x0144, "Trividia Health, Inc."},
  {0x0145, "GETMATIC"},
  {0x0146, "Apple, Inc."},
  {0x0147, "Stalmart Technology Limited"},
  {0x0148, "AMETEK, Inc."},
  {0x0149, "SYNCHROVISION Inc."},
  {0x014A, "Volkswagen AG"},
  {0x014B, "Skoda Auto a.s."},
  {0x014C, "Ford Motor Company"},
  {0x014D, "GM Global Solutions Technologies LLC"},
  {0x014E, "Honda Motor Co., Ltd."},
  {0x014F, "Hyundai Motor Company"},
  {0x0150, "KIA Motors Corporation"},
  {0x0151, "Valeo Systems"},
  {0x0152, "Mercedes AG"},
  {0x0153, "BMW GmbH"},
  {0x0154, "PT. Adira Digital Networking"},
  {0x0155, "Zhong Guan Cun Information Technology Co., Ltd"},
  {0x0156, "ALS Technologies AB"},
  {0x0157, "SwaggerHeck"},
  {0x0158, "Wabilogic Services"},
  {0x0159, "Société des Produits Bergeron Inc."},
  {0x015A, "Nearfield Solutions Ltd"},
  {0x015B, "Spaceek LTD"},
  {0x015C, "TTS Tooltechnic Systems AG & Co. KG"},
  {0x015D, "XLN Technologies Limited"},
  {0x015E, "Pharynks Corporation"},
  {0x015F, "LEGO System A/S"},
  {0x0160, "Thalmic Labs Inc."},
  {0x0161, "Cochlear Limited"},
  {0x0162, "Savitech Systems, Inc."},
  {0x0163, "Produal Oy"},
  {0x0164, "EyeTech Digital Systems Limited"},
  {0x0165, "FMW electronic Futterer u. Maier-Wulf"},
  {0x0166, "CareView Communications, Inc."},
  {0x0167, "C.O.B.O. SpA"},
  {0x0168, "ABS Instrumental"},
  {0x0169, "Lapstar Co., Ltd."},
  {0x016A, "C.R. Bard, Inc."},
  {0x016B, "TMS International Mechatronics GmbH"},
  {0x016C, "Pearson Imaging Ltd"},
  {0x016D, "Caterpillar Motive Technologies & Solutions"},
  {0x016E, "Hanna Instruments Inc."},
  {0x016F, "Kocomojo"},
  {0x0170, "Everykey Inc."},
  {0x0171, "Acuity Brands Lighting, Inc"},
  {0x0172, "Panda Scout AG"},
  {0x0173, "Brilliant Home Technology, Inc."},
  {0x0174, "Panasonic Corporation"},
  {0x0175, "Airoha Technology Corp."},
  {0x0176, "Newlab S.r.l."},
  {0x0177, "Sky Wave Design"},
  {0x0178, "Grip Health"},
  {0x0179, "Yale"},
  {0x017A, "Inplay Technologies"},
  {0x017B, "Cleveron Ltd"},
  {0x017C, "Remedee Lab"},
  {0x017D, "STARLITE Co., Ltd."},
  {0x017E, "microTracer Team"},
  {0x017F, "Telink Semiconductor Co. Ltd"},
  {0x0180, "iCOGNIZE GmbH"},
  {0x0181, "ScentConnect Inc"},
  {0x0182, "Dolby Labs"},
  {0x0183, "EllieGrid"},
  {0x0184, "Lowerstep Studios Inc"},
  {0x0185, "FNIRCI Technology Co., Ltd."},
  {0x0186, "Beco, Inc"},
  {0x0187, "Scosche Industries, Inc."},
  {0x0188, "Mightylogic Ltd."},
  {0x0189, "Vocera Communications Inc"},
  {0x018A, "CSR Building Products Limited"},
  {0x018B, "Freebird Instrument Inc"},
  {0x018C, "PMD Solutions"},
  {0x018D, "Sino Wealth Electronic Ltd."},
  {0x018E, "Aptcode Solutions Ltd."},
  {0x018F, "LEGO System A/S"},
  {0x0190, "Eijkelkamp Soil & Water"},
  {0x0191, "B-Soft"},
  {0x0192, "Stamford Corporation"},
  {0x0193, "SPICA SYSTEMS SNC"},
  {0x0194, "Avack Oy"},
  {0x0195, "WAFER-SCALE SYSTEMS INC."},
  {0x0196, "Metronomik Health"},
  {0x0197, "SimpleSafe Inc"},
  {0x0198, "Dotty Interactive Ltd."},
  {0x0199, "American Music Environments"},
  {0x019A, "Interaction Technology Design (ITD)"},
  {0x019B, "Sibel Inc"},
  {0x019C, "Schawbel Technologies LLC"},
  {0x019D, "SL Corporation"},
  {0x019E, "Coresite Publishing"},
  {0x019F, "SwiftSensors"},
  {0x01A0, "Bluehackes GmbH"},
  {0x01A1, "UiT the Arctic University of Norway"},
  {0x01A2, "LookX BVBA"},
  {0x01A3, "Smart Innovation Network"},
  {0x01A4, "CeoTronics AG"},
  {0x01A5, "Taobao"},
  {0x01A6, "WS Audio Ltd"},
  {0x01A7, "LEGO System A/S"},
  {0x01A8, "Nike, s.a.s."},
  {0x01A9, "iData Technology Co., Ltd."},
  {0x01AA, "Rivet Systems"},
  {0x01AB, "G-Sight Technologies"},
  {0x01AC, "Qingping Technology (Beijing) Co., Ltd."},
  {0x01AD, "Pal Technologies Pty Ltd"},
  {0x01AE, "FloDesign Inc"},
  {0x01AF, "SonicSensory Inc"},
  {0x01B0, "LinkSquares, Inc."},
  {0x01B1, "TeAM BV"},
  {0x01B2, "Hach - Danaher"},
  {0x01B3, "Nanoleaf Canada Limited"},
  {0x01B4, "Spacelabs Medical Inc."},
  {0x01B5, "instagrid GmbH"},
  {0x01B6, "Mist Systems, Inc."},
  {0x01B7, "noa GmbH"},
  {0x01B8, "Polidea Sp. z o.o."},
  {0x01B9, "Tenstorrel Inc."},
  {0x01BA, "Sigma Connectivity AB"},
  {0x01BB, "HOP Ubiquitous"},
  {0x01BC, "Nikunj Electronics"},
  {0x01BD, "GeLo Inc."},
  {0x01BE, "WDF Systems Inc"},
  {0x01BF, "Tokai-rika engineering co., ltd."},
  {0x01C0, "XIAMENINE CHIGER HIGH-TECH CO., LTD."},
  {0x01C1, "SOLUX PTY LTD"},
  {0x01C2, "Lumens For Less, Inc"},
  {0x01C3, "Brother Industries, Ltd"},
  {0x01C4, "C.O.B.O SpA"},
  {0x01C5, "Milestone AV Technologies LLC"},
  {0x01C6, "BELL DESIGN INC."},
  {0x01C7, "Rivata, Inc."},
  {0x01C8, "Almendo Technologies GmbH"},
  {0x01C9, "SILVER TREE LABS"},
  {0x01CA, "Vimar SpA"},
  {0x01CB, "Moxa Inc."},
  {0x01CC, "Harman International"},
  {0x01CD, "Systemic Solutions Ltd"},
  {0x01CE, "Nuvoton"},
  {0x01CF, "Zhu Hai City Xin Fei Electronics Co., Ltd."},
  {0x01D0, "Corvex Connected Safety"},
  {0x01D1, "Aii Solutions AS"},
  {0x01D2, "Neatebox Ltd"},
  {0x01D3, "Dell Inc."},
  {0x01D4, "Airbly Inc."},
  {0x01D5, "AIAIAI ApS"},
  {0x01D6, "SafeLine Sweden AB"},
  {0x01D7, "Denso Corporation"},
  {0x01D8, "Prolon, Inc."},
  {0x01D9, "The Shadow on the Moon"},
  {0x01DA, "Appside co., ltd."},
  {0x01DB, "DELABIE"},
  {0x01DC, "SensiLow GmbH"},
  {0x01DD, "SimpliSafe, Inc."},
  {0x01DE, "ROCKWELL AUTOMATION, INC."},
  {0x01DF, "RTC Industries, Inc."},
  {0x01E0, "Mode Lighting Limited"},
  {0x01E1, "Sunstone-ES S.L."},
  {0x01E2, "Atrius"},
  {0x01E3, "INIA"},
  {0x01E4, "Cochlear Limited"},
  {0x01E5, "GlideTech Medical Inc"},
  {0x01E6, "Chengdu C3 Technology"},
  {0x01E7, "WRLabs"},
  {0x01E8, "Inseego Corp."},
  {0x01E9, "Fugoo, Inc."},
  {0x01EA, "Keepen Technologies"},
  {0x01EB, "SimpliSafe, Inc."},
  {0x01EC, "Littelfuse"},
  {0x01ED, "T&A Laboratories LLC"},
  {0x01EE, "Espressif Incorporated"},
  {0x01EF, "Lumo Bodytech Inc."},
  {0x01F0, "PageWide Systems US LLC"},
  {0x01F1, "RDA"},
  {0x01F2, "Alcon Laboratories Inc."},
  {0x01F3, "P.I.Engineering"},
  {0x01F4, "Sensogram Technologies Inc"},
  {0x01F5, "Radinn AB"},
  {0x01F6, "KONO2 Inc"},
  {0x01F7, "HCP GmbH"},
  {0x01F8, "Parallel Wireless Inc"},
  {0x01F9, "Alango Technologies Ltd"},
  {0x01FA, "Lumi Road Inc"},
  {0x01FB, "Amazon.com Services, LLC"},
  {0x01FC, "Volkswagen AG"},
  {0x01FD, "Cleveron Oy"},
  {0x01FE, "Ingchips Technology Co., Ltd."},
  {0x01FF, "Eneso Technology SL"},
  {0x0200, "Shenzhen Mewu Technology Ltd."},
  {0x0201, "HabitAware, LLC"},
  {0x0202, "Relumion, Inc."},
  {0x0203, "JSV Corporation"},
  {0x0204, "PRN Corporation"},
  {0x0205, "Autec GmbH"},
  {0x0206, "LEDVANCE GmbH"},
  {0x0207, "C.A.M.P. Electronic Design B.V."},
  {0x0208, "FUSE NETWORKS, INC."},
  {0x0209, "Steinel Professional Solutions GmbH"},
  {0x020A, "Valve Corporation"},
  {0x020B, "HUSKYLENS Technologies Inc."},
  {0x020C, "Peak Energy"},
  {0x020D, "Nuuva, Inc."},
  {0x020E, "Swedlock AB"},
  {0x020F, "Netvox Co., Ltd."},
  {0x0210, "Cypak AB"},
  {0x0211, "Guard RFID Solutions AS"},
  {0x0212, "Pepperl+Fuchs GmbH"},
  {0x0213, "Dyewear"},
  {0x0214, "Lumens For Less, Inc"},
  {0x0215, "SENNHEISER electronic GmbH & Co. KG"},
  {0x0216, "Kartographers Technologies Pvt. Ltd."},
  {0x0217, "Wabilogic Services"},
  {0x0218, "Socomec Connect"},
  {0x0219, "TRACMO, INC."},
  {0x021A, "Hive Box (Shenzhen) Intelligent Technology Co., Ltd."},
  {0x021B, "Rayden.Earth LTD"},
  {0x021C, "LANix, S.A.B. de C.V."},
  {0x021D, "Angel Medical Systems, Inc."},
  {0x021E, "G3Fighting, Inc."},
  {0x021F, "Runteq Oy Ltd"},
  {0x0220, "TiVo Corp"},
  {0x0221, "SmartMovt Technology Co., Ltd"},
  {0x0222, "Lumi United Technology Co., Ltd"},
  {0x0223, "Authomate Inc"},
  {0x0224, "Koyox Technology"},
  {0x0225, "Nanjing Xinxiangeng Technology Co., Ltd"},
  {0x0226, "Laxmi Therapeutic Devices, Inc."},
  {0x0227, "Dmac Mobile Communications GmbH"},
  {0x0228, "AIAIAI ApS"},
  {0x0229, "Geopal system A/S"},
  {0x022A, "NITTO KOGYO CORPORATION"},
  {0x022B, "SALTO SYSTEMS S.L."},
  {0x022C, "TRON Link Systems Inc."},
  {0x022D, "Kobian Technologies PTY LTD"},
  {0x022E, "RHA TECHNOLOGIES LTD"},
  {0x022F, "SNOOK Technologies"},
  {0x0230, "Shenzhen CoolKit Technology Co., Ltd"},
  {0x0231, "VIMANA TECHNOLOGY PVT LTD"},
  {0x0232, "Ooma"},
  {0x0233, "Lumi United Technology Co., Ltd"},
  {0x0234, "LuminAID LLC"},
  {0x0235, "MIRAelectronics"},
  {0x0236, "Shenzhen Sunricher Technology Limited"},
  {0x0237, "Innovacionnye Resheniya"},
  {0x0238, "NewTec GmbH"},
  {0x0239, "Sage Therapeutics"},
  {0x023A, "Leica Camera AG"},
  {0x023B, "Tyto Life LLC"},
  {0x023C, "Amazon Lab126"},
  {0x023D, "C2 Development, Inc."},
  {0x023E, "BlueID GmbH"},
  {0x023F, "Schneider Electric"},
  {0x0240, "Omnivoltaic Energy Solutions Limited Company"},
  {0x0241, "ACS-Control-System GmbH"},
  {0x0242, "SRAM"},
  {0x0243, "Artex, Inc."},
  {0x0244, "Giatec Scientific Inc."},
  {0x0245, "A.I. Security Technologies Company LLC"},
  {0x0246, "RAB Lighting, Inc."},
  {0x0247, "Sensyloop"},
  {0x0248, "HongKong SmartLink Limited"},
  {0x0249, "Eaglerx, Inc."},
  {0x024A, "Savebot"},
  {0x024B, "KCM Technologies"},
  {0x024C, "Link Labs, Inc."},
  {0x024D, "HONG KONG MICRO BIG TECHNOLOGY"},
  {0x024E, "Telink Semiconductor Co. Ltd"},
  {0x024F, "iGKT Sporting Goods B.V."},
  {0x0250, "SmartSensor Labs Ltd"},
  {0x0251, "DJO Global"},
  {0x0252, "Uwanna, Inc."},
  {0x0253, "Shenzhen H&T Intelligent Control Co., Ltd"},
  {0x0254, "Planmeca Oy"},
  {0x0255, "WAFER-SCALE SYSTEMS INC."},
  {0x0256, "Orion Labs, Inc."},
  {0x0257, "WISYCOM S.R.L."},
  {0x0258, "DME Microelectronics B.V."},
  {0x0259, "Bitstrata Technologies Inc."},
  {0x025A, "Brilliant Home Technology, Inc."},
  {0x025B, "Shenzhen Aimorelion Technologies Co., Ltd"},
  {0x025C, "JMR embedded systems GmbH"},
  {0x025D, "Swirl Networks"},
  {0x025E, "Lumenetix, Inc"},
  {0x025F, "Minut, Inc."},
  {0x0260, "Geeknet, Inc."},
  {0x0261, "Hong Kong I Love Design Limited"},
  {0x0262, "Sensibo, Inc."},
  {0x0263, "Current Technology"},
  {0x0264, "Ningbo Jeli Electronics Co., Ltd."},
  {0x0265, "CLIM8 LTD"},
  {0x0266, "T3 Solutions"},
  {0x0267, "Orlan LLC"},
  {0x0268, "Dottir Solutions"},
  {0x0269, "Shoof Technologies"},
  {0x026A, "Pur3 Ltd"},
  {0x026B, "Kome Technology Pty Ltd"},
  {0x026C, "PneuSafe LTD"},
  {0x026D, "Apple, Inc."},
  {0x026E, "Almendo Technologies GmbH"},
  {0x026F, "SolvonKade Ltd"},
  {0x0270, "Sky UK Limited"},
  {0x0271, "Cleware"},
  {0x0272, "HIMFron AB"},
  {0x0273, "Lacefield Electronics Ltd"},
  {0x0274, "Polidea Sp. z o.o."},
  {0x0275, "Greenwald Industries"},
  {0x0276, "inQs Co., Ltd."},
  {0x0277, "Chargifi Limited"},
  {0x0278, "Delcom Products Inc."},
  {0x0279, "TSE BRAKES, INC."},
  {0x027A, "Lighting Science Corp"},
  {0x027B, "C3-WIRELESS, INC."},
  {0x027C, "HMS Industrial Networks AB"},
  {0x027D, "Produal Oy"},
  {0x027E, "Austco Communication Systems"},
  {0x027F, "KODAK REALITY Inc."},
  {0x0280, "Embedded Electronic Solutions Ltd."},
  {0x0281, "bellacurtain"},
  {0x0282, "Shenzhen Feasycom Technology Co., Ltd."},
  {0x0283, "NIPPON SYSTEMWARE CO.,LTD."},
  {0x0284, "Seiko Instruments Inc."},
  {0x0285, "Rakuten Mobility Inc."},
  {0x0286, "HAPPIEST BABY, INC."},
  {0x0287, "Siro", "Unknown"},
  {0x0288, "Shenzhen Sunno Technology Co., Ltd"},
  {0x0289, "Genosense"},
  {0x028A, "Microoled"},
  {0x028B, "The Shadow on the Moon"},
  {0x028C, "Wabilogic"},
  {0x028D, "Safera Oy"},
  {0x028E, "Playfinity AS"},
  {0x028F, "Hansun TECHNOLOGIES INC."},
  {0x0290, "Samsara Networks, Inc"},
  {0x0291, "NIRI"},
  {0x0292, "ROOSM"},
  {0x0293, "ABAX AB"},
  {0x0294, "Audiodo AB"},
  {0x0295, "Amazon.com Services, Inc."},
  {0x0296, "Shenzhen Injoinic Technology Co., Ltd."},
  {0x0297, "Nisense AB"},
  {0x0298, "KOUKAAM a.s."},
  {0x0299, "onsemi"},
  {0x029A, "Array Networks, Inc."},
  {0x029B, "Cochlear Limited"},
  {0x029C, "Dolby Labs"},
  {0x029D, "Marquardt"},
  {0x029E, "LUGLOC LLC"},
  {0x029F, "HONG KONG SINO CORE ELECTRONCO"},
  {0x02A0, "Airtago"},
  {0x02A1, "Jami soft"},
  {0x02A2, "gd Moko Technology Co., Ltd"},
  {0x02A3, "Dongguan Daybright Inc"},
  {0x02A4, "Valve Corporation"},
  {0x02A5, "Naver Cloud Platform Corp."},
  {0x02A6, "Dongguan Liesheng Electronic Technology Co., Ltd"},
  {0x02A7, "Helios Sports, Inc."},
  {0x02A8, "The L.S. Starrett Company"},
  {0x02A9, "Scentco Inc"},
  {0x02AA, "Klipsch Western Technologies, Inc."},
  {0x02AB, "Direct Communication Solutions, Inc."},
  {0x02AC, "Ubiquitous Computing Technology Corporation"},
  {0x02AD, "Plus One Innovation"},
  {0x02AE, "VMAP"},
  {0x02AF, "Square Panda, Inc."},
  {0x02B0, "Myzee Technology"},
  {0x02B1, "Melange Systems Pvt. Ltd."},
  {0x02B2, "Kumo West USA"},
  {0x02B3, "PAC Manufacturing Inc."},
  {0x02B4, "NewAcre Co., Ltd."},
  {0x02B5, "C.O.B.O. SpA"},
  {0x02B6, "Security Enhancement Systems, LLC"},
  {0x02B7, "KYB Corporation"},
  {0x02B8, "Sinnoz"},
  {0x02B9, "Ross Video"},
  {0x02BA, "Chengdu Aich Technology"},
  {0x02BB, "LivaNova USA Inc."},
  {0x02BC, "LivaNova"},
  {0x02BD, "Yale"},
  {0x02BE, "SharkNinja Operating LLC"},
  {0x02BF, "Networking Technologies, Inc."},
  {0x02C0, "GPS103"},
  {0x02C1, "TrackTik"},
  {0x02C2, "Candy Hooper GmbH"},
  {0x02C3, "Baidu"},
  {0x02C4, "SmartSens Technology Co., Ltd."},
  {0x02C5, "NIO USA, Inc."},
  {0x02C6, "Sonos Inc"},
  {0x02C7, "Pride Therapy"},
  {0x02C8, "LifePlus, Inc."},
  {0x02C9, "Harman International"},
  {0x02CA, "Google, LLC"},
  {0x02CB, "ArcSoft"},
  {0x02CC, "Ingenieue Team UG"},
  {0x02CD, "Tineco"},
  {0x02CE, "Kobian Technologies Pty Ltd"},
  {0x02CF, "NIO USA, Inc."},
  {0x02D0, "D-MAX"},
  {0x02D1, "ALEX DENKO INC."},
  {0x02D2, "UpRight Technologies LTD"},
  {0x02D3, "VIMANA TECHNOLOGY PVT LTD"},
  {0x02D4, "Lumo Bodytech Inc."},
  {0x02D5, "Nanjing Xinxiangeng Technology Co., Ltd."},
  {0x02D6, "Pangaea Solution"},
  {0x02D7, "Beco"},
  {0x02D8, "HAI ROBOTS GmbH"},
  {0x02D9, "Lishu Technologies Inc."},
  {0x02DA, "Allterco Robotics ltd"},
  {0x02DB, "RAB Lighting"},
  {0x02DC, "Rakuten Mobility Inc."},
  {0x02DD, "U-Shin"},
  {0x02DE, "Check Technology LLC"},
  {0x02DF, "Wallbox"},
  {0x02E0, "TCI C.A."},
  {0x02E1, "Plantronics"},
  {0x02E2, "Kodak Realty Inc"},
  {0x02E3, "Pura"},
  {0x02E4, "JMR embedded systems"},
  {0x02E5, "Swedlock AB"},
  {0x02E6, "Nest Labs"},
  {0x02E7, "Amazon"},
  {0x02E8, "Cleveron"},
  {0x02E9, "Pitsch Speedwear"},
  {0x02EA, "Tricorder Arraay Technologies LLC"},
  {0x02EB, "Aclara LLC"},
  {0x02EC, "Kohler Company"},
  {0x02ED, "Unknown"},
  {0x02EE, "Mammut Networks AG"},
  {0x02EF, "Vocera Communications"},
  {0x02F0, "StarLeaf Ltd"},
  {0x02F1, "HEMMET AB"},
  {0x02F2, "Cardo Systems, Ltd"},
  {0x02F3, "DJO Global"},
  {0x02F4, "Nouvi Ltd"},
  {0x02F5, "iRobot Corporation"},
  {0x02F6, "Schrader Electronics"},
  {0x02F7, "Goodnet-CTC, Inc."},
  {0x02F8, "Luzumini Inc."},
  {0x02F9, "Cybex GmbH"},
  {0x02FA, "Fengfan (Beijing) Technology Co., Ltd"},
  {0x02FB, "Halfree"},
  {0x02FC, "Artificial Intelligence Infrastructure, Ltd."},
  {0x02FD, "Eli Lilly and Company"},
  {0x02FE, "Baidu"},
  {0x02FF, "Chengdu Tietong Electronic Technology Co., Ltd."},
  {0x0300, "CAME by Ident"},
  {0x0301, "Shenzhen iMCOGas Technology Co., Ltd"},
  {0x0302, "Pollen Robotics"},
  {0x0303, "Altonics"},
  {0x0304, "Mantis Tech LLC"},
  {0x0305, "Twocanoes Labs, LLC"},
  {0x0306, "Nami"},
  {0x0307, "LuminUltra Technologies Ltd"},
  {0x0308, "Merlinia A/S"},
  {0x0309, "Smart Parks"},
  {0x030A, "Lofelt"},
  {0x030B, "Strainstall"},
  {0x030C, "Channel Systems Inc"},
  {0x030D, "NITTO KOGYO"},
  {0x030E, "SENNHEISER"},
  {0x030F, "Cisco"},
  {0x0310, "iHealth Labs"},
  {0x0311, "TCL Connected Technologies"},
  {0x0312, "OJ Electronics"},
  {0x0313, "ABAX AB"},
  {0x0314, "Apple"},
  {0x0315, "Resmed Limited"},
  {0x0316, "Cochlear Limited"},
  {0x0317, "Luxer Corporation"},
  {0x0318, "SOLUX"},
  {0x0319, "Insulet Corporation"},
  {0x031A, "Candy Hoover Group"},
  {0x031B, "D-MAX"},
  {0x031C, "GD Midea Consumer Heating Technology Co., Ltd"},
  {0x031D, "CAME"},
  {0x031E, "Husqvarna AB"},
  {0x031F, "Focus Imaging"},
  {0x0320, "Sensorian Ltd"},
  {0x0321, "Truma Geraete GmbH"},
  {0x0322, "Offcode Oy"},
  {0x0323, "BENNING Digital GmbH"},
  {0x0324, "J. Wagner GmbH"},
  {0x0325, "Embrava Pty Ltd"},
  {0x0326, "Senscomm"},
  {0x0327, "Delphi"},
  {0x0328, "NITTO"},
  {0x0329, "Chargifi"},
  {0x032A, "Helios"},
  {0x032B, "Senscomm"},
  {0x032C, "BlueKitchen"},
  {0x032D, "Rigado"},
  {0x032E, "CUBE"},
  {0x032F, "TRSystems"},
  {0x0330, "CUBETECH"},
  {0x0331, "DPTechnics"},
  {0x0332, "Ztoic"},
  {0x0333, "Sena"},
  {0x0334, "SmartSensor"},
  {0x0335, "SAFATECH"},
  {0x0336, "Kartographers"},
  {0x0337, "Valeo"},
  {0x0338, "NIDEC"},
  {0x0339, "EasySRing"},
  {0x033A, "Shenzhen SuLong"},
  {0x033B, "ViewStar"},
  {0x033C, "Reoqoo"},
  {0x033D, "Sencilion"},
  {0x033E, "Avid"},
  {0x033F, "Littelfuse"},
  {0x0340, "Belparts"},
  {0x0341, "NeST"},
  {0x0342, "Southco"},
  {0x0343, "LEGO"},
  {0x0344, "Thetatronics"},
  {0x0345, "Nanoleaf"},
  {0x0346, "Urban Compass"},
  {0x0347, "digma"},
  {0x0348, "Optek"},
  {0x0349, "Bluepeaks"},
  {0x034A, "TIVIA"},
  {0x034B, "Alango"},
  {0x034C, "Mist"},
  {0x034D, "Startup Labs"},
  {0x034E, "Geopal"},
  {0x034F, "Openbrain"},
  {0x0350, "Xicato"},
  {0x0351, "Byton"},
  {0x0352, "GierLab"},
  {0x0353, "S4C"},
  {0x0354, "Matting"},
  {0x0355, "OJ"},
  {0x0356, "Schrack"},
  {0x0357, "Kiiroo"},
  {0x0358, "FlameCo"},
  {0x0359, "iRhythm"},
  {0x035A, "Ravenseft"},
  {0x035B, "SOLUX"},
  {0x035C, "Clover"},
  {0x035D, "Beflex"},
  {0x035E, "Smartspace"},
  {0x035F, "SAVOY"},
  {0x0360, "Nordic"},
  {0x0361, "DASH"},
  {0x0362, "Blincam"},
  {0x0363, "Luxer"},
  {0x0364, "RealTerm"},
  {0x0365, "Chargifi"},
  {0x0366, "Trivida"},
  {0x0367, "GETMATIC"},
  {0x0368, "Apple"},
  {0x0369, "Stalmart"},
  {0x036A, "AMETEK"},
  {0x036B, "SYNCHROVISION"},
  {0x036C, "Volkswagen"},
  {0x036D, "Skoda"},
  {0x036E, "Ford"},
  {0x036F, "GM"},
  {0x0370, "Honda"},
  {0x0371, "Hyundai"},
  {0x0372, "KIA"},
  {0x0373, "Valeo"},
  {0x0374, "Mercedes"},
  {0x0375, "BMW"},
  {0x0376, "PT Adira"},
  {0x0377, "Zhong Guan"},
  {0x0378, "ALS"},
  {0x0379, "SwaggerHeck"},
  {0x037A, "Wabilogic"},
  {0x037B, "Societe des Produits Bergeron"},
  {0x037C, "Nearfield Solutions"},
  {0x037D, "Spaceek"},
  {0x037E, "TTS"},
  {0x037F, "XLN"},
  {0x0380, "Pharynks"},
  {0x0381, "LEGO"},
  {0x0382, "Thalmic"},
  {0x0383, "Cochlear"},
  {0x0384, "Savitech"},
  {0x0385, "Produal"},
  {0x0386, "EyeTech"},
  {0x0387, "FMW"},
  {0x0388, "CareView"},
  {0x0389, "C.O.B.O."},
  {0x038A, "ABS"},
  {0x038B, "Lapstar"},
  {0x038C, "C.R. Bard"},
  {0x038D, "TMS"},
  {0x038E, "Pearl"},
  {0x038F, "Caterpillar"},
  {0x0390, "Hanna"},
  {0x0391, "Kocomojo"},
  {0x0392, "Everykey"},
  {0x0393, "Acuity"},
  {0x0394, "Panda Scout"},
  {0x0395, "Brilliant"},
  {0x0396, "Panasonic"},
  {0x0397, "Airoha"},
  {0x0398, "Newlab"},
  {0x0399, "Sky Wave"},
  {0x039A, "Grip Health"},
  {0x039B, "Yale"},
  {0x039C, "Inplay"},
  {0x039D, "Cleveron"},
  {0x039E, "Remedee"},
  {0x039F, "STARLITE"},
  {0x03A0, "microTracer"},
  {0x03A1, "Telink"},
  {0x03A2, "iCOGNIZE"},
  {0x03A3, "ScentConnect"},
  {0x03A4, "Dolby"},
  {0x03A5, "EllieGrid"},
  {0x03A6, "Lowerstep"},
  {0x03A7, "FNIRCI"},
  {0x03A8, "Beco"},
  {0x03A9, "Scosche"},
  {0x03AA, "Mightylogic"},
  {0x03AB, "Vocera"},
  {0x03AC, "CSR Building"},
  {0x03AD, "Freebird"},
  {0x03AE, "PMD"},
  {0x03AF, "Sino Wealth"},
  {0x03B0, "Aptcode"},
  {0x03B1, "LEGO"},
  {0x03B2, "Eijkelkamp"},
  {0x03B3, "B-Soft"},
  {0x03B4, "Stamford"},
  {0x03B5, "SPICA"},
  {0x03B6, "Avack"},
  {0x03B7, "WAFER-SCALE"},
  {0x03B8, "Metronomik"},
  {0x03B9, "SimpleSafe"},
  {0x03BA, "Dotty"},
  {0x03BB, "American Music"},
  {0x03BC, "ITD"},
  {0x03BD, "Sibel"},
  {0x03BE, "Schawbel"},
  {0x03BF, "SL"},
  {0x03C0, "Coresite"},
  {0x03C1, "SwiftSensors"},
  {0x03C2, "Bluehackes"},
  {0x03C3, "UiT"},
  {0x03C4, "LookX"},
  {0x03C5, "Smart Innovation"},
  {0x03C6, "CeoTronics"},
  {0x03C7, "Taobao"},
  {0x03C8, "WS Audio"},
  {0x03C9, "LEGO"},
  {0x03CA, "Nike"},
  {0x03CB, "iData"},
  {0x03CC, "Rivet"},
  {0x03CD, "G-Sight"},
  {0x03CE, "Qingping"},
  {0x03CF, "Pal"},
  {0x03D0, "FloDesign"},
  {0x03D1, "SonicSensory"},
  {0x03D2, "LinkSquares"},
  {0x03D3, "TeAM"},
  {0x03D4, "Hach"},
  {0x03D5, "Nanoleaf"},
  {0x03D6, "Spacelabs"},
  {0x03D7, "instagrid"},
  {0x03D8, "Mist"},
  {0x03D9, "noa"},
  {0x03DA, "Polidea"},
  {0x03DB, "Tenstorrel"},
  {0x03DC, "Sigma"},
  {0x03DD, "HOP"},
  {0x03DE, "Nikunj"},
  {0x03DF, "GeLo"},
  {0x03E0, "WDF"},
  {0x03E1, "Tokai-rika"},
  {0x03E2, "XIAMENINE"},
  {0x03E3, "SOLUX"},
  {0x03E4, "Lumens For Less"},
  {0x03E5, "Brother"},
  {0x03E6, "C.O.B.O."},
  {0x03E7, "Milestone"},
  {0x03E8, "BELL"},
  {0x03E9, "Rivata"},
  {0x03EA, "Almendo"},
  {0x03EB, "SILVER TREE"},
  {0x03EC, "Vimar"},
  {0x03ED, "Moxa"},
  {0x03EE, "Harman"},
  {0x03EF, "Systemic"},
  {0x03F0, "Nuvoton"},
  {0x03F1, "Chengdu"},
  {0x03F2, "Corvex"},
  {0x03F3, "Aii"},
  {0x03F4, "Neatebox"},
  {0x03F5, "Dell"},
  {0x03F6, "Airbly"},
  {0x03F7, "AIAIAI"},
  {0x03F8, "SafeLine"},
  {0x03F9, "Denso"},
  {0x03FA, "Prolon"},
  {0x03FB, "Shadow"},
  {0x03FC, "Appside"},
  {0x03FD, "DELABIE"},
  {0x03FE, "SensiLow"},
  {0x03FF, "SimpliSafe"}
};

#define MANUFACTURER_COUNT (sizeof(manufacturers) / sizeof(manufacturers[0]))

// ===== BLE Callbacks =====
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (btState != BT_SCANNING) return;
    
    // Get MAC address
    String mac = advertisedDevice.getAddress().toString().c_str();
    
    // Get name from advertising data
    String name = advertisedDevice.getName();
    if (name == "" || name == "Unknown") name = "Unknown Device";
    String completeName = name;
    
    // Get RSSI
    int rssi = advertisedDevice.getRSSI();
    
    // Get manufacturer data
    String mfrData = advertisedDevice.getManufacturerData();
    String manufacturer = "Unknown";
    String mfrId = "N/A";
    if (mfrData.length() >= 2) {
      uint16_t id = (uint8_t)mfrData[1] | ((uint8_t)mfrData[0] << 8);
      mfrId = "0x" + String(id, HEX);
      
      // Lookup manufacturer name
      for (int i = 0; i < MANUFACTURER_COUNT; i++) {
        if (manufacturers[i].id == id) {
          manufacturer = manufacturers[i].name;
          break;
        }
      }
    }
    
    // Get service UUIDs
    String uuids = "";
    if (advertisedDevice.haveServiceUUID()) {
      BLEUUID serviceUuid = advertisedDevice.getServiceUUID();
      uuids = serviceUuid.toString().c_str();
    }
    
    // Get TX Power (some NimBLE versions support getTXPower())
    String txPower = "";
    if (advertisedDevice.getTXPower() != 127) { // 127 means not available
      txPower = String(advertisedDevice.getTXPower()) + " dBm";
    }
    
    // Get appearance
    String appearance = "";
    if (advertisedDevice.haveAppearance()) {
      appearance = "0x" + String(advertisedDevice.getAppearance(), HEX);
    }
    
    // Advertising interval not directly available via NimBLE API
    String advInterval = "";
    
    // Store device if not already in list
    bool found = false;
    for (int i = 0; i < deviceCount; i++) {
      if (devices[i].macAddress == mac) {
        found = true;
        if (rssi > devices[i].rssi) {
          devices[i].rssi = rssi;
        }
        if (completeName.length() > 0 && name.length() == 0) {
          name = completeName;
        }
        devices[i].name = name;
        break;
      }
    }
    
    if (!found && deviceCount < MAX_DEVICES) {
      devices[deviceCount].name = name;
      devices[deviceCount].macAddress = mac;
      devices[deviceCount].rssi = rssi;
      devices[deviceCount].manufacturer = manufacturer;
      devices[deviceCount].manufacturerId = mfrId;
      devices[deviceCount].serviceUuids = uuids;
      devices[deviceCount].txPower = txPower;
      devices[deviceCount].appearance = appearance;
      devices[deviceCount].advInterval = advInterval;
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

// Draw BT scanner - scanning state
void drawBtScanning() {
  tft.fillScreen(0x000000);
  tft.setTextColor(0xFFFFFF);
  tft.setTextDatum(TC_DATUM);
  tft.drawCentreString("Scanning...", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 20, 2);
  tft.drawCentreString("BLE devices nearby", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 20, 2);
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

// Perform a BLE scan
void performBtScan() {
  btState = BT_SCANNING;
  deviceCount = 0;
  
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); // Active scan to get SCAN_RSP
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  drawBtScanning();
  
  // Scan for 10 seconds
 BLEScanResults* results = pBLEScan->start(10, false);
  
  // Sort devices by RSSI (strongest first)
  for (int i = 0; i < deviceCount - 1; i++) {
    for (int j = i + 1; j < deviceCount; j++) {
      if (devices[j].rssi > devices[i].rssi) {
        DeviceInfo temp = devices[i];
        devices[i] = devices[j];
        devices[j] = temp;
      }
    }
  }
  
  totalDevices = deviceCount;
  currentDeviceIndex = 0;
  
  if (deviceCount > 0) {
    btState = BT_SHOWING_RESULTS;
    drawBtResults();
  } else {
    btState = BT_IDLE;
    drawMainMenu();
  }
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
                performBtScan();
                currentScreen = SCREEN_BT_SCANNER;
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
          drawMainMenu();
          break;
        }
        
        switch (btState) {
          case BT_SCANNING:
            // Do nothing during scan
            break;
            
          case BT_SHOWING_RESULTS: {
            // Check scan again button at bottom
            if (ty > SCREEN_HEIGHT - 50 && tx > 50 && tx < SCREEN_WIDTH - 50) {
              performBtScan();
            }
            break;
          }
          
          case BT_SHOWING_DEVICE: {
            // Check prev button
            if (currentDeviceIndex > 0 && tx >= 10 && tx <= 80 && ty >= SCREEN_HEIGHT - 40 && ty <= SCREEN_HEIGHT - 5) {
              currentDeviceIndex--;
              drawBtDevice(currentDeviceIndex);
              break;
            }
            
            // Check next button
            if (currentDeviceIndex < deviceCount - 1 && tx >= SCREEN_WIDTH - 80 && tx <= SCREEN_WIDTH - 10 && ty >= SCREEN_HEIGHT - 40 && ty <= SCREEN_HEIGHT - 5) {
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
