module;
#include <utility>
#include <string>
#include <vector>
#include <cmath>
module Script.Python.CoreAPI;

import Script.Python.Engine;

namespace ArtifactCore {

void CorePythonAPI::registerAll() {
    auto& py = PythonEngine::instance();
    // Pre-create the artifact.core submodules
    py.execute(R"PYCODE(
import sys
if 'artifact' not in sys.modules:
    class _ArtifactStub: pass
    sys.modules['artifact'] = _ArtifactStub()

class _CoreMath: pass
class _CoreColor: pass
class _CoreDSP: pass
class _CoreSystem: pass
class _CoreComposition: pass

class _CoreModule:
    math = _CoreMath()
    color = _CoreColor()
    dsp = _CoreDSP()
    system = _CoreSystem()
    composition = _CoreComposition()

sys.modules['artifact'].core = _CoreModule()
sys.modules['artifact.core'] = sys.modules['artifact'].core
)PYCODE");

    registerMathAPI();
    registerColorAPI();
    registerDSPAPI();
    registerSystemAPI();
    registerCompositionAPI();
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
    py.execute(code);
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
    py.execute(code);
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
    py.execute(code);
}

void CorePythonAPI::registerSystemAPI() {
    auto& py = PythonEngine::instance();
    std::string code = R"PYCODE(
import artifact.core
import os
import sys

# System / Engine level API
def _get_engine_version():
    return "ArtifactCore v1.0"

def _get_platform():
    return sys.platform

def _resolve_path(path):
    """Resolve an Artifact relative path to absolute OS path"""
    return os.path.abspath(path)

artifact.core.system.get_engine_version = _get_engine_version
artifact.core.system.get_platform = _get_platform
artifact.core.system.resolve_path = _resolve_path
)PYCODE";
    py.execute(code);
}

void CorePythonAPI::registerCompositionAPI() {
    auto& py = PythonEngine::instance();
    std::string code = R"PYCODE(
import artifact.core
import json

# Composition API - exposes workspace automation through Python
# These are placeholders that call into WorkspaceAutomation::invokeMethod

def _get_current_comp_id():
    """Get the ID of the currently active composition."""
    # Placeholder - actual implementation would query the app
    return ""

def _get_composition_list():
    """Get list of all compositions in the project."""
    return []

def _create_composition(name, width=1920, height=1080):
    """Create a new composition."""
    return {"success": False, "message": "Use WorkspaceAutomation.invokeMethod from Artifact UI"}

def _set_work_area(start_frame, end_frame):
    """Set the work area (in/out points) for playback/export."""
    return False

def _get_playback_frame():
    """Get current playback head position in frames."""
    return 0

def _set_playback_frame(frame):
    """Set playback head position."""
    return False

def _playback_play():
    """Start playback."""
    return False

def _playback_pause():
    """Pause playback."""
    return False

def _playback_stop():
    """Stop playback and return to start."""
    return False

def _export_comp(composition_id, output_path, fmt, codec, width, height, fps, bitrate):
    """Export a composition to file."""
    return {"success": False, "message": "Exported via WorkspaceAutomation"}

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
)PYCODE";
    py.execute(code);
}

} // namespace ArtifactCore
