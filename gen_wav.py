import wave
import struct
import math

sample_rate = 48000
num_samples = 48000 * 2  # 2 seconds
freq = 1000.0

with wave.open('test.wav', 'w') as wav_file:
    wav_file.setnchannels(1)
    wav_file.setsampwidth(2)
    wav_file.setframerate(sample_rate)
    
    for i in range(num_samples):
        value = int(32767.0 * math.sin(2.0 * math.pi * freq * (i / sample_rate)))
        data = struct.pack('<h', value)
        wav_file.writeframesraw(data)
