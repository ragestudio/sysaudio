import { createRequire } from "node:module"
import { dirname, join } from "node:path"

import type { NativeSysAudioAddon } from "./types.js"

const require = createRequire(import.meta.url)

// prefer the freshly compiled binary next to this module (node-gyp build) and
// fall back to the prebuilt binary shipped with the package
export function loadNativeAddon(): NativeSysAudioAddon {
	let lastError: unknown = null

	try {
		return require(
			join(import.meta.dirname, "sysaudio.node"),
		) as NativeSysAudioAddon
	} catch (error) {
		lastError = error
	}

	try {
		const nodeGypBuild = require("node-gyp-build") as (
			dir: string,
		) => unknown

		return nodeGypBuild(dirname(import.meta.dirname)) as NativeSysAudioAddon
	} catch (error) {
		lastError = error
	}

	const message =
		lastError instanceof Error ? lastError.message : String(lastError)

	throw new Error(`Failed to load the sysaudio native addon: ${message}`)
}
