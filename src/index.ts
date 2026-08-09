import { createRequire } from "module"

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
	devide_app_icon_name?: string
}

export type CaptureCallback = (buff: Buffer, format: Format) => void

class SysAudio {
	mod: any

	get _require(): NodeJS.Require {
		return createRequire(import.meta.url)
	}

	constructor() {
		this.mod = this._require("./sysaudio.node")
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

	output_supported = (): Boolean => {
		return this.mod.output_supported()
	}
}

export default SysAudio
