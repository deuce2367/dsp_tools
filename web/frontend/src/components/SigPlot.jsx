import React, { useEffect, useRef } from 'react';
import { Plot } from 'sigplot';

const SigPlot = ({ dataUrl, type, zmin, zmax, theme = 'dark', fftColor = '#00ff00', colormap = 'jet', onDataLoaded, onZoom }) => {
  const plotRef = useRef(null);
  const sigplotInstance = useRef(null);

  useEffect(() => {
    if (!plotRef.current) return;

    const SIGPLOT_CMAP_MAPPING = {
      jet: 1, greyscale: 0, sunset: 6, hot: 7, cold: 8,
      electric: 8, frog: 1, gqrx: 8, grape: 6, pablo: 1, turbo: 1, websdr: 7
    };
    const cmapInt = SIGPLOT_CMAP_MAPPING[colormap] !== undefined ? SIGPLOT_CMAP_MAPPING[colormap] : 1;

    if (!sigplotInstance.current) {
      sigplotInstance.current = new Plot(plotRef.current, {
        autohide_panes: true,
        autolink: false,
        nopan: true,
        hide_panning: true,
        cmap: cmapInt,
        colors: {
          bg: theme === 'dark' ? '#000000' : '#ffffff',
          fg: theme === 'dark' ? '#ffffff' : '#000000'
        }
      });
    }

    if (sigplotInstance.current && dataUrl) {
      // Clear previous layers and unzoom to reset axis limits for new data
      sigplotInstance.current.deoverlay();
      sigplotInstance.current.unzoom();
      
      let layerOptions = {};
      if (type === '1D' || type === 'time_domain') {
          layerOptions = { color: fftColor };
      } else if (type === 'constellation') {
          layerOptions = { color: fftColor, line: 0, symbol: 1, radius: 2 };
      } else {
          layerOptions = { cmap: cmapInt };
      }

      console.log("Loading sigplot from:", dataUrl, "type:", type);
      try {
        sigplotInstance.current.overlay_href(dataUrl, (layer) => {
          console.log("Sigplot layer loaded successfully:", layer);

          if (zmin !== '' && zmin !== undefined && zmax !== '' && zmax !== undefined) {
             let opts = { cmap: cmapInt };
             if (type === '1D') {
                 opts.ymin = parseFloat(zmin);
                 opts.ymax = parseFloat(zmax);
             } else if (type === 'constellation') {
                 opts.cmode = 5;
                 opts.ymin = parseFloat(zmin);
                 opts.ymax = parseFloat(zmax);
                 opts.xmin = parseFloat(zmin);
                 opts.xmax = parseFloat(zmax);
             } else {
                 opts.zmin = parseFloat(zmin);
                 opts.zmax = parseFloat(zmax);
             }
             sigplotInstance.current.change_settings(opts);
          } else if (onDataLoaded && layer) {
             if (type === 'constellation') {
                 sigplotInstance.current.change_settings({ cmode: 5 });
             }
             
             // Auto-scaled! Tell parent what bounds were chosen
             if ((type === '1D' || type === 'time_domain') && layer.ymin !== undefined && layer.ymax !== undefined) {
                 let ymin = layer.ymin;
                 let ymax = layer.ymax;
                 
                 // If data is available, discount 10% rolloff edges to find true min/max
                 if (type === '1D' && layer.data && layer.data.length > 0) {
                     const arr = layer.data;
                     const trim = Math.floor(arr.length * 0.1);
                     let cmin = Infinity;
                     let cmax = -Infinity;
                     for (let i = trim; i < arr.length - trim; i++) {
                         if (arr[i] < cmin) cmin = arr[i];
                         if (arr[i] > cmax) cmax = arr[i];
                     }
                     if (cmin !== Infinity) {
                         ymin = cmin;
                         ymax = cmax;
                     }
                 }
                 
                 let newYmin = ymin - 5.0;
                 let newYmax = ymax + 5.0;
                 
                 if (type === '1D') {
                     // Cap dynamic range to 85 dB from peak to avoid deep nulls pulling it down
                     newYmin = Math.max(newYmin, newYmax - 85.0);
                 }
                 
                 sigplotInstance.current.change_settings({ymin: newYmin, ymax: newYmax});
                 onDataLoaded(newYmin, newYmax);
             } else if (type === '2D' && layer.zmin !== undefined && layer.zmax !== undefined) {
                 let newZmax = layer.zmax;
                 let newZmin = Math.max(layer.zmin, newZmax - 85.0);
                 onDataLoaded(newZmin, newZmax);
             }
          }
        }, layerOptions);
      } catch (e) {
        console.error("Sigplot overlay threw an exception:", e);
      }
    }
    
    if (sigplotInstance.current && !sigplotInstance.current.__hasZoomListener) {
      sigplotInstance.current.addListener("zoom", (evt) => {
        if (onZoom && evt) {
          try {
            onZoom({
              xmin: evt.xmin,
              xmax: evt.xmax,
              ymin: evt.ymin,
              ymax: evt.ymax,
              type: type
            });
          } catch(e) {}
        }
      });
      sigplotInstance.current.__hasZoomListener = true;
    }

    return () => {
      // Don't fully destroy to avoid issues on remount if unnecessary, but can deoverlay
    };
  }, [dataUrl, type]);

  useEffect(() => {
    if (sigplotInstance.current) {
      sigplotInstance.current.change_settings({
        colors: {
          bg: theme === 'dark' ? '#000000' : '#ffffff',
          fg: theme === 'dark' ? '#ffffff' : '#000000'
        }
      });
    }
  }, [theme]);

  useEffect(() => {
    if (sigplotInstance.current) {
      const SIGPLOT_CMAP_MAPPING = {
        jet: 1, greyscale: 0, sunset: 6, hot: 7, cold: 8,
        electric: 8, frog: 1, gqrx: 8, grape: 6, pablo: 1, turbo: 1, websdr: 7
      };
      const cmapInt = SIGPLOT_CMAP_MAPPING[colormap] !== undefined ? SIGPLOT_CMAP_MAPPING[colormap] : 1;
      sigplotInstance.current.change_settings({ cmap: cmapInt });
    }
  }, [colormap]);

  useEffect(() => {
    if (sigplotInstance.current && (type === '1D' || type === 'time_domain' || type === 'constellation')) {
        try {
            const plot = sigplotInstance.current;
            const Gx = plot._Gx || plot._plot1d;
            if (Gx && Gx.lyr) {
                for (let i = 0; i < Gx.lyr.length; i++) {
                    if (Gx.lyr[i]) {
                        Gx.lyr[i].color = fftColor;
                    }
                }
                plot.refresh();
            } else {
                plot.change_settings({ color: fftColor });
            }
        } catch(e) {
            console.warn("Failed to update sigplot color dynamically:", e);
        }
    }
  }, [fftColor, type]);

  useEffect(() => {
    if (sigplotInstance.current && zmin !== '' && zmax !== '') {
      const zminVal = parseFloat(zmin);
      const zmaxVal = parseFloat(zmax);
      if (!isNaN(zminVal) && !isNaN(zmaxVal)) {
        if (type === '1D') {
            sigplotInstance.current.change_settings({ymin: zminVal, ymax: zmaxVal});
        } else {
            sigplotInstance.current.change_settings({zmin: zminVal, zmax: zmaxVal});
        }
      }
    }
  }, [zmin, zmax]);

  return <div ref={plotRef} className="sigplot-box" style={{ width: '100%', height: '100%' }}></div>;
}

export default SigPlot;
