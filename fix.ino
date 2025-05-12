#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"         
#include "soc/soc.h"            
#include "soc/rtc_cntl_reg.h"   
#include <PubSubClient.h>  // Agregar librería MQTT

// Configuración WiFi original como respaldo
const char* ssid = "Megacable_2.4G_454A";
const char* password = "pizzadepeperoni123";

// Nueva configuración WiFi principal
const char* ssid_iphone = "iPhone de Jesús Manuel";
const char* password_iphone = "12345678";

int pulse = 14;
int head = 1;

String Feedback = "";   
String Command = "", cmd = "", P1 = "", P2 = "", P3 = "", P4 = "", P5 = "", P6 = "", P7 = "", P8 = "", P9 = "";

byte ReceiveState = 0, cmdState = 1, strState = 1, questionstate = 0, equalstate = 0, semicolonstate = 0;

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

WiFiServer server(80);

// Configuración IP para red Megacable
IPAddress local_IP(192, 168, 1, 184);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// Configuración IP para red iPhone
IPAddress local_IP_iphone(172, 20, 10, 10);    // Cambiado a .10 para estar en el rango seguro
IPAddress gateway_iphone(172, 20, 10, 1);      // Gateway del iPhone
IPAddress subnet_iphone(255, 255, 255, 240);   // Máscara exacta del iPhone
IPAddress primaryDNS_iphone(8, 8, 8, 8);
IPAddress secondaryDNS_iphone(8, 8, 4, 4);

// Configuración del broker MQTT público
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;  // Puerto estándar MQTT para ESP32
const char* mqtt_topic = "esp32cam/stream/";
String deviceId;  // Para almacenar ID único del dispositivo

// Cliente MQTT
WiFiClient mqttWiFiClient;
PubSubClient mqttClient(mqttWiFiClient);

// Aumentar el tamaño de los chunks para mejor rendimiento
const size_t CHUNK_SIZE = 4096;  // Aumentar a 4KB por chunk

