import { createRequire } from "node:module"

import {
	captureStats,
	createSharedAudioBuffer,
	createTone,
	measurePeak,
	playbackStats,
	readCapture,
	readFormat,
	writePlayback,
} from "./shared-buffer.js"

const sysaudio = createRequire(import.meta.url)("../dist/sysaudio.node")

const TEST_DURATION_MS = 2000
const SAMPLE_RATE = 44100
const CHANNELS = 2
const BITS_PER_SAMPLE = 16

const shared = createSharedAudioBuffer()
const tone = createTone({
	sampleRate: SAMPLE_RATE,
	channels: CHANNELS,
})
const captureBuffer = Buffer.alloc(4096)

let written = 0
let captured = 0
let peak = 0
let running = true
let writer = null
let reader = null

function cleanup() {
	running = false

	if (writer) clearInterval(writer)
	if (reader) clearInterval(reader)

	try {
		sysaudio.stop_capture()
	} catch {
		// capture was not started
	}

	try {
		sysaudio.detach_shared_buffer()
	} catch {
		// buffer was not attached
	}

	try {
		sysaudio.stop()
	} catch {
		// engine was not initialized
	}
}

console.log("=== SysAudio shared buffer capture test ===")
console.log(`duration: ${TEST_DURATION_MS}ms`)
console.log(`capture ring: ${shared.capture.length} bytes`)
console.log(`playback ring: ${shared.playback.length} bytes`)
console.log("")

try {
	sysaudio.initialize({
		excluded_pid: 0,
		node_name: "shared_capture_test",
		device_app_name: "shared_capture_test",
		device_app_id: "sysaudio.test",
		device_app_icon_name: "sysaudio",
	})

	sysaudio.attach_shared_buffer(new Uint8Array(shared.buffer))

	// no callback: capture goes straight into the shared ring
	sysaudio.start_capture(0)

	// write a tone through the shared playback ring to exercise the output
	// side. capture excludes no process, so it reads every other output
	// stream (the addon's own output node is excluded by name)
	if (sysaudio.output_supported) {
		writer = setInterval(() => {
			if (!running) return

			written += writePlayback(shared, tone)
		}, 20)
	}

	reader = setInterval(() => {
		if (!running) return

		let bytes = readCapture(shared, captureBuffer)

		while (bytes > 0) {
			captured += bytes

			const samplePeak = measurePeak(captureBuffer.subarray(0, bytes))

			if (samplePeak > peak) {
				peak = samplePeak
			}

			bytes = readCapture(shared, captureBuffer)
		}
	}, 5)

	setTimeout(() => {
		const format = readFormat(shared.header)
		const capture = captureStats(shared.header)
		const playback = playbackStats(shared.header)

		cleanup()

		console.log("format:", JSON.stringify(format))
		console.log("capture:", JSON.stringify({ ...capture, captured }))
		console.log("playback:", JSON.stringify({ ...playback, written }))
		console.log(`captured peak: ${peak}`)
		console.log("")

		if (captured > 0 && format.sampleRate > 0) {
			console.log("shared capture test PASSED")
			process.exit(0)
		}

		console.error("shared capture test FAILED (no frames received)")
		process.exit(1)
	}, TEST_DURATION_MS)
} catch (error) {
	cleanup()
	console.error("shared capture test FAILED", error)
	process.exit(1)
}

process.on("SIGINT", () => {
	cleanup()
	process.exit(0)
})
