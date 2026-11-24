module;
#include <utility>
#include <string>
#include <vector>
#include <cmath>
module Script.Python.CoreAPI;

import Container.NamedVector;

import Script.Python.Engine;

namespace ArtifactCore {

namespace {
CorePythonAPI::CompositionBridge g_compositionBridge;
}

void CorePythonAPI::registerAll() {
    auto& py = PythonEngine::instance();
    // Pre-create the artifact.core submodules
    py.registerPreludeCode(R"PYCODE(
import sys
if 'artifact' not in sys.modules:
    class _ArtifactStub: pass
    sys.modules['artifact'] = _ArtifactStub()

class _CoreMath: pass
class _CoreColor: pass
class _CoreDSP: pass
class _CoreSystem: pass
class _CoreComposition: pass
class _CoreAutomation: pass

class _CoreModule:
    math = _CoreMath()
    color = _CoreColor()
    dsp = _CoreDSP()
    system = _CoreSystem()
    composition = _CoreComposition()
    automation = _CoreAutomation()

sys.modules['artifact'].core = _CoreModule()
sys.modules['artifact.core'] = sys.modules['artifact'].core
)PYCODE");

    registerMathAPI();
    registerColorAPI();
    registerDSPAPI();
    registerSystemAPI();
    registerCompositionAPI();
}

void CorePythonAPI::setCompositionBridge(CompositionBridge bridge) {
    g_compositionBridge = std::move(bridge);
    if (!g_compositionBridge) return;
    PythonEngine::instance().registerFunction(
        "_workspace_invoke",
        [](const std::vector<std::string>& args) -> std::string {
            if (args.empty() || !g_compositionBridge) {
                return "{\"success\":false,\"message\":\"Composition bridge unavailable\"}";
            }
            std::vector<std::string> parameters;
            if (args.size() > 1) {
                parameters.assign(args.begin() + 1, args.end());
            }
            return g_compositionBridge(args.front(), parameters);
        }, true);
}

void CorePythonAPI::registerMathAPI() {
    auto& py = PythonEngine::instance();
    std::string code = R"PYCODE(
import artifact.core
import math

# Math & Vector API
def _clamp(val, min_val, max_val):
    return max(min_val, min(val, max_val))

def _lerp(a, b, t):
    return a + (b - a) * t

def _smoothstep(edge0, edge1, x):
    t = _clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)

def _length(v):
    return math.sqrt(sum(x*x for x in v))

def _normalize(v):
    l = _length(v)
    return [x/l for x in v] if l > 0 else v

def _distance(v1, v2):
    return math.sqrt(sum((a - b) ** 2 for a, b in zip(v1, v2)))

artifact.core.math.clamp = _clamp
artifact.core.math.lerp = _lerp
artifact.core.math.smoothstep = _smoothstep
artifact.core.math.length = _length
artifact.core.math.normalize = _normalize
artifact.core.math.distance = _distance
)PYCODE";
    py.registerPreludeCode(code);
}

void CorePythonAPI::registerColorAPI() {
    auto& py = PythonEngine::instance();
    std::string code = R"PYCODE(
import artifact.core
import colorsys

# Color API
def _hsv_to_rgb(h, s, v):
    """Convert HSV (0..1) to RGB tuple"""
    return colorsys.hsv_to_rgb(h, s, v)

def _rgb_to_hsv(r, g, b):
    """Convert RGB (0..1) to HSV tuple"""
    return colorsys.rgb_to_hsv(r, g, b)

def _hex_to_rgb(hex_str):
    """Convert #RRGGBB to RGB float tuple"""
    hex_str = hex_str.lstrip('#')
    return tuple(int(hex_str[i:i+2], 16)/255.0 for i in (0, 2, 4))

artifact.core.color.hsv_to_rgb = _hsv_to_rgb
artifact.core.color.rgb_to_hsv = _rgb_to_hsv
artifact.core.color.hex_to_rgb = _hex_to_rgb
)PYCODE";
    py.registerPreludeCode(code);
}

void CorePythonAPI::registerDSPAPI() {
    auto& py = PythonEngine::instance();
    std::string code = R"PYCODE(
import artifact.core
import math

# Audio DSP Utility API
def _db_to_linear(db):
    """Convert decibels to linear gain"""
    return math.pow(10.0, db / 20.0)

def _linear_to_db(linear):
    """Convert linear gain to decibels"""
    return 20.0 * math.log10(max(1e-9, linear))

def _midi_to_hz(note):
    """Convert MIDI note number to Frequency (Hz)"""
    return 440.0 * math.pow(2.0, (note - 69) / 12.0)

def _hz_to_midi(hz):
    """Convert Frequency (Hz) to MIDI note number"""
    return 69 + 12.0 * math.log2(hz / 440.0)

artifact.core.dsp.db_to_linear = _db_to_linear
artifact.core.dsp.linear_to_db = _linear_to_db
artifact.core.dsp.midi_to_hz = _midi_to_hz
artifact.core.dsp.hz_to_midi = _hz_to_midi
)PYCODE";
    py.registerPreludeCode(code);
}

