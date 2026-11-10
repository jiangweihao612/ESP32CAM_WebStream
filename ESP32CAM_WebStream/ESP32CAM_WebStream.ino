/* =====================================================================
 *  ESP32-CAM 实时图传 + SD 文件管理 + AVI 录屏  (STA + AP 双模)
 * ---------------------------------------------------------------------
 *  功能:
 *    1. STA(连路由器) + AP(自发热点) 同时在线
 *    2. 网页 MJPEG 实时预览, 可调分辨率/画质/亮度/对比度/镜像/翻转
 *    3. 录屏: 把画面录成 .avi (MJPEG) 直接写进 SD 卡, 电脑可直接播放
 *    4. SD 文件管理: 浏览目录 / 下载 / 上传 / 重命名 / 删除 / 新建文件夹 / 清空
 *    5. 抓拍下载 / 单张存 SD / 补光灯 / 实时状态
 *
 *  Arduino IDE 环境准备:
 *    - 开发板管理器网址: https://espressif.github.io/arduino-esp32/package_esp32_index.json
 *    - 开发板: "AI Thinker ESP32-CAM"
 *    - Partition Scheme: "Huge APP (3MB No OTA/1MB SPIFFS)"
 *    - PSRAM: Enabled
 *    - 上传时: GPIO0 接 GND, 按一下 RST; 传完拔掉 GPIO0 再按 RST
 *
 *  接线 (USB-TTL): 5V->5V  GND->GND  TX->U0R  RX->U0T
 * ===================================================================== */

#include <WiFi.h>
#include <WebServer.h>
#include <esp_camera.h>
#include <SD_MMC.h>

/* ==================== 1. WiFi 凭据 ====================
 *  真实 SSID / 密码放在 secrets.h (已被 .gitignore 排除)
 *  首次使用: 复制 secrets.example.h 为 secrets.h 再填写
 * ========================================================= */
#if defined(__has_include)
#  if __has_include("secrets.h")
#    include "secrets.h"
#  else
#    error "缺少 secrets.h —— 请把 secrets.example.h 复制一份改名为 secrets.h 并填写 WiFi 信息"
#  endif
#else
#  include "secrets.h"
#endif

const uint16_t WEB_PORT    = 80;             // 网页 / 控制 / 文件管理
const uint16_t STREAM_PORT = 81;             // 视频流(独立端口, 不阻塞控制)

const uint16_t REC_MAX_SEC   = 600;                   // 单次录制最长秒数
const uint32_t REC_MAX_BYTES = 200UL * 1024 * 1024;   // 单个文件最大 200MB
/* ========================================================= */

/* ---------- AI-Thinker ESP32-CAM 摄像头引脚 ---------- */
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22
#define FLASH_LED_PIN    4

/* ---------- MJPEG 流协议 ---------- */
#define BOUNDARY "esp32camframe"
static const char *STREAM_CT       = "multipart/x-mixed-replace; boundary=" BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" BOUNDARY "\r\n";
static const char *STREAM_PART     = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

/* ---------- AVI 文件布局常量 (见 recStart 注释) ---------- */
#define AVI_OFF_RIFF_SIZE   4     // RIFF 整文件长度
#define AVI_OFF_AVIH_FRAMES 48    // avih.dwTotalFrames
#define AVI_OFF_STRH_LEN    140   // strh.dwLength
#define AVI_OFF_MOVI_SIZE   216   // movi LIST 长度
#define AVI_MOVI_BASE       220   // movi 数据起点(索引偏移基准)
#define AVI_HDR_LEN         224   // 头部总长度

/* ---------- 全局对象 ---------- */
WebServer  server(WEB_PORT);
WiFiServer streamServer(STREAM_PORT);
WiFiClient streamClient;

bool     sdReady       = false;
bool     hasPSRAM      = false;
uint32_t framesSent    = 0;
uint32_t lastFrameMs   = 0;
uint32_t lastWifiCheck = 0;
int      curFsIdx      = FRAMESIZE_SVGA;
int      curQuality    = 12;
bool     flashOn       = false;

/* 录屏状态 */
bool      recActive = false;
File      recFile;
uint8_t  *recIdx     = nullptr;   // 帧索引缓冲: 每帧 8 字节(offset,size)
uint32_t  recIdxCap  = 0;
uint32_t  recFrames  = 0;
uint32_t  recBytes   = 0;
uint32_t  recStartMs = 0;
uint32_t  recLastMs  = 0;
uint16_t  recFps     = 10;
char      recPath[40] = "";

/* 上传状态 */
File     upFile;
size_t   upWritten = 0;
bool     upOk      = false;

