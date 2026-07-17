import sys
sys.path.append("/home/apsmith/.gemini/antigravity/scratch/dsp_tools/build")
import dsp_plotter_py

try:
    pg = dsp_plotter_py.PlotGenerator("")
    res = pg.get_file_info("/home/apsmith/.gemini/antigravity/scratch/dsp_tools/build/timecode.prm")
    print(res)
except Exception as e:
    import traceback
    traceback.print_exc()
