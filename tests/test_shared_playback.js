import { createRequire } from "node:module"

import {
	createSharedAudioBuffer,
	createTone,
	playbackStats,
	writePlayback,
} from "./shared-buffer.js"

const sysaudio = createRequire(import.meta.url)("../dist/sysaudio.node")

const TEST_DURATION_MS = 1500
const SAMPLE_RATE = 44100
const CHANNELS = 2
const BITS_PER_SAMPLE = 16

const shared = createSharedAudioBuffer()
const tone = createTone({
	sampleRate: SAMPLE_RATE,
	channels: CHANNELS,
})

let running = true
let written = 0
let writer = null

function cleanup() {
	running = false

	if (writer) clearInterval(writer)

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

console.log("=== SysAudio shared buffer playback test ===")
console.log(`duration: ${TEST_DURATION_MS}ms`)
console.log(`playback ring: ${shared.playback.length} bytes`)
console.log("")

try {
	if (!sysaudio.output_supported) {
		console.log("shared playback test SKIPPED (output not supported)")
		process.exit(0)
	}

	sysaudio.initialize({
		excluded_pid: process.pid,
		node_name: "shared_playback_test",
		device_app_name: "shared_playback_test",
		device_app_id: "sysaudio.test",
		device_app_icon_name: "sysaudio",
	})

	sysaudio.attach_shared_buffer(new Uint8Array(shared.buffer))

	// no capture and no output() call: the native output stream reads the
	// playback ring directly
	writer = setInterval(() => {
		if (!running) return

		written += writePlayback(shared, tone)
	}, 20)

	setTimeout(() => {
		const stats = playbackStats(shared.header)

		cleanup()

		console.log("playback:", JSON.stringify({ ...stats, written }))
		console.log("")

		if (stats.read > 0) {
			console.log("shared playback test PASSED")
			process.exit(0)
		}

		console.error(
			"shared playback test FAILED (the output stream did not read)",
		)
		process.exit(1)
	}, TEST_DURATION_MS)
} catch (error) {
	cleanup()
	console.error("shared playback test FAILED", error)
	process.exit(1)
}

process.on("SIGINT", () => {
	cleanup()
	process.exit(0)
})