/* 分辨率名字表(避免不同 core 版本枚举顺序差异) */
struct FsEntry { const char *name; framesize_t fs; };
static const FsEntry FS_TABLE[] = {
  {"QQVGA", FRAMESIZE_QQVGA}, {"CIF",  FRAMESIZE_CIF},  {"VGA",  FRAMESIZE_VGA},
  {"SVGA",  FRAMESIZE_SVGA},  {"XGA",  FRAMESIZE_XGA},  {"HD",   FRAMESIZE_HD},
  {"SXGA",  FRAMESIZE_SXGA},  {"UXGA", FRAMESIZE_UXGA},
};
static const int FS_TABLE_N = sizeof(FS_TABLE) / sizeof(FS_TABLE[0]);

/* 注意: FS_TABLE 的下标 != framesize_t 枚举值, 必须查表 */
const char *fsName(int fs) {
  for (int i = 0; i < FS_TABLE_N; i++)
    if ((int)FS_TABLE[i].fs == fs) return FS_TABLE[i].name;
  return "VGA";
}

/* ==================== 网页 (PROGMEM, 见 web_page.h) ==================== */
#include "web_page.h"

/* ==================== 工具函数 ==================== */
static void putU32(uint8_t *p, uint32_t v) {
  p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}
static void putU16(uint8_t *p, uint16_t v) {
  p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
}
static void aviWr32(File &f, uint32_t v) { uint8_t b[4]; putU32(b, v); f.write(b, 4); }
static void aviWr4CC(File &f, const char *cc) { f.write((const uint8_t *)cc, 4); }

String jsonEsc(const String &s) {
  String o; o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char ch = s[i];
    if (ch == '"')       o += "\\\"";
    else if (ch == '\\') o += "\\\\";
    else if ((uint8_t)ch < 0x20) o += ' ';
    else o += ch;
  }
  return o;
}

String baseName(const String &p) {
  int i = p.lastIndexOf('/');
  return (i >= 0) ? p.substring(i + 1) : p;
}

String dirNameOf(const String &p) {
  int i = p.lastIndexOf('/');
  return (i > 0) ? p.substring(0, i) : "/";
}

String normalizePath(String p) {
  if (!p.startsWith("/")) p = "/" + p;
  while (p.indexOf("//") >= 0) p.replace("//", "/");
  while (p.indexOf("..") >= 0) p.replace("..", "");
  if (p.length() > 1 && p.endsWith("/")) p.remove(p.length() - 1);
  return p;
}

bool validName(const String &n) {
  if (n.length() == 0 || n.length() > 64) return false;
  if (n.indexOf('/') >= 0 || n.indexOf('\\') >= 0) return false;
  return true;
}

/* ==================== 摄像头初始化 ==================== */
bool initCamera() {
  camera_config_t cfg;
  cfg.ledc_channel   = LEDC_CHANNEL_0;
  cfg.ledc_timer     = LEDC_TIMER_0;
  cfg.pin_pwdn       = PWDN_GPIO_NUM;
  cfg.pin_reset      = RESET_GPIO_NUM;
  cfg.pin_xclk       = XCLK_GPIO_NUM;
  cfg.pin_sccb_sda   = SIOD_GPIO_NUM;
  cfg.pin_sccb_scl   = SIOC_GPIO_NUM;
  cfg.pin_d7         = Y9_GPIO_NUM;
  cfg.pin_d6         = Y8_GPIO_NUM;
  cfg.pin_d5         = Y7_GPIO_NUM;
  cfg.pin_d4         = Y6_GPIO_NUM;
  cfg.pin_d3         = Y5_GPIO_NUM;
  cfg.pin_d2         = Y4_GPIO_NUM;
  cfg.pin_d1         = Y3_GPIO_NUM;
  cfg.pin_d0         = Y2_GPIO_NUM;
  cfg.pin_vsync      = VSYNC_GPIO_NUM;
  cfg.pin_href       = HREF_GPIO_NUM;
  cfg.pin_pclk       = PCLK_GPIO_NUM;
  cfg.xclk_freq_hz   = 20000000;
  cfg.pixel_format   = PIXFORMAT_JPEG;
  cfg.grab_mode      = CAMERA_GRAB_LATEST;

  hasPSRAM = psramFound();

  if (hasPSRAM) {
    cfg.frame_size   = FRAMESIZE_SVGA;
    cfg.jpeg_quality = 12;
    cfg.fb_count     = 2;
    cfg.fb_location  = CAMERA_FB_IN_PSRAM;
    curFsIdx         = FRAMESIZE_SVGA;
  } else {
    cfg.frame_size   = FRAMESIZE_CIF;
    cfg.jpeg_quality = 14;
    cfg.fb_count     = 1;
    cfg.fb_location  = CAMERA_FB_IN_DRAM;
    curFsIdx         = FRAMESIZE_CIF;
  }

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    Serial.printf("[CAM] 初始化失败 0x%x\n", err);
    return false;
  }
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_gain_ctrl(s, 1);
  }
  Serial.println("[CAM] 初始化成功");
  return true;
}

