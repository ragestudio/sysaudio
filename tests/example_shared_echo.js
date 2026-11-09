import { createRequire } from "node:module"

import {
	captureStats,
	createSharedAudioBuffer,
	playbackStats,
	readCapture,
	readFormat,
	writePlayback,
} from "./shared-buffer.js"

const sysaudio = createRequire(import.meta.url)("../dist/sysaudio.node")

const DURATION_MS = Number(process.env["SHARED_ECHO_DURATION_MS"] ?? 10000)

const shared = createSharedAudioBuffer()
const captureBuffer = Buffer.alloc(4096)

let running = true
let captured = 0
let echoed = 0
let reader = null
let report = null

function cleanup() {
	running = false

	if (reader) clearInterval(reader)
	if (report) clearInterval(report)

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

console.log("=== SysAudio shared buffer echo example ===")
console.log(`duration: ${DURATION_MS}ms`)
console.log("captures system audio and plays it back through the native output")
console.log("the addon's own output node is excluded from capture")
console.log("")

try {
	if (!sysaudio.output_supported) {
		console.log("echo example SKIPPED (output not supported)")
		process.exit(0)
	}

	sysaudio.initialize({
		excluded_pid: process.pid,
		node_name: "shared_echo_example",
		device_app_name: "shared_echo_example",
		device_app_id: "sysaudio.test",
		device_app_icon_name: "sysaudio",
	})

	sysaudio.attach_shared_buffer(new Uint8Array(shared.buffer))

	// shared mode: capture writes into the ring without a callback
	sysaudio.start_capture(0)

	// move every captured frame into the playback ring
	reader = setInterval(() => {
		if (!running) return

		let bytes = readCapture(shared, captureBuffer)

		while (bytes > 0) {
			captured += bytes
			echoed += writePlayback(shared, captureBuffer.subarray(0, bytes))

			bytes = readCapture(shared, captureBuffer)
		}
	}, 5)

	// periodic stats
	report = setInterval(() => {
		const capture = captureStats(shared.header)
		const playback = playbackStats(shared.header)
		const format = readFormat(shared.header)

		console.log(
			`captured=${captured} echoed=${echoed} ` +
				`capture_buffer=${capture.available} ` +
				`playback_buffer=${playback.available} ` +
				`format=${format.sampleRate}Hz/${format.channels}ch/${format.bitsPerSample}bit`,
		)
	}, 1000)

	setTimeout(() => {
		const format = readFormat(shared.header)

		cleanup()

		console.log("")
		console.log("final format:", JSON.stringify(format))
		console.log(`captured bytes: ${captured}`)
		console.log(`echoed bytes: ${echoed}`)
		console.log("")

		if (captured > 0) {
			console.log("shared echo example finished")
			process.exit(0)
		}

		console.error("shared echo example FAILED (no frames captured)")
		process.exit(1)
	}, DURATION_MS)
} catch (error) {
	cleanup()
	console.error("shared echo example FAILED", error)
	process.exit(1)
}

process.on("SIGINT", () => {
	cleanup()
	process.exit(0)
})
