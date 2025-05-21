#include "esp_camera.h"
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "esp_heap_caps.h"
#include <HTTPClient.h>  // Para realizar peticiones HTTP
#include <time.h>        // Para NTP (hora real)

// Selecciona el modelo de cámara
#define CAMERA_MODEL_ESP32S3_EYE

// PIN del sensor MQ2 (pin que no interfiere con la cámara)
#define MQ2_PIN 2  // GPIO2 (verificar que no interfiera con otros usos)

// LED integrado para alertas visuales (pin puede variar según modelo)
#define LED_PIN 4  // GPIO4, ajustar si es necesario

#include "camera_pins.h"

// WiFi
const char* ssid = "Megacable_2.4G_454A";
const char* password = "pizzadepeperoni123";             // Tu contraseña WiFi

// Servidor de señalización - CAMBIA ESTA IP POR LA QUE MUESTRA EL SERVIDOR PYTHON
const char* serverIP = "clever-koi-freely.ngrok-free.app"; // <-- CAMBIA ESTA IP
const int serverPort = 443; // Cambiado a 443 para HTTPS

// Configuración para JSONBin
const char* jsonbinBinId = "68292f218960c979a59bba9e";  // BIN_ID real para guardar alertas
const char* jsonbinBinUrl = "https://api.jsonbin.io/v3/b/68292f218960c979a59bba9e"; // URL del bin existente
const char* jsonbinKey = "$2a$10$V.YptTzI41YfMisRBnWgvOabeAhka7sPBh/c1q4KsBWWccz2XrIW2";  // Tu X-Master-Key
const unsigned long alertCooldown = 60000; // 60 segundos entre alertas para eviar spam
unsigned long lastAlertTime = 0;  // Última vez que se envió una alerta

// Configuración NTP
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -6 * 3600;  // GMT-6 (Mexico, ajusta según tu zona horaria)
const int   daylightOffset_sec = 0;     // Sin horario de verano (ajustar según corresponda)
bool ntpConfigured = false;

// WebSocket client
WebSocketsClient webSocket;

// Variables para WebRTC
camera_fb_t *fb = NULL;
bool clientConnected = false;
unsigned long lastFrameTime = 0;
const unsigned long frameInterval = 100; // Reducido a ~10 FPS para mejor compatibilidad

// Variables para MQ2
unsigned long lastMQ2Time = 0;
const unsigned long mq2Interval = 1000; // Leer cada segundo
bool sensorMQ2Enabled = true;  // Siempre activo
unsigned long sensorStartTime = 0;
const unsigned long sensorWarmupTime = 20000; // 20 segundos de calentamiento
int baselineMQ2 = 0;           // Valor base en aire limpio

// Umbrales para diferentes tipos de alertas
int umbralHumoLeve = 300;      // Umbral para detección de humo leve
int umbralHumoModerado = 1000; // Umbral para humo moderado
int umbralHumoAlto = 2000;     // Umbral para humo alto/peligroso
int umbralGasLeve = 500;       // Umbral para detección de gas leve
int umbralGasModerado = 1500;  // Umbral para gas moderado
int umbralGasAlto = 3000;      // Umbral para gas alto/peligroso

