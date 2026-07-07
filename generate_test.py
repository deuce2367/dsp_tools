import numpy as np
import ctypes
import os

class BlueHeader(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ('version', ctypes.c_char * 4),
        ('head_rep', ctypes.c_char * 4),
        ('data_rep', ctypes.c_char * 4),
        ('detached', ctypes.c_int32),
        ('protected_flag', ctypes.c_int32),
        ('pipe', ctypes.c_int32),
        ('ext_start', ctypes.c_int32),
        ('ext_size', ctypes.c_int32),
        ('data_start', ctypes.c_double),
        ('data_size', ctypes.c_double),
        ('type', ctypes.c_int32),
        ('format', ctypes.c_char * 2),
        ('flagmask', ctypes.c_int16),
        ('timecode', ctypes.c_double),
        ('inlet', ctypes.c_int16),
        ('outlets', ctypes.c_int16),
        ('outmask', ctypes.c_int32),
        ('pipeloc', ctypes.c_int32),
        ('pipesize', ctypes.c_int32),
        ('in_byte', ctypes.c_double),
        ('out_byte', ctypes.c_double),
        ('outbytes', ctypes.c_double * 8),
        ('keylength', ctypes.c_int32),
        ('keywords', ctypes.c_char * 92),
        ('xstart', ctypes.c_double),
        ('xdelta', ctypes.c_double),
        ('xunits', ctypes.c_int32),
        ('padding', ctypes.c_char * 236)
    ]

def main():
    sample_rate = 1000000.0 # 1 MHz
    num_samples = 4000000 # 4 seconds
    
    t = np.arange(num_samples) / sample_rate
    
    signal = (np.random.randn(num_samples) + 1j * np.random.randn(num_samples)) * 0.1
    signal += 2.0 * np.exp(1j * 2 * np.pi * 100000.0 * t) # +100 kHz Tone
    signal += 1.5 * np.exp(1j * 2 * np.pi * -300000.0 * t) # -300 kHz Tone
    
    pulse_width = int(0.0001 * sample_rate) # 100 microseconds
    pulse_interval = int(0.1 * sample_rate) # every 100 ms
    
    for i in range(int(0.05 * sample_rate), num_samples, pulse_interval):
        pulse = (np.random.randn(pulse_width) + 1j * np.random.randn(pulse_width)) * 50.0
        signal[i:i+pulse_width] += pulse
    
    signal = signal.astype(np.complex64)
    data_bytes = signal.tobytes()
    
    hdr = BlueHeader()
    hdr.version = b"BLUE"
    hdr.head_rep = b"IEEE"
    hdr.data_rep = b"IEEE"
    hdr.ext_start = 0
    hdr.ext_size = 0
    hdr.data_start = 512.0
    hdr.data_size = float(len(data_bytes))
    hdr.type = 1000
    hdr.format = b"CF"
    hdr.timecode = 0.0
    hdr.xstart = 0.0
    hdr.xdelta = 1.0 / sample_rate
    hdr.xunits = 1
    
    with open("/tmp/interference.prm", "wb") as f:
        f.write(bytes(hdr))
        f.write(data_bytes)
        
    print("Test file created at /tmp/interference.prm")

if __name__ == "__main__":
    main()