/* ==================== SD 卡 (1-bit SDMMC) ==================== */
bool initSD() {
  SD_MMC.setPins(14, 15, 2);   // 避开 GPIO4(补光灯) 和 SD 的 D1/D2/D3
  if (!SD_MMC.begin("/sdcard", true, false, 20000)) {
    Serial.println("[SD ] 挂载失败(没插卡或接触不良)");
    return false;
  }
  if (SD_MMC.cardType() == CARD_NONE) {
    Serial.println("[SD ] 未检测到 SD 卡");
    return false;
  }
  Serial.printf("[SD ] 就绪  容量 %llu MB  已用 %llu MB\n",
                SD_MMC.cardSize() / (1024 * 1024), SD_MMC.usedBytes() / (1024 * 1024));
  return true;
}

/* ==================== 录屏: AVI(MJPEG) 封装 ==================== */
/*
 * 文件布局:
 *   0    'RIFF' <size>            <- size 收尾时回填
 *   8    'AVI '
 *   12   'LIST' <192> 'hdrl'
 *   24     'avih' <56> ... dwTotalFrames @48
 *   88     'LIST' <116> 'strl'
 *   100      'strh' <56> ... dwLength @140
 *   164      'strf' <40> BITMAPINFOHEADER
 *   212  'LIST' <size> 'movi'     <- size 收尾时回填
 *   224    '00dc' <len> <jpeg>    <- 每帧一个块, 奇数长度补 1 字节
 *   ...  'idx1' <16*n> 索引
 */
bool recStart(uint16_t fps) {
  if (recActive) return false;
  if (!sdReady)  return false;
  if (fps < 1 || fps > 30) fps = 10;

  uint64_t total = SD_MMC.totalBytes(), used = SD_MMC.usedBytes();
  uint32_t freeMB = (total > used) ? (uint32_t)((total - used) / (1024 * 1024)) : 0;
  if (freeMB < 20) {
    Serial.println("[REC] SD 剩余空间不足 20MB");
    return false;
  }

  int w = resolution[curFsIdx].width;
  int h = resolution[curFsIdx].height;

  // 找一个没用过的文件名
  int n = -1;
  for (int i = 1; i <= 9999; i++) {
    snprintf(recPath, sizeof(recPath), "/REC_%04d.avi", i);
    if (!SD_MMC.exists(recPath)) { n = i; break; }
  }
  if (n < 0) return false;

  recFile = SD_MMC.open(recPath, FILE_WRITE);
  if (!recFile) { recPath[0] = 0; return false; }

  // 帧索引缓冲
  uint32_t maxFrames = (uint32_t)fps * REC_MAX_SEC;
  uint32_t capBytes  = maxFrames * 8;
  if (capBytes > 96 * 1024) { capBytes = 96 * 1024; maxFrames = capBytes / 8; }
  recIdx = (uint8_t *)(hasPSRAM ? ps_malloc(capBytes) : malloc(capBytes));
  if (!recIdx) {
    recFile.close(); SD_MMC.remove(recPath); recPath[0] = 0;
    Serial.println("[REC] 索引内存不足");
    return false;
  }
  recIdxCap  = maxFrames;
  recFrames  = 0;
  recBytes   = 0;
  recFps     = fps;
  recStartMs = millis();
  recLastMs  = 0;

  // ---- 写 224 字节头部 ----
  uint8_t hdr[AVI_HDR_LEN];
  memset(hdr, 0, sizeof(hdr));
  uint32_t usPerFrame = 1000000UL / fps;

  memcpy(hdr + 0, "RIFF", 4);
  putU32(hdr + AVI_OFF_RIFF_SIZE, 0);            // 回填
  memcpy(hdr + 8, "AVI ", 4);

  memcpy(hdr + 12, "LIST", 4);
  putU32(hdr + 16, 192);
  memcpy(hdr + 20, "hdrl", 4);

  memcpy(hdr + 24, "avih", 4);
  putU32(hdr + 28, 56);
  putU32(hdr + 32, usPerFrame);        // dwMicroSecPerFrame
  putU32(hdr + 36, 2000000);           // dwMaxBytesPerSec
  putU32(hdr + 40, 0);                 // dwPaddingGranularity
  putU32(hdr + 44, 0x10);              // dwFlags = AVIF_HASINDEX
  putU32(hdr + AVI_OFF_AVIH_FRAMES, 0);// dwTotalFrames 回填
  putU32(hdr + 52, 0);                 // dwInitialFrames
  putU32(hdr + 56, 1);                 // dwStreams
  putU32(hdr + 60, 200000);            // dwSuggestedBufferSize
  putU32(hdr + 64, w);
  putU32(hdr + 68, h);

  memcpy(hdr + 88, "LIST", 4);
  putU32(hdr + 92, 116);
  memcpy(hdr + 96, "strl", 4);

  memcpy(hdr + 100, "strh", 4);
  putU32(hdr + 104, 56);
  memcpy(hdr + 108, "vids", 4);
  memcpy(hdr + 112, "MJPG", 4);
  putU32(hdr + 116, 0);                // dwFlags
  putU16(hdr + 120, 0);                // wPriority
  putU16(hdr + 122, 0);                // wLanguage
  putU32(hdr + 124, 0);                // dwInitialFrames
  putU32(hdr + 128, 1);                // dwScale
  putU32(hdr + 132, fps);              // dwRate
  putU32(hdr + 136, 0);                // dwStart
  putU32(hdr + AVI_OFF_STRH_LEN, 0);   // dwLength 回填
  putU32(hdr + 144, 200000);           // dwSuggestedBufferSize
  putU32(hdr + 148, 0);                // dwQuality
  putU32(hdr + 152, 0);                // dwSampleSize
  putU16(hdr + 156, 0);                // rcFrame.left
  putU16(hdr + 158, 0);                // rcFrame.top
  putU16(hdr + 160, (uint16_t)w);      // rcFrame.right
  putU16(hdr + 162, (uint16_t)h);      // rcFrame.bottom

  memcpy(hdr + 164, "strf", 4);
  putU32(hdr + 168, 40);
  putU32(hdr + 172, 40);               // biSize
  putU32(hdr + 176, w);
  putU32(hdr + 180, h);
  putU16(hdr + 184, 1);                // biPlanes
  putU16(hdr + 186, 24);               // biBitCount
  memcpy(hdr + 188, "MJPG", 4);        // biCompression
  putU32(hdr + 192, (uint32_t)w * h * 3);  // biSizeImage

  memcpy(hdr + 212, "LIST", 4);
  putU32(hdr + AVI_OFF_MOVI_SIZE, 0);  // 回填
  memcpy(hdr + 220, "movi", 4);

  recFile.write(hdr, AVI_HDR_LEN);

  recActive = true;
  Serial.printf("[REC] 开始 %s  %dx%d @%dfps  可用 %uMB\n", recPath, w, h, fps, freeMB);
  return true;
}