// Variables para validación de alertas
int contadorAlertasHumo = 0;
int contadorAlertasGas = 0;
bool alertaActivaHumo = false;
bool alertaActivaGas = false;
const int lecturasSeguidas = 3; // Lecturas consecutivas necesarias para confirmar alerta

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  Serial.println();

  // Configurar pin del sensor MQ2 y LED
  pinMode(MQ2_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // LED apagado por defecto
  
  Serial.println("Pin MQ2 y LED de alerta configurados");
  Serial.println("Sensor MQ2 para detección de humo y gases siempre activo");
  
  // Recordatorio para JSONBin
  Serial.println("IMPORTANTE: Recuerda configurar tu JSONBin ID y API Key en el código");
  
  // Iniciar el sensor MQ2
  sensorStartTime = millis();
  Serial.println("Iniciando calentamiento del sensor MQ2 (20 segundos)...");
  
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
  
  // Configurar tiempo NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Sincronizando reloj con NTP...");
  
  // Esperar a que se configure el tiempo NTP (hasta 5 segundos)
  unsigned long startAttemptTime = millis();
  while (!ntpConfigured && millis() - startAttemptTime < 5000) {
    time_t now;
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      ntpConfigured = true;
      Serial.println("Reloj sincronizado correctamente con NTP");
      Serial.print("Fecha/hora actual: ");
      Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
    }
    delay(100);
  }
  
  if (!ntpConfigured) {
    Serial.println("No se pudo sincronizar con NTP, se usará tiempo relativo");
  }
  
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
  
  // Comprobar comandos seriales solo para calibración
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "calibrar") {
      // Tomar múltiples lecturas y promediar para mejorar precisión
      Serial.println("Calibrando sensor en aire limpio...");
      int total = 0;
      for (int i = 0; i < 10; i++) {
        total += analogRead(MQ2_PIN);
        delay(100);
      }
      baselineMQ2 = total / 10;
      Serial.print("Línea base establecida: ");
      Serial.println(baselineMQ2);
      
      // Resetear contadores y alertas
      alertaActivaHumo = false;
      alertaActivaGas = false;
      contadorAlertasHumo = 0;
      contadorAlertasGas = 0;
      digitalWrite(LED_PIN, LOW); // Apagar LED de alerta
    } else if (command == "test_jsonbin") {
      // Comando para probar el envío a JSONBin directamente
      Serial.println("Enviando alerta de prueba a JSONBin...");
      enviarAlertaJsonBin("PRUEBA_MANUAL", 9999);
    } else if (command == "simular_humo") {
      // Simular detección de humo para pruebas
      Serial.println("Simulando detección de humo alto...");
      validarNiveles(umbralHumoAlto + 100);
    } else if (command == "forzar_envio_humo") {
      // Forzar el envío de una alerta de humo aunque no se cumplan los criterios de persistencia
      Serial.println("Forzando envío de alerta de humo...");
      forzarEnvioAlerta("HUMO_FORZADO", analogRead(MQ2_PIN));
    } else if (command == "check_wifi") {
      // Verificar estado de WiFi
      Serial.print("Estado WiFi: ");
      Serial.println(WiFi.status() == WL_CONNECTED ? "Conectado" : "Desconectado");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("Fuerza de señal (RSSI): ");
      Serial.println(WiFi.RSSI());
    }
  }
  
  if (clientConnected) {
    unsigned long currentTime = millis();
    if (currentTime - lastFrameTime >= frameInterval) {
      sendVideoFrame();
      lastFrameTime = currentTime;
    }
  }

  // Lectura del sensor MQ2 cada segundo - siempre activo
  unsigned long currentTime = millis();
  if (currentTime - lastMQ2Time >= mq2Interval) {
    // Verificar si el sensor ha completado su período de calentamiento
    if (currentTime - sensorStartTime >= sensorWarmupTime) {
      int mq2Value = analogRead(MQ2_PIN);
      Serial.print("Lectura MQ2: ");
      Serial.println(mq2Value);
      
      // Calcular nivel relativo a la línea base
      int nivelRelativo = mq2Value - baselineMQ2;
      
      // Análisis de nivel de humo
      validarNiveles(nivelRelativo);
      
      // Si hay humo moderado o alto pero aún no se ha cumplido el criterio de persistencia,
      // verificamos si debemos enviar una alerta temprana ocasionalmente
      if (nivelRelativo > umbralHumoModerado && !alertaActivaHumo && 
          contadorAlertasHumo < lecturasSeguidas && 
          currentTime - lastAlertTime > alertCooldown * 2) {
        // Enviar una alerta temprana ocasionalmente para asegurar que se registre
        Serial.println("Nivel de humo significativo detectado - Enviando alerta temprana");
        forzarEnvioAlerta("HUMO_TEMPRANO", nivelRelativo);
      }
      
    } else {
      // Mostrar tiempo restante de calentamiento
      int remainingSecs = (sensorWarmupTime - (currentTime - sensorStartTime)) / 1000;
      Serial.print("Calentando sensor MQ2: ");
      Serial.print(remainingSecs);
      Serial.println(" segundos restantes");
    }
    
    lastMQ2Time = currentTime;
  }
  
  // Control del LED para alertas
  if (alertaActivaHumo || alertaActivaGas) {
    // Hacer parpadear el LED para alertas activas
    digitalWrite(LED_PIN, (millis() / 250) % 2); // Parpadea cada 250ms
  } else {
    digitalWrite(LED_PIN, LOW); // LED apagado si no hay alertas
  }
}

