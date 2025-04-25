#include "esp_camera.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Selecciona el modelo de cámara
#define CAMERA_MODEL_ESP32S3_EYE

#include "camera_pins.h"

// WiFi
const char* ssid = "Megacable_2.4G_454A";
const char* password = "pizzadepeperoni123";   // Tu contraseña WiFi

// Configuración MQTT
const char* mqtt_server = "test.mosquitto.org";  // Broker MQTT público gratuito
const int mqtt_port = 1883;                     // Puerto MQTT estándar
const char* mqtt_topic = "esp32camera/sara";    // Topic personalizado para evitar conflictos
const char* mqtt_client_id = "ESP32CameraSara";  // ID de cliente único

// Clientes WiFi y MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// Variables para la cámara y buffers
camera_fb_t *fb = NULL;
camera_fb_t *fb_queue[2] = {NULL, NULL}; // Sistema de doble buffer
SemaphoreHandle_t xMutex = NULL;
TaskHandle_t captureTaskHandle = NULL;
TaskHandle_t publishTaskHandle = NULL;

// Control de FPS
unsigned long lastFrameTime = 0;
const unsigned long frameInterval = 100; // 10 FPS (era 300 = ~3 FPS)

// Estadísticas
unsigned long frameCount = 0;
unsigned long lastFpsCalcTime = 0;
float currentFps = 0;

// --- Tarea para capturar frames ---
void captureTask(void *parameter) {
  while (true) {
    // Obtener frame
    camera_fb_t *new_fb = esp_camera_fb_get();
    
    if (!new_fb) {
      Serial.println("Error capturando frame");
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }
    
    if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
      // Buscar espacio libre en la cola
      for (int i = 0; i < 2; i++) {
        if (fb_queue[i] == NULL) {
          fb_queue[i] = new_fb;
          new_fb = NULL; // Transferimos propiedad
          break;
        }
      }
      
      // Si no encontramos espacio, devolvemos el frame
      if (new_fb != NULL) {
        esp_camera_fb_return(new_fb);
      }
      
      xSemaphoreGive(xMutex);
    }
    
    // Dar tiempo al sistema
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

// --- Tarea para publicar frames ---
void publishTask(void *parameter) {
  while (true) {
    camera_fb_t *process_fb = NULL;
    
    // Verificar si hay un frame para procesar
    if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
      for (int i = 0; i < 2; i++) {
        if (fb_queue[i] != NULL) {
          process_fb = fb_queue[i];
          fb_queue[i] = NULL; // Liberar el slot
          break;
        }
      }
      xSemaphoreGive(xMutex);
    }
    
    // Si tenemos un frame, publicarlo
    if (process_fb != NULL) {
      // Publicar el frame en el topic MQTT
      if (client.connected() && process_fb->len > 0 && process_fb->buf != NULL) {
        unsigned long publishStart = millis();
        
        if (process_fb->len < client.getBufferSize() - 100) {
          if (client.publish(mqtt_topic, (const uint8_t*)process_fb->buf, process_fb->len, false)) {
            // Incrementar contador para cálculo FPS
            frameCount++;
            unsigned long publishTime = millis() - publishStart;
            Serial.printf("Frame enviado: %d bytes, tiempo: %lu ms\n", process_fb->len, publishTime);
          } else {
            Serial.println("Error enviando frame por MQTT");
          }
        } else {
          Serial.printf("Frame demasiado grande (%d bytes) para buffer MQTT (%d bytes)\n", 
                      process_fb->len, client.getBufferSize());
        }
      }
      
      // Devolver el buffer a la cámara
      esp_camera_fb_return(process_fb);
      
      // Calcular FPS cada segundo
      unsigned long currentTime = millis();
      if (currentTime - lastFpsCalcTime >= 1000) {
        currentFps = frameCount * 1000.0 / (currentTime - lastFpsCalcTime);
        Serial.printf("FPS: %.2f, PSRAM libre: %d bytes\n", currentFps, ESP.getFreePsram());
        frameCount = 0;
        lastFpsCalcTime = currentTime;
      }
    }
    
    // Dar tiempo al sistema
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

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

  // Conectar a WiFi
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nNo se pudo conectar a WiFi. Reiniciando...");
    ESP.restart();
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.print("Dirección IP del ESP32: ");
  Serial.println(WiFi.localIP());
  
  // Configurar MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(25000); // Aumentado para mejor rendimiento (era 10000)
  
  // Intentar conectar al broker MQTT
  connectToMQTT();
  
  // Inicializar semáforo y crear tareas
  xMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(captureTask, "Capture", 8192, NULL, 1, &captureTaskHandle, 0);
  xTaskCreatePinnedToCore(publishTask, "Publish", 8192, NULL, 1, &publishTaskHandle, 1);
  
  lastFpsCalcTime = millis();
  Serial.println("Sistema optimizado inicializado, enviando imágenes por MQTT...");
}

// --- LOOP ---
void loop() {
  // Comprobar la conexión MQTT y reconectar si es necesario
  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();
  
  // El procesamiento principal ahora está en las tareas
  delay(100); // Dar tiempo al sistema
}

// Conectarse al broker MQTT
void connectToMQTT() {
  Serial.println("Intentando conectar al broker MQTT...");
  
  int attempts = 0;
  while (!client.connected() && attempts < 5) {
    Serial.print("Conectando a MQTT...");
    
    // Conectar con ID único
    if (client.connect(mqtt_client_id)) {
      Serial.println("conectado");
      break;
    } else {
      Serial.print("falló, rc=");
      Serial.print(client.state());
      Serial.println(" intentando de nuevo en 5 segundos");
      delay(5000);
      attempts++;
    }
  }
  
  if (!client.connected()) {
    Serial.println("No se pudo conectar al broker MQTT después de varios intentos");
    Serial.println("Códigos de error MQTT:");
    Serial.println("-1: Conexión rechazada por el servidor");
    Serial.println("-2: Problema de conexión (timeout o problemas de red)");
    Serial.println("-3: Cliente ID no aceptado");
    Serial.println("-4: Servidor no disponible");
  }
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
  
  // Optimizado para mejor rendimiento con PSRAM
  config.xclk_freq_hz = 20000000;           // 20MHz XCLK
  config.frame_size = FRAMESIZE_QVGA;       // 320x240 - balance entre calidad y tamaño
  config.pixel_format = PIXFORMAT_JPEG;     // Usamos JPEG
  config.grab_mode = CAMERA_GRAB_LATEST;    // Siempre el frame más reciente (era CAMERA_GRAB_WHEN_EMPTY)
  config.fb_location = CAMERA_FB_IN_PSRAM;  // Usar PSRAM
  config.jpeg_quality = 15;                 // Mayor compresión (era 20)
  config.fb_count = 2;                      // Usar dos framebuffers para pipeline

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
    // Ajustes para optimización
    s->set_framesize(s, FRAMESIZE_QVGA);
    s->set_quality(s, 15);      // Mayor compresión (era 20)
    s->set_brightness(s, 1);
    s->set_contrast(s, 1);
    s->set_saturation(s, 1);
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
    s->set_gainceiling(s, GAINCEILING_2X); // Mejorar captura en baja luz
    s->set_whitebal(s, 1);      // Habilitar balance de blancos
    s->set_dcw(s, 1);           // Down-sampling para mejorar calidad
  }
  
  return true;
}