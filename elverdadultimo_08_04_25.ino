#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"         
#include "soc/soc.h"            
#include "soc/rtc_cntl_reg.h"   
#include <PubSubClient.h>
#include <HTTPClient.h>

// Configuración WiFi original como respaldo
const char* ssid = "Megacable_2.4G_454A";
const char* password = "pizzadepeperoni123";

// Nueva configuración WiFi principal
const char* ssid_iphone = "iPhone de Jesús Manuel";
const char* password_iphone = "12346678";

// Configuración MQTT
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
const char* mqtt_base_topic = "esp32cam2";
String deviceId;

// Cliente MQTT
WiFiClient mqttWiFiClient;
PubSubClient mqttClient(mqttWiFiClient);

// Configuración de la cámara
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

// Tamaño de chunks para MQTT
const size_t CHUNK_SIZE = 4096;

// Configuración del buzzer
const int BUZZER_PIN = 13;  // Cambia este pin según tu conexión

// URL del endpoint
const char* detectionEndpoint = "https://willingly-worthy-pig.ngrok-free.app/detect_object";

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  Serial.setDebugOutput(true);  
  Serial.println();

  // Configuración de la cámara
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
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if(psramFound()){
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_LATEST;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  // Configuración WiFi
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(true);
  delay(1000);

  // Configurar DHCP para ambas redes
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  delay(100);

  // Intentar conectar al iPhone primero
  Serial.println("Intentando conectar a red iPhone...");
  WiFi.begin(ssid_iphone, password_iphone);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    // Si falla iPhone, intentar Megacable
    WiFi.disconnect(true);
    delay(1000);
    
    // Limpiar configuración WiFi anterior
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    delay(100);
    
    WiFi.begin(ssid, password);
    Serial.println("Conectando a Megacable...");
    
    attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {    
    Serial.println("\nConexión exitosa!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Máscara de subred: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("DNS: ");
    Serial.println(WiFi.dnsIP());
    
    // Generar ID único usando MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    deviceId = String(mac[0], HEX) + String(mac[1], HEX) + String(mac[2], HEX) +
               String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
    
    // Configurar MQTT
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setKeepAlive(60);
    
    // Intentar conexión MQTT
    int mqttAttempts = 0;
    while (!mqttClient.connected() && mqttAttempts < 5) {
      Serial.println("Intentando conexión MQTT...");
      
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Conexión WiFi perdida, reconectando...");
        WiFi.reconnect();
        delay(1000);
      }
      
      String clientId = "ESP32CAM-" + deviceId + "-" + String(random(0xffff), HEX);
      if (mqttClient.connect(clientId.c_str())) {
        Serial.println("Conectado a MQTT exitosamente");
        String accessUrl = "http://" + WiFi.localIP().toString();
        String mqttMessage = "{\"deviceId\":\"" + deviceId + "\",\"url\":\"" + accessUrl + "\"}";
        mqttClient.publish((mqtt_base_topic + deviceId + "/info").c_str(), mqttMessage.c_str());
        break;
      } else {
        Serial.print("Fallo MQTT, rc=");
        Serial.println(mqttClient.state());
        delay(2000);
        mqttAttempts++;
      }
    }
  } else {
    Serial.println("No se pudo conectar a ninguna red");
    ESP.restart();
  }

  // Configurar pin del buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void connectMQTT() {
    int retries = 0;
    while (!mqttClient.connected() && retries < 3) {
        Serial.println("Reconectando a MQTT...");
        
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi desconectado, reconectando...");
            WiFi.reconnect();
            delay(1000);
        }
        
        String clientId = "ESP32CAM-" + deviceId + "-" + String(random(0xffff), HEX);
        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("Reconectado a MQTT");
            mqttClient.subscribe((mqtt_base_topic + deviceId + "/cmd").c_str());
            return;
        } else {
            Serial.print("Fallo reconexión MQTT, rc=");
            Serial.println(mqttClient.state());
            retries++;
            delay(2000);
        }
    }
}

void publishFrame(camera_fb_t * fb) {
    if (mqttClient.connected() && fb) {
        size_t fbLen = fb->len;
        uint8_t *fbBuf = fb->buf;
        int chunks = (fbLen + CHUNK_SIZE - 1) / CHUNK_SIZE;
        
        // Publicar metadata
        String metadata = "{\"deviceId\":\"" + deviceId + "\",\"size\":" + String(fbLen) + 
                         ",\"chunks\":" + String(chunks) + "}";
        String metadataTopic = mqtt_base_topic + deviceId + "/stream/metadata";
        mqttClient.publish(metadataTopic.c_str(), metadata.c_str(), true);
        
        // Publicar chunks de video
        for (int i = 0; i < chunks; i++) {
            size_t chunkLen = min(CHUNK_SIZE, fbLen - (i * CHUNK_SIZE));
            String frameTopic = mqtt_base_topic + deviceId + "/stream/frame/" + String(i);
            mqttClient.beginPublish(frameTopic.c_str(), chunkLen, false);
            mqttClient.write(fbBuf + (i * CHUNK_SIZE), chunkLen);
            mqttClient.endPublish();
        }
    }
}

void checkObjectDetection() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Configurar cliente HTTPS
    WiFiClientSecure *client = new WiFiClientSecure;
    if(client) {
      client->setInsecure(); // Ignorar certificado SSL
      
      http.begin(*client, detectionEndpoint);
      
      int httpCode = http.GET();
      
      if (httpCode > 0) {
        String payload = http.getString();
        
        // Activar buzzer si se detecta persona o reloj
        if (payload.indexOf("person") != -1 || payload.indexOf("clock") != -1) {
          digitalWrite(BUZZER_PIN, HIGH);
          delay(60000); // Mantener el buzzer activo por 60 segundos
          digitalWrite(BUZZER_PIN, LOW);
        }
      }
      
      http.end();
      delete client;
    }
  }
}

void loop() {
    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.loop();

    static unsigned long lastFrame = 0;
    static unsigned long lastDetection = 0;
    
    // Verificar detección cada 5 segundos
    if (millis() - lastDetection > 5000) {
        checkObjectDetection();
        lastDetection = millis();
    }

    if (millis() - lastFrame > 33) { // Publicar cada ~33ms (30 FPS)
        camera_fb_t * fb = esp_camera_fb_get();
        if (fb) {
            publishFrame(fb);
            esp_camera_fb_return(fb);
            lastFrame = millis();
        }
    }
}