// Función para validar y analizar lecturas del sensor
void validarNiveles(int nivel) {
  // Análisis de nivel de humo (más sensible)
  if (nivel > umbralHumoAlto) {
    contadorAlertasHumo++;
    Serial.println("DETECTADO: Nivel de humo por encima del umbral ALTO");
    if (contadorAlertasHumo >= lecturasSeguidas && !alertaActivaHumo) {
      alertaActivaHumo = true;
      Serial.println("¡¡¡PELIGRO!!! NIVEL DE HUMO CRÍTICO DETECTADO");
      Serial.println("Se recomienda evacuar inmediatamente");
      // Enviar alerta a JSONBin
      Serial.println("Iniciando envío de alerta HUMO_CRITICO a JSONBin...");
      enviarAlertaJsonBin("HUMO_CRITICO", nivel);
    } else if (contadorAlertasHumo >= lecturasSeguidas) {
      Serial.println("¡ALERTA! Nivel de humo peligroso persistente");
      // Recordatorio periódico para alertas continuas
      Serial.println("Iniciando envío de recordatorio HUMO_CRITICO_PERSISTENTE a JSONBin...");
      enviarAlertaJsonBin("HUMO_CRITICO_PERSISTENTE", nivel);
    }
  } else if (nivel > umbralHumoModerado) {
    Serial.println("DETECTADO: Nivel de humo por encima del umbral MODERADO");
    contadorAlertasHumo++;
    if (contadorAlertasHumo >= lecturasSeguidas && !alertaActivaHumo) {
      alertaActivaHumo = true;
      Serial.println("¡ALERTA! Nivel de humo moderado/alto confirmado");
      Serial.println("Verificar fuente de humo y ventilar el área");
      // Enviar alerta a JSONBin
      Serial.println("Iniciando envío de alerta HUMO_MODERADO a JSONBin...");
      enviarAlertaJsonBin("HUMO_MODERADO", nivel);
    }
  } else if (nivel > umbralHumoLeve) {
    Serial.println("Nivel de humo: LEVE");
    // Reducir contador pero no a cero para mantener historial
    if (contadorAlertasHumo > 0) contadorAlertasHumo--;
    if (contadorAlertasHumo == 0) alertaActivaHumo = false;
  } else {
    Serial.println("Nivel de humo: NORMAL");
    // Si hay lecturas normales consecutivas, resetear alertas
    if (contadorAlertasHumo > 0) contadorAlertasHumo--;
    if (contadorAlertasHumo == 0) alertaActivaHumo = false;
  }
  
  // Análisis específico para gases (valores dependientes del tipo de gas)
  if (nivel > umbralGasAlto) {
    contadorAlertasGas++;
    Serial.println("DETECTADO: Nivel de gas por encima del umbral ALTO");
    if (contadorAlertasGas >= lecturasSeguidas && !alertaActivaGas) {
      alertaActivaGas = true;
      Serial.println("¡¡¡PELIGRO!!! FUGA DE GAS CRÍTICA DETECTADA");
      Serial.println("Posible presencia de gas inflamable - EVACUAR INMEDIATAMENTE");
      // Enviar alerta a JSONBin
      Serial.println("Iniciando envío de alerta GAS_CRITICO a JSONBin...");
      enviarAlertaJsonBin("GAS_CRITICO", nivel);
    } else if (contadorAlertasGas >= lecturasSeguidas) {
      Serial.println("¡ALERTA! Nivel de gas peligroso persistente");
      // Recordatorio periódico para alertas continuas
      Serial.println("Iniciando envío de recordatorio GAS_CRITICO_PERSISTENTE a JSONBin...");
      enviarAlertaJsonBin("GAS_CRITICO_PERSISTENTE", nivel);
    }
  } else if (nivel > umbralGasModerado) {
    Serial.println("DETECTADO: Nivel de gas por encima del umbral MODERADO");
    contadorAlertasGas++;
    if (contadorAlertasGas >= lecturasSeguidas && !alertaActivaGas) {
      alertaActivaGas = true;
      Serial.println("¡ALERTA! Nivel de gas moderado/alto confirmado");
      Serial.println("Verificar posibles fugas y ventilar el área");
      // Enviar alerta a JSONBin
      Serial.println("Iniciando envío de alerta GAS_MODERADO a JSONBin...");
      enviarAlertaJsonBin("GAS_MODERADO", nivel);
    }
  } else if (nivel > umbralGasLeve) {
    Serial.println("Nivel de gas: LEVE");
    if (contadorAlertasGas > 0) contadorAlertasGas--;
    if (contadorAlertasGas == 0) alertaActivaGas = false;
  } else {
    Serial.println("Nivel de gas: NORMAL");
    if (contadorAlertasGas > 0) contadorAlertasGas--;
    if (contadorAlertasGas == 0) alertaActivaGas = false;
  }
  
  // Mostrar advertencia sobre calibración si los niveles son extremos
  if (nivel < -200 || nivel > 4000) {
    Serial.println("NOTA: Lecturas extremas detectadas. Considere recalibrar el sensor.");
  }
}

