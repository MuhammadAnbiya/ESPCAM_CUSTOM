/*********
 * Proyek ESP32-CAM Deteksi Wajah dan Simpan ke SD Card (VERSI KUALITAS VGA)
 * --------------------------------------------------------------------------
 * Perbaikan:
 * - Resolusi ditingkatkan ke VGA (640x480) untuk kualitas gambar yang lebih baik.
 * - Lampu flash LED dinonaktifkan secara permanen.
 * - Tetap stabil dengan tidak mengubah resolusi saat menyimpan foto.
 *********************************************************************************/

// Pustaka yang dibutuhkan
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "fb_gfx.h"
#include "soc/soc.h"          // Untuk mengatasi brownout detector
#include "soc/rtc_cntl_reg.h" // Untuk mengatasi brownout detector
#include "driver/ledc.h"
#include <WiFi.h>
#include "SD_MMC.h"
#include "FS.h"
#include <vector>
#include <list>

// Pustaka untuk deteksi wajah
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"

// --- KONFIGURASI ---
const char* ssid = "anbi";
const char* password = "88888888";

// Pengaturan Kamera & Kualitas
#define STREAM_FRAME_SIZE FRAMESIZE_VGA     // Resolusi untuk streaming & foto (640x480)
#define STREAM_JPEG_QUALITY 15              // Kualitas JPEG (10-63, lebih rendah lebih baik/lancar)
#define SAVE_COOLDOWN_SECONDS 5             // Jeda antar penyimpanan foto (dalam detik)

// Definisi warna untuk kotak deteksi
#define FACE_COLOR_GREEN 0x0000FF00

// --- Pinout untuk model AI-THINKER ---
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define FLASH_GPIO_NUM     4  // Pin untuk lampu flash LED

// Variabel global
static int face_detect_enabled = 1; 
static int save_to_sd_enabled = 1;  
static int photo_count = 0;         
static unsigned long last_save_time = 0;

// Inisialisasi model deteksi wajah
static HumanFaceDetectMSR01 s1(0.1F, 0.5F, 10, 0.2F);
static HumanFaceDetectMNP01 s2(0.5F, 0.3F, 5);

// Fungsi untuk menggambar kotak di sekitar wajah
static void draw_face_boxes(fb_data_t *fb, std::list<dl::detect::result_t> *results) {
    int x, y, w, h;
    uint32_t color = FACE_COLOR_GREEN;
    if (fb->bytes_per_pixel == 2) {
        color = ((color >> 16) & 0x001F) | ((color >> 3) & 0x07E0) | ((color << 8) & 0xF800);
    }
    for (auto const& prediction : *results) {
        x = (int)prediction.box[0];
        y = (int)prediction.box[1];
        w = (int)prediction.box[2] - x + 1;
        h = (int)prediction.box[3] - y + 1;
        fb_gfx_drawFastHLine(fb, x, y, w, color);
        fb_gfx_drawFastHLine(fb, x, y + h - 1, w, color);
        fb_gfx_drawFastVLine(fb, x, y, h, color);
        fb_gfx_drawFastVLine(fb, x + w - 1, y, h, color);
    }
}

// Fungsi untuk menyimpan frame yang sedang di-stream ke SD Card
void saveFrameToSD(camera_fb_t *fb) {
  if (!fb) {
    Serial.println("Frame kosong, penyimpanan dibatalkan.");
    return;
  }

  uint8_t *_jpg_buf = NULL;
  size_t _jpg_buf_len = 0;
  bool jpeg_converted = fmt2jpg(fb->buf, fb->len, fb->width, fb->height, PIXFORMAT_RGB565, STREAM_JPEG_QUALITY, &_jpg_buf, &_jpg_buf_len);
  
  if (!jpeg_converted) {
    Serial.println("Konversi ke JPEG untuk penyimpanan gagal.");
    return;
  }

  String path = "/face_capture_" + String(photo_count) + ".jpg";
  fs::FS &fs = SD_MMC;
  File file = fs.open(path.c_str(), FILE_WRITE);
  if (!file) {
    Serial.printf("Gagal membuka file %s\n", path.c_str());
  } else {
    file.write(_jpg_buf, _jpg_buf_len);
    Serial.printf("Foto disimpan: %s (%d bytes)\n", path.c_str(), _jpg_buf_len);
    photo_count++;
  }
  file.close();

  free(_jpg_buf);
}