void recWriteFrame(camera_fb_t *fb) {
  uint32_t pos = recFile.position();
  uint8_t ch[8];
  memcpy(ch, "00dc", 4);
  putU32(ch + 4, fb->len);
  recFile.write(ch, 8);
  recFile.write(fb->buf, fb->len);
  if (fb->len & 1) { uint8_t z = 0; recFile.write(&z, 1); }   // 块对齐

  putU32(recIdx + recFrames * 8, pos - AVI_MOVI_BASE);
  putU32(recIdx + recFrames * 8 + 4, fb->len);
  recFrames++;
  recBytes += fb->len + 8;
}

void recStop() {
  if (!recActive) return;
  recActive = false;

  uint32_t moviEnd = recFile.position();

  // 回填各处长度
  recFile.seek(48);  aviWr32(recFile, recFrames);
  recFile.seek(140); aviWr32(recFile, recFrames);
  recFile.seek(216); aviWr32(recFile, moviEnd - AVI_MOVI_BASE);

  // 追加 idx1 索引
  recFile.seek(moviEnd);
  aviWr4CC(recFile, "idx1");
  aviWr32(recFile, recFrames * 16);
  for (uint32_t i = 0; i < recFrames; i++) {
    uint8_t e[16];
    memcpy(e, "00dc", 4);
    putU32(e + 4, 0x10);                               // AVIIF_KEYFRAME
    putU32(e + 8, *(uint32_t *)(recIdx + i * 8));       // offset
    putU32(e + 12, *(uint32_t *)(recIdx + i * 8 + 4));  // size
    recFile.write(e, 16);
  }
  uint32_t total = recFile.position();
  recFile.seek(4); aviWr32(recFile, total - 8);
  recFile.close();

  if (recIdx) {
    free(recIdx);
    recIdx = nullptr;
  }
  Serial.printf("[REC] 停止 %s  %u 帧  %u KB  %u 秒\n",
                recPath, recFrames, total / 1024, (millis() - recStartMs) / 1000);
}

