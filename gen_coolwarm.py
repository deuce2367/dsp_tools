import matplotlib.pyplot as plt
import numpy as np

cmap = plt.get_cmap('coolwarm')
n = 7
x = np.linspace(0, 1, n)
rgba_values = cmap(x)
rgb_values = (rgba_values[:, :3] * 255).astype(int)

stops_str = ", ".join(f"{v}f" for v in x)
print(f"const int num_stops = {n};")
print(f"float stops[num_stops] = {{{stops_str}}};")
print("RGB colors[num_stops] = {")
for r, g, b in rgb_values:
    print(f"    {{{r}, {g}, {b}}},")
print("};")
