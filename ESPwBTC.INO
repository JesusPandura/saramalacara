#include "esp_camera.h"
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "esp_heap_caps.h"

// Selecciona el modelo de cámara
#define CAMERA_MODEL_ESP32S3_EYE

#include "camera_pins.h"

// WiFi
const char* ssid = "Megacable_2.4G_454A";
const char* password = "pizzadepeperoni123";             // Tu contraseña WiFi

// Servidor de señalización - CAMBIA ESTA IP POR LA QUE MUESTRA EL SERVIDOR PYTHON
const char* serverIP = "clever-koi-freely.ngrok-free.app"; // <-- CAMBIA ESTA IP
const int serverPort = 443; // Cambiado a 443 para HTTPS

// WebSocket client
WebSocketsClient webSocket;

// Variables para WebRTC
camera_fb_t *fb = NULL;
bool clientConnected = false;
unsigned long lastFrameTime = 0;
const unsigned long frameInterval = 100; // Reducido a ~10 FPS para mejor compatibilidad

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  Serial.println();

  // Verificar PSRAM
  if(!psramInit()) {
    Serial.println("PSRAM no inicializado correctamente!");
    Serial.println("Verifica que el ESP32-S3 tenga PSRAM habilitado en el menú de configuración de Arduino IDE");
    Serial.println("Tools -> PSRAM -> OPI PSRAM");
    while(1) {
      delay(1000);
    }
  }
  
  Serial.printf("PSRAM total: %d bytes\n", ESP.getPsramSize());
  Serial.printf("PSRAM libre: %d bytes\n", ESP.getFreePsram());

  // Inicializar cámara primero
  if (!cameraInit()) {
    Serial.println("Fallo al inicializar cámara. Deteniendo...");
    while (1) {
      delay(1000);
    }
  }

  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.print("Dirección IP del ESP32: ");
  Serial.println(WiFi.localIP());
  
  Serial.print("Conectando al servidor de señalización en: ");
  Serial.println(serverIP);

  // Conectar al servidor de señalización
  webSocket.beginSSL(serverIP, serverPort, "/ws/esp32");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  
  Serial.println("Iniciando conexión WebSocket...");
}

// --- LOOP ---
void loop() {
  webSocket.loop();
  
  if (clientConnected) {
    unsigned long currentTime = millis();
    if (currentTime - lastFrameTime >= frameInterval) {
      sendVideoFrame();
      lastFrameTime = currentTime;
    }
  }
}

// --- WebSocket Event Handler ---
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("Desconectado del servidor de señalización");
      clientConnected = false;
      break;
      
    case WStype_CONNECTED:
      Serial.println("Conectado al servidor de señalización");
      break;
      
    case WStype_TEXT: {
      String message = String((char*)payload);
      Serial.println("Mensaje recibido: " + message);
      
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, message);
      
      if (error) {
        Serial.print("Error parseando JSON: ");
        Serial.println(error.c_str());
        return;
      }
      
      const char* type = doc["type"];
      Serial.println("Tipo de mensaje: " + String(type));
      
      if (strcmp(type, "offer") == 0) {
        Serial.println("Procesando oferta WebRTC");
        handleOffer(doc);
      }
      break;
    }
  }
}

void handleOffer(DynamicJsonDocument& doc) {
  clientConnected = true;
  
  // Crear respuesta
  DynamicJsonDocument answer(2048); // Aumentado para evitar desbordamiento
  answer["type"] = "answer";
  answer["target"] = doc["target"].as<const char*>();
  
  // Configuración SDP simplificada para JPEG (no H264)
  String sdpAnswer = "v=0\r\n"
                   "o=- " + String(random(1000000)) + " 2 IN IP4 127.0.0.1\r\n"
                   "s=ESP32 Camera WebRTC\r\n"
                   "t=0 0\r\n"
                   "a=group:BUNDLE video\r\n"
                   "a=msid-semantic: WMS ESP32_CAMERA\r\n"
                   "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
                   "c=IN IP4 0.0.0.0\r\n"
                   "a=rtcp:9 IN IP4 0.0.0.0\r\n"
                   "a=ice-ufrag:" + String(random(100000)) + "\r\n"
                   "a=ice-pwd:" + String(random(100000)) + "\r\n"
                   "a=ice-options:trickle\r\n"
                   "a=fingerprint:sha-256 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF\r\n"
                   "a=setup:active\r\n"
                   "a=mid:video\r\n"
                   "a=extmap:1 urn:ietf:params:rtp-hdrext:toffset\r\n"
                   "a=sendonly\r\n"
                   "a=rtcp-mux\r\n"
                   "a=rtpmap:96 H264/90000\r\n"
                   "a=rtcp-fb:96 nack\r\n"
                   "a=rtcp-fb:96 nack pli\r\n"
                   "a=rtcp-fb:96 ccm fir\r\n"
                   "a=fmtp:96 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\r\n"
                   "a=msid:ESP32_CAMERA ESP32_CAMERA_TRACK\r\n"
                   "a=ssrc:" + String(random(100000)) + " cname:ESP32_CAMERA\r\n";

  JsonObject answerSdp = answer.createNestedObject("sdp");
  answerSdp["type"] = "answer";
  answerSdp["sdp"] = sdpAnswer;

  String answerStr;
  serializeJson(answer, answerStr);
  Serial.println("Enviando respuesta SDP");
  webSocket.sendTXT(answerStr);
}

void sendVideoFrame() {
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Error capturando frame");
    return;
  }

  if (fb->len > 0) {
    // Enviamos el frame como un mensaje binario a través de WebSocket
    Serial.printf("Enviando frame: %d bytes\n", fb->len);
    
    // Verificar que los datos del buffer son válidos
    if (fb->buf != NULL) {
      webSocket.sendBIN(fb->buf, fb->len);
      Serial.println("Frame enviado correctamente");
    } else {
      Serial.println("Error: buffer de frame nulo");
    }
  } else {
    Serial.println("Error: frame con longitud 0");
  }
  
  // Siempre devolver el buffer a la cámara
  esp_camera_fb_return(fb);
}

// --- INICIALIZA CÁMARA ---
bool cameraInit() {
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
  
  // Configuración simplificada para JPEG
  config.xclk_freq_hz = 20000000;    // 20MHz XCLK 
  config.frame_size = FRAMESIZE_QVGA; // 320x240
  config.pixel_format = PIXFORMAT_JPEG; // Usamos JPEG
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 10; // Mejor calidad de JPEG (menos compresión)
  config.fb_count = 1;

  // Verificar memoria PSRAM disponible
  size_t fb_len = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  Serial.printf("Bloque más grande de PSRAM disponible: %d bytes\n", fb_len);
  
  if (fb_len < 1024 * 1024) { // Necesitamos al menos 1MB de PSRAM libre
    Serial.println("Error: No hay suficiente PSRAM disponible para la cámara");
    return false;
  }

  // Intentar inicializar la cámara
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Error al iniciar cámara: 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    // Configuración básica del sensor
    s->set_framesize(s, FRAMESIZE_QVGA);
    s->set_quality(s, 10); 
    s->set_brightness(s, 1);
    s->set_saturation(s, 0);
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1); // Puede ayudar con la orientación
  }
  
  return true;
}
