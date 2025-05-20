from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.staticfiles import StaticFiles
import uvicorn
import json
import asyncio
import socket
import os
import cv2
import numpy as np
from datetime import datetime, timedelta
import b2sdk.v2

def get_local_ip():
    try:
        # Crea un socket UDP
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # No necesitamos realmente conectarnos, pero esto nos ayuda a determinar la IP local
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"

app = FastAPI()

# Store connected clients
connected_clients = {}

# Variables para grabación de video
RECORDINGS_DIR = 'recordings'
FRAME_WIDTH = 640  # Ajusta según tu cámara
FRAME_HEIGHT = 480  # Ajusta según tu cámara
FPS = 15  # Ajusta según tu cámara
RECORD_INTERVAL = 5 * 60  # 5 minutos en segundos

# Credenciales de Backblaze B2 (puedes moverlas a variables de entorno para mayor seguridad)
B2_ACCOUNT_ID = "343ca4974772"
B2_APPLICATION_KEY = "005ee92e8483c9d39830366544e0d47e90f22bb957"
B2_BUCKET_NAME = "videos-escuela"

# === NUEVO: Variables de entorno para URLs ===
BROWSER_URL = os.environ.get("BROWSER_URL", "https://10.250.12.40")
ESP32_URL = os.environ.get("ESP32_URL", "wss://10.250.12.40/ws/esp32")
# ============================================

def get_today_dir():
    today_str = datetime.now().strftime("%Y%m%d")
    today_dir = os.path.join(RECORDINGS_DIR, today_str)
    if not os.path.exists(today_dir):
        os.makedirs(today_dir)
    return today_dir

if not os.path.exists(RECORDINGS_DIR):
    os.makedirs(RECORDINGS_DIR)

video_writer = None
frames_buffer = []
recording_start_time = None

def upload_to_b2(filepath, filename):
    try:
        info = b2sdk.v2.InMemoryAccountInfo()
        b2_api = b2sdk.v2.B2Api(info)
        b2_api.authorize_account("production", B2_ACCOUNT_ID, B2_APPLICATION_KEY)
        bucket = b2_api.get_bucket_by_name(B2_BUCKET_NAME)
        with open(filepath, "rb") as f:
            bucket.upload_bytes(f.read(), filename)
        print(f"Archivo subido a Backblaze B2: {filename}")
    except Exception as e:
        print(f"Error subiendo a Backblaze B2: {e}")

@app.websocket("/ws/{client_id}")
async def websocket_endpoint(websocket: WebSocket, client_id: str):
    global video_writer, frames_buffer, recording_start_time
    print(f"Nuevo cliente conectado: {client_id}")
    await websocket.accept()
    connected_clients[client_id] = websocket
    
    try:
        while True:
            # Recibir mensajes (texto o binario)
            message = await websocket.receive()
            
            # Determinar si el mensaje es de texto o binario
            if "text" in message:
                # Mensaje de texto (señalización)
                try:
                    data = message["text"]
                    parsed_data = json.loads(data)
                    print(f"Mensaje recibido de {client_id}: {parsed_data}")
                    
                    # Forward the message to the target client
                    target = parsed_data.get("target")
                    if target in connected_clients:
                        print(f"Reenviando mensaje a {target}")
                        target_ws = connected_clients[target]
                        await target_ws.send_text(data)
                    else:
                        print(f"Cliente objetivo {target} no encontrado")
                except json.JSONDecodeError:
                    print(f"Error al decodificar JSON de {client_id}")
            
            elif "bytes" in message:
                # Mensaje binario (frames de video)
                if client_id == "esp32":
                    binary_data = message["bytes"]
                    print(f"Recibido frame de video de {client_id}: {len(binary_data)} bytes")

                    # --- GRABACIÓN DE VIDEO ---
                    # Decodificar el frame JPEG a imagen
                    np_arr = np.frombuffer(binary_data, np.uint8)
                    frame = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
                    if frame is not None:
                        # Redimensionar si es necesario
                        frame = cv2.resize(frame, (FRAME_WIDTH, FRAME_HEIGHT))
                        if recording_start_time is None:
                            recording_start_time = datetime.now()
                            # Nombre de archivo con timestamp
                            filename = datetime.now().strftime("%Y%m%d_%H%M%S") + ".avi"
                            # Carpeta del día actual
                            today_dir = get_today_dir()
                            filepath = os.path.join(today_dir, filename)
                            # Inicializar VideoWriter
                            fourcc = cv2.VideoWriter_fourcc(*'XVID')
                            video_writer = cv2.VideoWriter(filepath, fourcc, FPS, (FRAME_WIDTH, FRAME_HEIGHT))
                            print(f"Iniciando grabación: {filepath}")
                        # Escribir frame
                        video_writer.write(frame)
                        # Verificar si pasaron 5 minutos
                        if (datetime.now() - recording_start_time).total_seconds() >= RECORD_INTERVAL:
                            video_writer.release()
                            print(f"Video guardado: {filepath}")
                            # Subir a Backblaze B2
                            try:
                                upload_to_b2(filepath, filename)
                            except Exception as e:
                                print(f"Error al subir el video a B2: {e}")
                            recording_start_time = None
                            video_writer = None
                    else:
                        print("Error al decodificar el frame JPEG")
                    # --- FIN GRABACIÓN DE VIDEO ---
                    # Reenviar a todos los clientes web (excluir ESP32)
                    for cid, client_ws in connected_clients.items():
                        if cid != "esp32" and cid.startswith("web_"):
                            try:
                                await client_ws.send_bytes(binary_data)
                            except Exception as e:
                                print(f"Error enviando frame a {cid}: {e}")
                else:
                    print(f"Ignorando mensaje binario de cliente no-ESP32: {client_id}")
    
    except WebSocketDisconnect:
        print(f"Cliente {client_id} desconectado")
    except Exception as e:
        print(f"Error en cliente {client_id}: {e}")
    finally:
        if client_id in connected_clients:
            del connected_clients[client_id]
            print(f"Cliente {client_id} desconectado")
        if client_id == "esp32" and video_writer is not None:
            video_writer.release()
            print("Grabación finalizada por desconexión del ESP32.")
            # Subir a Backblaze B2 si hay un archivo pendiente
            try:
                if 'filepath' in locals() and 'filename' in locals():
                    upload_to_b2(filepath, filename)
            except Exception as e:
                print(f"Error al subir el video a B2: {e}")
            video_writer = None
            recording_start_time = None

# Mount static files
app.mount("/", StaticFiles(directory="static", html=True), name="static")

if __name__ == "__main__":
    local_ip = get_local_ip()
    port = int(os.environ.get("PORT", 8000))  # Usa el puerto de Railway, o 8000 por defecto
    # === NUEVO: URLs dinámicos ===
    browser_url = os.environ.get("BROWSER_URL", f"http://{local_ip}:{port}")
    esp32_url = os.environ.get("ESP32_URL", f"ws://{local_ip}:{port}/ws/esp32")
    print(f"\n=== Servidor de señalización iniciado ===")
    print(f"IP Local: {local_ip}")
    print(f"URL para navegador: {browser_url}")
    print(f"URL para ESP32: {esp32_url}")
    print("=====================================\n")
    uvicorn.run(app, host="0.0.0.0", port=port) 