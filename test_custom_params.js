import { createRequire } from "module"

const sysaudio = createRequire(import.meta.url)("./sysaudio.node")

console.log("=== Testing Custom Initialization Parameters ===\n")

// test custom parameters
const customParams = {
	excluded_pid: process.pid,
	node_name: "custom-sysaudio-test",
	device_app_name: "Custom Test App",
	device_app_id: "com.custom.test",
	device_app_icon_name: "custom-test-icon",
}

console.log("Initializing with custom parameters:")
console.log(`  node_name: ${customParams.node_name}`)
console.log(`  device_app_name: ${customParams.device_app_name}`)
console.log(`  device_app_id: ${customParams.device_app_id}`)
console.log(`  device_app_icon_name: ${customParams.device_app_icon_name}`)
console.log(`  excluded_pid: ${customParams.excluded_pid}`)
console.log("")

try {
	// initialize with custom parameters
	console.log("Calling initialize()...")
	const initResult = sysaudio.initialize(customParams)
	console.log(`Initialize result: ${initResult}\n`)

	if (!initResult) {
		console.error("Failed to initialize with custom parameters")
		process.exit(1)
	}

	// test capture with custom parameters
	let frameCount = 0
	const testDuration = 3000 // 3 seconds

	const audioCallback = (buffer, format) => {
		frameCount++

		if (frameCount === 1) {
			console.log("First audio frame received:")
			console.log(`  Buffer size: ${buffer.length} bytes`)
			console.log(`  Sample rate: ${format.sampleRate}Hz`)
			console.log(`  Channels: ${format.channels}`)
			console.log(`  Bits per sample: ${format.bitsPerSample}`)
			console.log("")
		}

		if (frameCount % 20 === 0) {
			console.log(`Received ${frameCount} frames...`)
		}
	}

	console.log("Starting capture...")
	sysaudio.start_capture(process.pid, audioCallback)

	console.log(`Capturing audio for ${testDuration}ms...\n`)

	// stop after test duration
	setTimeout(() => {
		console.log("\nStopping capture...")
		sysaudio.stop_capture()

		console.log(`\nTest completed:`)
		console.log(`  Total frames received: ${frameCount}`)
		console.log(`  Test duration: ${testDuration}ms`)

		if (frameCount > 0) {
			console.log("✅ Custom parameters test PASSED")
			console.log("\nThe system should now show:")
			console.log(`  - Node name: "${customParams.node_name}"`)
			console.log(`  - App name: "${customParams.device_app_name}"`)
			console.log(`  - App ID: "${customParams.device_app_id}"`)
			console.log(`  - App icon: "${customParams.device_app_icon_name}"`)
		} else {
			console.error("❌ No frames received - test FAILED")
		}

		// cleanup
		console.log("\nCleaning up...")
		sysaudio.stop()
		process.exit(frameCount > 0 ? 0 : 1)
	}, testDuration)

	// handle graceful shutdown
	process.on("SIGINT", () => {
		console.log("\n\nInterrupted by user")
		sysaudio.stop_capture()
		sysaudio.stop()
		process.exit(0)
	})
} catch (error) {
	console.error("Error during test:", error)
	console.error(error.stack)
	process.exit(1)
}
