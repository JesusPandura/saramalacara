/**********************************************************************
  Filename    : Video Web Server with MQTT
  Description : The camera images captured by the ESP32S3 are sent via MQTT.
  Auther      : www.freenove.com
  Modification: 2024/03/21
**********************************************************************/
#include "esp_camera.h"
#include <WiFi.h>
#include <PubSubClient.h>

// Select camera model
#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM

#include "camera_pins.h"

const char* ssid     = "Megacable_2.4G_454A";
const char* password = "pizzadepeperoni123";

// Configuración IP estática
IPAddress local_IP(192, 168, 0, 100);  // Cambia esto según tu red
IPAddress gateway(192, 168, 0, 1);     // Cambia esto según tu router
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);      // DNS de Google
IPAddress secondaryDNS(8, 8, 4, 4);

// MQTT Configuration
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_topic = "esp32cam2/stream";
const char* mqtt_client_id = "ESP32CAM2_Client";

WiFiClient espClient;
PubSubClient client(espClient);

// Doble buffer para mejorar rendimiento
camera_fb_t *fb_buffer[2] = {NULL, NULL};
bool buffer_ready[2] = {false, false};
int current_buffer = 0;

void cameraInit(void);
void reconnectMQTT();
void sendFrame();
void connectToWiFi();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // Configurar WiFi con IP estática
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }

  // Configurar WiFi con parámetros optimizados
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  
  // Conectar a WiFi
  connectToWiFi();

  cameraInit();

  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(8192);
  
  reconnectMQTT();

  Serial.print("Camera Ready! IP: ");
  Serial.println(WiFi.localIP());
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("WiFi connection failed");
    ESP.restart();
  }
}

void loop() {
  // Verificar conexión WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost, reconnecting...");
    connectToWiFi();
  }
  
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
  
  sendFrame();
  delay(50); // 20 FPS objetivo
}

void cameraInit(void) {
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 8;
  config.fb_count = 2;

  // Inicialización específica para ESP32-S3
  if(psramFound()){
    config.jpeg_quality = 8;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  } else {
    config.frame_size = FRAMESIZE_VGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.jpeg_quality = 8;
    config.fb_count = 1;
  }

  // Inicializar cámara
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Configurar sensor
  sensor_t * s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 0);     // -2 to 2
    s->set_contrast(s, 0);       // -2 to 2
    s->set_saturation(s, 0);     // -2 to 2
    s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
    s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
    s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
    s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
    s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
    s->set_aec2(s, 0);          // 0 = disable , 1 = enable
    s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
    s->set_agc_gain(s, 0);       // 0 to 30
    s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
    s->set_bpc(s, 0);           // 0 = disable , 1 = enable
    s->set_wpc(s, 1);           // 0 = disable , 1 = enable
    s->set_raw_gma(s, 1);       // 0 = disable , 1 = enable
    s->set_lenc(s, 1);          // 0 = disable , 1 = enable
    s->set_hmirror(s, 0);       // 0 = disable , 1 = enable
    s->set_vflip(s, 0);         // 0 = disable , 1 = enable
    s->set_dcw(s, 1);           // 0 = disable , 1 = enable
    s->set_colorbar(s, 0);      // 0 = disable , 1 = enable
  }
  
  Serial.println("Camera configuration complete");
}

void reconnectMQTT() {
  int attempts = 0;
  while (!client.connected() && attempts < 3) {
    Serial.print("Attempting MQTT connection...");
    String clientId = String(mqtt_client_id) + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      break;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying...");
      attempts++;
      delay(1000);
    }
  }
}

void sendFrame() {
  if (!client.connected()) return;

  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    esp_camera_deinit(); // Reiniciar cámara si falla
    delay(1000);
    cameraInit();        // Reinicializar cámara
    return;
  }

  if (fb->len > 0) {
    if (client.publish(mqtt_topic, fb->buf, fb->len, false)) {
      //Serial.println("Frame sent");
    } else {
      Serial.println("Failed to send frame");
    }
  }

  esp_camera_fb_return(fb);
}

