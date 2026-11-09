export type Format = {
	sampleRate: number
	channels: number
	bitsPerSample: number
	format?: number
}

export type InitializeParams = {
	excluded_pid: number
	node_name: string
	device_app_name?: string
	device_app_id?: string
	device_app_icon_name?: string
}

export type CaptureCallback = (buff: Buffer, format: Format) => void

// raw n-api surface exposed by sysaudio.node
export interface NativeSysAudioAddon {
	initialize(params: InitializeParams): void
	start_capture(excluded_pid: number, callback?: CaptureCallback): void
	stop_capture(): void
	stop(): void
	output(buffer: Buffer, format: Format): void

	// shared memory transport
	attach_shared_buffer(view: Uint8Array): void
	detach_shared_buffer(): void

	get_format(): Format
	readonly output_supported: boolean
}
