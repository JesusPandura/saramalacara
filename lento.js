lento  

<!DOCTYPE html>
<html>
  <head>
    <title>ESP32-CAM Remote Viewer</title>
    <meta
      name="viewport"
      content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no"
    />
    <meta name="apple-mobile-web-app-capable" content="yes" />
    <meta
      name="apple-mobile-web-app-status-bar-style"
      content="black-translucent"
    />
    <script src="https://cdnjs.cloudflare.com/ajax/libs/mqtt/4.3.7/mqtt.min.js"></script>
    <script src="https://webrtc.github.io/adapter/adapter-latest.js"></script>
    <style>
      html,
      body {
        height: 100%;
        width: 100%;
        margin: 0;
        padding: 0;
        -webkit-overflow-scrolling: touch;
      }

      #videoContainer {
        position: relative;
        max-width: 100%;
        margin: 0 auto;
        padding: 10px;
        box-sizing: border-box;
        overflow: hidden;
      }

      #status {
        position: fixed;
        bottom: 20px;
        left: 0;
        right: 0;
        padding: 10px;
        background: rgba(0, 0, 0, 0.7);
        color: white;
        text-align: center;
        z-index: 1000;
        font-family: -apple-system, system-ui, BlinkMacSystemFont, "Segoe UI",
          Roboto, "Helvetica Neue", Arial, sans-serif;
      }

      canvas {
        width: 100%;
        height: auto;
        display: block;
        border: none;
      }

      * {
        -webkit-tap-highlight-color: transparent;
        -webkit-touch-callout: none;
      }

      #detectionResults {
        position: fixed;
        top: 20px;
        left: 20px;
        background: rgba(0, 0, 0, 0.7);
        color: white;
        padding: 10px;
        border-radius: 5px;
        font-family: -apple-system, system-ui, BlinkMacSystemFont, "Segoe UI",
          Roboto, "Helvetica Neue", Arial, sans-serif;
        z-index: 1000;
      }

      .detection-box {
        position: absolute;
        border: 5px solid red;
        pointer-events: none;
        box-shadow: 0 0 10px rgba(255, 0, 0, 1);
        z-index: 1000;
        background-color: rgba(255, 0, 0, 0.3);
      }

      .detection-label {
        position: absolute;
        background: black;
        color: white;
        padding: 5px 10px;
        font-size: 16px;
        font-weight: bold;
        border-radius: 4px;
        pointer-events: none;
        z-index: 1001;
        white-space: nowrap;
        box-shadow: 0 0 5px black;
        border: 1px solid red;
      }

      #videoStream {
        width: 100%;
        height: auto;
        display: none;
      }

      #rtcVideo {
        width: 100%;
        height: auto;
        display: block;
      }
    </style>
  </head>
  <body>
    <div id="videoContainer">
      <canvas id="videoCanvas" style="display: none"></canvas>
      <canvas id="videoStream" style="display: none"></canvas>
      <video id="rtcVideo" autoplay playsinline></video>
      <div id="detectionResults"></div>
      <p id="status">Conectando...</p>
    </div>

    <button
      id="debugButton"
      style="
        position: fixed;
        bottom: 60px;
        right: 10px;
        z-index: 2000;
        padding: 10px;
        background: red;
        color: white;
        border: none;
        border-radius: 5px;
      "
    >
      Debug
    </button>

    <script>
      const isIOS =
        /iPad|iPhone|iPod/.test(navigator.userAgent) && !window.MSStream;

      const mqttConfig = {
        clientId: "viewer_" + Math.random().toString(16).substr(2, 8),
        protocol: "wss",
        keepalive: isIOS ? 20 : 30,
        connectTimeout: isIOS ? 15000 : 10000,
        reconnectPeriod: 2000,
        clean: true,
        qos: 0,
        rejectUnauthorized: false,
        will: {
          topic: "esp32cam/stream/status",
          payload: "Viewer disconnected",
          qos: 0,
          retain: false,
        },
      };

      function connectMQTT() {
        try {
          return mqtt.connect("wss://test.mosquitto.org:8081", mqttConfig);
        } catch (e) {
          console.error("Error al conectar:", e);
          status.textContent = "Error de conexión. Reintentando...";
          return null;
        }
      }

      let client = connectMQTT();
      const canvas = document.getElementById("videoCanvas");
      const ctx = canvas.getContext("2d");
      const status = document.getElementById("status");

      let currentFrame = [];
      let expectedChunks = 0;
      let receivedChunks = 0;
      let deviceId = null;
      let reconnectAttempts = 0;
      const MAX_RECONNECT_ATTEMPTS = 3;

      let connectionTimeout = setTimeout(
        () => {
          if (!client?.connected) {
            handleConnectionFailure();
          }
        },
        isIOS ? 20000 : 15000
      );

      function handleConnectionFailure() {
        if (reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
          status.textContent = "Reintentando conexión...";
          reconnectAttempts++;

          if (client) {
            client.end(true);
          }

          setTimeout(() => {
            client = connectMQTT();
            setupEventListeners();
          }, 2000);
        } else {
          status.textContent =
            "No se pudo establecer conexión. Por favor, recarga la página.";
        }
      }

      // Configuración WebRTC
      const mediaStreamConstraints = {
        video: {
          width: { ideal: 640 },
          height: { ideal: 480 },
          frameRate: { ideal: 30 },
        },
      };

      let mediaRecorder;
      let recordedChunks = [];
      let videoBuffer;
      let sourceBuffer;
      let mediaSource;
      let rtcVideo;

      async function setupVideoBuffer() {
        rtcVideo = document.getElementById("rtcVideo");
        mediaSource = new MediaSource();
        rtcVideo.src = URL.createObjectURL(mediaSource);

        return new Promise((resolve) => {
          mediaSource.addEventListener("sourceopen", () => {
            const mimeType = "video/webm;codecs=vp8";
            sourceBuffer = mediaSource.addSourceBuffer(mimeType);
            sourceBuffer.mode = "sequence";
            resolve();
          });
        });
      }

      function setupEventListeners() {
        if (!client) return;

        client.on("connect", async () => {
          clearTimeout(connectionTimeout);
          reconnectAttempts = 0;
          console.log("Conectado al broker MQTT");
          status.textContent = "Inicializando buffer de video...";

          // Inicializar buffer de video
          await setupVideoBuffer();
          status.textContent = "Conectado al broker MQTT, esperando stream...";

          const topics = [
            "esp32cam/stream/#",
            "esp32cam/+/metadata",
            "esp32cam/+/frame/#",
          ];

          topics.forEach((topic) => {
            client.subscribe(topic, { qos: 0 }, (err) => {
              if (err) {
                console.error("Error al suscribirse a " + topic + ":", err);
                status.textContent = "Error al suscribirse: " + err.message;
              }
            });
          });
        });

        client.on("close", () => {
          console.log("Desconectado del broker MQTT");
          status.textContent = "Desconectado del broker MQTT. Reconectando...";
        });

        client.on("message", async (topic, message) => {
          console.log(`Mensaje en ${topic} (${message.length} bytes)`);

          try {
            if (topic.endsWith("/metadata")) {
              const metadata = JSON.parse(message.toString());
              console.log("Metadata recibida:", metadata);
              deviceId = metadata.deviceId;
              expectedChunks = metadata.chunks;
              currentFrame = new Array(expectedChunks).fill(null);
              receivedChunks = 0;
              status.textContent = `Recibiendo frame (0/${expectedChunks} chunks)`;
            } else if (topic.includes("/frame/")) {
              const frameIndex = parseInt(topic.split("/").pop());
              currentFrame[frameIndex] = message;
              receivedChunks++;
              status.textContent = `Recibiendo frame (${receivedChunks}/${expectedChunks} chunks)`;

              if (receivedChunks === expectedChunks) {
                try {
                  // Clear previous detections
                  clearDetections();

                  const fullFrame = new Uint8Array(
                    currentFrame.reduce((acc, chunk) => acc + chunk.length, 0)
                  );
                  let offset = 0;
                  currentFrame.forEach((chunk) => {
                    if (chunk) {
                      fullFrame.set(chunk, offset);
                      offset += chunk.length;
                    }
                  });

                  // Actualizar el stream de video
                  await updateVideoStream(fullFrame);

                  // Procesar detección de objetos
                  const blob = new Blob([fullFrame], { type: "image/jpeg" });
                  const detectionResult = await detectObjects(blob);
                  if (detectionResult && detectionResult.status === "success") {
                    drawDetections(detectionResult.detections);
                    status.textContent = "Detección completada";
                  } else {
                    status.textContent = "Error en la detección";
                  }
                } catch (err) {
                  console.error("Error procesando frame:", err);
                  status.textContent = "Error procesando frame";
                }
              }
            }
          } catch (error) {
            console.error("Error procesando mensaje:", error);
          }
        });

        client.on("error", (error) => {
          console.error("Error MQTT:", error);
          status.textContent = `Error de conexión: ${error.message}. Reintentando...`;
          setTimeout(() => {
            if (!client.connected) {
              location.reload();
            }
          }, 5000);
        });

        client.on("offline", () => {
          console.log("Conexión perdida, intentando reconectar...");
          status.textContent = "Conexión perdida. Reconectando...";
        });

        client.on("reconnect", () => {
          console.log("Intentando reconexión MQTT...");
          status.textContent = "Reconectando al servidor...";
        });
      }

      async function detectObjects(imageBlob) {
        const formData = new FormData();
        console.log("Original image size:", imageBlob.size, "bytes");

        try {
          formData.append("image", imageBlob);
          console.log("Sending detection request to server...");

          const response = await fetch(
            "https://concrete-monkey-selected.ngrok-free.app/detect",
            {
              method: "POST",
              body: formData,
              headers: {
                Accept: "application/json",
              },
            }
          );

          if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
          }

          const result = await response.json();
          console.log("Detection result:", result);

          if (result.detections && result.detections.length > 0) {
            console.log(`Found ${result.detections.length} objects:`);
            result.detections.forEach((det, i) => {
              console.log(
                `  ${i + 1}. ${det.class} (${det.confidence.toFixed(
                  2
                )}) at [${det.bbox.join(", ")}]`
              );
            });
          } else {
            console.log("No objects detected");
          }

          return result;
        } catch (error) {
          console.error("Error en la detección:", error);
          console.error("Error details:", {
            message: error.message,
            stack: error.stack,
            type: error.name,
          });
          return null;
        }
      }

      function clearDetections() {
        const container = document.getElementById("videoContainer");
        // Remove all existing detection boxes and labels from the container
        const boxes = container.querySelectorAll(".detection-box");
        const labels = container.querySelectorAll(".detection-label");

        boxes.forEach((box) => box.remove());
        labels.forEach((label) => label.remove());
      }

      function drawDetections(detections) {
        clearDetections();

        if (!detections || detections.length === 0) {
          return;
        }

        const rect = rtcVideo.getBoundingClientRect();
        const scaleX = rect.width / rtcVideo.videoWidth;
        const scaleY = rect.height / rtcVideo.videoHeight;

        detections.forEach((detection) => {
          const [x1, y1, x2, y2] = detection.bbox;
          const confidence = Math.round(detection.confidence * 100);

          const scaledX1 = x1 * scaleX;
          const scaledY1 = y1 * scaleY;
          const scaledX2 = x2 * scaleX;
          const scaledY2 = y2 * scaleY;

          const box = document.createElement("div");
          box.className = "detection-box";
          box.style.left = `${scaledX1}px`;
          box.style.top = `${scaledY1}px`;
          box.style.width = `${scaledX2 - scaledX1}px`;
          box.style.height = `${scaledY2 - scaledY1}px`;

          const label = document.createElement("div");
          label.className = "detection-label";
          label.style.left = `${scaledX1}px`;
          label.style.top = `${scaledY1 - 25}px`;
          label.textContent = `${detection.class} ${confidence}%`;

          document.getElementById("videoContainer").appendChild(box);
          document.getElementById("videoContainer").appendChild(label);
        });
      }

      const videoCanvas = document.getElementById("videoCanvas");
      const videoStream = document.getElementById("videoStream");
      const streamCtx = videoStream.getContext("2d");

      // Modificar la función updateVideoStream
      function updateVideoStream(imageData) {
        return new Promise(async (resolve, reject) => {
          try {
            const img = new Image();
            img.onload = async () => {
              // Ajustar el tamaño del canvas al tamaño de la imagen
              if (videoStream.width !== img.width) {
                videoStream.width = img.width;
                videoStream.height = img.height;

                // Inicializar MediaRecorder si no existe
                if (!mediaRecorder) {
                  const stream = videoStream.captureStream(30); // 30 FPS
                  mediaRecorder = new MediaRecorder(stream, {
                    mimeType: "video/webm;codecs=vp8",
                    videoBitsPerSecond: 2500000, // 2.5 Mbps
                  });

                  mediaRecorder.ondataavailable = async (event) => {
                    if (event.data.size > 0) {
                      recordedChunks.push(event.data);

                      // Agregar chunks al sourceBuffer cuando tengamos suficientes
                      if (recordedChunks.length >= 5) {
                        // Buffer de 5 frames
                        const blob = new Blob(recordedChunks, {
                          type: "video/webm",
                        });
                        const arrayBuffer = await blob.arrayBuffer();

                        try {
                          if (
                            !sourceBuffer.updating &&
                            mediaSource.readyState === "open"
                          ) {
                            sourceBuffer.appendBuffer(arrayBuffer);
                            recordedChunks = []; // Limpiar buffer
                          }
                        } catch (e) {
                          console.warn("Error adding buffer:", e);
                        }
                      }
                    }
                  };

                  mediaRecorder.start(1000 / 30); // Capturar cada frame (30fps)
                }
              }

              // Dibujar la imagen en el canvas
              streamCtx.drawImage(img, 0, 0);
              URL.revokeObjectURL(img.src);
              resolve();
            };

            img.onerror = (err) => {
              console.error("Error loading image:", err);
              reject(err);
            };

            const blob = new Blob([imageData], { type: "image/jpeg" });
            img.src = URL.createObjectURL(blob);
          } catch (error) {
            console.error("Error in updateVideoStream:", error);
            reject(error);
          }
        });
      }

      setupEventListeners();

      document.addEventListener("visibilitychange", () => {
        if (document.hidden) {
          if (client?.connected) {
            client.end();
          }
        } else {
          client = connectMQTT();
          setupEventListeners();
        }
      });

      document.addEventListener("gesturestart", (e) => {
        e.preventDefault();
      });

      // Add window resize handler to update bounding boxes
      let resizeTimeout;
      window.addEventListener("resize", () => {
        clearTimeout(resizeTimeout);
        resizeTimeout = setTimeout(() => {
          const lastDetections =
            document.querySelector("#videoContainer").__lastDetections;
          if (lastDetections) {
            clearDetections();
            drawDetections(lastDetections);
          }
        }, 250);
      });

      document
        .getElementById("debugButton")
        .addEventListener("click", function () {
          // Crear una detección de prueba
          const testDetections = [
            {
              class: "test",
              confidence: 0.95,
              bbox: [50, 50, 200, 200],
            },
          ];

          // Dibujar la detección de prueba
          drawDetections(testDetections);

          // Mostrar información de depuración
          const canvas = document.getElementById("videoCanvas");
          const rect = canvas.getBoundingClientRect();

          alert(`Debug Info:
        - Canvas size: ${canvas.width}x${canvas.height}
        - Display size: ${rect.width}x${rect.height}
        - Position: ${rect.left},${rect.top}
        - Detections drawn: ${testDetections.length}
        - URL: ${window.location.href}
        - User Agent: ${navigator.userAgent}`);
        });
    </script>
  </body>
</html>