void recTick() {
  if (!recActive) return;
  uint32_t now = millis();
  if (now - recLastMs < (uint32_t)(1000 / recFps)) return;
  recLastMs = now;

  if (recBytes > REC_MAX_BYTES || recFrames >= recIdxCap ||
      (millis() - recStartMs) > (uint32_t)REC_MAX_SEC * 1000) {
    Serial.println("[REC] 达到上限, 自动停止");
    recStop();
    return;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;
  recWriteFrame(fb);
  esp_camera_fb_return(fb);
}

/* ==================== 网页处理 ==================== */
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
  uint64_t total = SD_MMC.totalBytes(), used = SD_MMC.usedBytes();
  uint32_t freeMB = (sdReady && total > used) ? (uint32_t)((total - used) / (1024 * 1024)) : 0;

  String j = "{";
  j += "\"apip\":\""  + WiFi.softAPIP().toString() + "\",";
  j += "\"ap\":\""    + String(AP_SSID) + "\",";
  j += "\"staip\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("")) + "\",";
  j += "\"staid\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String("")) + "\",";
  j += "\"rssi\":"    + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0) + ",";
  j += "\"heap\":"    + String(ESP.getFreeHeap()) + ",";
  j += "\"sd\":"      + String(sdReady ? "true" : "false") + ",";
  j += "\"sdfree\":"  + String(freeMB) + ",";
  j += "\"fs\":\""    + String(fsName(curFsIdx)) + "\",";
  j += "\"quality\":" + String(curQuality) + ",";
  // 传感器真实回读值: 让网页端和小程序端都能看到"另一端"改成了什么
  sensor_t *sn = esp_camera_sensor_get();
  j += "\"bri\":"     + String(sn ? sn->status.brightness : 0) + ",";
  j += "\"con\":"     + String(sn ? sn->status.contrast   : 0) + ",";
  j += "\"hm\":"      + String(sn ? sn->status.hmirror    : 0) + ",";
  j += "\"vf\":"      + String(sn ? sn->status.vflip      : 0) + ",";
  j += "\"led\":"     + String(flashOn ? 1 : 0) + ",";
  j += "\"frames\":"  + String(framesSent) + ",";
  j += "\"rec\":"     + String(recActive ? "true" : "false") + ",";
  j += "\"recsec\":"  + String(recActive ? (millis() - recStartMs) / 1000 : 0) + ",";
  j += "\"recf\":"    + String(recFrames) + ",";
  j += "\"up\":"      + String(millis() / 1000);
  j += "}";
  server.send(200, "application/json", j);
}

void handleControl() {
  sensor_t *s = esp_camera_sensor_get();
  if (!s) { server.send(500, "application/json", "{\"ok\":false}"); return; }

  if (server.hasArg("fs") && !recActive) {
    String fs = server.arg("fs");
    for (int i = 0; i < FS_TABLE_N; i++) {
      if (fs == FS_TABLE[i].name) {
        if (s->set_framesize(s, FS_TABLE[i].fs) == 0) curFsIdx = FS_TABLE[i].fs;
        break;
      }
    }
  }
  if (server.hasArg("quality")) {
    curQuality = constrain(server.arg("quality").toInt(), 4, 63);
    s->set_quality(s, curQuality);
  }
  if (server.hasArg("brightness"))
    s->set_brightness(s, constrain(server.arg("brightness").toInt(), -2, 2));
  if (server.hasArg("contrast"))
    s->set_contrast(s, constrain(server.arg("contrast").toInt(), -2, 2));
  if (server.hasArg("hmirror"))
    s->set_hmirror(s, server.arg("hmirror").toInt() ? 1 : 0);
  if (server.hasArg("vflip"))
    s->set_vflip(s, server.arg("vflip").toInt() ? 1 : 0);
  if (server.hasArg("flash")) {
    flashOn = server.arg("flash").toInt() != 0;
    digitalWrite(FLASH_LED_PIN, flashOn ? HIGH : LOW);
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleCapture() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) { server.send(500, "text/plain", "capture failed"); return; }
  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

/* ---- 单张存 SD ---- */
void handleSave() {
  if (!sdReady) {
    server.send(503, "application/json", "{\"ok\":false,\"msg\":\"SD 卡未就绪\"}");
    return;
  }
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "application/json", "{\"ok\":false,\"msg\":\"取图失败\"}");
    return;
  }
  char path[32];
  int n = -1;
  for (int i = 1; i <= 9999; i++) {
    snprintf(path, sizeof(path), "/IMG_%04d.jpg", i);
    if (!SD_MMC.exists(path)) { n = i; break; }
  }
  if (n < 0) {
    esp_camera_fb_return(fb);
    server.send(507, "application/json", "{\"ok\":false,\"msg\":\"文件名已满\"}");
    return;
  }
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) {
    esp_camera_fb_return(fb);
    server.send(500, "application/json", "{\"ok\":false,\"msg\":\"无法写入\"}");
    return;
  }
  size_t w = f.write(fb->buf, fb->len);
  f.close();
  size_t len = fb->len;
  esp_camera_fb_return(fb);
  if (w != len) {
    server.send(500, "application/json", "{\"ok\":false,\"msg\":\"写入不完整\"}");
    return;
  }
  String j = "{\"ok\":true,\"name\":\"" + String(path) + "\",\"size\":" + String((unsigned)len) + "}";
  server.send(200, "application/json", j);
  Serial.printf("[SD ] 已保存 %s (%u bytes)\n", path, (unsigned)len);
}