// Definir INDEX_HTML antes de loop()
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <style>
    .video-container {
      max-width: 800px;
      margin: 0 auto;
    }
    .status {
      color: #666;
      text-align: center;
    }
    canvas {
      width: 100%;
      border: 1px solid #ccc;
    }
    :root {
      --primary-color: #0A84FF;    /* iOS blue */
      --secondary-color: #32D74B;  /* iOS green */
      --danger-color: #FF453A;     /* iOS red */
      --bg-color: #000000;         /* Pure black background */
      --surface-color: #1C1C1E;    /* iOS dark gray */
      --text-color: #FFFFFF;       /* White text */
      --text-secondary: #98989F;   /* iOS secondary text */
      --border-radius: 12px;       /* iOS style rounded corners */
    }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
      margin: 0;
      padding: 20px;
      background: var(--bg-color);
      color: var(--text-color);
      min-height: 100vh;
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
      padding: 20px;
    }
    .stream-container {
      position: relative;
      width: 400px;
      height: 296px;
      margin: 0 auto 20px;
      overflow: hidden;
      border-radius: var(--border-radius);
      box-shadow: 0 8px 16px rgba(0,0,0,0.3);
      background: var(--surface-color);
      border: 1px solid rgba(255,255,255,0.1);
      backdrop-filter: blur(20px);
      -webkit-backdrop-filter: blur(20px);
      transition: all 0.3s ease;
    }
    #ShowImage {
      width: 100%;
      height: 100%;
      object-fit: contain;
      display: block;
    }
    #canvas {
      position: absolute;
      left: 0;
      top: 0;
      width: 100%;
      height: 100%;
      pointer-events: none;
    }
    .controls {
      display: grid;
      grid-template-columns: 1fr 1fr;  /* Cambiado a 2 columnas */
      grid-template-rows: auto auto;   /* 2 filas */
      gap: 24px;
      background: var(--surface-color);
      padding: 24px;
      border-radius: var(--border-radius);
      box-shadow: 0 4px 12px rgba(0,0,0,0.2);
      border: 1px solid rgba(255,255,255,0.1);
      backdrop-filter: blur(20px);
      -webkit-backdrop-filter: blur(20px);
    }
    .control-group {
      padding: 16px;
      background: rgba(255,255,255,0.05);
      border-radius: var(--border-radius);
      border: 1px solid rgba(255,255,255,0.1);
    }
    .control-group label {
      display: block;
      margin-bottom: 8px;
      color: var(--text-secondary);
      font-weight: 500;
    }
    .btn {
      background: var(--primary-color);
      color: var(--text-color);  /* Asegura que el texto sea blanco */
      border: none;
      padding: 12px 24px;
      border-radius: var(--border-radius);
      cursor: pointer;
      transition: all 0.3s ease;
      font-weight: 600;
      font-size: 15px;
      letter-spacing: 0.3px;
      width: 100%;
      margin-bottom: 12px;
    }
    .btn:hover {
      transform: translateY(-1px);
      box-shadow: 0 4px 12px rgba(10,132,255,0.3);
    }
    .btn:active {
      transform: translateY(1px);
    }
    .btn.danger {
      background: var(--danger-color);
    }
    .btn.success {
      background: var(--secondary-color);
      color: var(--text-color);  /* Texto blanco para el botón de captura */
    }
    .btn:disabled {
      background: #333;
      cursor: not-allowed;
      opacity: 0.7;
    }
    input[type="range"] {
      width: 100%;
      margin: 10px 0;
      -webkit-appearance: none;
      background: transparent;
    }
    input[type="range"]::-webkit-slider-runnable-track {
      width: 100%;
      height: 4px;
      background: rgba(255,255,255,0.2);
      border-radius: 2px;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      height: 18px;
      width: 18px;
      border-radius: 50%;
      background: var(--primary-color);
      margin-top: -7px;
      box-shadow: 0 2px 4px rgba(0,0,0,0.2);
      cursor: pointer;
    }
    select {
      width: 100%;
      padding: 12px;
      border-radius: var(--border-radius);
      border: 1px solid rgba(255,255,255,0.1);
      background: rgba(255,255,255,0.05);
      color: var(--text-color); /* Color del texto del select cuando está abierto */
      font-size: 15px;
      appearance: none;
      cursor: pointer;
    }
    select option {
      background: var(--surface-color); /* Fondo oscuro para las opciones */
      color: #000000; /* Texto negro para las opciones no seleccionadas */
    }
    select option:checked {
      background: var(--primary-color);
      color: var(--text-color); /* Texto blanco para la opción seleccionada */
    }
    select option:hover {
      background: var(--primary-color);
      color: var(--text-color);
    }
    select:focus {
      outline: none;
      border-color: var(--primary-color);
    }
    #captureStatus {
      margin-top: 20px;
      padding: 16px;
      border-radius: var(--border-radius);
      background: var(--surface-color);
      border: 1px solid rgba(255,255,255,0.1);
    }
    .success {
      color: var(--secondary-color);
    }
    .error {
      color: var(--danger-color);
    }
    @media (max-width: 768px) {
      .stream-container {
        width: 100%;
        height: auto;
        aspect-ratio: 4/3;
      }
      .controls {
        grid-template-columns: 1fr;  /* Una columna en móviles */
        gap: 16px;
        padding: 16px;
      }
      .control-group {
        padding: 12px;
      }
    }
    @keyframes fadeIn {
      from { opacity: 0; transform: translateY(10px); }
      to { opacity: 1; transform: translateY(0); }
    }
    .container > * {
      animation: fadeIn 0.3s ease-out forwards;
    }
    .value-display {
      font-size: 24px;
      font-weight: 600;
      color: var(--primary-color);
      text-align: right;
      margin-top: 8px;
    }
    .fullscreen {
      position: fixed !important;
      top: 0 !important;
      left: 0 !important;
      width: 100vw !important;
      height: 100vh !important;
      z-index: 9999;
      margin: 0 !important;
      padding: 0 !important;
      border-radius: 0 !important;
      background: black !important;
      border: none !important;
      overflow: hidden !important;
    }
    .fullscreen #ShowImage {
      position: absolute !important;
      top: 0 !important;
      left: 0 !important;
      width: 100vw !important;
      height: 100vh !important;
      object-fit: cover !important;
      margin: 0 !important;
    }
    .fullscreen #canvas {
      position: absolute !important;
      top: 0 !important;
      left: 0 !important;
      width: 100vw !important;
      height: 100vh !important;
      object-fit: cover !important;
    }
    .fullscreen-btn {
      position: absolute;
      top: 10px;
      right: 10px;
      background: rgba(0, 0, 0, 0.5);
      border: none;
      color: white;
      width: 40px;
      height: 40px;
      border-radius: 50%;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 20px;
      transition: all 0.3s ease;
      z-index: 1001;
      padding: 0;
    }
    .fullscreen .fullscreen-btn {
      top: 20px;
      right: 20px;
      background: rgba(0, 0, 0, 0.7);
      z-index: 10000;
    }
    .exit-fullscreen-btn {
      position: fixed;
      top: 20px;
      right: 20px;
      background: rgba(255, 69, 58, 0.7);  /* iOS red con transparencia */
      color: white;
      border: none;
      width: 40px;
      height: 40px;
      border-radius: 50%;
      cursor: pointer;
      display: none;  /* Oculto por defecto */
      align-items: center;
      justify-content: center;
      font-size: 20px;
      z-index: 10001;
      transition: all 0.3s ease;
    }
    .fullscreen .exit-fullscreen-btn {
      display: flex;  /* Se muestra cuando está en fullscreen */
    }
    .exit-fullscreen-btn:hover {
      background: rgba(255, 69, 58, 0.9);
      transform: scale(1.1);
    }
  </style>
