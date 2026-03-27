import { createRequire } from "module"
import { performance } from "perf_hooks"

const sysaudio = createRequire(import.meta.url)("./sysaudio.node")

// latency test configuration
const TEST_DURATION_MS = 5000
const BUFFER_SIZE = 1024
const SAMPLE_RATE = 44100
const CHANNELS = 2
const BITS_PER_SAMPLE = 16

// statistics
let totalFrames = 0
let totalLatency = 0
let maxLatency = 0
let minLatency = Infinity
let frameCount = 0
let startTime = 0
let lastFrameTime = 0

// circular buffer for echo test
const echoBuffer = new Uint8Array(BUFFER_SIZE * 4) // 4x buffer size for safety
let echoWritePos = 0
let echoReadPos = 0

console.log("=== SysAudio Latency Test ===")
console.log(`Duration: ${TEST_DURATION_MS}ms`)
console.log(`Sample rate: ${SAMPLE_RATE}Hz`)
console.log(`Channels: ${CHANNELS}`)
console.log(`Bits per sample: ${BITS_PER_SAMPLE}`)
console.log("")

// audio callback for latency measurement
const audioCallback = (buffer, format) => {
	const now = performance.now()

	if (startTime === 0) {
		startTime = now
		lastFrameTime = now
		return
	}

	// calculate inter-frame latency
	const frameLatency = now - lastFrameTime
	lastFrameTime = now

	// update statistics
	totalLatency += frameLatency
	maxLatency = Math.max(maxLatency, frameLatency)
	minLatency = Math.min(minLatency, frameLatency)
	frameCount++

	// calculate expected frame duration
	const bytesPerSample = BITS_PER_SAMPLE / 8
	const bytesPerFrame = bytesPerSample * CHANNELS
	const samples = buffer.length / bytesPerSample
	const frames = samples / CHANNELS
	const expectedDuration = (frames / SAMPLE_RATE) * 1000

	// echo test: write to circular buffer
	const writeSize = Math.min(buffer.length, echoBuffer.length - echoWritePos)
	buffer.copy(echoBuffer, echoWritePos, 0, writeSize)
	echoWritePos = (echoWritePos + writeSize) % echoBuffer.length

	// read from circular buffer for echo (simulated output)
	if (echoReadPos !== echoWritePos) {
		const readSize = Math.min(BUFFER_SIZE, echoBuffer.length - echoReadPos)
		// simulate output processing
		echoReadPos = (echoReadPos + readSize) % echoBuffer.length
	}

	// periodic logging
	if (frameCount % 50 === 0) {
		const elapsed = now - startTime
		const avgLatency = totalLatency / frameCount
		console.log(`Frame ${frameCount}:`)
		console.log(`  Current latency: ${frameLatency.toFixed(2)}ms`)
		console.log(`  Average latency: ${avgLatency.toFixed(2)}ms`)
		console.log(
			`  Expected frame duration: ${expectedDuration.toFixed(2)}ms`,
		)
		console.log(`  Buffer size: ${buffer.length} bytes`)
		console.log(`  Elapsed time: ${elapsed.toFixed(0)}ms`)
		console.log("")
	}

	// stop test after duration
	if (now - startTime > TEST_DURATION_MS) {
		sysaudio.stop_capture()

		// calculate final statistics
		const avgLatency = totalLatency / frameCount
		const totalTime = now - startTime
		const expectedFrames = totalTime / expectedDuration
		const frameRate = (frameCount / totalTime) * 1000

		console.log("=== Test Results ===")
		console.log(`Total frames processed: ${frameCount}`)
		console.log(`Total test time: ${totalTime.toFixed(0)}ms`)
		console.log(`Average frame latency: ${avgLatency.toFixed(2)}ms`)
		console.log(`Minimum frame latency: ${minLatency.toFixed(2)}ms`)
		console.log(`Maximum frame latency: ${maxLatency.toFixed(2)}ms`)
		console.log(`Frame rate: ${frameRate.toFixed(1)} fps`)
		console.log(`Expected frames: ${expectedFrames.toFixed(0)}`)
		console.log(
			`Frame loss: ${((1 - frameCount / expectedFrames) * 100).toFixed(1)}%`,
		)
		console.log("")

		// latency analysis
		console.log("=== Latency Analysis ===")
		if (avgLatency > expectedDuration * 1.5) {
			console.log("⚠️  HIGH LATENCY DETECTED")
			console.log(
				`Average latency (${avgLatency.toFixed(2)}ms) is >1.5x expected frame duration (${expectedDuration.toFixed(2)}ms)`,
			)
		} else if (avgLatency > expectedDuration * 1.2) {
			console.log("⚠️  Moderate latency detected")
		} else {
			console.log("✅ Latency within acceptable range")
		}

		if (maxLatency > expectedDuration * 3) {
			console.log(
				`⚠️  High peak latency detected: ${maxLatency.toFixed(2)}ms`,
			)
		}

		if (frameCount < expectedFrames * 0.9) {
			console.log(
				`⚠️  Frame loss detected: ${((1 - frameCount / expectedFrames) * 100).toFixed(1)}%`,
			)
		}

		process.exit(0)
	}
}

// start the test
console.log("Starting latency test...")
console.log("")

try {
	sysaudio.start_capture(process.pid, audioCallback)

	// handle graceful shutdown
	process.on("SIGINT", () => {
		console.log("\nStopping test...")
		sysaudio.stop_capture()
		sysaudio.stop()
		process.exit(0)
	})

	// auto-stop after test duration + buffer
	setTimeout(() => {
		console.log("Test timeout reached")
		sysaudio.stop_capture()
		sysaudio.stop()
		process.exit(0)
	}, TEST_DURATION_MS + 2000)
} catch (error) {
	console.error("Error during test:", error)
	process.exit(1)
}
