import { existsSync, readFileSync, writeFileSync } from "node:fs"
import { createRequire } from "node:module"
import path from "node:path"
import { fileURLToPath } from "node:url"

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..")
const require = createRequire(import.meta.url)

const configPath = path.join(root, "build", "config.gypi")

if (!existsSync(configPath)) {
	console.error(
		"[sysaudio] missing build/config.gypi, run `pnpm run configure` first",
	)
	process.exit(1)
}

// config.gypi is json with a leading comment line
function readGypConfig() {
	const raw = readFileSync(configPath, "utf8")
	const json = raw
		.split("\n")
		.filter((line) => !line.trimStart().startsWith("#"))
		.join("\n")

	return JSON.parse(json)
}

function resolveNodeAddonApi() {
	try {
		return path.dirname(require.resolve("node-addon-api/package.json"))
	} catch {
		return path.join(root, "node_modules", "node-addon-api")
	}
}

function resolveNodeHeaders(config) {
	const nodedir = config.variables?.nodedir

	if (nodedir) {
		return nodedir
	}

	return path.resolve(path.dirname(process.execPath), "..")
}

const config = readGypConfig()
const nodedir = resolveNodeHeaders(config)

const includeDirs = [
	resolveNodeAddonApi(),
	path.join(nodedir, "include", "node"),
	path.join(nodedir, "src"),
	path.join(nodedir, "deps", "openssl", "config"),
	path.join(nodedir, "deps", "openssl", "openssl", "include"),
	path.join(nodedir, "deps", "uv", "include"),
	path.join(nodedir, "deps", "zlib"),
	path.join(nodedir, "deps", "v8", "include"),
]

if (process.platform === "linux") {
	includeDirs.push("/usr/include/pipewire-0.3", "/usr/include/spa-0.2")
}

const defines = [
	"NDEBUG",
	"NAPI_DISABLE_CPP_EXCEPTIONS",
	"BUILDING_NODE_EXTENSION",
	"_FILE_OFFSET_BITS=64",
	"_LARGEFILE_SOURCE",
]

if (process.platform === "linux") {
	defines.push("_GLIBCXX_USE_CXX11_ABI=1")
}

const compilerFlags = [
	"-std=gnu++20",
	"-fPIC",
	"-pthread",
	"-fno-rtti",
	"-fno-exceptions",
	"-fno-strict-aliasing",
	"-Wall",
	"-Wextra",
]

const flags = [
	...includeDirs.filter((dir) => existsSync(dir)).map((dir) => `-I${dir}`),
	...defines.map((define) => `-D${define}`),
	...compilerFlags,
]

writeFileSync(path.join(root, "compile_flags.txt"), `${flags.join("\n")}\n`)

const sources = ["src/addon.cpp"]

if (process.platform === "linux") {
	sources.push(
		"src/linux/engine.cpp",
		"src/linux/utils.cpp",
		"src/linux/router.cpp",
		"src/linux/input.cpp",
		"src/linux/output.cpp",
	)
}

if (process.platform === "win32") {
	sources.push("src/win/capture.cpp")
}

const compileCommands = sources.map((source) => {
	const file = path.join(root, source)
	const command = ["c++", ...flags, "-c", file].join(" ")

	return {
		directory: root,
		command,
		file,
	}
})

writeFileSync(
	path.join(root, "compile_commands.json"),
	JSON.stringify(compileCommands, null, "\t"),
)

console.log(`[sysaudio] wrote compile_flags.txt and compile_commands.json`)
console.log(`[sysaudio] node headers: ${nodedir}`)
console.log(`[sysaudio] sources: ${sources.length}`)
