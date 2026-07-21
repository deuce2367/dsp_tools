import React, { useEffect, useRef, useState } from 'react';

const ConstellationPlot = ({ dataUrl, theme = 'dark', color = '#00ff00', zmin, zmax }) => {
  const canvasRef = useRef(null);
  const containerRef = useRef(null);
  const [iqData, setIqData] = useState(null);
  const [error, setError] = useState(null);
  
  // Viewport bounds
  const [xBounds, setXBounds] = useState({ min: zmin !== undefined ? parseFloat(zmin) : -1.0, max: zmax !== undefined ? parseFloat(zmax) : 1.0 });
  const [yBounds, setYBounds] = useState({ min: zmin !== undefined ? parseFloat(zmin) : -1.0, max: zmax !== undefined ? parseFloat(zmax) : 1.0 });
  
  // Handle zooming/panning
  const isDragging = useRef(false);
  const lastMousePos = useRef({ x: 0, y: 0 });

  useEffect(() => {
    if (zmin !== '' && zmin !== undefined && zmax !== '' && zmax !== undefined) {
      setXBounds({ min: parseFloat(zmin), max: parseFloat(zmax) });
      setYBounds({ min: parseFloat(zmin), max: parseFloat(zmax) });
    }
  }, [zmin, zmax]);

  useEffect(() => {
    if (!dataUrl) return;
    
    let isMounted = true;
    
    const fetchData = async () => {
      try {
        const res = await fetch(dataUrl);
        if (!res.ok) throw new Error("Failed to load constellation data");
        
        const arrayBuffer = await res.arrayBuffer();
        if (isMounted) {
          const floats = new Float32Array(arrayBuffer);
          setIqData(floats);
          setError(null);
          
          // Auto-scale if bounds are not strictly provided
          if (zmin === '' || zmin === undefined || zmax === '' || zmax === undefined) {
            let actualMin = Number.MAX_VALUE;
            let actualMax = -Number.MAX_VALUE;
            for (let i = 0; i < floats.length; i++) {
              if (floats[i] < actualMin) actualMin = floats[i];
              if (floats[i] > actualMax) actualMax = floats[i];
            }
            if (actualMin > actualMax) { actualMin = -1; actualMax = 1; }
            setXBounds({ min: actualMin, max: actualMax });
            setYBounds({ min: actualMin, max: actualMax });
          }
        }
      } catch (err) {
        if (isMounted) setError(err.toString());
      }
    };
    
    fetchData();
    
    return () => { isMounted = false; };
  }, [dataUrl]);

  const draw = () => {
    const canvas = canvasRef.current;
    if (!canvas || !iqData) return;
    
    const ctx = canvas.getContext('2d');
    const width = canvas.width;
    const height = canvas.height;
    
    // Clear background
    ctx.fillStyle = theme === 'dark' ? '#000000' : '#ffffff';
    ctx.fillRect(0, 0, width, height);
    
    // Draw Axes
    ctx.strokeStyle = theme === 'dark' ? '#444444' : '#cccccc';
    ctx.lineWidth = 1;
    ctx.beginPath();
    
    const xRange = xBounds.max - xBounds.min;
    const yRange = yBounds.max - yBounds.min;
    
    // Map a value to pixel coordinate
    const toX = (val) => ((val - xBounds.min) / xRange) * width;
    const toY = (val) => height - ((val - yBounds.min) / yRange) * height;
    
    // Draw X=0 and Y=0 axes if visible
    if (0 >= xBounds.min && 0 <= xBounds.max) {
      const zeroX = toX(0);
      ctx.moveTo(zeroX, 0); ctx.lineTo(zeroX, height);
    }
    if (0 >= yBounds.min && 0 <= yBounds.max) {
      const zeroY = toY(0);
      ctx.moveTo(0, zeroY); ctx.lineTo(width, zeroY);
    }
    ctx.stroke();
    
    // Draw points
    ctx.fillStyle = color;
    
    const numPoints = iqData.length / 2;
    let drawnPoints = 0;
    for (let i = 0; i < numPoints; i++) {
      const iVal = iqData[i * 2];
      const qVal = iqData[i * 2 + 1];
      
      // Fast clipping
      if (iVal < xBounds.min || iVal > xBounds.max || qVal < yBounds.min || qVal > yBounds.max) {
        continue;
      }
      
      const px = toX(iVal);
      const py = toY(qVal);
      
      // Draw 2x2 rect for the dot
      ctx.fillRect(px - 1, py - 1, 2, 2);
      drawnPoints++;
    }
    
    // DEBUG TEXT OVERLAY
    ctx.fillStyle = 'red';
    ctx.font = '16px monospace';
    ctx.fillText(`Points: ${numPoints}, Drawn: ${drawnPoints}`, 10, 20);
    ctx.fillText(`BoundsX: [${xBounds.min.toFixed(2)}, ${xBounds.max.toFixed(2)}]`, 10, 40);
    ctx.fillText(`BoundsY: [${yBounds.min.toFixed(2)}, ${yBounds.max.toFixed(2)}]`, 10, 60);
    ctx.fillText(`Canvas: ${width}x${height}`, 10, 80);
    
  };

  useEffect(() => {
    draw();
  }, [iqData, xBounds, yBounds, theme, color]);

  useEffect(() => {
    if (!containerRef.current || !canvasRef.current) return;
    const observer = new ResizeObserver((entries) => {
      const { width, height } = entries[0].contentRect;
      canvasRef.current.width = width;
      canvasRef.current.height = height;
      draw();
    });
    observer.observe(containerRef.current);
    return () => observer.disconnect();
  }, [iqData, xBounds, yBounds, theme, color]);

  const handleWheel = (e) => {
    e.preventDefault();
    const canvas = canvasRef.current;
    if (!canvas) return;
    
    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    
    const xRange = xBounds.max - xBounds.min;
    const yRange = yBounds.max - yBounds.min;
    
    const dataX = xBounds.min + (x / canvas.width) * xRange;
    const dataY = yBounds.min + ((canvas.height - y) / canvas.height) * yRange;
    
    const zoomFactor = e.deltaY > 0 ? 1.1 : 0.9;
    
    const newXRange = xRange * zoomFactor;
    const newYRange = yRange * zoomFactor;
    
    setXBounds({ min: dataX - (x / canvas.width) * newXRange, max: dataX + ((canvas.width - x) / canvas.width) * newXRange });
    setYBounds({ min: dataY - ((canvas.height - y) / canvas.height) * newYRange, max: dataY + (y / canvas.height) * newYRange });
  };

  const handleMouseDown = (e) => {
    isDragging.current = true;
    lastMousePos.current = { x: e.clientX, y: e.clientY };
  };

  const handleMouseMove = (e) => {
    if (!isDragging.current) return;
    const canvas = canvasRef.current;
    if (!canvas) return;
    
    const dx = e.clientX - lastMousePos.current.x;
    const dy = e.clientY - lastMousePos.current.y;
    lastMousePos.current = { x: e.clientX, y: e.clientY };
    
    const xRange = xBounds.max - xBounds.min;
    const yRange = yBounds.max - yBounds.min;
    
    const dataDx = (dx / canvas.width) * xRange;
    const dataDy = (dy / canvas.height) * yRange;
    
    setXBounds(prev => ({ min: prev.min - dataDx, max: prev.max - dataDx }));
    // Dragging mouse down (positive dy) means panning view UP, so Y data shifts up
    setYBounds(prev => ({ min: prev.min + dataDy, max: prev.max + dataDy }));
  };

  const handleMouseUp = () => {
    isDragging.current = false;
  };

  return (
    <div ref={containerRef} style={{ width: '100%', height: '100%', position: 'relative', overflow: 'hidden' }}>
      {error && (
        <div style={{ position: 'absolute', top: 10, left: 10, color: 'red', zIndex: 10 }}>
          {error}
        </div>
      )}
      <canvas 
        ref={canvasRef}
        onWheel={handleWheel}
        onMouseDown={handleMouseDown}
        onMouseMove={handleMouseMove}
        onMouseUp={handleMouseUp}
        onMouseLeave={handleMouseUp}
        style={{ position: 'absolute', top: 0, left: 0, width: '100%', height: '100%', display: 'block', cursor: isDragging.current ? 'grabbing' : 'grab' }}
      />
      <div style={{ position: 'absolute', bottom: 10, right: 10, color: theme === 'dark' ? '#ccc' : '#333', fontSize: '0.8rem', pointerEvents: 'none', background: theme === 'dark' ? 'rgba(0,0,0,0.5)' : 'rgba(255,255,255,0.7)', padding: '2px 5px', borderRadius: '4px' }}>
        X: [{xBounds.min.toFixed(2)}, {xBounds.max.toFixed(2)}] Y: [{yBounds.min.toFixed(2)}, {yBounds.max.toFixed(2)}]
      </div>
    </div>
  );
};

export default ConstellationPlot;
