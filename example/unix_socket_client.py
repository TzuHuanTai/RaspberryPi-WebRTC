import asyncio
import socket
import os

class AsyncUnixSocketClient:
    def __init__(self, socket_path):
        self.socket_path = socket_path
        self.reader = None
        self.writer = None

    async def connect(self):
        try:
            self.reader, self.writer = await asyncio.open_unix_connection(self.socket_path)
            print(f"Connected to {self.socket_path}")
        except Exception as e:
            print(f"Connection failed: {e}")

    async def send(self, message: str):
        if self.writer is not None:
            self.writer.write(message.encode())
            await self.writer.drain()
            print(f"Sent: {message}")

    async def receive_loop(self):
        try:
            while True:
                data = await self.reader.read(1024)
                if not data:
                    print("Disconnected from server")
                    break
                print(f"Received: {data.decode()}")
        except Exception as e:
            print(f"Error in receive loop: {e}")

    async def run(self):
        await self.connect()
        if self.reader and self.writer:
            asyncio.create_task(self.receive_loop())
            while True:
                await self.send("ping from client")
                await asyncio.sleep(2)

if __name__ == "__main__":
    SOCKET_PATH = "/tmp/pi-webrtc-ipc.sock"
    client = AsyncUnixSocketClient(SOCKET_PATH)
    asyncio.run(client.run())