// Función para enviar alerta a JSONBin
void enviarAlertaJsonBin(String tipoAlerta, int nivelDetectado) {
  unsigned long currentTime = millis();

  if (lastAlertTime > 0 && currentTime - lastAlertTime < alertCooldown) {
    Serial.println("Alerta en cooldown, esperando para enviar otra");
    Serial.print("Tiempo restante (segundos): ");
    Serial.println((alertCooldown - (currentTime - lastAlertTime)) / 1000);
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error: No hay conexión WiFi para enviar alerta");
    return;
  }

  char fechaStr[25];
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    strftime(fechaStr, sizeof(fechaStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
  } else {
    // Si falla, usa millis como fallback
    sprintf(fechaStr, "%lu", currentTime);
  }

  // 1. GET el bin actual
  HTTPClient httpGet;
  String getUrl = String("https://api.jsonbin.io/v3/b/") + jsonbinBinId + "/latest";
  httpGet.begin(getUrl);
  httpGet.addHeader("X-Master-Key", jsonbinKey);
  int httpCodeGet = httpGet.GET();
  if (httpCodeGet != 200) {
    Serial.print("Error al leer bin: ");
    Serial.println(httpGet.errorToString(httpCodeGet));
    httpGet.end();
    return;
  }
  String binResponse = httpGet.getString();
  httpGet.end();

  // 2. Parsear y agregar alerta
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, binResponse);
  if (error) {
    Serial.print("Error parseando bin: ");
    Serial.println(error.c_str());
    return;
  }
  JsonArray registros = doc["record"]["registros"].as<JsonArray>();
  if (registros.isNull()) {
    doc["record"]["registros"] = JsonArray();
    registros = doc["record"]["registros"].as<JsonArray>();
  }
  DynamicJsonDocument nuevaAlertaDoc(256);
  nuevaAlertaDoc["fecha_hora"] = fechaStr;

  // Traducción de tipoAlerta a texto amigable
  String claseAlerta;
  if (tipoAlerta == "HUMO_CRITICO") claseAlerta = "humo crítico";
  else if (tipoAlerta == "HUMO_MODERADO") claseAlerta = "humo moderado";
  else if (tipoAlerta == "HUMO_TEMPRANO") claseAlerta = "humo temprano";
  else if (tipoAlerta == "HUMO_FORZADO") claseAlerta = "humo forzado";
  else if (tipoAlerta == "GAS_CRITICO") claseAlerta = "gas crítico";
  else if (tipoAlerta == "GAS_MODERADO") claseAlerta = "gas moderado";
  else if (tipoAlerta == "GAS_CRITICO_PERSISTENTE") claseAlerta = "gas crítico persistente";
  else if (tipoAlerta == "HUMO_CRITICO_PERSISTENTE") claseAlerta = "humo crítico persistente";
  else if (tipoAlerta == "PRUEBA_MANUAL") claseAlerta = "prueba manual";
  else claseAlerta = tipoAlerta; // Por si acaso

  nuevaAlertaDoc["clase"] = claseAlerta;
  registros.add(nuevaAlertaDoc);

  // 3. PUT el bin actualizado
  String nuevoJson;
  serializeJson(doc["record"], nuevoJson);

  HTTPClient httpPut;
  String putUrl = String("https://api.jsonbin.io/v3/b/") + jsonbinBinId;
  httpPut.begin(putUrl);
  httpPut.addHeader("Content-Type", "application/json");
  httpPut.addHeader("X-Master-Key", jsonbinKey);
  int httpCodePut = httpPut.PUT(nuevoJson);

  Serial.print("Código de respuesta PUT: ");
  Serial.println(httpCodePut);
  if (httpCodePut > 0) {
    String response = httpPut.getString();
    Serial.println("Respuesta: " + response);
    lastAlertTime = currentTime;
    for (int i = 0; i < 5; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
    Serial.println("¡Alerta guardada en el bin!");
  } else {
    Serial.print("Error en PUT: ");
    Serial.println(httpPut.errorToString(httpCodePut));
  }
  httpPut.end();
  Serial.println("=================================================");
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

// Función para forzar el envío de alertas ignorando criterios de persistencia
void forzarEnvioAlerta(String tipoAlerta, int nivel) {
  // Esta función ignora los criterios de lecturas consecutivas
  Serial.println("Forzando envío de alerta a JSONBin...");
  enviarAlertaJsonBin(tipoAlerta, nivel);
}
