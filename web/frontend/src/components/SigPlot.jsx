import React, { useEffect, useRef } from 'react';
import { Plot } from 'sigplot';

const SigPlot = ({ dataUrl, type, zmin, zmax, theme = 'dark', fftColor = '#00ff00', sigplotColormap = 1, onDataLoaded }) => {
  const plotRef = useRef(null);
  const sigplotInstance = useRef(null);

  useEffect(() => {
    if (!plotRef.current) return;

    if (!sigplotInstance.current) {
      sigplotInstance.current = new Plot(plotRef.current, {
        autohide_panes: true,
        autolink: false,
        nopan: true,
        hide_panning: true,
        cmap: sigplotColormap,
        colors: {
          bg: theme === 'dark' ? '#000000' : '#ffffff',
          fg: theme === 'dark' ? '#ffffff' : '#000000'
        }
      });
    }

    if (sigplotInstance.current && dataUrl) {
      // Clear previous layers
      sigplotInstance.current.deoverlay();
      
      let layerOptions = type === '1D' ? { color: fftColor } : { cmap: sigplotColormap };
      console.log("Loading sigplot from:", dataUrl, "type:", type);
      try {
        sigplotInstance.current.overlay_href(dataUrl, (layer) => {
          console.log("Sigplot layer loaded successfully:", layer);
          if (zmin !== '' && zmin !== undefined && zmax !== '' && zmax !== undefined) {
             let opts = { cmap: sigplotColormap };
             if (type === '1D') {
                 opts.ymin = parseFloat(zmin);
                 opts.ymax = parseFloat(zmax);
             } else {
                 opts.zmin = parseFloat(zmin);
                 opts.zmax = parseFloat(zmax);
             }
             sigplotInstance.current.change_settings(opts);
          } else if (onDataLoaded && layer) {
             // Auto-scaled! Tell parent what bounds were chosen
             if (type === '1D' && layer.ymin !== undefined && layer.ymax !== undefined) {
                 onDataLoaded(layer.ymin, layer.ymax);
             } else if (type === '2D' && layer.zmin !== undefined && layer.zmax !== undefined) {
                 onDataLoaded(layer.zmin, layer.zmax);
             }
          }
        }, layerOptions);
      } catch (e) {
        console.error("Sigplot overlay threw an exception:", e);
      }
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
      sigplotInstance.current.change_settings({ cmap: sigplotColormap });
    }
  }, [sigplotColormap]);

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
