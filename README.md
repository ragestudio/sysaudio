# SysAudio

A cross-platform native Node.js addon for system audio capture and playback on Windows and Linux.

## Features

- **System Audio Capture**: Capture audio from any application or system-wide
- **Cross-Platform**: Windows (WASAPI) and Linux (PipeWire) support
- **Real-time Processing**: Stream audio data to JavaScript in real-time
- **Audio Playback**: Output audio to system speakers (Linux only)
- **Process Filtering**: Exclude specific processes from capture
- **Flexible Format**: Supports various audio formats and sample rates

## Installation

### Prerequisites

- Node.js (v14 or higher)
- Python 3.x and build tools (for node-gyp)
- Platform-specific dependencies:

#### Linux (PipeWire)
```bash
# Ubuntu/Debian
sudo apt-get install libpipewire-0.3-dev

# Fedora
sudo dnf install pipewire-devel

# Arch Linux
sudo pacman -S pipewire
```

#### Windows
- Windows SDK (for audio APIs)
- Visual Studio Build Tools

### Build from source
```bash
git clone https://git.ragestudio.net/ragestudio/sysaudio
cd sysaudio
npm install
npm run build
```

## API Reference

### `start_capture(pid, callback)`
Start capturing system audio.

**Parameters:**
- `pid` (number): Process ID to exclude from capture (0 to capture all)
- `callback` (function): Callback function receiving audio frames

**Callback signature:**
```javascript
(buff: Buffer<uint8Array>, format: { sampleRate: number, channels: number, bitsPerSample: number }) => {
  // buff: Buffer containing audio data (PCM)
  // format: Object with audio format information
}
```

**Example format object:**
```javascript
{
  sampleRate: number,    // e.g., 44100
  channels: number,      // e.g., 2 (stereo)
  bitsPerSample: number  // e.g., 16 or 32
}
```

### `stop_capture()`
Stop audio capture.

### `output(buffer, format)`
Output audio to default sink (Linux only).

**Parameters:**
- `buffer` (Buffer): Audio data to play (PCM Waveform)
- `format` (Object): Audio format (same as capture format)

### `stop()`
Stop the audio engine and clean up resources.

### `output_supported`
Boolean property indicating if audio output is supported on current platform.

## Usage Examples

### Basic Capture and Save to WAV
```javascript
import wav from 'wav';
import { createRequire } from 'module';

const sysaudio = createRequire(import.meta.url)('./sysaudio.node');

// Create WAV file writer
const file = new wav.FileWriter('./output.wav', {
  channels: 2,
  sampleRate: 44100,
  bitDepth: 16,
});

// Start capturing all system audio (exclude PID 0)
sysaudio.start_capture(0, (buff, format) => {
  console.log(`Received [${buff.length}] bytes, format:`, format);
  file.write(buff);
  
  // On linux, can pipe again to output
  if (sysaudio.output_supported) {
    sysaudio.output(buff, format);
  }
});

// Stop after 10 seconds
setTimeout(() => {
  sysaudio.stop_capture();
  file.end();
  console.log('Capture stopped');
}, 10000);
```

### Real-time Audio Processing
```javascript
import { createRequire } from 'module';

const sysaudio = createRequire(import.meta.url)('./sysaudio.node');

// Process audio in real-time
sysaudio.start_capture(1234, (buff, format) => {
  // Analyze audio data
  const samples = new Int16Array(buff.buffer, buff.byteOffset, buff.length / 2);
  const avg = samples.reduce((a, b) => a + Math.abs(b), 0) / samples.length;
  
  console.log(`Audio level: ${avg.toFixed(2)}`);
  
  // Apply effects or forward to WebSocket, etc.
});

// Clean up on exit
process.on('SIGINT', () => {
  sysaudio.stop_capture();
  sysaudio.stop();
  process.exit();
});
```

### Capture Specific Application
```javascript
// E.G. To capture audio from all applications except Chrome (PID 5678)
sysaudio.start_capture(5678, (buff, format) => {
  // Process audio from all apps except Chrome
});
```

## Platform Details

### Windows (WASAPI)
- Uses Windows Audio Session API (WASAPI) for loopback capture
- Automatic format conversion
- Process-specific audio isolation

### Linux (PipeWire)
- Uses PipeWire for modern audio handling
- Supports both capture and playback
- Low-latency audio processing
- Compatible with PulseAudio and JACK applications

## Architecture
```
src/
├── addon.cpp              # Main Node.js addon entry point
├── types.hpp             # Common type definitions
├── linux/
│   ├── engine.cpp       # Linux audio engine
│   ├── engine.hpp
│   └── pipewire/        # PipeWire implementation
└── win/
    ├── capture.cpp      # Windows WASAPI capture
    └── capture.hpp
```

## Error Handling
The module throws JavaScript exceptions for invalid arguments or runtime errors. Always wrap calls in try-catch blocks:

```javascript
try {
  sysaudio.start_capture(0, callback);
} catch (error) {
  console.error('Failed to start capture:', error.message);
}
```

## Performance Considerations
- Audio callbacks run in a separate thread
- Buffer sizes are optimized for real-time processing
- Consider using Web Workers for heavy processing in JavaScript

## Limitations
- Audio output (`output()`) only available on Linux
- macOS support is not implemented (yet)