/* ---- 录制接口 ---- */
void handleRecStart() {
  uint16_t fps = server.hasArg("fps") ? server.arg("fps").toInt() : 10;
  if (!sdReady) {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"SD 卡未就绪\"}");
    return;
  }
  if (!recStart(fps)) {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"无法开始(空间不足或写卡失败)\"}");
    return;
  }
  String j = "{\"ok\":true,\"name\":\"" + String(recPath) + "\"}";
  server.send(200, "application/json", j);
}

void handleRecStop() {
  if (!recActive) {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"当前没有在录制\"}");
    return;
  }
  char path[40];
  strncpy(path, recPath, sizeof(path) - 1);
  path[sizeof(path) - 1] = 0;
  uint32_t n = recFrames;
  recStop();

  size_t sz = 0;
  File f = SD_MMC.open(path, FILE_READ);
  if (f) { sz = f.size(); f.close(); }

  String j = "{\"ok\":true,\"name\":\"" + String(path) + "\",\"frames\":" +
             String(n) + ",\"size\":" + String((unsigned)sz) + "}";
  server.send(200, "application/json", j);
}

/* ==================== SD 文件管理 ==================== */
void handleList() {
  if (!sdReady) {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"SD 卡未就绪\"}");
    return;
  }
  String path = normalizePath(server.hasArg("path") ? server.arg("path") : "/");
  File dir = SD_MMC.open(path);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"目录不存在\"}");
    return;
  }

  String items = "";
  int count = 0;

  // 第一轮: 目录
  File e = dir.openNextFile();
  while (e) {
    String nm = baseName(String(e.name()));
    bool isDir = e.isDirectory();
    e.close();
    if (isDir) {
      if (count++) items += ",";
      items += "{\"n\":\"" + jsonEsc(nm) + "\",\"s\":0,\"d\":1}";
    }
    e = dir.openNextFile();
  }
  dir.close();

  // 第二轮: 文件
  dir = SD_MMC.open(path);
  if (dir) {
    e = dir.openNextFile();
    while (e) {
      String nm = baseName(String(e.name()));
      bool isDir = e.isDirectory();
      size_t sz  = e.size();
      e.close();
      if (!isDir) {
        if (count++) items += ",";
        items += "{\"n\":\"" + jsonEsc(nm) + "\",\"s\":" + String((unsigned)sz) + ",\"d\":0}";
      }
      e = dir.openNextFile();
    }
    dir.close();
  }

  uint64_t total = SD_MMC.totalBytes(), used = SD_MMC.usedBytes();
  String j = "{\"ok\":true,\"path\":\"" + jsonEsc(path) + "\",";
  j += "\"total\":" + String((unsigned long)(total / (1024ULL * 1024ULL))) + ",";
  j += "\"used\":"  + String((unsigned long)(used  / (1024ULL * 1024ULL))) + ",";
  j += "\"items\":[" + items + "]}";
  server.send(200, "application/json", j);
}

void handleDownload() {
  if (!sdReady) { server.send(503, "text/plain", "SD not ready"); return; }
  String path = normalizePath(server.hasArg("path") ? server.arg("path") : "/");
  File f = SD_MMC.open(path, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    server.send(404, "text/plain", "not found");
    return;
  }
  String name = baseName(path);
  String lower = name; lower.toLowerCase();
  String mime = "application/octet-stream";
  if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) mime = "image/jpeg";
  else if (lower.endsWith(".avi")) mime = "video/x-msvideo";
  else if (lower.endsWith(".mp4")) mime = "video/mp4";
  else if (lower.endsWith(".txt")) mime = "text/plain";
  else if (lower.endsWith(".json")) mime = "application/json";

  Serial.printf("[SD ] 下载 %s (%u bytes)\n", path.c_str(), (unsigned)f.size());
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
  server.streamFile(f, mime);
  f.close();
}

void handleDelete() {
  if (!sdReady) { server.send(200, "application/json", "{\"ok\":false,\"msg\":\"SD 卡未就绪\"}"); return; }
  String path = normalizePath(server.hasArg("path") ? server.arg("path") : "");
  if (path == "/" || path.length() < 2) {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"不能删除根目录\"}");
    return;
  }
  if (recActive && path == String(recPath)) {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"该文件正在录制中\"}");
    return;
  }
  File f = SD_MMC.open(path);
  if (!f) {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"文件不存在\"}");
    return;
  }
  bool isDir = f.isDirectory();
  f.close();

  bool ok = isDir ? SD_MMC.rmdir(path) : SD_MMC.remove(path);
  if (!ok) {
    server.send(200, "application/json",
                isDir ? "{\"ok\":false,\"msg\":\"目录非空, 请先删里面内容\"}"
                      : "{\"ok\":false,\"msg\":\"删除失败\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
  Serial.printf("[SD ] 删除 %s\n", path.c_str());
}

void handleMkdir() {
  if (!sdReady) { server.send(200, "application/json", "{\"ok\":false,\"msg\":\"SD 卡未就绪\"}"); return; }
  String path = normalizePath(server.hasArg("path") ? server.arg("path") : "");
  if (!validName(baseName(path))) {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"名称不合法\"}");
    return;
  }
  if (SD_MMC.exists(path)) {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"已存在同名项\"}");
    return;
  }
  bool ok = SD_MMC.mkdir(path);
  server.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false,\"msg\":\"创建失败\"}");
}