void CorePythonAPI::registerSystemAPI() {
    auto& py = PythonEngine::instance();
    std::string code = R"PYCODE(
import artifact.core
import os
import sys
import pathlib
import shutil
import subprocess

# System / Engine level API
def _get_engine_version():
    return "ArtifactCore v1.0"

def _get_platform():
    return sys.platform

def _resolve_path(path):
    """Resolve an Artifact relative path to absolute OS path"""
    return os.path.abspath(path)

def _path_entry(path):
    return pathlib.Path(path).expanduser()

def _path_exists(path):
    return _path_entry(path).exists()

def _path_is_file(path):
    return _path_entry(path).is_file()

def _path_is_directory(path):
    return _path_entry(path).is_dir()

def _list_directory(path, max_entries=10000):
    if type(max_entries) is not int or max_entries < 0 or max_entries > 10000:
        raise ValueError("max_entries must be between 0 and 10000")
    directory = _path_entry(path)
    entries = []
    for entry in directory.iterdir():
        if len(entries) >= max_entries:
            raise ValueError("directory exceeds max_entries")
        entries.append(entry)
    result = []
    for entry in sorted(entries, key=lambda item: item.name.casefold()):
        result.append({
            "name": entry.name,
            "path": str(entry.resolve(strict=False)),
            "is_file": entry.is_file(),
            "is_directory": entry.is_dir(),
        })
    return result

def _read_text(path, encoding="utf-8", max_bytes=16777216):
    if type(max_bytes) is not int or max_bytes < 0 or max_bytes > 268435456:
        raise ValueError("max_bytes must be between 0 and 268435456")
    source_path = _path_entry(path)
    if not source_path.is_file():
        raise ValueError("path is not a regular file")
    with source_path.open("rb") as source:
        data = source.read(max_bytes + 1)
    if len(data) > max_bytes:
        raise ValueError("file exceeds max_bytes")
    return data.decode(encoding)

def _write_text(path, content, encoding="utf-8", overwrite=False):
    if not isinstance(content, str):
        raise TypeError("content must be a string")
    destination = _path_entry(path)
    if destination.is_symlink():
        raise ValueError("symbolic-link destinations are not supported")
    if destination.exists() and not destination.is_file():
        raise ValueError("destination is not a regular file")
    mode = "w" if overwrite else "x"
    with destination.open(mode, encoding=encoding, newline="") as target:
        target.write(content)
    return str(destination.resolve(strict=False))

def _create_directory(path, parents=True, exist_ok=True):
    directory = _path_entry(path)
    directory.mkdir(parents=parents, exist_ok=exist_ok)
    return str(directory.resolve(strict=False))

def _delete_file(path):
    target = _path_entry(path)
    if target.is_symlink():
        target.unlink()
        return True
    if target.is_dir():
        raise IsADirectoryError(str(target))
    if not target.is_file():
        raise ValueError("path is not a regular file or symbolic link")
    target.unlink()
    return True

def _copy_file(source, destination, overwrite=False):
    source_path = _path_entry(source)
    destination_path = _path_entry(destination)
    if source_path.is_symlink() or not source_path.is_file():
        raise FileNotFoundError(str(source_path))
    if destination_path.is_symlink():
        raise ValueError("symbolic-link destinations are not supported")
    if destination_path.is_dir():
        raise IsADirectoryError(str(destination_path))
    if destination_path.exists() and not overwrite:
        raise FileExistsError(str(destination_path))
    shutil.copy2(source_path, destination_path)
    return str(destination_path.resolve(strict=False))

def _move_file(source, destination, overwrite=False):
    source_path = _path_entry(source)
    destination_path = _path_entry(destination)
    if source_path.is_symlink() or not source_path.is_file():
        raise FileNotFoundError(str(source_path))
    if destination_path.is_symlink():
        raise ValueError("symbolic-link destinations are not supported")
    if destination_path.is_dir():
        raise IsADirectoryError(str(destination_path))
    if destination_path.exists() and not overwrite:
        raise FileExistsError(str(destination_path))
    shutil.move(str(source_path), str(destination_path))
    return str(destination_path.resolve(strict=False))

_processes = {}
_max_process_handles = 128

def _start_process(args, cwd=None, env=None, show_window=False):
    if not isinstance(args, (list, tuple)) or not args:
        raise ValueError("args must be a non-empty list or tuple")
    if len(_processes) >= _max_process_handles:
        raise RuntimeError("too many tracked processes; forget exited process handles first")
    command = [os.fspath(argument) for argument in args]
    if any(not isinstance(argument, str) for argument in command):
        raise TypeError("all process arguments must be strings or string paths")
    child_env = None
    if env is not None:
        if not isinstance(env, dict) or any(
            not isinstance(key, str) or not isinstance(value, str)
            for key, value in env.items()
        ):
            raise TypeError("env must be a mapping of strings to strings")
        child_env = os.environ.copy()
        child_env.update(env)
    creationflags = 0
    if os.name == "nt" and not show_window:
        creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    process = subprocess.Popen(
        command,
        cwd=None if cwd is None else os.fspath(cwd),
        env=child_env,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        shell=False,
        creationflags=creationflags,
    )
    _processes[process.pid] = process
    return process.pid

def _process_status(pid):
    process = _processes.get(int(pid))
    if process is None:
        return {"status": "unknown", "pid": int(pid)}
    exit_code = process.poll()
    if exit_code is None:
        return {"status": "running", "pid": process.pid}
    return {"status": "exited", "pid": process.pid, "exit_code": exit_code}

def _terminate_process(pid):
    process = _processes.get(int(pid))
    if process is None or process.poll() is not None:
        return False
    process.terminate()
    return True

def _forget_process(pid):
    process_id = int(pid)
    process = _processes.get(process_id)
    if process is None or process.poll() is None:
        return False
    del _processes[process_id]
    return True

artifact.core.system.get_engine_version = _get_engine_version
artifact.core.system.get_platform = _get_platform
artifact.core.system.resolve_path = _resolve_path
artifact.core.system.path_exists = _path_exists
artifact.core.system.is_file = _path_is_file
artifact.core.system.is_directory = _path_is_directory
artifact.core.system.list_directory = _list_directory
artifact.core.system.read_text = _read_text
artifact.core.system.write_text = _write_text
artifact.core.system.create_directory = _create_directory
artifact.core.system.delete_file = _delete_file
artifact.core.system.copy_file = _copy_file
artifact.core.system.move_file = _move_file
artifact.core.system.start_process = _start_process
artifact.core.system.process_status = _process_status
artifact.core.system.terminate_process = _terminate_process
artifact.core.system.forget_process = _forget_process
)PYCODE";
    py.registerPreludeCode(code);
}

