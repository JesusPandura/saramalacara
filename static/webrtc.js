const video = document.getElementById("video");
let peerConnection;
let ws;

// Configuración de WebRTC
const configuration = {
  iceServers: [{ urls: "stun:stun.l.google.com:19302" }],
};

// Conectar al WebSocket del servidor de señalización
function connect() {
  const clientId = "web_" + Math.random().toString(36).substr(2, 9);
  console.log("Conectando con ID:", clientId);

  ws = new WebSocket(`ws://${window.location.hostname}:8000/ws/${clientId}`);

  ws.onopen = async () => {
    console.log("WebSocket conectado");
    await startWebRTC();
  };

  ws.onmessage = async (e) => {
    console.log("Mensaje recibido:", e.data);
    try {
      const message = JSON.parse(e.data);
      await handleSignalingMessage(message);
    } catch (error) {
      console.error("Error procesando mensaje:", error);
    }
  };

  ws.onerror = (error) => {
    console.error("Error WebSocket:", error);
  };

  ws.onclose = () => {
    console.log("WebSocket desconectado");
    setTimeout(connect, 1000);
  };
}

async function startWebRTC() {
  console.log("Iniciando WebRTC");

  // Crear nueva conexión
  peerConnection = new RTCPeerConnection(configuration);

  // Crear un MediaStream vacío para recibir el video
  const stream = new MediaStream();
  video.srcObject = stream;

  // Manejar la llegada de tracks remotos
  peerConnection.ontrack = (event) => {
    console.log("Stream remoto recibido", event);
    const [remoteStream] = event.streams;
    if (remoteStream) {
      video.srcObject = remoteStream;
    }
  };

  // Manejar candidatos ICE
  peerConnection.onicecandidate = (event) => {
    if (event.candidate) {
      console.log("Nuevo candidato ICE:", event.candidate);
      ws.send(
        JSON.stringify({
          type: "candidate",
          target: "esp32",
          candidate: event.candidate,
        })
      );
    }
  };

  // Manejar cambios de estado de conexión
  peerConnection.oniceconnectionstatechange = () => {
    console.log("Estado ICE:", peerConnection.iceConnectionState);
  };

  // Crear y enviar oferta
  try {
    const offer = await peerConnection.createOffer({
      offerToReceiveVideo: true,
      offerToReceiveAudio: false,
    });

    console.log("Oferta creada:", offer);
    await peerConnection.setLocalDescription(offer);
    console.log("Descripción local establecida");

    ws.send(
      JSON.stringify({
        type: "offer",
        target: "esp32",
        sdp: offer,
      })
    );
  } catch (error) {
    console.error("Error creando oferta:", error);
  }
}

async function handleSignalingMessage(message) {
  console.log("Procesando mensaje de señalización:", message);

  try {
    switch (message.type) {
      case "answer":
        console.log("Recibida respuesta SDP:", message.sdp);
        await peerConnection.setRemoteDescription(
          new RTCSessionDescription(message.sdp)
        );
        console.log("Descripción remota establecida");
        break;

      case "candidate":
        if (message.candidate) {
          console.log("Agregando candidato ICE:", message.candidate);
          await peerConnection.addIceCandidate(
            new RTCIceCandidate(message.candidate)
          );
        }
        break;

      default:
        console.log("Mensaje no manejado:", message.type);
    }
  } catch (error) {
    console.error("Error procesando mensaje de señalización:", error);
  }
}

// Iniciar conexión cuando la página cargue
window.addEventListener("load", () => {
  console.log("Página cargada, iniciando conexión...");
  connect();
});