</head>
<body>
  <div class="video-container">
    <canvas id="videoCanvas"></canvas>
    <p class="status">Conectando...</p>
  </div>
  <div class="container">
    <div class="stream-container">
      <button class="fullscreen-btn" id="fullscreenBtn">⛶</button>
      <button class="exit-fullscreen-btn" id="exitFullscreenBtn">×</button>
      <img id="ShowImage" src="">
      <canvas id="canvas"></canvas>
    </div>

    <div class="controls">
      <!-- Grupo 1: Botones de control -->
      <div class="control-group">
        <button class="btn" id="restart">Restart</button>
        <button class="btn" id="toggleStream">Stop Stream</button>
        <button class="btn success" id="captureBtn">Capture Image</button>
      </div>

      <!-- Grupo 2: Resolución -->
      <div class="control-group">
        <label>Resolution</label>
        <select id="framesize" disabled>
          <option value="CIF" selected="selected">CIF (400x296)</option>
        </select>
      </div>

      <!-- Grupo 3: Flash y Quality -->
      <div class="control-group">
        <label>Flash</label>
        <input type="range" id="flash" min="0" max="255" value="0">
        <div class="value-display" id="flashValue">0</div>
        
        <label>Quality</label>
        <input type="range" id="quality" min="10" max="63" value="10">
        <div class="value-display" id="qualityValue">10</div>
      </div>

      <!-- Grupo 4: Brightness y Contrast -->
      <div class="control-group">
        <label>Brightness</label>
        <input type="range" id="brightness" min="-2" max="2" value="0">
        <div class="value-display" id="brightnessValue">0</div>
        
        <label>Contrast</label>
        <input type="range" id="contrast" min="-2" max="2" value="0">
        <div class="value-display" id="contrastValue">0</div>
      </div>
    </div>

    <div id="captureStatus"></div>
  </div>
  
  <div id="fpsCounter" style="position: fixed; top: 10px; right: 10px; background: rgba(0,0,0,0.7); color: white; padding: 5px; border-radius: 5px;"></div>
  
  <script>
    var getStill = document.getElementById('getStill');
    var ShowImage = document.getElementById('ShowImage');
    var canvas = document.getElementById('canvas');
    var ctx = canvas.getContext('2d');
    var framesize = document.getElementById('framesize');
    var flash = document.getElementById('flash');
    var quality = document.getElementById('quality');
    var brightness = document.getElementById('brightness');
    var contrast = document.getElementById('contrast');
    var toggleStream = document.getElementById('toggleStream');
    var streaming = false;
    var streamTimer;
    
    const flashValue = document.getElementById('flashValue');
    const qualityValue = document.getElementById('qualityValue');
    const brightnessValue = document.getElementById('brightnessValue');
    const contrastValue = document.getElementById('contrastValue');
    
    function startStream() {
      if(streaming) return;
      streaming = true;
      ShowImage.src = `${location.origin}/?getstill=1`;
    }

    function stopStream() {
      streaming = false;
      if(streamTimer) clearTimeout(streamTimer);
    }

    let frameCounter = 0;
    const DETECTION_INTERVAL = 3; // Procesar cada 3 frames

    ShowImage.onload = function() {
      canvas.width = ShowImage.naturalWidth;
      canvas.height = ShowImage.naturalHeight;
      ctx.drawImage(ShowImage, 0, 0);

      frameCounter++;
      if (frameCounter >= DETECTION_INTERVAL) {
        processDetection();
        frameCounter = 0;
      }

      if(streaming) {
        requestAnimationFrame(() => {
          ShowImage.src = `${location.origin}/?getstill=1&t=${Date.now()}`;
        });
      }
    }

    // Implementar un control de FPS
    let lastFrameTime = 0;
    const targetFPS = 30; // Ajustar según necesidad
    const frameInterval = 1000 / targetFPS;

    function updateFrame(currentTime) {
      if (!streaming) return;
      
      const elapsed = currentTime - lastFrameTime;
      
      if (elapsed > frameInterval) {
        lastFrameTime = currentTime;
        ShowImage.src = `${location.origin}/?getstill=1&t=${Date.now()}`;
      }
      
      requestAnimationFrame(updateFrame);
    }

    function processDetection() {
      const originalWidth = ShowImage.naturalWidth;
      const originalHeight = ShowImage.naturalHeight;
      
      canvas.toBlob(function(blob) {
        const formData = new FormData();
        formData.append('image', blob, 'image.jpg');
        
        fetch('https://optimal-stirring-viper.ngrok-free.app/detect', {
          method: 'POST',
          body: formData
        })
        .then(response => response.json())
        .then(data => {
          if(data.status === 'success') {
            drawDetections(data.detections, 1, 1); // Usar escala 1:1
          }
        })
        .catch(error => {
          console.error('Error:', error);
        });
      }, 'image/jpeg', 0.8); // Aumentar calidad
    }

    function drawDetections(detections, scaleX, scaleY) {
      // Limpiar el canvas y redibujar la imagen
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      ctx.drawImage(ShowImage, 0, 0);
      
      // Variable para detectar si hay reloj
      let clockDetected = false;
      const COOLDOWN_TIME = 8000;
      
      if(window.lastActionTime && Date.now() - window.lastActionTime < COOLDOWN_TIME) {
        return;
      }
      
      detections.forEach(det => {
        const [x1, y1, x2, y2] = det.bbox;
        
        // Calcular dimensiones precisas
        const scaledX1 = Math.round(Number(x1) * scaleX);
        const scaledY1 = Math.round(Number(y1) * scaleY);
        const scaledWidth = Math.round((Number(x2) - Number(x1)) * scaleX);
        const scaledHeight = Math.round((Number(y2) - Number(y1)) * scaleY);
        
        // Formatear etiqueta con precisión de 1 decimal
        const confidence = (det.confidence * 100).toFixed(1);
        const label = `${det.class} ${confidence}%`;
        
        // Estilo del bounding box
        ctx.lineWidth = 3;
        ctx.strokeStyle = '#00FFFF';
        ctx.strokeRect(scaledX1, scaledY1, scaledWidth, scaledHeight);
        
        // Agregar fondo semitransparente al bbox
        ctx.fillStyle = 'rgba(0, 255, 255, 0.2)';
        ctx.fillRect(scaledX1, scaledY1, scaledWidth, scaledHeight);
        
        // Configurar estilo del texto
        ctx.font = 'bold 16px Arial';
        const textMetrics = ctx.measureText(label);
        const textWidth = textMetrics.width;
        const textHeight = 20;
        const padding = 4;
        
        // Dibujar fondo del texto
        ctx.fillStyle = 'rgba(0, 0, 0, 0.7)';
        ctx.fillRect(
          scaledX1, 
          scaledY1 > textHeight ? scaledY1 - textHeight : scaledY1,
          textWidth + (padding * 2),
          textHeight
        );
        
        // Dibujar texto
        ctx.fillStyle = '#00FFFF';
        ctx.fillText(
          label,
          scaledX1 + padding,
          scaledY1 > textHeight ? scaledY1 - 5 : scaledY1 + 15
        );
        
        // Verificar si es un reloj
        if(det.class.toLowerCase() === 'clock') {
          clockDetected = true;
        }
      });
      
      // Código para manejar la detección del reloj
      if(clockDetected && (!window.lastActionTime || Date.now() - window.lastActionTime >= COOLDOWN_TIME)) {
        window.lastActionTime = Date.now(); // Establecer tiempo de última acción
        
        // Secuencia de acciones
        // 1. Activar flash
        fetch(location.origin+'/?flash=255;stop')
          .then(() => {
            // 2. Capturar imagen
            return fetch(location.origin+'/?capture=1;stop');
          })
          .then(response => {
            if(!response.ok) throw new Error('Error en la captura');
            return response.blob();
          })
          .then(blob => {
            // Procesar la imagen capturada
            const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
            const filename = `clock_detected_${timestamp}.jpg`;
            const formData = new FormData();
            formData.append('image', blob, filename);
            
            // 3. Activar buzzer
            fetch(location.origin+'/?detectCount=clock=1;stop');
            
            // Guardar la imagen
            return fetch('https://optimal-stirring-viper.ngrok-free.app/save_image', {
              method: 'POST',
              body: formData
            });
          })
          .then(response => response.json())
          .then(data => {
            if(data.success) {
              // Mostrar mensaje en el captureStatus en lugar de result
              captureStatus.innerHTML = `<span style="color: green;">Reloj detectado - Imagen guardada como: ${data.filename}</span>`;
              
              // Apagar flash inmediatamente después de guardar la imagen
              fetch(location.origin+'/?flash=0;stop')
                .then(() => {
                  console.log('Flash apagado después de la detección');
                })
                .catch(error => {
                  console.error('Error al apagar el flash:', error);
                });
            }
          })
          .catch(error => {
            console.error('Error:', error);
            // Mostrar error en el captureStatus
            captureStatus.innerHTML = `<span style="color: red;">Error en captura automática: ${error.message}</span>`;
            
            // Asegurarse de apagar el flash incluso si hay error
            fetch(location.origin+'/?flash=0;stop')
              .then(() => {
                console.log('Flash apagado después de error');
              })
              .catch(err => {
                console.error('Error al apagar el flash:', err);
              });
          });
      }
    }

    toggleStream.onclick = function(event) {
      if(!streaming) {
        startStream();
        this.value = "Stop Stream";
      } else {
        stopStream();
        this.value = "Start Stream";
      }
    }

    framesize.onchange = function (event) {
      stopStream();  // Detener stream antes de cambiar resolución
      fetch(document.location.origin+'/?framesize='+this.value+';stop')
        .then(() => {
          // Esperar un momento para que se aplique el cambio
          setTimeout(() => {
            // Actualizar dimensiones del contenedor según la resolución
            const resolution = this.value;
            const container = document.querySelector('.stream-container');
            switch(resolution) {
              case 'UXGA':
                container.style.width = '1600px';
                container.style.height = '1200px';
                break;
              case 'SXGA':
                container.style.width = '1280px';
                container.style.height = '1024px';
                break;
              case 'XGA':
                container.style.width = '1024px';
                container.style.height = '768px';
                break;
              case 'SVGA':
                container.style.width = '800px';
                container.style.height = '600px';
                break;
              case 'VGA':
                container.style.width = '640px';
                container.style.height = '480px';
                break;
              case 'CIF':
                container.style.width = '400px';
                container.style.height = '296px';
                break;
              case 'QVGA':
                container.style.width = '320px';
                container.style.height = '240px';
                break;
              case 'HQVGA':
                container.style.width = '240px';
                container.style.height = '176px';
                break;
              case 'QQVGA':
                container.style.width = '160px';
                container.style.height = '120px';
                break;
            }
            startStream();  // Reiniciar stream con nueva resolución
          }, 500);
        });
    }
    
    flash.onchange = function(event) {
      const value = this.value;
      flashValue.textContent = value;
      fetch(location.origin+'/?flash='+value+';stop');
    }
    
    quality.onchange = function(event) {
      const value = this.value;
      qualityValue.textContent = value;
      fetch(document.location.origin+'/?quality='+value+';stop');
    }
    
    brightness.onchange = function(event) {
      const value = this.value;
      brightnessValue.textContent = value;
      fetch(document.location.origin+'/?brightness='+value+';stop');
    }
    
    contrast.onchange = function(event) {
      const value = this.value;
      contrastValue.textContent = value;
      fetch(document.location.origin+'/?contrast='+value+';stop');
    }
    
    restart.onclick = function (event) {
      fetch(location.origin+'/?restart=stop');
    }
    
    var captureBtn = document.getElementById('captureBtn');
    var captureStatus = document.getElementById('captureStatus');
    
    captureBtn.onclick = function() {
      captureStatus.innerHTML = "Capturando...";
      captureBtn.disabled = true;
      
      fetch(location.origin+'/?capture=1;stop')
        .then(response => {
          if(!response.ok) throw new Error('Error en la captura');
          return response.blob();
        })
        .then(blob => {
          const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
          const filename = `captura_${timestamp}.jpg`;
          
          // Crear link para descargar la imagen
          const url = window.URL.createObjectURL(blob);
          const a = document.createElement('a');
          a.style.display = 'none';
          a.href = url;
          a.download = filename;
          document.body.appendChild(a);
          a.click();
          window.URL.revokeObjectURL(url);
          
          // Enviar al servidor Python para guardar
          const formData = new FormData();
          formData.append('image', blob, filename);
          
          return fetch('https://optimal-stirring-viper.ngrok-free.app/save_image', {
            method: 'POST',
            body: formData
          });
        })
        .then(response => response.json())
        .then(data => {
          if(data.success) {
            captureStatus.innerHTML = "Imagen guardada exitosamente como: " + data.filename;
            captureStatus.style.color = "green";
          } else {
            throw new Error(data.message || 'Error al guardar la imagen');
          }
        })
        .catch(error => {
          captureStatus.innerHTML = "Error: " + error.message;
          captureStatus.style.color = "red";
        })
        .finally(() => {
          captureBtn.disabled = false;
        });
    };
    
    window.onload = function() {
      // Iniciar streaming automáticamente
      toggleStream.click();
    }

    let frameCount = 0;
    let lastFPSUpdate = Date.now();
    const fpsCounter = document.getElementById('fpsCounter');

    function updateFPSCounter() {
      frameCount++;
      const now = Date.now();
      const elapsed = now - lastFPSUpdate;
      
      if (elapsed >= 1000) {
        const fps = Math.round((frameCount * 1000) / elapsed);
        fpsCounter.textContent = `FPS: ${fps}`;
        frameCount = 0;
        lastFPSUpdate = now;
      }
      
      requestAnimationFrame(updateFPSCounter);
    }

    updateFPSCounter();

    var streamContainer = document.querySelector('.stream-container');
    var fullscreenBtn = document.getElementById('fullscreenBtn');
    var lastClick = 0;

    // Agregar el manejador de doble clic al contenedor
    streamContainer.addEventListener('dblclick', function() {
      if (document.fullscreenElement) {
        if (document.exitFullscreen) {
          document.exitFullscreen();
        } else if (document.webkitExitFullscreen) {
          document.webkitExitFullscreen();
        } else if (document.msExitFullscreen) {
          document.msExitFullscreen();
        }
        streamContainer.classList.remove('fullscreen');
      } else {
        if (streamContainer.requestFullscreen) {
          streamContainer.requestFullscreen();
        } else if (streamContainer.webkitRequestFullscreen) {
          streamContainer.webkitRequestFullscreen();
        } else if (streamContainer.msRequestFullscreen) {
          streamContainer.msRequestFullscreen();
        }
        streamContainer.classList.add('fullscreen');
      }
    });

    fullscreenBtn.onclick = function() {
      if (!document.fullscreenElement) {
        if (streamContainer.requestFullscreen) {
          streamContainer.requestFullscreen();
        } else if (streamContainer.webkitRequestFullscreen) {
          streamContainer.webkitRequestFullscreen();
        } else if (streamContainer.msRequestFullscreen) {
          streamContainer.msRequestFullscreen();
        }
        streamContainer.classList.add('fullscreen');
      } else {
        if (document.exitFullscreen) {
          document.exitFullscreen();
        } else if (document.webkitExitFullscreen) {
          document.webkitExitFullscreen();
        } else if (document.msExitFullscreen) {
          document.msExitFullscreen();
        }
        streamContainer.classList.remove('fullscreen');
      }
    }

    document.addEventListener('fullscreenchange', function() {
      if (!document.fullscreenElement) {
        streamContainer.classList.remove('fullscreen');
      }
    });

    var exitFullscreenBtn = document.getElementById('exitFullscreenBtn');

    exitFullscreenBtn.onclick = function() {
      if (document.exitFullscreen) {
        document.exitFullscreen();
      } else if (document.webkitExitFullscreen) {
        document.webkitExitFullscreen();
      } else if (document.msExitFullscreen) {
        document.msExitFullscreen();
      }
      streamContainer.classList.remove('fullscreen');
    };
  </script>