void CorePythonAPI::registerCompositionAPI() {
    auto& py = PythonEngine::instance();
    std::string code = R"PYCODE(
import artifact.core
import json

# Composition API - delegates to an optional host-provided bridge.
def _invoke_workspace(method, *args):
    bridge = getattr(artifact, "_workspace_invoke", None)
    if bridge is None:
        return {"success": False, "message": "Workspace automation bridge is not registered", "method": method}
    try:
        result = bridge(method, *args)
        if isinstance(result, str):
            try:
                return json.loads(result)
            except Exception:
                return {"success": True, "value": result}
        return result
    except Exception as error:
        return {"success": False, "message": str(error), "method": method}

def _get_current_comp_id():
    """Get the ID of the currently active composition."""
    result = _invoke_workspace("currentCompositionSnapshot")
    return result.get("id", "") if isinstance(result, dict) else result

def _get_composition_list():
    """Get list of all compositions in the project."""
    result = _invoke_workspace("listCompositions")
    return result.get("compositions", []) if isinstance(result, dict) else result

def _create_composition(name, width=1920, height=1080):
    """Create a new composition."""
    return _invoke_workspace("createComposition", name, width, height)

def _set_work_area(start_frame, end_frame):
    """Set the work area (in/out points) for playback/export."""
    return _invoke_workspace("setWorkArea", start_frame, end_frame)

def _get_playback_frame():
    """Get current playback head position in frames."""
    result = _invoke_workspace("playbackGetCurrentFrame")
    return result.get("value", 0) if isinstance(result, dict) else result

def _set_playback_frame(frame):
    """Set playback head position."""
    return _invoke_workspace("playbackSetCurrentFrame", frame)

def _playback_play():
    """Start playback."""
    return _invoke_workspace("playbackStart")

def _playback_pause():
    """Pause playback."""
    return _invoke_workspace("playbackPause")

def _playback_stop():
    """Stop playback and return to start."""
    return _invoke_workspace("playbackStop")

def _export_comp(composition_id, output_path, fmt, codec, width, height, fps, bitrate):
    """Export a composition to file."""
    return _invoke_workspace("exportComposition", composition_id, output_path, fmt, codec, width, height, fps, bitrate)

artifact.core.composition.get_current_comp_id = _get_current_comp_id
artifact.core.composition.get_composition_list = _get_composition_list
artifact.core.composition.create = _create_composition
artifact.core.composition.set_work_area = _set_work_area
artifact.core.composition.get_playback_frame = _get_playback_frame
artifact.core.composition.set_playback_frame = _set_playback_frame
artifact.core.composition.playback_play = _playback_play
artifact.core.composition.playback_pause = _playback_pause
artifact.core.composition.playback_stop = _playback_stop
artifact.core.composition.export = _export_comp

def _command_vocabulary():
    """Return the WorkspaceAutomation Command IR type catalog."""
    return _invoke_workspace("commandVocabulary")

def _validate_command(command):
    """Validate a Command IR mapping without executing it."""
    if not isinstance(command, dict):
        raise TypeError("command must be a dict")
    return _invoke_workspace("validateCommand", command)

def _execute_command(command):
    """Execute a Command IR mapping and return its structured result."""
    if not isinstance(command, dict):
        raise TypeError("command must be a dict")
    return _invoke_workspace("executeCommand", command)

artifact.core.automation.command_vocabulary = _command_vocabulary
artifact.core.automation.validate_command = _validate_command
artifact.core.automation.execute_command = _execute_command
)PYCODE";
    py.registerPreludeCode(code);
}

} // namespace ArtifactCore
