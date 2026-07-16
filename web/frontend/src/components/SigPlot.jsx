import React, { useEffect, useRef } from 'react';
import { Plot } from 'sigplot';

const SigPlot = ({ dataUrl, type, zmin, zmax, theme = 'dark' }) => {
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
        colors: {
          bg: theme === 'dark' ? '#000000' : '#ffffff',
          fg: theme === 'dark' ? '#ffffff' : '#000000'
        }
      });
    }

    if (sigplotInstance.current && dataUrl) {
      // Clear previous layers
      sigplotInstance.current.deoverlay();
      
      let layerOptions = {};
      console.log("Loading sigplot from:", dataUrl, "type:", type);
      try {
        sigplotInstance.current.overlay_href(dataUrl, (layer) => {
          console.log("Sigplot layer loaded successfully:", layer);
          if (zmin !== '' && zmin !== undefined && zmax !== '' && zmax !== undefined) {
             sigplotInstance.current.change_settings({zmin: parseFloat(zmin), zmax: parseFloat(zmax)});
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
    if (sigplotInstance.current && zmin !== '' && zmax !== '') {
      const zminVal = parseFloat(zmin);
      const zmaxVal = parseFloat(zmax);
      if (!isNaN(zminVal) && !isNaN(zmaxVal)) {
        sigplotInstance.current.change_settings({zmin: zminVal, zmax: zmaxVal});
      }
    }
  }, [zmin, zmax]);

  return <div ref={plotRef} className="sigplot-box" style={{ width: '100%', height: '100%' }}></div>;
}

export default SigPlot;