void handleRename() {
  if (!sdReady) { server.send(200, "application/json", "{\"ok\":false,\"msg\":\"SD 卡未就绪\"}"); return; }
  String path = normalizePath(server.hasArg("path") ? server.arg("path") : "");
  String nn   = server.hasArg("name") ? server.arg("name") : "";
  if (path == "/" || path.length() < 2) {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"不能重命名根目录\"}");
    return;
  }
  if (!validName(nn)) {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"名称不合法\"}");
    return;
  }
  if (recActive && path == String(recPath)) {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"该文件正在录制中\"}");
    return;
  }
  String np = dirNameOf(path);
  if (np == "/") np = "";
  np += "/" + nn;
  if (SD_MMC.exists(np)) {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"目标名称已存在\"}");
    return;
  }
  bool ok = SD_MMC.rename(path, np);
  server.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false,\"msg\":\"重命名失败\"}");
}

bool wipeDir(const String &path, int depth) {
  if (depth > 6) return false;
  File dir = SD_MMC.open(path);
  if (!dir || !dir.isDirectory()) { if (dir) dir.close(); return false; }
  File e = dir.openNextFile();
  while (e) {
    String full = String(e.name());
    bool isDir = e.isDirectory();
    e.close();
    if (isDir) {
      wipeDir(full, depth + 1);
      SD_MMC.rmdir(full);
    } else {
      SD_MMC.remove(full);
    }
    e = dir.openNextFile();
  }
  dir.close();
  return true;
}

void handleWipe() {
  if (!sdReady) { server.send(200, "application/json", "{\"ok\":false,\"msg\":\"SD 卡未就绪\"}"); return; }
  if (recActive) { server.send(200, "application/json", "{\"ok\":false,\"msg\":\"正在录制, 请先停止\"}"); return; }
  if (server.arg("confirm") != "YES") {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"缺少确认参数\"}");
    return;
  }
  wipeDir("/", 0);
  server.send(200, "application/json", "{\"ok\":true}");
  Serial.println("[SD ] 已清空");
}

/* ---- 上传 ---- */
void handleUploadDone() {
  String msg = upOk ? "{\"ok\":true,\"size\":" + String((unsigned)upWritten) + "}"
                    : "{\"ok\":false,\"msg\":\"写入失败\"}";
  server.send(200, "application/json", msg);
}

void handleUploadChunk() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    upWritten = 0;
    upOk = false;
    if (!sdReady) return;
    String dir = server.hasArg("dir") ? normalizePath(server.arg("dir")) : String("/");
    if (dir == "/") dir = "";
    String nm = up.filename;
    if (!validName(nm)) return;
    String path = dir + "/" + nm;
    upFile = SD_MMC.open(path, FILE_WRITE);
    upOk = (bool)upFile;
    Serial.printf("[UP ] → %s\n", path.c_str());
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (upFile) upWritten += upFile.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (upFile) { upFile.close(); upOk = upOk && upWritten > 0; }
    Serial.printf("[UP ] 完成 %u bytes\n", (unsigned)upWritten);
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (upFile) upFile.close();
    upOk = false;
  }
}

/* ==================== 视频流 (端口 81) ==================== */
void pollStreamServer() {
  if (streamClient && !streamClient.connected()) {
    streamClient.stop();
    Serial.println("[STR] 客户端断开");
  }
  if (streamClient) return;

  WiFiClient c = streamServer.available();
  if (!c) return;

  c.setTimeout(500);
  String req;
  req.reserve(512);
  unsigned long t0 = millis();
  while (millis() - t0 < 400) {
    while (c.available()) {
      req += (char)c.read();
      if (req.length() > 1200) break;
    }
    if (req.indexOf("\r\n\r\n") >= 0) break;
    delay(2);
  }

  int sp1 = req.indexOf(' ');
  int sp2 = (sp1 >= 0) ? req.indexOf(' ', sp1 + 1) : -1;
  String path = (sp1 > 0 && sp2 > sp1) ? req.substring(sp1 + 1, sp2) : "";

  if (path.startsWith("/stream")) {
    c.setNoDelay(true);
    c.setTimeout(3000);
    c.print("HTTP/1.1 200 OK\r\n");
    c.print("Content-Type: "); c.print(STREAM_CT); c.print("\r\n");
    c.print("Cache-Control: no-cache, no-store, must-revalidate\r\n");
    c.print("Pragma: no-cache\r\n");
    c.print("Connection: close\r\n");
    c.print("Access-Control-Allow-Origin: *\r\n\r\n");
    streamClient = c;
    lastFrameMs = 0;
    Serial.printf("[STR] 新客户端 %s\n", c.remoteIP().toString().c_str());
  } else if (path.startsWith("/capture")) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      c.print("HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: ");
      c.print((unsigned)fb->len);
      c.print("\r\nConnection: close\r\n\r\n");
      c.write(fb->buf, fb->len);
      esp_camera_fb_return(fb);
    } else {
      c.print("HTTP/1.1 500 Internal Error\r\nContent-Length: 0\r\n\r\n");
    }
    delay(5);
    c.stop();
  } else {
    c.print("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    delay(5);
    c.stop();
  }
}

