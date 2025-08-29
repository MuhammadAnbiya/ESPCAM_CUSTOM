#include "FS.h"
#include "SD_MMC.h"
#include "esp_camera.h"

// ===============================================================
// PENGATURAN
// ===============================================================
// Pengaturan Timer untuk Simpan ke SD Card (dalam milidetik)
const int captureIntervalSeconds = 5; // Ambil gambar setiap 5 detik

// Model Kamera (AI-THINKER)
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
// ===============================================================

// Variabel untuk menghitung nomor file gambar
int pictureCounter = 0;

void setup() {
    Serial.begin(115200);
    Serial.println("\nMemulai ESP32-CAM Mode Time-Lapse...");

    // Konfigurasi Kamera
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG; // Format kompresi bawaan, efisien
    
    // Atur resolusi dan kualitas ke TERBAIK
    if (psramFound()) {
        config.frame_size = FRAMESIZE_UXGA;  // Resolusi tertinggi (1600x1200)
        config.jpeg_quality = 10;           // Kualitas terbaik (angka lebih rendah = kualitas lebih baik)
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_SVGA;  // Resolusi maksimal tanpa PSRAM
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    // Inisialisasi kamera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Inisialisasi kamera gagal dengan error 0x%x", err);
        ESP.restart();
    }
    Serial.println("Kamera berhasil diinisialisasi.");

    // Inisialisasi SD Card
    if(!SD_MMC.begin()){
        Serial.println("Inisialisasi SD Card Gagal! Periksa kartu dan coba lagi.");
        return; // Hentikan program jika SD card gagal
    }
    
    uint8_t cardType = SD_MMC.cardType();
    if(cardType == CARD_NONE){
        Serial.println("Tidak ada SD Card terpasang.");
        return;
    }
    
    Serial.println("SD Card berhasil diinisialisasi. Siap mengambil gambar.");
}

void loop() {
    Serial.printf("Mengambil gambar ke-%d...\n", pictureCounter);
    
    // Ambil frame dari kamera
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Gagal mengambil frame dari kamera.");
        delay(1000); // Tunggu sejenak sebelum mencoba lagi
        return;
    }

    // Buat nama file yang unik
    String path = "/timelapse_" + String(pictureCounter) + ".jpg";

    // Buka file di SD card untuk ditulis
    File file = SD_MMC.open(path.c_str(), FILE_WRITE);
    if (!file) {
        Serial.println("Gagal membuat file di SD Card.");
    } else {
        // Tulis buffer gambar ke file
        file.write(fb->buf, fb->len);
        Serial.printf("Gambar berhasil disimpan ke: %s\n", path.c_str());
        pictureCounter++;
    }
    file.close();

    // Kembalikan frame buffer (SANGAT PENTING!)
    esp_camera_fb_return(fb);

    // Tunggu sesuai interval yang ditentukan
    Serial.printf("Menunggu %d detik untuk pengambilan berikutnya...\n\n", captureIntervalSeconds);
    delay(captureIntervalSeconds * 1000);
}