// Handler untuk video stream
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char *part_buf[128];

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=123456789000000000000987654321");
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Gagal mengambil frame");
      res = ESP_FAIL;
    } else {
      if (face_detect_enabled && fb->format == PIXFORMAT_RGB565) {
          std::list<dl::detect::result_t> &candidates = s1.infer((uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3});
          std::list<dl::detect::result_t> &results = s2.infer((uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3}, candidates);
          
          if (!results.empty()) {
            fb_data_t rfb;
            rfb.width = fb->width;
            rfb.height = fb->height;
            rfb.data = fb->buf;
            rfb.bytes_per_pixel = 2;
            rfb.format = FB_RGB565;
            
            draw_face_boxes(&rfb, &results);

            if (save_to_sd_enabled && (millis() - last_save_time > SAVE_COOLDOWN_SECONDS * 1000)) {
               last_save_time = millis();
               saveFrameToSD(fb);
            }
          }
      }
      
      if (!fmt2jpg(fb->buf, fb->len, fb->width, fb->height, PIXFORMAT_RGB565, STREAM_JPEG_QUALITY, &_jpg_buf, &_jpg_buf_len)) {
        Serial.println("Konversi JPEG untuk stream gagal");
        res = ESP_FAIL;
      }
      esp_camera_fb_return(fb);
      fb = NULL;
    }

    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)"\r\n--123456789000000000000987654321\r\n", 38);
      if (res == ESP_OK) {
        size_t hlen = snprintf((char *)part_buf, 128, "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", _jpg_buf_len);
        res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
      }
      if (res == ESP_OK) {
        res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
      }
    }

    if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }

    if (res != ESP_OK) break;
  }
  return res;
}

// Handler untuk halaman utama
static esp_err_t index_handler(httpd_req_t *req){
  httpd_resp_set_type(req, "text/html");
  String ip = WiFi.localIP().toString();
  String html = R"rawliteral(
    <html>
    <head>
      <title>ESP32-CAM Stabil</title>
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <style>
        body { font-family: Arial, sans-serif; text-align: center; margin: 20px; background-color: #f4f4f4; }
        h1 { color: #333; }
        #stream-container { border: 2px solid #ccc; display: inline-block; box-shadow: 0 4px 8px rgba(0,0,0,0.1); max-width: 95%;}
        img { max-width: 100%; height: auto; display: block; }
        p { color: #555; }
        a { color: #007bff; text-decoration: none; font-weight: bold; }
      </style>
    </head>
    <body>
      <h1>ESP32-CAM Face Detection (Stabil)</h1>
      <div id="stream-container">
        <img src="/stream">
      </div>
      <p>Saat wajah terdeteksi, foto akan disimpan ke SD Card.</p>
      <p>Kualitas foto: 640x480 (sama dengan stream)</p>
    </body>
    </html>
  )rawliteral";
  return httpd_resp_send(req, html.c_str(), html.length());
}

// Fungsi untuk memulai server web
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  httpd_handle_t server = NULL;
  httpd_uri_t index_uri = { "/", HTTP_GET, index_handler, NULL };
  httpd_uri_t stream_uri = { "/stream", HTTP_GET, stream_handler, NULL };
  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &stream_uri);
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("\n--- ESP32-CAM Face Detect & Save (VGA) ---");

  // BARU: Nonaktifkan lampu flash LED secara permanen
  pinMode(FLASH_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_GPIO_NUM, LOW);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM; config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM; config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM; config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM; config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM; config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = STREAM_FRAME_SIZE; // Menggunakan resolusi VGA
  
  config.jpeg_quality = STREAM_JPEG_QUALITY;
  config.fb_count = 2;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Inisialisasi kamera gagal dengan error 0x%x", err);
    ESP.restart();
  }
  Serial.println("Inisialisasi kamera berhasil");

  if (!SD_MMC.begin()) {
    Serial.println("Gagal me-mount SD Card. Fitur simpan foto dinonaktifkan.");
    save_to_sd_enabled = 0;
  } else {
    Serial.println("SD Card berhasil diinisialisasi.");
  }

  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi terhubung!");
  Serial.print("Alamat IP: ");
  Serial.println(WiFi.localIP());

  startCameraServer();
  Serial.println("Server kamera dimulai.");
  Serial.printf("Buka browser dan akses: http://%s\n", WiFi.localIP().toString().c_str());
}

void loop() {
  delay(10000);
}