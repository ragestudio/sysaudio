import { loadNativeAddon } from "./native.js"

import type {
	CaptureCallback,
	Format,
	InitializeParams,
	NativeSysAudioAddon,
} from "./types.js"

export * from "./types.js"
export { loadNativeAddon } from "./native.js"

export class SysAudio {
	readonly mod: NativeSysAudioAddon

	constructor() {
		this.mod = loadNativeAddon()
	}

	initialize = (params: InitializeParams): void => {
		this.mod.initialize(params)
	}

	stop = (): void => {
		this.mod.stop()
	}

	start_capture = (
		excluded_pid: number,
		callback?: CaptureCallback,
	): void => {
		this.mod.start_capture(excluded_pid, callback)
	}

	stop_capture = (): void => {
		this.mod.stop_capture()
	}

	output = (buffer: Buffer, format: Format): void => {
		this.mod.output(buffer, format)
	}

	attach_shared_buffer = (view: Uint8Array): void => {
		this.mod.attach_shared_buffer(view)
	}

	detach_shared_buffer = (): void => {
		this.mod.detach_shared_buffer()
	}

	get_format = (): Format => {
		return this.mod.get_format()
	}

	output_supported = (): boolean => {
		// the native addon exports this as a boolean, not a function
		return !!this.mod.output_supported
	}
}

export default SysAudio
