import wav from "wav"
import { createRequire } from "module"

const sysaudio = createRequire(import.meta.url)("./sysaudio.node")

const file = new wav.FileWriter("./output.wav", {
	channels: 2,
	sampleRate: 44100,
	bitDepth: 16,
})

sysaudio.initialize({
	excluded_pid: 123,
	node_name: "Default test",
	device_app_name: "SysaudioTest",
})

sysaudio.start_capture(123, (buff, format) => {
	file.write(buff)
	sysaudio.output(buff, format)
})

setTimeout(() => {
	sysaudio.stop_capture()
}, 10000)

setTimeout(() => {
	sysaudio.stop()
}, 12000)
