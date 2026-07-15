import struct
import sys

def parse_bluefile(filepath):
    with open(filepath, 'rb') as f:
        data = f.read(512)
    
    if len(data) < 512:
        print("File too small")
        return
        
    print(f"Header: {data[:4]}")
    type_code = struct.unpack('<I', data[48:52])[0]
    print(f"Type: {type_code}")
    
    # Dump adjunct from 256
    adjunct = data[256:320]
    
    print("\nTrying X-Midas Type 1000/2000 standard:")
    xstart, xdelta, xunits = struct.unpack('<ddi', data[256:256+20])
    print(f"xstart: {xstart}, xdelta: {xdelta}, xunits: {xunits}")
    
    # Try different combinations for the rest of the adjunct
    print("\nNext bytes as doubles:")
    for i in range(276, 320, 8):
        if i + 8 <= len(data):
            val = struct.unpack('<d', data[i:i+8])[0]
            print(f"Offset {i} (double): {val}")
            
    print("\nNext bytes as ints:")
    for i in range(276, 320, 4):
        if i + 4 <= len(data):
            val = struct.unpack('<i', data[i:i+4])[0]
            print(f"Offset {i} (int32): {val}")

if __name__ == "__main__":
    parse_bluefile(sys.argv[1])
