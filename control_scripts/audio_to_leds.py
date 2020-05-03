

import numpy as np
import asyncio
import websockets
import json
import time
import sys

import matplotlib.pyplot as plt

ws = 'ws://192.168.0.17:1234/'

DTYPE = np.int16
MAX_INT = 32768.0
CHANNELS = 1
RATE = 48000
CHUNK = 1024

async def clear():
    async with websockets.connect(ws, ping_interval=None) as websocket:
        json_data = json.dumps([{"clear": 1}])
        await websocket.send(json_data)

async def send_data(filter=False):
    color = [200, 200, 0]
    async with websockets.connect(ws, ping_interval=None, max_queue=512) as websocket:
        while True:
            if rasp == False:
                audio_data = np.frombuffer(stream.read(CHUNK), dtype=DTYPE) 
            else:
                l, audio_data = stream.read()
                stream.pause(1)
                audio_data = unpack("%dh" % (len(audio_data)/2), audio_data)
                audio_data = np.array(audio_data, dtype=DTYPE)
            norm = (audio_data - np.min(audio_data)) / (np.max(audio_data) - np.min(audio_data))
            fourier = np.abs(np.fft.rfft(norm))
            fourier = fourier[:64]
            matrix = np.reshape(fourier, (32, len(fourier)//32))
            matrix = np.int_(np.divide(np.multiply(np.mean(matrix, axis=1), 8), 150))
            cmd = [{"clear": 1}]
            for x, y in enumerate(matrix):
                for _y in range(y):
                    cmd.append({"pos": [x, 7 - int(_y)], "color": [color[0]*int(_y)/8, color[1]*(8 - int(_y))/8, 0]})
            json_data = json.dumps(cmd)
            await websocket.send(json_data)
            try:
                await websocket.recv()
            except:
                websocket = await websockets.connect(ws, ping_interval=None, max_queue=512)
            if rasp:
                stream.pause(0)
rasp = False
stream = None
if __name__ == "__main__":
    if "rasp" in sys.argv:
        rasp = True
    if rasp == False:
        import pyaudio
        p=pyaudio.PyAudio() # start the PyAudio class
        stream=p.open(format=pyaudio.paInt16,channels=CHANNELS,rate=RATE,input=True,
                      input_device_index=1, frames_per_buffer=CHUNK) #uses default input device
    else:
        import alsaaudio as aa
        from struct import unpack
        stream = aa.PCM(aa.PCM_CAPTURE, aa.PCM_NORMAL)
        stream.setchannels(CHANNELS)
        stream.setrate(RATE)
        stream.setformat(aa.PCM_FORMAT_S16_LE)
        stream.setperiodsize(CHUNK)
    #try:
    asyncio.get_event_loop().run_until_complete(send_data(True))
    #except:
    asyncio.get_event_loop().run_until_complete(clear())

    # close the stream gracefully
    if rasp == None:
        stream.stop_stream()
    stream.close()
    if rasp == None:
        p.terminate()
