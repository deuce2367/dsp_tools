import React, { useState, useEffect, useRef } from 'react';
import SigPlot from './components/SigPlot.jsx';
import Slider from 'rc-slider';
import 'rc-slider/assets/index.css';
const formatJ1950ToISO = (timecode) => {
  if (!timecode) return '0';
  const val = Number(timecode);
  if (val < 1e8) return `${val}`; // Small number, not a timestamp
  // Convert J1950 to J1970 UNIX timestamp
  const unixTime = val - 631152000;
  try {
    const dt = new Date(Math.floor(unixTime) * 1000);
    let iso = dt.toISOString();
    let fracStr = (val % 1).toFixed(6).substring(2);
    // Replace the millisecond part with our 6-digit fractional part
    return iso.replace(/\.\d+Z$/, `.${fracStr}Z`);
  } catch(e) {
    return `${val}`;
  }
};

const ColormapSelect = ({ options, value, onChange, gradients }) => {
  const [isOpen, setIsOpen] = useState(false);
  const ref = useRef(null);
  
  useEffect(() => {
    const handleClick = (e) => {
      if (ref.current && !ref.current.contains(e.target)) setIsOpen(false);
    };
    document.addEventListener('mousedown', handleClick);
    return () => document.removeEventListener('mousedown', handleClick);
  }, []);

  const selectedOpt = options.find(o => String(o.value) === String(value)) || options[0];

  return (
    <div ref={ref} style={{position: 'relative', width: '100%', userSelect: 'none'}}>
      <div 
        onClick={() => setIsOpen(!isOpen)}
        style={{
          display: 'flex', alignItems: 'center', justifyContent: 'space-between',
          background: 'var(--input-bg)', border: '1px solid var(--border-color)', 
          padding: '6px 10px', borderRadius: '4px', cursor: 'pointer', fontSize: '0.9rem'
        }}>
        <div style={{display: 'flex', alignItems: 'center', gap: '10px'}}>
          <div style={{width: '30px', height: '12px', borderRadius: '2px', background: gradients[selectedOpt.value]}}></div>
          <span>{selectedOpt.label}</span>
        </div>
        <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><polyline points="6 9 12 15 18 9"></polyline></svg>
      </div>
      {isOpen && (
        <div style={{
          position: 'absolute', top: '100%', left: 0, right: 0, zIndex: 1000, 
          background: 'var(--panel-bg)', border: '1px solid var(--border-color)', 
          borderRadius: '4px', marginTop: '2px', maxHeight: '200px', overflowY: 'auto',
          boxShadow: '0 4px 12px rgba(0,0,0,0.5)'
        }}>
          {options.map(opt => (
            <div 
              key={opt.value}
              onClick={() => { onChange(opt.value); setIsOpen(false); }}
              style={{
                display: 'flex', alignItems: 'center', gap: '10px',
                padding: '6px 10px', cursor: 'pointer',
                background: String(value) === String(opt.value) ? 'var(--accent-color)' : 'transparent',
                color: String(value) === String(opt.value) ? '#000' : 'var(--text-color)'
              }}>
              <div style={{width: '30px', height: '12px', borderRadius: '2px', background: gradients[opt.value]}}></div>
              <span>{opt.label}</span>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

function App() {
  const [file, setFile] = useState('timecode.prm');
  const [availableFiles, setAvailableFiles] = useState([]);
  const [centerFreq, setCenterFreq] = useState(0);
  const [windowSize, setWindowSize] = useState(4096);
  const [smoothing, setSmoothing] = useState(8);
  const [colormap, setColormap] = useState('jet');
  const [sigplotColormap, setSigplotColormap] = useState(1);
  const [fftColor, setFftColor] = useState('#00ff00');
  
  const [zmin, setZmin] = useState(-80);
  const [zmax, setZmax] = useState(-20);
  const [gainBounds, setGainBounds] = useState([-150, 0]);
  const [gainHover, setGainHover] = useState(false);

  const [imagePlot, setImagePlot] = useState('');
  const [sigplotUrl, setSigplotUrl] = useState('');
  const [sigplotType, setSigplotType] = useState('1D');
  const [panels, setPanels] = useState([
    { id: 'static-waterfall', type: 'static', subType: 'waterfall', title: 'Static Waterfall (PSD)', url: '' }
  ]);
  const [layoutMode, setLayoutMode] = useState('auto');

  const [staticPlotType, setStaticPlotType] = useState('');
  const [loading, setLoading] = useState(false);
  const [uploadProgress, setUploadProgress] = useState(-1);
  const [elapsedTime, setElapsedTime] = useState(0);
  const [uploadTimer, setUploadTimer] = useState(null);
  const [theme, setTheme] = useState('dark');
  const [fillMode, setFillMode] = useState('gradient');
  const [fillColor, setFillColor] = useState('#00ff00');

  const [showImportModal, setShowImportModal] = useState(false);
  const [importFileName, setImportFileName] = useState('');
  const [importFormat, setImportFormat] = useState('CF');
  const [importRate, setImportRate] = useState(1.0);
  const [importFreq, setImportFreq] = useState(0.0);
  const [importTimecode, setImportTimecode] = useState('0');
  const [importOutName, setImportOutName] = useState('');
  const [importFileObj, setImportFileObj] = useState(null);
  const [importFileSize, setImportFileSize] = useState(0);
  const [isWavImport, setIsWavImport] = useState(false);
  const xhrRef = useRef(null);
  const [fileInfo, setFileInfo] = useState(null);
  
  const [zoomBounds, setZoomBounds] = useState(null);
  const [showTunerModal, setShowTunerModal] = useState(false);
  const [tunerParams, setTunerParams] = useState({center: 0, bw: 0, start: 0, dur: 0});
  const [tunerOutName, setTunerOutName] = useState('');
  
  const [editTimecode, setEditTimecode] = useState('');
  const [editFreq, setEditFreq] = useState('');
  const [isEditingInfo, setIsEditingInfo] = useState(false);

  const interactivePanelRef = useRef(null);
  const staticPanelRef = useRef(null);

  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
  }, [theme]);

  const toggleTheme = () => {
    setTheme(theme === 'dark' ? 'light' : 'dark');
  };

  useEffect(() => {
    panels.forEach(p => {
        if (p.type === 'static') {
            handleStaticPlot(p.subType === 'fft');
        }
    });
  }, [theme]);

  const fetchFiles = async () => {
    try {
      const res = await fetch('/api/files');
      const data = await res.json();
      if (data.files) {
        const sorted = data.files.sort((a,b) => a.localeCompare(b));
        setAvailableFiles(sorted);
        if (!sorted.includes(file) && sorted.length > 0) {
          setFile(sorted[0]);
        }
      }
    } catch (e) {
      console.error("Error fetching files", e);
    }
  };

  useEffect(() => {
    fetchFiles();
  }, []);

  useEffect(() => {
    if (!file) return;
    const fetchInfo = async () => {
      try {
        const res = await fetch(`/api/info/${file}`);
        if (res.ok) {
          const data = await res.json();
          setFileInfo(data);
          setEditTimecode(formatJ1950ToISO(data.timecode));
          setEditFreq(data.center_freq);
          if (data.center_freq !== undefined) {
            setCenterFreq(data.center_freq);
          }
          setZoomBounds(null); // Reset bounds when file changes
        } else {
          setFileInfo(null);
        }
      } catch (e) {
        console.error("Error fetching file info", e);
        setFileInfo(null);
      }
    };
    fetchInfo();
  }, [file]);

  const generateUniqueName = (originalName) => {
    let baseName = originalName.substring(0, originalName.lastIndexOf('.'));
    if (!baseName) baseName = originalName;
    let outName = baseName + '.prm';
    let count = 1;
    while (availableFiles.includes(outName)) {
       outName = `${baseName}_${count}.prm`;
       count++;
    }
    return outName;
  };

  const parseWavHeader = (file) => {
    return new Promise((resolve) => {
      const reader = new FileReader();
      reader.onload = (e) => {
        const buffer = e.target.result;
        const view = new DataView(buffer);
        try {
          const numChannels = view.getUint16(22, true);
          const sampleRate = view.getUint32(24, true);
          const bitsPerSample = view.getUint16(34, true);
          let formatStr = 'CI'; // default
          if (numChannels === 2 && bitsPerSample === 32) formatStr = 'CF';
          else if (numChannels === 1 && bitsPerSample === 32) formatStr = 'SF';
          else if (numChannels === 2 && bitsPerSample === 16) formatStr = 'CI';
          else if (numChannels === 1 && bitsPerSample === 16) formatStr = 'SI';
          else if (numChannels === 2 && bitsPerSample === 8) formatStr = 'CB';
          else if (numChannels === 1 && bitsPerSample === 8) formatStr = 'SB';
          resolve({ sampleRate, formatStr });
        } catch (err) {
          resolve(null);
        }
      };
      reader.readAsArrayBuffer(file.slice(0, 44));
    });
  };

  const handleUpload = async (e) => {
    if (!e.target.files || e.target.files.length === 0) return;
    const selectedFile = e.target.files[0];
    const name = selectedFile.name;
    const nameLower = name.toLowerCase();
    
    setImportFileObj(selectedFile);
    setImportFileName(name);
    setImportFileSize(selectedFile.size);
    setImportOutName(generateUniqueName(name));
    
    if (nameLower.endsWith('.wav')) {
      setIsWavImport(true);
      const parsed = await parseWavHeader(selectedFile);
      if (parsed) {
        setImportRate(parsed.sampleRate / 1e6);
        setImportFormat(parsed.formatStr);
      }
    } else {
      setIsWavImport(false);
    }
    
    setShowImportModal(true);
    e.target.value = null; // reset input
  };

  const startImport = () => {
    const uploadData = new FormData();
    uploadData.append('file', importFileObj);
    
    setUploadProgress(0);
    setElapsedTime(0);
    const startTime = Date.now();
    const tId = setInterval(() => {
      setElapsedTime(Math.floor((Date.now() - startTime) / 1000));
    }, 1000);
    setUploadTimer(tId);
    
    const xhr = new XMLHttpRequest();
    xhrRef.current = xhr;
    xhr.open('POST', '/api/upload', true);
    
    xhr.upload.onprogress = (event) => {
      if (event.lengthComputable) {
        setUploadProgress((event.loaded / event.total) * 100);
      }
    };
    
    xhr.onload = async () => {
      if (xhr.status === 200) {
        setUploadProgress(100);
        await convertFile(importFileName, importOutName, importFormat, importRate, importFreq, false);
        clearInterval(tId);
        setUploadProgress(-1);
        setShowImportModal(false);
      } else {
        alert("Upload failed.");
        clearInterval(tId);
        setUploadProgress(-1);
      }
    };
    
    xhr.onerror = () => {
      alert("Error uploading file.");
      clearInterval(tId);
      setUploadProgress(-1);
    };
    
    xhr.send(uploadData);
  };

  const cancelImport = () => {
    if (xhrRef.current && uploadProgress >= 0) {
      xhrRef.current.abort();
    }
    if (uploadTimer) clearInterval(uploadTimer);
    setUploadProgress(-1);
    setShowImportModal(false);
  };

  const convertFile = async (inputName, outputName, format, rateMhz, freqMhz, sigmf) => {
    setLoading(true);
    try {
      const res = await fetch('/api/run/convert', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          input_file: inputName,
          output_file: outputName,
          format: format,
          rate: Number(rateMhz) * 1e6,
          freq_mhz: Number(freqMhz),
          sigmf: sigmf,
          timecode: importTimecode
        })
      });
      if (res.ok) {
        await fetchFiles();
        setFile(outputName);
      } else {
        const data = await res.json();
        alert("Conversion failed: " + (data.detail || "Unknown error"));
      }
    } catch(err) {
      alert("Error converting: " + err);
    }
    setLoading(false);
  };

  const saveFileInfo = async () => {
    try {
      const res = await fetch(`/api/update/${file}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          timecode: String(editTimecode),
          center_freq: Number(editFreq)
        })
      });
      if (res.ok) {
        setIsEditingInfo(false);
        // Refresh file info
        const infoRes = await fetch(`/api/info/${file}`);
        let newFreq = centerFreq;
        if (infoRes.ok) {
          const data = await infoRes.json();
          setFileInfo(data);
          if (data.center_freq !== undefined) {
            setCenterFreq(data.center_freq);
            newFreq = data.center_freq;
          }
        }
        
        // Force refresh plots
        setPanels(prev => {
            prev.forEach(p => {
                if (p.id === 'interactive-fft') handleInteractiveFFT(newFreq);
                if (p.id === 'interactive-psd') handleInteractivePSD(newFreq);
                if (p.id === 'static-fft') handleStaticPlot(true, newFreq);
                if (p.id === 'static-waterfall') handleStaticPlot(false, newFreq);
            });
            return prev;
        });

      } else {
        const data = await res.json();
        alert("Update failed: " + (data.detail || "Unknown error"));
      }
    } catch(err) {
      alert("Error saving info: " + err);
    }
  };

  const getComputedFftSize = (val, ref, isInteractive = false) => {
    if (val !== 'auto' && val !== '') return Number(val);
    if (!ref.current) return isInteractive ? 4096 : 1024;
    // Nearest power of 2 for FFT size based on panel width
    let base = Math.pow(2, Math.round(Math.log2(ref.current.clientWidth)));
    return isInteractive ? base * 4 : base;
  };

  const handleStaticPlot = async (isFft, overrideFreq = null, targetFile = null) => {
    setLoading(true);
    try {
      const activeFile = targetFile || file;
      const reqWindowSize = getComputedFftSize(windowSize, {current: {clientWidth: 800}}, false);
      const res = await fetch('/api/run/plot', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          input_file: activeFile,
          center_freq: overrideFreq !== null ? Number(overrideFreq) : Number(centerFreq),
          zoom_bw: 0,
          width: 800,
          height: 600,
          window_size: reqWindowSize,
          smoothing: smoothing,
          colormap: colormap,
          plot_fft: isFft,
          plot_waterfall: !isFft,
          theme: theme,
          fill_mode: fillMode,
          fill_color: fillColor,
          zmin: zmin !== '' ? Number(zmin) : null,
          zmax: zmax !== '' ? Number(zmax) : null
        })
      });
      const data = await res.json();
      if (!res.ok) {
        alert("Error from server:\n" + (data.detail || JSON.stringify(data)));
        setLoading(false);
        return;
      }
      if (data.output_file) {
        const newUrl = `/api/data/${data.output_file}?t=${Date.now()}`;
        const pId = isFft ? 'static-fft' : 'static-waterfall';
        const pTitle = isFft ? 'Static Spectrum (FFT)' : 'Static Waterfall (PSD)';
        setPanels(prev => {
            const existingIdx = prev.findIndex(p => p.id === pId);
            if (existingIdx >= 0) {
                const next = [...prev];
                next[existingIdx] = { ...next[existingIdx], url: newUrl };
                return next;
            } else {
                return [...prev, { id: pId, type: 'static', subType: isFft ? 'fft' : 'waterfall', title: pTitle, url: newUrl }];
            }
        });
      }
    } catch (e) {
      alert("Error generating plot: " + e);
    }
    setLoading(false);
  };

  const handleInteractivePSD = async (overrideFreq = null, targetFile = null) => {
    setLoading(true);
    try {
      const activeFile = targetFile || file;
      const reqWindowSize = getComputedFftSize(windowSize, {current: {clientWidth: 800}}, true);
      const res = await fetch('/api/run/psd', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          input_file: activeFile,
          center_freq: (overrideFreq !== null && typeof overrideFreq !== 'object') ? Number(overrideFreq) : Number(centerFreq),
          zoom_bw: 0,
          window_size: reqWindowSize,
          smoothing: smoothing
        })
      });
      const data = await res.json();
      if (!res.ok) {
        alert("Error from server:\n" + (data.detail || JSON.stringify(data)));
        setLoading(false);
        return;
      }
      if (data.output_file) {
        setSigplotType('2D');
        const newUrl = `/api/data/${data.output_file}?t=${Date.now()}`;
        setPanels(prev => {
            const existingIdx = prev.findIndex(p => p.id === 'interactive-psd');
            if (existingIdx >= 0) {
                const next = [...prev];
                next[existingIdx] = { ...next[existingIdx], url: newUrl };
                return next;
            } else {
                return [...prev, { id: 'interactive-psd', type: 'interactive', subType: '2D', title: 'Interactive Waterfall (PSD)', url: newUrl }];
            }
        });
      }
    } catch (e) {
      alert("Error generating PSD: " + e);
    }
    setLoading(false);
  };

  const handleInteractiveFFT = async (overrideFreq = null, targetFile = null) => {
    setLoading(true);
    try {
      const activeFile = targetFile || file;
      const reqWindowSize = getComputedFftSize(windowSize, {current: {clientWidth: 800}}, true);
      const res = await fetch('/api/run/fft', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          input_file: activeFile,
          center_freq: (overrideFreq !== null && typeof overrideFreq !== 'object') ? Number(overrideFreq) : Number(centerFreq),
          zoom_bw: 0,
          window_size: reqWindowSize,
          smoothing: smoothing
        })
      });
      const data = await res.json();
      if (!res.ok) {
        alert("Error from server:\n" + (data.detail || JSON.stringify(data)));
        setLoading(false);
        return;
      }
      if (data.output_file) {
        setSigplotType('1D');
        const newUrl = `/api/data/${data.output_file}?t=${Date.now()}`;
        setPanels(prev => {
            const existingIdx = prev.findIndex(p => p.id === 'interactive-fft');
            if (existingIdx >= 0) {
                const next = [...prev];
                next[existingIdx] = { ...next[existingIdx], url: newUrl };
                return next;
            } else {
                return [...prev, { id: 'interactive-fft', type: 'interactive', subType: '1D', title: 'Interactive Spectrum (FFT)', url: newUrl }];
            }
        });
      }
    } catch (e) {
      alert("Error generating FFT: " + e);
    }
    setLoading(false);
  };

  const handleDataLoaded = (min_val, max_val) => {
    // Round and add 20dB padding for slider bounds
    const boundsMin = Math.floor(min_val - 20);
    const boundsMax = Math.ceil(max_val + 20);
    setGainBounds([boundsMin, boundsMax]);
    // Don't auto-set zmin/zmax here, leave them empty to allow auto-scaling initially
    // but the slider will display the bounds.
  };

  const handleZoom = (bounds) => {
    setZoomBounds(bounds);
  };

  const openTuner = () => {
    if (!zoomBounds || !fileInfo) return;
    const xmin = zoomBounds.xmin;
    const xmax = zoomBounds.xmax;
    const ymin = zoomBounds.ymin;
    const ymax = zoomBounds.ymax;
    
    const center = (xmin + xmax) / 2.0;
    const bw = Math.abs(xmax - xmin);
    
    let start = 0;
    let dur = 0;
    if (sigplotType === '2D') {
      start = Math.min(ymin, ymax);
      dur = Math.abs(ymax - ymin);
    }
    
    setTunerParams({center, bw, start, dur});
    
    // Propose reasonable name
    let baseName = file.substring(0, file.lastIndexOf('.'));
    if (!baseName) baseName = file;
    let proposed = `${baseName}_tuned_${center.toFixed(3)}MHz_${bw.toFixed(3)}MHz.prm`;
    setTunerOutName(proposed);
    
    setShowTunerModal(true);
  };

  const startTuner = async () => {
    setShowTunerModal(false);
    setLoading(true);
    try {
      const res = await fetch('/api/run/tuner', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          input_file: file,
          output_file: tunerOutName,
          center_freq: tunerParams.center * 1e6, // zoom bounds are in MHz, convert to Hz
          bandwidth: tunerParams.bw * 1e6,
          start_time: tunerParams.start,
          duration: tunerParams.dur,
          file_center: Number(centerFreq) * 1e6,
          quality: 'normal'
        })
      });
      const data = await res.json();
      if (!res.ok) {
        alert("Error tuning: " + (data.detail || JSON.stringify(data)));
      } else {
        await fetchFiles();
        setFile(tunerOutName);
        setShowTunerModal(false);
        setZoomBounds(null); // Reset bounds
        
        // Refresh the plot to show the newly tuned data
        setTimeout(() => {
          setPanels(prev => {
              prev.forEach(p => {
                  if (p.id === 'interactive-fft') handleInteractiveFFT(tunerParams.center, tunerOutName);
                  if (p.id === 'interactive-psd') handleInteractivePSD(tunerParams.center, tunerOutName);
                  if (p.id === 'static-fft') handleStaticPlot(true, tunerParams.center, tunerOutName);
                  if (p.id === 'static-waterfall') handleStaticPlot(false, tunerParams.center, tunerOutName);
              });
              return prev;
          });
        }, 300);
      }
    } catch(e) {
      alert("Error: " + e);
    }
    setLoading(false);
  };

  return (
    <div className="container">
      <div style={{display: 'flex', justifyContent: 'space-between', alignItems: 'center'}}>
        <h1>DSP Tools Interface</h1>
        <div style={{display: 'flex', gap: '15px', alignItems: 'center'}}>
          <select value={layoutMode} onChange={e => setLayoutMode(e.target.value)} style={{width: 'auto', margin: 0}}>
            <option value="auto">Layout: Auto</option>
            <option value="horizontal">Layout: Horizontal</option>
            <option value="vertical">Layout: Vertical</option>
          </select>
          <a href="/docs" target="_blank" rel="noreferrer" style={{color: 'var(--text-color)', fontSize: '0.9rem', opacity: 0.7, textDecoration: 'none'}}>API (Swagger)</a>
          <button onClick={toggleTheme} style={{width: 'auto', margin: 0}}>{theme === 'dark' ? '☀️ Light' : '🌙 Dark'}</button>
        </div>
      </div>
      <div className="content">
        
        {/* Controls Panel */}
        <div className="panel" style={{flex: 1, overflowY: 'auto'}}>
          <h2 style={{marginTop: 0, fontSize: '1.2rem'}}>Controls</h2>
          <div className="form-group">
            <label>Input File</label>
            <div style={{ display: 'flex', gap: '5px', alignItems: 'center' }}>
              {file && (
                <a href={`/api/data/${file}`} download={file} style={{
                  background: 'var(--accent-color)', color: '#000', padding: '6px 10px', borderRadius: '4px', cursor: 'pointer', textDecoration: 'none', display: 'flex', alignItems: 'center', justifyContent: 'center'
                }} title="Download File">
                  <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path><polyline points="7 10 12 15 17 10"></polyline><line x1="12" y1="15" x2="12" y2="3"></line></svg>
                </a>
              )}
              <select value={file} onChange={(e) => setFile(e.target.value)} style={{ flex: 1 }}>
                {availableFiles.map(f => <option key={f} value={f}>{f}</option>)}
                {availableFiles.length === 0 && <option value={file}>{file}</option>}
              </select>
              <input type="file" accept=".prm,.wav,.bin,.raw,.dat" onChange={handleUpload} style={{ display: 'none' }} id="file-upload" />
              <label htmlFor="file-upload" style={{
                background: 'var(--accent-color)', color: '#000', padding: '6px 10px', borderRadius: '4px', cursor: 'pointer', margin: 0, display: 'flex', alignItems: 'center', justifyContent: 'center'
              }} title="Upload File">
                <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path><polyline points="17 8 12 3 7 8"></polyline><line x1="12" y1="3" x2="12" y2="15"></line></svg>
              </label>
            </div>
            {fileInfo && (
              <div style={{ marginTop: '8px', padding: '8px', background: 'var(--input-bg)', borderRadius: '4px', fontSize: '0.85rem' }}>
                <div style={{display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px', marginBottom: '10px'}}>
                  <div><span style={{color: 'var(--text-color)', opacity: 0.7}}>Type:</span> <span style={{fontWeight: 'bold'}}>{fileInfo.is_wav ? 'WAV' : 'X-Midas Blue'}</span></div>
                  <div><span style={{color: 'var(--text-color)', opacity: 0.7}}>Format:</span> <span style={{fontWeight: 'bold'}}>{fileInfo.format_str} ({fileInfo.channels === 2 ? 'Complex' : 'Real'})</span></div>
                  <div><span style={{color: 'var(--text-color)', opacity: 0.7}}>Size:</span> <span style={{fontWeight: 'bold'}}>{(fileInfo.file_size / (1024*1024)).toFixed(2)} MB</span></div>
                  <div><span style={{color: 'var(--text-color)', opacity: 0.7}}>Samples:</span> <span style={{fontWeight: 'bold'}}>{(fileInfo.total_samples / 1e6).toFixed(2)}M</span></div>
                  <div><span style={{color: 'var(--text-color)', opacity: 0.7}}>Sample Rate:</span> <span style={{fontWeight: 'bold'}}>{(fileInfo.sample_rate / 1e6).toFixed(3)} MHz</span></div>
                  <div><span style={{color: 'var(--text-color)', opacity: 0.7}}>Bandwidth:</span> <span style={{fontWeight: 'bold'}}>{(fileInfo.sample_rate / 1e6).toFixed(3)} MHz</span></div>
                  <div><span style={{color: 'var(--text-color)', opacity: 0.7}}>Duration:</span> <span style={{fontWeight: 'bold'}}>{fileInfo.duration_seconds.toFixed(3)}s</span></div>
                </div>

                <div style={{borderTop: '1px solid var(--border-color)', paddingTop: '8px', position: 'relative'}}>
                  <div style={{position: 'absolute', right: 0, top: '8px', display: 'flex', gap: '5px'}}>
                    {isEditingInfo ? (
                      <>
                        <button onClick={saveFileInfo} title="Save" style={{background: 'var(--accent-color)', color: '#000', padding: '4px', borderRadius: '4px', border: 'none', cursor: 'pointer', display: 'flex', alignItems: 'center', margin: 0}}>
                          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="3" strokeLinecap="round" strokeLinejoin="round"><polyline points="20 6 9 17 4 12"></polyline></svg>
                        </button>
                        <button onClick={() => { setIsEditingInfo(false); setEditTimecode(formatJ1950ToISO(fileInfo.timecode)); setEditFreq(fileInfo.center_freq); }} title="Cancel" style={{background: '#ff4444', color: '#fff', padding: '4px', borderRadius: '4px', border: 'none', cursor: 'pointer', display: 'flex', alignItems: 'center', margin: 0}}>
                          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="3" strokeLinecap="round" strokeLinejoin="round"><line x1="18" y1="6" x2="6" y2="18"></line><line x1="6" y1="6" x2="18" y2="18"></line></svg>
                        </button>
                      </>
                    ) : (
                      <button onClick={() => setIsEditingInfo(true)} title="Edit Header" style={{background: 'var(--input-bg)', color: 'var(--text-color)', padding: '4px', borderRadius: '4px', border: '1px solid var(--border-color)', cursor: 'pointer', display: 'flex', alignItems: 'center', margin: 0}}>
                        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"></path><path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"></path></svg>
                      </button>
                    )}
                  </div>

                  <div style={{display: 'flex', flexWrap: 'wrap', alignItems: 'center', gap: '20px', marginBottom: '5px'}}>
                    
                    <div style={{display: 'flex', alignItems: 'center', gap: '10px', flex: 2}}>
                      <span style={{color: 'var(--text-color)', opacity: 0.7, whiteSpace: 'nowrap'}}>Start Time:</span>
                      {isEditingInfo ? (
                        <div style={{flex: 1, display: 'flex', alignItems: 'center', gap: '5px'}}>
                          <div style={{position: 'relative', width: '24px', height: '24px', flexShrink: 0}}>
                            <input type="date" onChange={(e) => { 
                              if (e.target.value) { 
                                const timePart = editTimecode.indexOf('T') !== -1 ? editTimecode.substring(editTimecode.indexOf('T')) : 'T00:00:00.000000Z';
                                setEditTimecode(e.target.value + timePart); 
                              }
                            }} style={{position: 'absolute', opacity: 0, top: 0, left: 0, width: '100%', height: '100%', cursor: 'pointer'}} />
                            <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" style={{pointerEvents: 'none'}}><rect x="3" y="4" width="18" height="18" rx="2" ry="2"></rect><line x1="16" y1="2" x2="16" y2="6"></line><line x1="8" y1="2" x2="8" y2="6"></line><line x1="3" y1="10" x2="21" y2="10"></line></svg>
                          </div>
                          <span style={{opacity: 0.5}}>{editTimecode.indexOf('T') !== -1 ? editTimecode.substring(0, editTimecode.indexOf('T')) : ''}</span>
                          <input type="text" placeholder="HH:MM:SS.DDDDDD" value={editTimecode.indexOf('T') !== -1 ? editTimecode.substring(editTimecode.indexOf('T')+1, editTimecode.indexOf('Z') !== -1 ? editTimecode.indexOf('Z') : editTimecode.length) : "00:00:00.000000"} onChange={(e) => {
                            const datePart = editTimecode.indexOf('T') !== -1 ? editTimecode.substring(0, editTimecode.indexOf('T')) : '1970-01-01';
                            setEditTimecode(datePart + 'T' + e.target.value + 'Z');
                          }} style={{flex: 1, padding: '2px', fontSize: '0.8rem', background: 'var(--input-bg)', color: 'var(--text-color)', border: '1px solid var(--border-color)'}} />
                        </div>
                      ) : (
                        <span style={{fontWeight: 'bold', fontSize: '0.9rem', wordBreak: 'break-all'}}>{formatJ1950ToISO(fileInfo.timecode)}</span>
                      )}
                    </div>
                    
                    <div style={{display: 'flex', alignItems: 'center', gap: '10px', flex: 1, minWidth: '150px'}}>
                      <span style={{color: 'var(--text-color)', opacity: 0.7, whiteSpace: 'nowrap'}}>Freq (MHz):</span>
                      {isEditingInfo ? (
                        <div style={{flex: 1}}>
                          <input type="text" value={editFreq} onChange={e => setEditFreq(e.target.value)} style={{width: '100%', padding: '2px', fontSize: '0.8rem', boxSizing: 'border-box'}} />
                        </div>
                      ) : (
                        <span style={{fontWeight: 'bold', flex: 1}}>{fileInfo.center_freq}</span>
                      )}
                    </div>

                  </div>
                </div>
              </div>
            )}
          </div>
          <div className="form-group" style={{display: 'flex', gap: '10px', alignItems: 'flex-end'}}>
            <div style={{flex: 1}}>
              <label>DSP Processing</label>
              <button 
                disabled={!zoomBounds} 
                onClick={openTuner}
                style={{
                  width: '100%', 
                  margin: 0, 
                  background: zoomBounds ? 'var(--accent-color)' : 'var(--border-color)',
                  color: zoomBounds ? '#000' : 'var(--text-color)',
                  opacity: zoomBounds ? 1 : 0.5,
                  cursor: zoomBounds ? 'pointer' : 'not-allowed'
                }}>
                Tuner (DDC)
              </button>
            </div>
          </div>
          <div className="form-group" style={{display: 'flex', gap: '10px'}}>
            <div style={{flex: 1}}>
              <label>FFT Size</label>
              <select value={windowSize} onChange={(e) => setWindowSize(e.target.value)}>
                {[5,6,7,8,9,10,11,12,13,14,15,16,17].map(i => {
                  const val = Math.pow(2, i);
                  return <option key={val} value={val}>{val}</option>;
                })}
              </select>
            </div>
            <div style={{flex: 1}}>
              <label>Smoothing {smoothing > 0 ? `(${smoothing})` : '(Off)'}</label>
              <input type="range" min="0" max="64" value={smoothing} onChange={(e) => setSmoothing(Number(e.target.value))} style={{width: '100%', accentColor: 'var(--accent-color)'}} />
            </div>
          </div>
          
          <h3 style={{marginTop: '15px', borderBottom: '1px solid var(--border-color)', paddingBottom: '3px', fontSize: '1.1rem'}}>Interactive Plot Options</h3>
          
          <div className="form-group" style={{display: 'flex', gap: '10px'}}>
            <div style={{flex: 1}}>
              <label>Spectrum Color</label>
              <select value={fftColor} onChange={(e) => setFftColor(e.target.value)}>
                <option value="#00ff00">Green</option>
                <option value="#00ffff">Cyan</option>
                <option value="#ffff00">Yellow</option>
                <option value="#ff00ff">Magenta</option>
                <option value="#ffffff">White</option>
                <option value="#ff0000">Red</option>
              </select>
            </div>
            <div style={{flex: 1}}>
              <label>Color Map</label>
              <ColormapSelect 
                value={sigplotColormap} 
                onChange={(v) => setSigplotColormap(Number(v))}
                options={[
                  {value: 10, label: 'BuGn'},
                  {value: 4, label: 'calewhite'},
                  {value: 8, label: 'Cold'},
                  {value: 2, label: 'Color Wheel'},
                  {value: 0, label: 'Greyscale'},
                  {value: 14, label: 'GreyNRed'},
                  {value: 7, label: 'Hot'},
                  {value: 1, label: 'Ramp Colormap'},
                  {value: 3, label: 'Spectrum'},
                  {value: 6, label: 'Sunset'},
                  {value: 12, label: 'YlGnBu'},
                  {value: 11, label: 'YlOrBr'},
                  {value: 13, label: 'YlOrRd'}
                ]}
                gradients={{
                  0: 'linear-gradient(to right, #000000, #ffffff)',
                  1: 'linear-gradient(to right, #000080, #0000ff, #00ffff, #ffff00, #ff0000, #800000)',
                  2: 'linear-gradient(to right, #000, #fff)',
                  3: 'linear-gradient(to right, #000080, #0000ff, #00ffff, #ffff00, #ff0000, #800000)',
                  4: 'linear-gradient(to right, #ffffff, #000000)',
                  6: 'linear-gradient(to right, #000000, #4400aa, #dd3344, #ffaa00, #ffffff)',
                  7: 'linear-gradient(to right, #000000, #ff0000, #ffff00, #ffffff)',
                  8: 'linear-gradient(to right, #000000, #0000ff, #00ffff, #ffffff)',
                  10: 'linear-gradient(to right, #e5f5f9, #99d8c9, #2ca25f)',
                  11: 'linear-gradient(to right, #fff7bc, #fec44f, #d95f0e)',
                  12: 'linear-gradient(to right, #edf8b1, #7fcdbb, #2c7fb8)',
                  13: 'linear-gradient(to right, #ffeda0, #feb24c, #f03b20)',
                  14: 'linear-gradient(to right, #000000, #888888, #ff0000)'
                }}
              />
            </div>
          </div>
          
          <div style={{display: 'flex', gap: '10px'}}>
            <button onClick={() => handleInteractiveFFT()} disabled={loading}>Spectrum (FFT)</button>
            <button onClick={() => handleInteractivePSD()} disabled={loading}>Waterfall (PSD)</button>
          </div>
          
          {loading && <p style={{color: 'var(--accent-color)', fontWeight: 'bold', margin: '5px 0'}}>Processing...</p>}

          <h3 style={{marginTop: '15px', borderBottom: '1px solid var(--border-color)', paddingBottom: '3px', fontSize: '1.1rem'}}>Static Plot Options</h3>
          <div className="form-group" style={{display: 'flex', gap: '10px'}}>
            <div style={{flex: 1}}>
              <label>Fill Mode</label>
              <select value={fillMode} onChange={(e) => setFillMode(e.target.value)}>
                <option value="gradient">Gradient</option>
                <option value="solid">Solid</option>
                <option value="none">None</option>
              </select>
              {fillMode !== 'gradient' ? (
                <div style={{marginTop: '5px'}}>
                  <input type="color" value={fillColor} onChange={(e) => setFillColor(e.target.value)} style={{width: '100%', height: '25px', border: 'none', padding: '0', background: 'transparent', cursor: 'pointer'}} />
                </div>
              ) : (
                <div style={{marginTop: '5px', height: '25px', borderRadius: '3px', background: 
                  colormap === 'jet' ? 'linear-gradient(to right, #00007f, #0000ff, #00ffff, #ffff00, #ff0000, #7f0000)' :
                  colormap === 'electric' ? 'linear-gradient(to right, #000000, #000064, #0000ff, #00ffff, #ffff00, #ffffff)' :
                  colormap === 'turbo' ? 'linear-gradient(to right, #30123b, #4288eb, #28fb95, #a4fc3c, #f39913, #d12907, #7a0403)' :
                  colormap === 'pablo' ? 'linear-gradient(to right, #000000, #0000ff, #00ff00, #ffff00, #ff0000, #ffffff)' :
                  colormap === 'frog' ? 'linear-gradient(to right, #000000, #005000, #00b400, #00ff00, #ffff00, #ff0000, #ffffff)' :
                  colormap === 'grape' ? 'linear-gradient(to right, #000000, #3c0078, #9600c8, #dc00ff, #ff64ff, #b4ff00, #ffffff)' :
                  colormap === 'gqrx' ? 'linear-gradient(to right, #000000, #000096, #0096ff, #ffff00, #ff0000, #ffffff)' :
                  colormap === 'websdr' ? 'linear-gradient(to right, #000000, #500096, #ff0000, #ffff00, #ffffff)' : '#888'
                }}></div>
              )}
            </div>
            <div style={{flex: 1}}>
              <label>Color Map</label>
              <ColormapSelect
                value={colormap}
                onChange={setColormap}
                options={[
                  {value: 'electric', label: 'Electric'},
                  {value: 'frog', label: 'Frog'},
                  {value: 'gqrx', label: 'GQRX'},
                  {value: 'grape', label: 'Grape'},
                  {value: 'jet', label: 'Jet'},
                  {value: 'pablo', label: 'Pablo'},
                  {value: 'turbo', label: 'Turbo'},
                  {value: 'websdr', label: 'WebSDR'}
                ]}
                gradients={{
                  electric: 'linear-gradient(to right, #000000, #000064, #0000ff, #00ffff, #ffff00, #ffffff)',
                  frog: 'linear-gradient(to right, #000000, #005000, #00b400, #00ff00, #ffff00, #ff0000, #ffffff)',
                  gqrx: 'linear-gradient(to right, #000000, #000096, #0096ff, #ffff00, #ff0000, #ffffff)',
                  grape: 'linear-gradient(to right, #000000, #3c0078, #9600c8, #dc00ff, #ff64ff, #b4ff00, #ffffff)',
                  jet: 'linear-gradient(to right, #00007f, #0000ff, #00ffff, #ffff00, #ff0000, #7f0000)',
                  pablo: 'linear-gradient(to right, #000000, #0000ff, #00ff00, #ffff00, #ff0000, #ffffff)',
                  turbo: 'linear-gradient(to right, #30123b, #4288eb, #28fb95, #a4fc3c, #f39913, #d12907, #7a0403)',
                  websdr: 'linear-gradient(to right, #000000, #500096, #ff0000, #ffff00, #ffffff)'
                }}
              />
            </div>
          </div>

          <div style={{display: 'flex', gap: '10px', marginTop: '5px'}}>
            <button onClick={() => handleStaticPlot(true)} disabled={loading}>Spectrum (FFT)</button>
            <button onClick={() => handleStaticPlot(false)} disabled={loading}>Waterfall (PSD)</button>
          </div>
          
        </div>

        
        {/* Visualization Panel */}
        <div className="plot-container" style={{
            flex: 3, 
            display: 'grid', 
            gap: '10px',
            gridTemplateColumns: panels.length === 1 ? '1fr' : 
                                 panels.length === 2 ? (layoutMode === 'horizontal' || layoutMode === 'auto' ? '1fr 1fr' : '1fr') :
                                 panels.length === 3 ? (layoutMode === 'vertical' ? '1fr' : '1fr 1fr 1fr') :
                                 panels.length === 4 ? '1fr 1fr' :
                                 panels.length <= 6 ? '1fr 1fr 1fr' :
                                 '1fr 1fr 1fr 1fr',
            gridTemplateRows: panels.length === 1 ? '1fr' :
                              panels.length === 2 ? (layoutMode === 'horizontal' || layoutMode === 'auto' ? '1fr' : '1fr 1fr') :
                              panels.length === 3 ? (layoutMode === 'vertical' ? '1fr 1fr 1fr' : '1fr') :
                              panels.length <= 6 ? '1fr 1fr' :
                              '1fr 1fr 1fr',
        }}>
          {panels.map(panel => (
            <div key={panel.id} className="panel" style={{ flex: 1, display: 'flex', flexDirection: 'column', position: 'relative' }}>
              <div style={{display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '5px'}}>
                  <h2 style={{marginTop: 0, marginBottom: 0, fontSize: '0.9rem'}}>{panel.title}</h2>
                  <button onClick={() => setPanels(prev => prev.filter(p => p.id !== panel.id))} style={{width: '20px', height: '20px', padding: 0, display: 'flex', alignItems: 'center', justifyContent: 'center', borderRadius: '4px', background: 'var(--accent-color)', color: '#000', margin: 0, flexShrink: 0}} title="Close Panel">
                      <svg width="12" height="12"  viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="3" strokeLinecap="round" strokeLinejoin="round"><line x1="18" y1="6" x2="6" y2="18"></line><line x1="6" y1="6" x2="18" y2="18"></line></svg>
                  </button>
              </div>

              {panel.type === 'interactive' ? (
                  <div style={{flex: 1, display: 'flex', gap: '2px'}}>
                    <div style={{flex: 1, position: 'relative', minHeight: '200px'}}>
                      {loading && panel.url === '' && <div style={{position: 'absolute', top: 0, left: 0, right: 0, bottom: 0, background: 'rgba(0,0,0,0.5)', zIndex: 10, display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#fff', fontSize: '1.2rem'}}>Processing...</div>}
                      {panel.url ? (
                        <SigPlot dataUrl={panel.url} type={panel.subType} zmin={zmin} zmax={zmax} theme={theme} fftColor={fftColor} sigplotColormap={sigplotColormap} onDataLoaded={handleDataLoaded} onZoom={setZoomBounds} />
                      ) : (
                        <div style={{position: 'absolute', top: 0, left: 0, right: 0, bottom: 0, display: 'flex', alignItems: 'center', justifyContent: 'center'}}>
                            <p style={{margin: 0}}>Select an interactive mode to load sigplot.</p>
                        </div>
                      )}
                    </div>
                    <div style={{display: 'flex', alignItems: 'center', justifyContent: 'center', width: '15px', marginLeft: '5px'}}>
                      <div style={{fontSize: '0.8rem', writingMode: 'vertical-rl', transform: 'rotate(180deg)', color: 'var(--text-color)', fontWeight: 'bold'}}>Gain (dB)</div>
                    </div>

                    <div style={{width: '35px', display: 'flex', flexDirection: 'column', alignItems: 'center', padding: '5px 0'}}>
                      <div style={{fontSize: '0.7rem', fontWeight: 'bold', marginBottom: '8px'}}>{zmax === '' ? 'Auto' : zmax}</div>
                      <div style={{flex: 1, width: '100%', display: 'flex', justifyContent: 'center'}}>
                        <Slider 
                          vertical 
                          range 
                          min={gainBounds[0]} 
                          max={gainBounds[1]} 
                          value={[
                            zmin === '' ? gainBounds[0] + 10 : Number(zmin), 
                            zmax === '' ? gainBounds[1] - 10 : Number(zmax)
                          ]} 
                          onChange={(val) => { setZmin(val[0]); setZmax(val[1]); }}
                          handleStyle={[
                            { width: 16, height: 16, borderRadius: 2, left: 5, backgroundColor: '#ffd700', borderColor: '#ffd700', zIndex: 100, cursor: 'ns-resize', opacity: 1, boxShadow: 'none' },
                            { width: 16, height: 16, borderRadius: 2, left: 5, backgroundColor: '#ffd700', borderColor: '#ffd700', zIndex: 100, cursor: 'ns-resize', opacity: 1, boxShadow: 'none' }
                          ]}
                          activeHandleStyle={[
                            { backgroundColor: '#ffd700', borderColor: '#ffd700', boxShadow: 'none', width: 16, height: 16, borderRadius: 2, left: 5, zIndex: 100 },
                            { backgroundColor: '#ffd700', borderColor: '#ffd700', boxShadow: 'none', width: 16, height: 16, borderRadius: 2, left: 5, zIndex: 100 }
                          ]}
                          trackStyle={[{ backgroundColor: 'var(--accent-color)' }]}
                          railStyle={{ backgroundColor: 'var(--border-color)' }}
                        />
                      </div>
                      <div style={{fontSize: '0.7rem', fontWeight: 'bold', marginTop: '8px'}}>{zmin === '' ? 'Auto' : zmin}</div>
                    </div>
                  </div>
              ) : (
                  <div style={{flex: 1, position: 'relative', minHeight: '200px', overflow: 'hidden'}}>
                    {panel.url ? (
                      <div style={{position: 'absolute', top: 0, left: 0, right: 0, bottom: 0}}>
                        <img src={panel.url} alt="DSP Plotter Output" style={{width: '100%', height: '100%', objectFit: 'fill', border: '1px solid var(--border-color)', borderRadius: '4px', boxSizing: 'border-box'}} />
                      </div>
                    ) : (
                      <div style={{position: 'absolute', top: 0, left: 0, right: 0, bottom: 0, display: 'flex', alignItems: 'center', justifyContent: 'center'}}>
                        <p style={{margin: 0}}>Select static plot to view image.</p>
                      </div>
                    )}
                  </div>
              )}
            </div>
          ))}
          {panels.length === 0 && (
             <div style={{display: 'flex', alignItems: 'center', justifyContent: 'center', flex: 1, background: 'var(--panel-bg)', borderRadius: '8px', border: '1px dashed var(--border-color)'}}>
                 <h2 style={{opacity: 0.5}}>No panels open. Select an option from Controls.</h2>
             </div>
          )}
        </div>

      </div>
      
      {/* Raw File Modal */}
      {showImportModal && (
        <div style={{position: 'fixed', top: 0, left: 0, right: 0, bottom: 0, background: 'rgba(0,0,0,0.7)', display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 1000}}>
          <div style={{background: 'var(--panel-bg)', padding: '20px', borderRadius: '8px', border: '1px solid var(--border-color)', width: '350px'}}>
            <h3 style={{marginTop: 0}}>Import File</h3>
            <p style={{fontSize: '0.9rem', marginBottom: '15px'}}>
              Input filename: <br/>
              <b style={{display: 'inline-block', marginTop: '4px', wordBreak: 'break-all', background: 'var(--input-bg)', padding: '4px 8px', borderRadius: '4px', border: '1px solid var(--border-color)', width: '100%', boxSizing: 'border-box'}}>{importFileName}</b>
            </p>
            
            <div className="form-group">
              <label>Output Filename</label>
              <input type="text" value={importOutName} onChange={e => setImportOutName(e.target.value)} />
            </div>
            
            <div className="form-group">
              <label>Data Format</label>
              <select value={importFormat} onChange={e => setImportFormat(e.target.value)}>
                <option value="CF">Complex Float32 (CF)</option>
                <option value="SF">Real Float32 (SF)</option>
                <option value="CI">Complex Int16 (CI)</option>
                <option value="SI">Real Int16 (SI)</option>
                <option value="CB">Complex Int8 (CB)</option>
                <option value="SB">Real Int8 (SB)</option>
              </select>
            </div>
            <div style={{display: 'flex', gap: '10px'}}>
              <div className="form-group" style={{flex: 1}}>
                <label>Sample Rate (MHz)</label>
                <input type="text" value={importRate} onChange={e => {
                  let val = e.target.value;
                  setImportRate(val);
                }} />
              </div>
              <div className="form-group" style={{flex: 1}}>
                <label>Center Freq (MHz)</label>
                <input type="text" value={importFreq} onChange={e => {
                  let val = e.target.value;
                  setImportFreq(val);
                }} />
              </div>
            </div>
            <div className="form-group">
              <label>Start Date/Time (ISO, J1970, J1950)</label>
              <div style={{display: 'flex', alignItems: 'center', gap: '5px'}}>
                <div style={{position: 'relative', width: '28px', height: '28px', background: 'var(--input-bg)', borderRadius: '4px', border: '1px solid var(--border-color)', display: 'flex', alignItems: 'center', justifyContent: 'center'}}>
                  <input type="date" onChange={(e) => { 
                    if (e.target.value) { 
                      const timePart = importTimecode.indexOf('T') !== -1 ? importTimecode.substring(importTimecode.indexOf('T')) : 'T00:00:00.000000Z';
                      setImportTimecode(e.target.value + timePart); 
                    }
                  }} style={{position: 'absolute', opacity: 0, top: 0, left: 0, width: '100%', height: '100%', cursor: 'pointer'}} />
                  <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" style={{pointerEvents: 'none'}}><rect x="3" y="4" width="18" height="18" rx="2" ry="2"></rect><line x1="16" y1="2" x2="16" y2="6"></line><line x1="8" y1="2" x2="8" y2="6"></line><line x1="3" y1="10" x2="21" y2="10"></line></svg>
                </div>
                <span style={{opacity: 0.5, fontSize: '0.9rem'}}>{importTimecode.indexOf('T') !== -1 ? importTimecode.substring(0, importTimecode.indexOf('T')) : ''}</span>
                <input type="text" placeholder="HH:MM:SS.DDDDDD" value={importTimecode.indexOf('T') !== -1 ? importTimecode.substring(importTimecode.indexOf('T')+1, importTimecode.indexOf('Z') !== -1 ? importTimecode.indexOf('Z') : importTimecode.length) : "00:00:00.000000"} onChange={(e) => {
                  const datePart = importTimecode.indexOf('T') !== -1 ? importTimecode.substring(0, importTimecode.indexOf('T')) : '1970-01-01';
                  setImportTimecode(datePart + 'T' + e.target.value + 'Z');
                }} style={{flex: 1, padding: '4px', fontSize: '0.9rem'}} />
              </div>
            </div>

            
            <div style={{marginTop: '15px', padding: '8px', background: 'var(--input-bg)', borderRadius: '4px', fontSize: '0.8rem', display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px'}}>
              <div><span style={{opacity: 0.7}}>File Size:</span> <b>{(importFileSize / (1024*1024)).toFixed(2)} MB</b></div>
              <div><span style={{opacity: 0.7}}>Rate:</span> <b>{importRate} MHz</b></div>
              {(() => {
                const bps = importFormat.includes('F') ? 8 : (importFormat.includes('I') ? 4 : 2);
                const channels = importFormat.startsWith('C') ? 2 : 1;
                const headerSize = isWavImport ? 44 : 0;
                const dataSize = Math.max(0, importFileSize - headerSize);
                
                let totalBytes = 8;
                if (importFormat === 'CF') totalBytes = 8;
                else if (importFormat === 'SF' || importFormat === 'CI') totalBytes = 4;
                else if (importFormat === 'SI' || importFormat === 'CB') totalBytes = 2;
                else if (importFormat === 'SB') totalBytes = 1;
                
                const totalSamples = Math.floor(dataSize / totalBytes);
                const dur = importRate > 0 ? (totalSamples / (importRate * 1e6)) : 0;
                return (
                  <>
                    <div><span style={{opacity: 0.7}}>Samples:</span> <b>{(totalSamples / 1e6).toFixed(2)}M</b></div>
                    <div><span style={{opacity: 0.7}}>Duration:</span> <b>{dur.toFixed(3)}s</b></div>
                  </>
                );
              })()}
            </div>

            {uploadProgress >= 0 && (
              <div style={{ marginTop: '15px' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '0.8rem', marginBottom: '4px' }}>
                  <span>{uploadProgress < 100 ? 'Uploading...' : 'Converting...'}</span>
                  <span>{Math.round(uploadProgress)}% - {elapsedTime}s</span>
                </div>
                <div style={{ width: '100%', height: '10px', background: 'var(--border-color)', borderRadius: '5px', overflow: 'hidden' }}>
                  <div style={{ width: `${uploadProgress}%`, height: '100%', background: 'var(--accent-color)', transition: 'width 0.2s' }}></div>
                </div>
                {uploadProgress < 100 && elapsedTime > 0 && (
                  <div style={{ fontSize: '0.75rem', marginTop: '4px', textAlign: 'right', opacity: 0.8 }}>
                    {((importFileSize * (uploadProgress/100)) / (1024*1024) / elapsedTime).toFixed(1)} MB/s
                  </div>
                )}
              </div>
            )}

            <div style={{display: 'flex', gap: '10px', marginTop: '20px'}}>
              <button onClick={cancelImport} style={{background: 'var(--input-bg)', color: 'var(--text-color)'}}>Cancel</button>
              <button onClick={startImport} disabled={uploadProgress >= 0} style={{opacity: uploadProgress >= 0 ? 0.5 : 1}}>Import</button>
            </div>
          </div>
        </div>
      )}

      {showTunerModal && (
        <div style={{
          position: 'fixed', top: 0, left: 0, right: 0, bottom: 0,
          background: 'rgba(0,0,0,0.8)', zIndex: 9999,
          display: 'flex', alignItems: 'center', justifyContent: 'center'
        }}>
          <div style={{
            background: 'var(--panel-bg)', padding: '20px', borderRadius: '8px',
            width: '400px', border: '1px solid var(--border-color)', boxShadow: '0 10px 30px rgba(0,0,0,0.5)'
          }}>
            <h2 style={{marginTop: 0, color: 'var(--accent-color)'}}>Tuner Setup</h2>
            <div style={{display: 'flex', flexDirection: 'column', gap: '15px'}}>
              <div style={{background: 'var(--input-bg)', padding: '10px', borderRadius: '4px', fontSize: '0.9rem', display: 'flex', flexDirection: 'column', gap: '5px'}}>
                <div><strong>Center Freq:</strong> {tunerParams.center.toFixed(4)} MHz</div>
                <div><strong>Bandwidth:</strong> {tunerParams.bw.toFixed(4)} MHz</div>
                {tunerParams.dur > 0 && (
                  <>
                    <div><strong>Start Time:</strong> {tunerParams.start.toFixed(4)} s</div>
                    <div><strong>Duration:</strong> {tunerParams.dur.toFixed(4)} s</div>
                  </>
                )}
              </div>
              <div>
                <label style={{display: 'block', marginBottom: '5px'}}>Output Filename</label>
                <input 
                  type="text" 
                  value={tunerOutName} 
                  onChange={e => setTunerOutName(e.target.value)}
                  style={{width: '100%', padding: '8px', boxSizing: 'border-box'}}
                />
              </div>
              <div style={{display: 'flex', gap: '10px', marginTop: '10px'}}>
                <button onClick={() => setShowTunerModal(false)} style={{flex: 1, background: 'var(--border-color)', margin: 0}}>Cancel</button>
                <button onClick={startTuner} style={{flex: 1, background: 'var(--accent-color)', color: '#000', margin: 0, fontWeight: 'bold'}}>Tune</button>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

export default App;
