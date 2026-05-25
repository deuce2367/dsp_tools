import struct
import numpy as np

# Create a simple sine wave
fs = 48000.0
t = np.arange(0, 1.0, 1.0/fs)
signal = np.sin(2 * np.pi * 1000 * t) + 1j * np.cos(2 * np.pi * 1000 * t)

# Complex Float32 (CF)
data = np.zeros(len(signal) * 2, dtype=np.float32)
data[0::2] = np.real(signal)
data[1::2] = np.imag(signal)

header = bytearray(512)
header[0:4] = b"BLUE"
header[4:8] = b"EEEI"
header[8:12] = b"EEEI"
struct.pack_into('<i', header, 12, 0) # detached
struct.pack_into('<i', header, 16, 0) # protected
struct.pack_into('<i', header, 20, 0) # pipe
struct.pack_into('<i', header, 24, 0) # ext_start
struct.pack_into('<i', header, 28, 0) # ext_size
struct.pack_into('<d', header, 32, 512.0) # data_start
struct.pack_into('<d', header, 40, float(len(data)*4)) # data_size
struct.pack_into('<i', header, 48, 1000) # type

header[52:54] = b"CF" # format (Complex Float)

# xdelta
struct.pack_into('<d', header, 264, 1.0/fs)

with open('test_blue.prm', 'wb') as f:
    f.write(header)
    f.write(data.tobytes())

print("Generated test_blue.prm")
