import { useEffect, useRef, useState } from "react";
import mqtt from "mqtt";

export default function CamViewer() {
  const canvasRef = useRef(null);
  const [status, setStatus] = useState("Conectando...");

  useEffect(() => {
    const canvas = canvasRef.current;
    const ctx = canvas.getContext("2d");

    // Configuración MQTT con los mismos parámetros que el HTML
    const mqttClient = mqtt.connect("ws://test.mosquitto.org:8080", {
      clientId: "viewer_" + Math.random().toString(16).substr(2, 8),
      protocol: "ws",
      keepalive: 60,
      connectTimeout: 4000,
      reconnectPeriod: 1000,
      clean: true,
      qos: 1,
    });

    let currentFrame = [];
    let expectedChunks = 0;
    let receivedChunks = 0;
    let deviceId = null;

    mqttClient.on("connect", () => {
      console.log("Conectado al broker MQTT");
      setStatus("Conectado al broker MQTT, esperando stream...");
      // Suscribirse a todos los tópicos relacionados con ESP32-CAM
      const topics = [
        "esp32cam/stream/#",
        "esp32/cam_frame",
        "esp32cam/#",
        "camera/#",
      ];

      topics.forEach((topic) => {
        mqttClient.subscribe(topic, (err) => {
          if (err) {
            console.error(`Error al suscribirse a ${topic}:`, err);
          } else {
            console.log(`Suscrito exitosamente a: ${topic}`);
          }
        });
      });
    });

    mqttClient.on("message", (topic, message) => {
      console.log(`Mensaje recibido en tópico: ${topic}`);
      console.log(`Tamaño del mensaje: ${message.length} bytes`);
      console.log(`Tipo de mensaje:`, typeof message);

      try {
        if (topic.endsWith("/metadata")) {
          const metadata = JSON.parse(message.toString());
          console.log("Metadata recibida:", metadata);
          deviceId = metadata.deviceId;
          expectedChunks = metadata.chunks;
          currentFrame = new Array(expectedChunks).fill(null);
          receivedChunks = 0;
          setStatus(`Recibiendo frame (0/${expectedChunks} chunks)`);
        } else if (topic.includes("/frame/")) {
          const frameIndex = parseInt(topic.split("/").pop());
          console.log(`Recibiendo chunk ${frameIndex}`);
          currentFrame[frameIndex] = message;
          receivedChunks++;
          setStatus(
            `Recibiendo frame (${receivedChunks}/${expectedChunks} chunks)`
          );

          if (receivedChunks === expectedChunks) {
            console.log("Todos los chunks recibidos, procesando frame...");
            try {
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

              console.log("Frame completo creado, tamaño:", fullFrame.length);
              const blob = new Blob([fullFrame], { type: "image/jpeg" });
              const url = URL.createObjectURL(blob);
              const img = new Image();

              img.onload = () => {
                console.log(`Imagen cargada: ${img.width}x${img.height}`);
                canvas.width = img.width;
                canvas.height = img.height;
                ctx.drawImage(img, 0, 0);
                URL.revokeObjectURL(url);
                setStatus("Frame mostrado correctamente");
              };

              img.onerror = (err) => {
                console.error("Error al cargar la imagen:", err);
                setStatus("Error al mostrar el frame");
              };

              console.log("Intentando cargar la imagen desde:", url);
              img.src = url;
            } catch (err) {
              console.error("Error procesando frame:", err);
              setStatus("Error procesando frame: " + err.message);
            }
          }
        } else {
          // Intentar procesar como frame completo
          console.log("Intentando procesar como frame completo");
          try {
            const blob = new Blob([message], { type: "image/jpeg" });
            const url = URL.createObjectURL(blob);
            const img = new Image();

            img.onload = () => {
              console.log(`Imagen simple cargada: ${img.width}x${img.height}`);
              canvas.width = img.width;
              canvas.height = img.height;
              ctx.drawImage(img, 0, 0);
              URL.revokeObjectURL(url);
              setStatus("Frame simple mostrado correctamente");
            };

            img.onerror = (err) => {
              console.error("Error al cargar imagen simple:", err);
              setStatus("Error al mostrar frame simple");
            };

            img.src = url;
          } catch (err) {
            console.error("Error procesando frame simple:", err);
          }
        }
      } catch (error) {
        console.error("Error procesando mensaje:", error);
        setStatus("Error: " + error.message);
      }
    });

    mqttClient.on("close", () => {
      console.log("Desconectado del broker MQTT");
      setStatus("Desconectado del broker MQTT");
    });

    mqttClient.on("error", (error) => {
      console.error("Error MQTT:", error);
      setStatus(`Error: ${error.message}`);
    });

    mqttClient.on("offline", () => {
      console.log("Cliente MQTT desconectado, intentando reconectar...");
      setStatus("Reconectando...");
    });

    mqttClient.on("reconnect", () => {
      console.log("Intentando reconexión MQTT...");
      setStatus("Intentando reconexión...");
    });

    return () => {
      mqttClient.end();
    };
  }, []);

  return (
    <div className="video-container">
      <canvas
        ref={canvasRef}
        className="video-canvas"
        style={{
          width: "100%",
          maxWidth: "400px",
          border: "1px solid #ccc",
          margin: "0 auto",
          display: "block",
        }}
      />
      <p
        style={{
          textAlign: "center",
          color: "#666",
          margin: "10px 0",
        }}
      >
        {status}
      </p>
    </div>
  );
}
