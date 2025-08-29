#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"

// ===============================================================
// PENGATURAN - SILAKAN UBAH BAGIAN INI
// ===============================================================

// Ganti dengan kredensial WiFi Anda
const char* ssid = "anbi";
const char* password = "88888888";

// Model Kamera yang digunakan (AI-THINKER)
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
// AKHIR DARI PENGATURAN
// ===============================================================

WebServer server(80);

const char* HTML_PAGE = R"=====(
<!DOCTYPE html>
<html lang="id">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>DASHBOARD AI TAKANA JUO</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background-color: #2c3e50;
            color: #ecf0f1;
            margin: 0;
            padding: 0;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            min-height: 100vh;
        }
        .container {
            text-align: center;
            padding: 20px;
            border-radius: 15px;
            background-color: #34495e;
            box-shadow: 0 10px 20px rgba(0,0,0,0.3);
        }
        h1 {
            color: #3498db;
            margin-bottom: 20px;
            font-size: 2em;
        }
        #stream-container {
            border: 5px solid #3498db;
            border-radius: 10px;
            overflow: hidden;
            min-width: 320px;
            max-width: 640px;
            width: 100%;
            margin: 0 auto 20px auto;
            background-color: #000;
        }
        #stream {
            width: 100%;
            height: auto;
            display: block;
        }
        .btn-container {
            display: flex;
            justify-content: center;
            gap: 15px;
        }
        button {
            background-color: #3498db;
            color: white;
            border: none;
            padding: 12px 25px;
            font-size: 16px;
            border-radius: 5px;
            cursor: pointer;
            transition: background-color 0.3s, transform 0.2s;
        }
        button:hover {
            background-color: #2980b9;
            transform: scale(1.05);
        }
        .footer {
            margin-top: 30px;
            font-size: 0.8em;
            color: #95a5a6;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>DASHBOARD AI TAKANA JUO</h1>
        <div id="stream-container">
            <img id="stream" src="/stream" alt="Memuat Stream Kamera...">
        </div>
        <div class="btn-container">
            <button onclick="captureImage()">Ambil Gambar</button>
        </div>
        <p class="footer">Dibuat dengan ESP32-CAM</p>
    </div>

    <script>
        function captureImage() {
            window.open('/capture', '_blank');
        }
        
        const streamImg = document.getElementById('stream');
        streamImg.onerror = function() {
            console.log("Stream error, attempting to reconnect in 2 seconds...");
            setTimeout(() => {
                // Tambahkan timestamp untuk mencegah browser menggunakan cache
                streamImg.src = "/stream?" + new Date().getTime();
            }, 2000);
        };
    </script>
</body>
</html>
)=====";

void handleRoot() {
    server.send(200, "text/html", HTML_PAGE);
}

// ===============================================================
// FUNGSI INI TELAH DIPERBAIKI
// ===============================================================
void handleStream() {
    WiFiClient client = server.client();
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=--frame\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "\r\n";
    client.print(response);

    while (true) {
        if (!client.connected()) {
            break; // Keluar dari loop jika client terputus
        }

        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Gagal mengambil frame dari kamera");
            continue; // Coba lagi
        }

        client.print("--frame\r\n");
        client.print("Content-Type: image/jpeg\r\n");
        client.print("Content-Length: ");
        client.print(fb->len);
        client.print("\r\n\r\n");
        
        client.write(fb->buf, fb->len);
        
        client.print("\r\n");
        
        esp_camera_fb_return(fb);
    }
    Serial.println("Streaming dihentikan, client terputus.");
}
// ===============================================================

void handleCapture() {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Gagal mengambil frame untuk foto");
        server.send(500, "text/plain", "Gagal mengambil gambar");
        return;
    }
    
    server.sendHeader("Content-Type", "image/jpeg");
    server.sendHeader("Content-Disposition", "inline; filename=capture.jpg");
    server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
    
    esp_camera_fb_return(fb);
}


void setup() {
    Serial.begin(115200);
    Serial.println("\nMemulai ESP32-CAM...");

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
    config.pixel_format = PIXFORMAT_JPEG;

    if (psramFound()) {
        config.frame_size = FRAMESIZE_UXGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Inisialisasi kamera gagal dengan error 0x%x", err);
        ESP.restart();
    }
    
    Serial.println("Kamera berhasil diinisialisasi.");

    sensor_t * s = esp_camera_sensor_get();
    // Resolusi VGA (640x480) adalah resolusi yang baik untuk streaming
    s->set_framesize(s, FRAMESIZE_VGA); 
    s->set_vflip(s, 1);
    s->set_hmirror(s, 0);

    WiFi.begin(ssid, password);
    Serial.print("Menghubungkan ke WiFi ");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi terhubung!");

    Serial.print("Dashboard dapat diakses di: http://");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, handleRoot);
    server.on("/stream", HTTP_GET, handleStream);
    server.on("/capture", HTTP_GET, handleCapture);

    server.begin();
    Serial.println("Web server dimulai.");
}

void loop() {
    server.handleClient();
}