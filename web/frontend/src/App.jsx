import React, { useState, useEffect, useRef } from 'react';
import SigPlot from './components/SigPlot.jsx';

function App() {
  const [file, setFile] = useState('timecode.prm');
  const [availableFiles, setAvailableFiles] = useState([]);
  const [centerFreq, setCenterFreq] = useState(0);
  const [windowSize, setWindowSize] = useState('auto');
  const [smoothing, setSmoothing] = useState(1);
  const [width, setWidth] = useState('auto');
  const [height, setHeight] = useState('auto');
  const [colormap, setColormap] = useState('jet');
  
  const [zmin, setZmin] = useState('');
  const [zmax, setZmax] = useState('');

  const [imagePlot, setImagePlot] = useState('');
  const [sigplotUrl, setSigplotUrl] = useState('');
  const [sigplotType, setSigplotType] = useState('1D');
  const [loading, setLoading] = useState(false);
  const [theme, setTheme] = useState('dark');

  const interactivePanelRef = useRef(null);
  const staticPanelRef = useRef(null);

  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
  }, [theme]);

  const toggleTheme = () => {
    setTheme(theme === 'dark' ? 'light' : 'dark');
  };

  const fetchFiles = async () => {
    try {
      const res = await fetch('/api/files');
      const data = await res.json();
      if (data.files) {
        setAvailableFiles(data.files);
        if (!data.files.includes(file) && data.files.length > 0) {
          setFile(data.files[0]);
        }
      }
    } catch (e) {
      console.error("Error fetching files", e);
    }
  };

  useEffect(() => {
    fetchFiles();
  }, []);

  const handleUpload = async (e) => {
    if (!e.target.files || e.target.files.length === 0) return;
    const uploadData = new FormData();
    uploadData.append('file', e.target.files[0]);
    
    setLoading(true);
    try {
      const res = await fetch('/api/upload', {
        method: 'POST',
        body: uploadData
      });
      if (res.ok) {
        await fetchFiles();
        setFile(e.target.files[0].name);
      } else {
        alert("Upload failed.");
      }
    } catch (err) {
      alert("Error uploading: " + err);
    }
    setLoading(false);
  };

  const getComputedSize = (val, ref, isWidth) => {
    if (val !== 'auto' && val !== '') return Number(val);
    if (!ref.current) return isWidth ? 1024 : 512;
    return isWidth ? ref.current.clientWidth : ref.current.clientHeight;
  };

  const getComputedFftSize = (val, ref) => {
    if (val !== 'auto' && val !== '') return Number(val);
    if (!ref.current) return 1024;
    // Nearest power of 2 for FFT size based on panel width
    return Math.pow(2, Math.round(Math.log2(ref.current.clientWidth)));
  };

  const handleStaticPlot = async (isFft) => {
    setLoading(true);
    try {
      const reqWidth = getComputedSize(width, staticPanelRef, true);
      const reqHeight = getComputedSize(height, staticPanelRef, false);
      const res = await fetch('/api/run/plot', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          input_file: file,
          center_freq: centerFreq,
          zoom_bw: 0,
          width: reqWidth,
          height: reqHeight,
          smoothing: smoothing,
          colormap: colormap,
          plot_fft: isFft,
          plot_waterfall: !isFft
        })
      });
      const data = await res.json();
      if (!res.ok) {
        alert("Error from server:\n" + (data.detail || JSON.stringify(data)));
        setLoading(false);
        return;
      }
      if (data.output_file) {
        setImagePlot(`/api/data/${data.output_file}`);
      }
    } catch (e) {
      alert("Error generating plot: " + e);
    }
    setLoading(false);
  };

  const handleInteractivePSD = async () => {
    setLoading(true);
    try {
      const reqWindowSize = getComputedFftSize(windowSize, interactivePanelRef);
      const res = await fetch('/api/run/psd', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          input_file: file,
          center_freq: centerFreq,
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
        setSigplotUrl(`/api/data/${data.output_file}`);
      }
    } catch (e) {
      alert("Error generating PSD: " + e);
    }
    setLoading(false);
  };

  const handleInteractiveFFT = async () => {
    setLoading(true);
    try {
      const reqWindowSize = getComputedFftSize(windowSize, interactivePanelRef);
      const res = await fetch('/api/run/fft', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          input_file: file,
          center_freq: centerFreq,
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
        setSigplotUrl(`/api/data/${data.output_file}`);
      }
    } catch (e) {
      alert("Error generating FFT: " + e);
    }
    setLoading(false);
  };

  return (
    <div className="container">
      <div style={{display: 'flex', justifyContent: 'space-between', alignItems: 'center'}}>
        <h1>DSP Tools Interface</h1>
        <button onClick={toggleTheme} style={{width: 'auto', margin: 0}}>{theme === 'dark' ? '☀️ Light' : '🌙 Dark'}</button>
      </div>
      <div className="content">
        
        {/* Controls Panel */}
        <div className="panel" style={{flex: 1, overflowY: 'auto'}}>
          <h2 style={{marginTop: 0, fontSize: '1.2rem'}}>Controls</h2>
          <div className="form-group">
            <label>Input File</label>
            <div style={{ display: 'flex', gap: '5px' }}>
              <select value={file} onChange={(e) => setFile(e.target.value)} style={{ flex: 1 }}>
                {availableFiles.map(f => <option key={f} value={f}>{f}</option>)}
                {availableFiles.length === 0 && <option value={file}>{file}</option>}
              </select>
              <input type="file" accept=".prm,.wav,.bin" onChange={handleUpload} style={{ display: 'none' }} id="file-upload" />
              <label htmlFor="file-upload" style={{
                background: 'var(--border-color)', padding: '6px 10px', borderRadius: '4px', cursor: 'pointer', margin: 0, fontWeight: 'normal', whiteSpace: 'nowrap'
              }}>Upload</label>
            </div>
          </div>
          <div className="form-group">
            <label>Center Frequency (Hz)</label>
            <input type="number" value={centerFreq} onChange={(e) => setCenterFreq(Number(e.target.value))} />
          </div>
          <div className="form-group" style={{display: 'flex', gap: '10px'}}>
            <div style={{flex: 1}}>
              <label>FFT Size</label>
              <input type="text" value={windowSize} onChange={(e) => setWindowSize(e.target.value)} placeholder="auto" />
            </div>
            <div style={{flex: 1}}>
              <label>Smoothing</label>
              <input type="number" value={smoothing} onChange={(e) => setSmoothing(Number(e.target.value))} min="1" />
            </div>
          </div>
          
          <h3 style={{marginTop: '15px', borderBottom: '1px solid var(--border-color)', paddingBottom: '3px', fontSize: '1.1rem'}}>Interactive Plot Options</h3>
          
          <div className="form-group" style={{display: 'flex', gap: '10px'}}>
            <div style={{flex: 1}}>
              <label>Gain Min (zmin)</label>
              <input type="number" value={zmin} onChange={(e) => setZmin(e.target.value)} placeholder="Auto" />
            </div>
            <div style={{flex: 1}}>
              <label>Gain Max (zmax)</label>
              <input type="number" value={zmax} onChange={(e) => setZmax(e.target.value)} placeholder="Auto" />
            </div>
          </div>
          
          <div style={{display: 'flex', gap: '10px'}}>
            <button onClick={handleInteractiveFFT} disabled={loading}>Spectrum (FFT)</button>
            <button onClick={handleInteractivePSD} disabled={loading}>Waterfall (PSD)</button>
          </div>
          
          {loading && <p style={{color: 'var(--accent-color)', fontWeight: 'bold', margin: '5px 0'}}>Processing...</p>}

          <h3 style={{marginTop: '15px', borderBottom: '1px solid var(--border-color)', paddingBottom: '3px', fontSize: '1.1rem'}}>Static Plot Options</h3>
          <div className="form-group" style={{display: 'flex', gap: '10px'}}>
            <div style={{flex: 1}}>
              <label>Width</label>
              <input type="text" value={width} onChange={(e) => setWidth(e.target.value)} placeholder="auto" />
            </div>
            <div style={{flex: 1}}>
              <label>Height</label>
              <input type="text" value={height} onChange={(e) => setHeight(e.target.value)} placeholder="auto" />
            </div>
          </div>
          <div className="form-group" style={{display: 'flex', gap: '10px'}}>
            <div style={{flex: 1}}>
              <label>Colormap</label>
              <select value={colormap} onChange={(e) => setColormap(e.target.value)}>
                <option value="jet">Jet</option>
                <option value="gqrx">GQRX</option>
                <option value="turbo">Turbo</option>
                <option value="electric">Electric</option>
                <option value="websdr">WebSDR</option>
                <option value="pablo">Pablo</option>
                <option value="frog">Frog</option>
                <option value="grape">Grape</option>
              </select>
            </div>
          </div>

          <div style={{display: 'flex', gap: '10px', marginTop: '5px'}}>
            <button onClick={() => handleStaticPlot(true)} disabled={loading}>Spectrum (FFT)</button>
            <button onClick={() => handleStaticPlot(false)} disabled={loading}>Waterfall (PSD)</button>
          </div>
          
        </div>

        {/* Visualization Panel */}
        <div className="plot-container" style={{flex: 2, display: 'flex', flexDirection: 'column'}}>
          
          <div className="panel" style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
            <h2 style={{marginTop: 0, fontSize: '1.2rem'}}>Interactive SigPlot View</h2>
            <div ref={interactivePanelRef} style={{flex: 1, position: 'relative', minHeight: '200px'}}>
              {sigplotUrl ? (
                <SigPlot dataUrl={sigplotUrl} type={sigplotType} zmin={zmin} zmax={zmax} theme={theme} />
              ) : (
                <p style={{margin: 0}}>Select an interactive mode to load sigplot.</p>
              )}
            </div>
          </div>

          <div className="panel" style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
            <h2 style={{marginTop: 0, fontSize: '1.2rem'}}>Static Plot Output</h2>
            <div ref={staticPanelRef} style={{flex: 1, position: 'relative', minHeight: '200px', display: 'flex', alignItems: 'center', justifyContent: 'center', overflow: 'hidden'}}>
              {imagePlot ? (
                <img src={imagePlot} alt="DSP Plotter Output" style={{width: '100%', height: '100%', objectFit: 'contain', border: '1px solid var(--border-color)', borderRadius: '4px'}} />
              ) : (
                <p style={{margin: 0}}>Select static plot to view image.</p>
              )}
            </div>
          </div>

        </div>

      </div>
    </div>
  );
}

export default App;
