
import pyaudio
import numpy as np
import asyncio
import websockets
import json
import time

import matplotlib.pyplot as plt

ws = 'ws://192.168.0.17:1234/'

DTYPE = np.int16
MAX_INT = 32768.0
CHANNELS = 2
RATE = 44100
CHUNK = 4096

async def clear():
    async with websockets.connect(ws) as websocket:
        json_data = json.dumps([{"clear": 1}])
        await websocket.send(json_data)

async def send_data(filter=False):
    color = [0, 200, 0]
    async with websockets.connect(ws) as websocket:
        while True:
            audio_data = np.frombuffer(stream.read(CHUNK), dtype=DTYPE)
            norm = audio_data / MAX_INT
            fourier = np.abs(np.fft.rfft(norm))
            fourier = np.delete(fourier, len(fourier)-1)[:320]
            matrix = np.reshape(fourier, (32, len(fourier)//32))
            matrix = np.int_(np.divide(np.multiply(np.mean(matrix, axis=1), 8), 150))
            cmd = [{"clear": 1}]
            for x, y in enumerate(matrix):
                for _y in range(y):
                    cmd.append({"pos": [x, 8 - int(_y)], "color": color})
            json_data = json.dumps(cmd)
            await websocket.send(json_data)
            try:
                await websocket.recv()
            except:
                websocket = await websockets.connect(ws)


p=pyaudio.PyAudio() # start the PyAudio class
stream=p.open(format=pyaudio.paInt16,channels=CHANNELS,rate=RATE,input=True,
              frames_per_buffer=CHUNK) #uses default input device

try:
    asyncio.get_event_loop().run_until_complete(send_data(True))
except:
    asyncio.get_event_loop().run_until_complete(clear())

# close the stream gracefully
stream.stop_stream()
stream.close()
p.terminate()