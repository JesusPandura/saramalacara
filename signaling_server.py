from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.staticfiles import StaticFiles
import uvicorn
import json
import asyncio
import socket

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

@app.websocket("/ws/{client_id}")
async def websocket_endpoint(websocket: WebSocket, client_id: str):
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
                    # Reenviar frames de video a todos los clientes web
                    binary_data = message["bytes"]
                    print(f"Recibido frame de video de {client_id}: {len(binary_data)} bytes")
                    
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

# Mount static files
app.mount("/", StaticFiles(directory="static", html=True), name="static")

if __name__ == "__main__":
    local_ip = get_local_ip()
    print(f"\n=== Servidor de señalización iniciado ===")
    print(f"IP Local: {local_ip}")
    print(f"URL para navegador: https://clever-koi-freely.ngrok-free.app")
    print(f"URL para ESP32: wss://clever-koi-freely.ngrok-free.app/ws/esp32")
    print("=====================================\n")
    uvicorn.run(app, host="0.0.0.0", port=8000, ssl_keyfile=None, ssl_certfile=None) 