void sendOneFrame() {
  if (!streamClient) return;
  if (millis() - lastFrameMs < 8) return;

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  char head[80];
  int hl = snprintf(head, sizeof(head), STREAM_PART, (unsigned)fb->len);

  bool ok = true;
  if (streamClient.write((const uint8_t *)head, hl) != (size_t)hl) ok = false;
  if (ok && streamClient.write(fb->buf, fb->len) != fb->len)      ok = false;
  size_t bl = strlen(STREAM_BOUNDARY);
  if (ok && streamClient.write((const uint8_t *)STREAM_BOUNDARY, bl) != bl) ok = false;

  esp_camera_fb_return(fb);

  if (!ok) {
    streamClient.stop();
    Serial.println("[STR] 发送失败, 断开");
    return;
  }
  framesSent++;
  lastFrameMs = millis();
}

/* ==================== WiFi 维护 ==================== */
void maintainWiFi() {
  if (millis() - lastWifiCheck < 10000) return;
  lastWifiCheck = millis();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] STA 断开, 尝试重连...");
    WiFi.reconnect();
  }
}

/* ==================== setup ==================== */
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  delay(200);
  Serial.println();
  Serial.println("==================== ESP32-CAM 图传+录屏 ====================");

  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  if (!initCamera())
    Serial.println("[ERR] 摄像头初始化失败, 请检查排线/供电 (建议 5V 2A)");

  sdReady = initSD();

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);

  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("[AP  ] 热点 %s 密码 %s\n", AP_SSID, AP_PASS);
  Serial.printf("[AP  ] http://%s\n", WiFi.softAPIP().toString().c_str());

  if (String(STA_PASS) != "YOUR_WIFI_PASSWORD") {
    WiFi.begin(STA_SSID, STA_PASS);
    Serial.printf("[STA ] 连接 %s ", STA_SSID);
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
      delay(400); Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[STA ] http://%s  信号 %d dBm\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
      Serial.println("[STA ] 失败, 仅 AP 模式可用 (检查 SSID/密码)");
    }
  } else {
    Serial.println("[STA ] 未填写密码, 跳过 STA, 仅 AP 模式");
  }

  Serial.printf("[SYS ] PSRAM:%s  内存:%d KB\n", hasPSRAM ? "有" : "无", ESP.getFreeHeap() / 1024);

  server.on("/",              handleRoot);
  server.on("/status",        handleStatus);
  server.on("/control",       handleControl);
  server.on("/capture",       handleCapture);
  server.on("/save",          handleSave);
  server.on("/api/rec/start", handleRecStart);
  server.on("/api/rec/stop",  handleRecStop);
  server.on("/api/list",      handleList);
  server.on("/download",      handleDownload);
  server.on("/api/delete",    handleDelete);
  server.on("/api/mkdir",     handleMkdir);
  server.on("/api/rename",    handleRename);
  server.on("/api/wipe",      handleWipe);
  server.on("/api/upload", HTTP_POST, handleUploadDone, handleUploadChunk);
  server.on("/favicon.ico", []() { server.send(204, "text/plain", ""); });
  server.onNotFound([]() { server.send(404, "text/plain", "Not Found"); });
  server.begin();

  streamServer.begin();
  streamServer.setNoDelay(true);

  Serial.println("--------------------------------------------------");
  Serial.printf("浏览器打开: http://%s\n", WiFi.softAPIP().toString().c_str());
  Serial.println("==================================================");
}

/* ==================== loop ==================== */
void loop() {
  server.handleClient();   // 网页 / 控制 / 文件管理
  pollStreamServer();      // 接受新流客户端
  recTick();               // 录屏: 到点写一帧
  sendOneFrame();          // 推流: 每轮最多一帧
  maintainWiFi();

  if (!streamClient && !recActive) delay(2);
  else delay(1);   // 始终让出 CPU, 防止空闲任务饿死触发看门狗
}
