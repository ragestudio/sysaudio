import { createRequire } from "module"
import { dirname } from "path"

export type Format = {
	sampleRate: number
	channels: number
	bitsPerSample: number
}

export type InitializeParams = {
	excluded_pid: number | string
	node_name: string
	device_app_name?: string
	device_app_id?: string
	device_app_icon_name?: string
}

export type CaptureCallback = (buff: Buffer, format: Format) => void

export class SysAudio {
	mod: any

	get _require(): NodeJS.Require {
		return createRequire(import.meta.url)
	}

	constructor() {
		const require = createRequire(import.meta.url)

		// Resolve the prebuilt binary for the current platform from
		// `prebuilds/<platform>-<arch>/` (node-gyp-build). Fall back to the
		// locally built `./sysaudio.node` (from `node-gyp build`) when no
		// prebuild is available, e.g. when running straight from the repo.
		try {
			this.mod = require("node-gyp-build")(dirname(import.meta.dirname))
		} catch {
			this.mod = require("./sysaudio.node")
		}
	}

	initialize = (params: InitializeParams): void => {
		return this.mod.initialize(params)
	}

	stop = (): void => {
		return this.mod.stop()
	}

	start_capture = (
		excluded_pid: string | number,
		callback: CaptureCallback,
	): void => {
		return this.mod.start_capture(excluded_pid, callback)
	}

	stop_capture = (): void => {
		return this.mod.stop_capture()
	}

	output = (buffer: Buffer, format: Format): void => {
		return this.mod.output(buffer, format)
	}

	output_supported = (): boolean => {
		// the native addon exports this as a boolean, not a function
		return !!this.mod.output_supported
	}
}

export default SysAudio