</body>
</html>
)rawliteral";

// Luego define STREAM_HTML
static const char PROGMEM STREAM_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32-CAM Stream</title>
  <style>
    body {
      margin: 0;
      padding: 0;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      background: #000;
    }
    img {
      max-width: 100%;
      max-height: 100vh;
      object-fit: contain;
    }
  </style>
</head>
<body>
  <img id="stream" src="">
  <script>
    var img = document.getElementById('stream');
    function updateImage() {
      img.src = window.location.origin + '/?getstill=1&t=' + new Date().getTime();
    }
    img.onload = function() {
      setTimeout(updateImage, 50);
    }
    img.onerror = function() {
      setTimeout(updateImage, 500);
    }
    updateImage();
  </script>
</body>
</html>
)rawliteral";

void ExecuteCommand(WiFiClient& client) {
  if (cmd != "getstill") {
    Serial.println("cmd= " + cmd + " ,P1= " + P1 + " ,P2= " + P2 + " ,P3= " + P3 + " ,P4= " + P4 + " ,P5= " + P5 + " ,P6= " + P6 + " ,P7= " + P7 + " ,P8= " + P8 + " ,P9= " + P9);
    if (P2.toInt() >= head) {
      digitalWrite(pulse, 1);
      delay(3000);
      digitalWrite(pulse, 0);
    } 
    if (cmd == "detectCount" && P1 == "clock" && P2 == "1") {
      digitalWrite(pulse, 1);  // Activar buzzer
      
      // Capturar imagen cuando se activa el buzzer
      camera_fb_t * fb = esp_camera_fb_get();
      if(fb) {
        // Enviar la imagen al cliente web
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: image/jpeg");
        client.println("Content-Length: " + String(fb->len));
        client.println("Connection: close");
        client.println();
        client.write(fb->buf, fb->len);
        esp_camera_fb_return(fb);
      }
      
      delay(3000);  // Mantener buzzer por 3 segundos
      digitalWrite(pulse, 0);  // Apagar buzzer
    }
  }

  if (cmd == "resetwifi") {  
    WiFi.begin(P1.c_str(), P2.c_str());
    Serial.print("Connecting to ");
    Serial.println(P1);

    long int StartTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      if ((StartTime + 5000) < millis()) break;
    } 
    Serial.println("\nSTAIP: " + WiFi.localIP().toString());
    Feedback = "STAIP: " + WiFi.localIP().toString();
  }    
  else if (cmd == "restart") {
    ESP.restart();
  }    
  else if (cmd == "digitalwrite") {
    ledcDetach(P1.toInt());  // Corregido
    pinMode(P1.toInt(), OUTPUT);
    digitalWrite(P1.toInt(), P2.toInt());
  }   
   else if (cmd == "analogwrite") {
    if (P1 == "4") {
      ledcAttach(4, 5000, 8);  // pin, frecuencia, resolución
      ledcWrite(4, P2.toInt());  
    }
    else {
      ledcAttach(P1.toInt(), 5000, 8);  // pin, frecuencia, resolución
      ledcWrite(5, P2.toInt());
    }
  }    
  else if (cmd == "flash") {
    ledcAttach(4, 5000, 8);  // pin, frecuencia, resolución
    ledcWrite(4, P1.toInt());  // Establecer el valor del flash
    
    // Enviar respuesta con headers CORS
    client.println("HTTP/1.1 200 OK");
    client.println("Access-Control-Allow-Origin: *");
    client.println("Access-Control-Allow-Methods: GET, POST, OPTIONS");
    client.println("Access-Control-Allow-Headers: Content-Type");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.println("{\"status\":\"success\",\"value\":" + P1 + "}");
  }  
  else if (cmd == "framesize") { 
    sensor_t * s = esp_camera_sensor_get();  
    if (P1 == "QQVGA") s->set_framesize(s, FRAMESIZE_QQVGA);
    else if (P1 == "HQVGA") s->set_framesize(s, FRAMESIZE_HQVGA);
    else if (P1 == "QVGA") s->set_framesize(s, FRAMESIZE_QVGA);
    else if (P1 == "CIF") s->set_framesize(s, FRAMESIZE_CIF);
    else if (P1 == "VGA") s->set_framesize(s, FRAMESIZE_VGA);  
    else if (P1 == "SVGA") s->set_framesize(s, FRAMESIZE_SVGA);
    else if (P1 == "XGA") s->set_framesize(s, FRAMESIZE_XGA);
    else if (P1 == "SXGA") s->set_framesize(s, FRAMESIZE_SXGA);
    else if (P1 == "UXGA") s->set_framesize(s, FRAMESIZE_UXGA);           
    else s->set_framesize(s, FRAMESIZE_QVGA);     
  }
  else if (cmd == "quality") { 
    sensor_t * s = esp_camera_sensor_get();
    s->set_quality(s, P1.toInt());
  }
  else if (cmd == "contrast") {
    sensor_t * s = esp_camera_sensor_get();
    s->set_contrast(s, P1.toInt());
  }
  else if (cmd == "brightness") {
    sensor_t * s = esp_camera_sensor_get();
    s->set_brightness(s, P1.toInt());  
  }
  else if (cmd == "serial") {
    Serial.println(P1); 
  }     
  else if (cmd == "detectCount") {
    Serial.println(P1 + " = " + P2); 
  }
  else if (cmd == "capture") {
    camera_fb_t * fb = esp_camera_fb_get();
    if(!fb) {
      Serial.println("Error en captura");
      client.println("HTTP/1.1 500 Internal Server Error");
      client.println("Content-Type: application/json");
      client.println("Connection: close");
      client.println();
      client.println("{\"success\":false,\"message\":\"Error en captura\"}");
    } else {
      String timestamp = String(millis());
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: image/jpeg");
      client.println("Content-Disposition: attachment; filename=capture_" + timestamp + ".jpg");
      client.println("Content-Length: " + String(fb->len));
      client.println("Connection: close");
      client.println();
      client.write(fb->buf, fb->len);
      esp_camera_fb_return(fb);
      Serial.println("Captura exitosa");
    }
  }
  else {
    Feedback = "Command is not defined.";
  }
  if (Feedback == "") Feedback = Command;  
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  pinMode(pulse, OUTPUT);
  digitalWrite(pulse, 0);
  Serial.begin(115200);
  Serial.setDebugOutput(true);  
  Serial.println();


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
    config.frame_size = FRAMESIZE_CIF;  // Mantener CIF para balance entre calidad y velocidad
    config.jpeg_quality = 12;           // Reducir calidad para aumentar velocidad (valor menor = mejor calidad)
    config.fb_count = 2;                // Usar 2 frame buffers para pipeline
    config.grab_mode = CAMERA_GRAB_LATEST; // Asegurar que siempre tomamos el frame más reciente
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

  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_CIF);
  s->set_quality(s, 12);               // Calidad más baja = más FPS
  s->set_brightness(s, 0);
  s->set_saturation(s, 0);
  s->set_contrast(s, 0);
  s->set_special_effect(s, 0);
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  s->set_wb_mode(s, 0);
  s->set_exposure_ctrl(s, 1);
  s->set_aec2(s, 0);
  s->set_gain_ctrl(s, 1);
  s->set_agc_gain(s, 0);
  s->set_gainceiling(s, GAINCEILING_2X);
  s->set_hmirror(s, 0);                // Deshabilitar espejo horizontal si no es necesario
  s->set_vflip(s, 0);                  // Deshabilitar volteo vertical si no es necesario
  s->set_dcw(s, 1);                    // Habilitar downsize
  s->set_colorbar(s, 0);               // Deshabilitar barra de colores

  ledcAttach(4, 5000, 8);  // pin, frecuencia, resolución

  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(true);  // Desconectar cualquier conexión previa
  delay(1000);

  // Intentar conectar al iPhone primero
  Serial.println("Intentando conectar a red iPhone...");
  
  // Para iPhone, usar DHCP en lugar de IP estática
  WiFi.begin(ssid_iphone, password_iphone);    

  Serial.print("Conectando a ");
  Serial.println(ssid_iphone);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {  // 10 segundos máximo
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFallo conexión a iPhone, intentando red de respaldo");
    
    // Limpiar configuración WiFi anterior
    WiFi.disconnect(true);
    delay(1000);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    delay(100);
    
    // Configurar IP estática para Megacable
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
      Serial.println("Error en configuración IP estática Megacable");
    }
    
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
    Serial.print("IP configurada: ");
    Serial.println(WiFi.localIP());
    Serial.print("Máscara de subred: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    
    // Parpadear LED para indicar conexión exitosa
    for (int i = 0; i < 5; i++) {   
      ledcWrite(4, 10);
      delay(200);
      ledcWrite(4, 0);
      delay(200);    
    }         
  } else {
    Serial.println("No se pudo conectar a ninguna red");
    ESP.restart();  // Reiniciar si no se pudo conectar
  }

  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);  
  server.begin();
  Serial.println("Servidor HTTP iniciado");

  // Generar ID único usando MAC address
  uint8_t mac[6];
  WiFi.macAddress(mac);
  deviceId = String(mac[0], HEX) + String(mac[1], HEX) + String(mac[2], HEX) +
             String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
  
  // Configurar MQTT
  mqttClient.setServer(mqtt_server, mqtt_port);
  connectMQTT();
  
  // Publicar la URL de acceso
  String accessUrl = "http://" + WiFi.localIP().toString();
  String mqttMessage = "{\"deviceId\":\"" + deviceId + "\",\"url\":\"" + accessUrl + "\"}";
  mqttClient.publish((mqtt_topic + deviceId + "/info").c_str(), mqttMessage.c_str());
}

