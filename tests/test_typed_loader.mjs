import SysAudio, { loadNativeAddon } from "../dist/index.js"

const addon = loadNativeAddon()

console.log("=== SysAudio typed native loader ===")
console.log("output_supported:", addon.output_supported)
console.log("get_format:", JSON.stringify(addon.get_format()))

const instance = new SysAudio()
const methods = [
	"initialize",
	"start_capture",
	"stop_capture",
	"stop",
	"output",
	"attach_shared_buffer",
	"detach_shared_buffer",
	"get_format",
	"output_supported",
]
const missing = methods.filter((name) => typeof instance[name] !== "function")

console.log("wrapper output_supported:", instance.output_supported())
console.log("missing methods:", JSON.stringify(missing))
console.log("")

if (missing.length > 0) {
	console.error("typed loader test FAILED")
	process.exit(1)
}

console.log("typed loader test PASSED")
