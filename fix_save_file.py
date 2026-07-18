import re

with open("web/frontend/src/App.jsx", "r") as f:
    content = f.read()

# Fix saveFileInfo to refresh all active panels
new_save = """
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
"""

content = re.sub(
    r"// Force refresh plots\s*if \(staticPlotType\) handleStaticPlot\(staticPlotType === 'fft', newFreq\);\s*if \(sigplotType === '1D'\) handleInteractiveFFT\(newFreq\);\s*else if \(sigplotType === '2D'\) handleInteractivePSD\(newFreq\);",
    new_save,
    content
)

with open("web/frontend/src/App.jsx", "w") as f:
    f.write(content)