void connectMQTT() {
    while (!mqttClient.connected()) {
        Serial.println("Conectando a MQTT...");
        if (mqttClient.connect(("ESP32CAM-" + deviceId).c_str())) {
            Serial.println("Conectado a MQTT");
            // Suscribirse a comandos
            mqttClient.subscribe((mqtt_topic + deviceId + "/cmd").c_str());
        } else {
            Serial.print("Fallo MQTT, rc=");
            Serial.println(mqttClient.state());
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
        String metadataTopic = mqtt_topic + deviceId + "/metadata";
        mqttClient.publish(metadataTopic.c_str(), metadata.c_str(), true);
        
        // Eliminar delay después de metadata
        
        for (int i = 0; i < chunks; i++) {
            size_t chunkLen = min(CHUNK_SIZE, fbLen - (i * CHUNK_SIZE));
            String frameTopic = mqtt_topic + deviceId + "/frame/" + String(i);
            mqttClient.beginPublish(frameTopic.c_str(), chunkLen, false);
            mqttClient.write(fbBuf + (i * CHUNK_SIZE), chunkLen);
            mqttClient.endPublish();
            // Eliminar delay entre chunks
        }
    }
}

void loop() {
    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.loop();

    static unsigned long lastFrame = 0;
    if (millis() - lastFrame > 33) { // Publicar cada ~33ms (30 FPS)
        camera_fb_t * fb = esp_camera_fb_get();
        if (fb) {
            publishFrame(fb);
            esp_camera_fb_return(fb);
            lastFrame = millis();
        }
    }

    Feedback = ""; Command = ""; cmd = ""; P1 = ""; P2 = ""; P3 = ""; P4 = ""; P5 = ""; P6 = ""; P7 = ""; P8 = ""; P9 = "";
    ReceiveState = 0; cmdState = 1; strState = 1; questionstate = 0; equalstate = 0; semicolonstate = 0;

    WiFiClient client = server.available();

    if (client) { 
        String currentLine = "";

        while (client.connected()) {
            if (client.available()) {
                char c = client.read();  
                getCommand(c);   

                if (c == '\n') {
                    if (currentLine.length() == 0) {    
                        if (cmd == "getstill") {
                            camera_fb_t * fb = esp_camera_fb_get();  
                            if(!fb) {
                                Serial.println("Camera capture failed");
                                client.println("HTTP/1.1 503 Service Unavailable");
                                client.println();
                                return;
                            }

                            // Publicar frame por MQTT además de enviarlo por HTTP
                            publishFrame(fb);

                            // Usar buffers más grandes para la transmisión
                            client.setNoDelay(true);
                            client.setTimeout(3000);
                            
                            client.println("HTTP/1.1 200 OK");
                            client.println("Access-Control-Allow-Origin: *");
                            client.println("Content-Type: image/jpeg");
                            client.println("Content-Length: " + String(fb->len));
                            client.println("Cache-Control: no-store, no-cache, must-revalidate, max-age=0");
                            client.println("Pragma: no-cache");
                            client.println("Connection: close");
                            client.println();
                            
                            // Enviar en bloques más grandes
                            uint8_t *fbBuf = fb->buf;
                            size_t fbLen = fb->len;
                            const size_t BLOCK_SIZE = 4096;
                            
                            while(fbLen > 0) {
                                size_t blockSize = (fbLen > BLOCK_SIZE) ? BLOCK_SIZE : fbLen;
                                client.write(fbBuf, blockSize);
                                fbBuf += blockSize;
                                fbLen -= blockSize;
                            }
                            
                            esp_camera_fb_return(fb);
                        }
                        else {
                            // Interfaz web principal
                            client.println("HTTP/1.1 200 OK");
                            client.println("Content-Type: text/html; charset=utf-8");
                            client.println("Connection: close");
                            client.println();
                            client.println(INDEX_HTML);
                        }
                        break;
                    } else {
                        currentLine = "";
                    }
                } 
                else if (c != '\r') {
                    currentLine += c;
                }

                if ((currentLine.indexOf("/?") != -1) && (currentLine.indexOf(" HTTP") != -1)) {
                    currentLine = "";
                    Feedback = "";
                    ExecuteCommand(client);
                }
            }
        }
        delay(1);
        client.stop();
    }
}

void getCommand(char c) {
    if (c == '?') ReceiveState = 1;
    if ((c == ' ') || (c == '\r') || (c == '\n')) ReceiveState = 0;

    if (ReceiveState == 1) {
        Command += String(c);

        if (c == '=') cmdState = 0;
        if (c == ';') strState++;

        if ((cmdState == 1) && (c != '?')) cmd += String(c);
        if ((cmdState == 0) && (strState == 1) && (c != '=')) P1 += String(c);
        if ((cmdState == 0) && (strState == 2) && (c != ';')) P2 += String(c);
        
        // Operador lógico corregido
        if(P2.toInt() < 1 || P2.toInt() == 0) digitalWrite(pulse, 0);
    }
}


