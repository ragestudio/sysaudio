import { createRequire } from "node:module"

import { createTone } from "./shared-buffer.js"

const sysaudio = createRequire(import.meta.url)("../dist/sysaudio.node")

const SAMPLE_RATE = 44100
const CHANNELS = 2
const BITS_PER_SAMPLE = 16

const tone = createTone({
	sampleRate: SAMPLE_RATE,
	channels: CHANNELS,
})

let received = 0
let writer = null

function cleanup() {
	if (writer) clearInterval(writer)

	try {
		sysaudio.stop_capture()
	} catch {
		// capture was not started
	}

	try {
		sysaudio.stop()
	} catch {
		// engine was not initialized
	}
}

console.log("=== SysAudio callback regression test ===")
console.log("")

try {
	sysaudio.initialize({
		excluded_pid: process.pid,
		node_name: "callback_test",
		device_app_name: "callback_test",
		device_app_id: "sysaudio.test",
		device_app_icon_name: "sysaudio",
	})

	// generate a tone through the legacy output ring so the test works on a
	// silent system. capture excludes no process, so the tone is captured
	writer = setInterval(() => {
		sysaudio.output(tone, {
			sampleRate: SAMPLE_RATE,
			channels: CHANNELS,
			bitsPerSample: BITS_PER_SAMPLE,
		})
	}, 20)

	sysaudio.start_capture(0, (buffer, format) => {
		received++

		if (received < 3) return

		cleanup()

		console.log(
			"first frame:",
			JSON.stringify({
				bytes: buffer.length,
				format,
			}),
		)
		console.log(`frames received: ${received}`)
		console.log("")
		console.log("callback regression test PASSED")

		process.exit(0)
	})

	setTimeout(() => {
		cleanup()
		console.error(
			`callback regression test FAILED (frames received: ${received})`,
		)
		process.exit(1)
	}, 5000)
} catch (error) {
	cleanup()
	console.error("callback regression test FAILED", error)
	process.exit(1)
}

process.on("SIGINT", () => {
	cleanup()
	process.exit(0)
})
