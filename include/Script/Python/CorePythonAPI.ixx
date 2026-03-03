module;
export module Script.Python.CoreAPI;

import std;
import Script.Python.Engine;

export namespace ArtifactCore {

/**
 * @brief Registers ArtifactCore's built-in utility functions into Python.
 * These are stateless, underlying engine capabilities (Math, Color, DSP, System).
 * 
 * Usage in Python:
 *   import artifact.core
 *   artifact.core.math.distance([0,0], [10,10])
 *   artifact.core.color.hsv_to_rgb(0.5, 1.0, 1.0)
 *   artifact.core.dsp.db_to_linear(-6.0)
 */
class CorePythonAPI {
public:
    static void registerAll();

private:
    static void registerMathAPI();
    static void registerColorAPI();
    static void registerDSPAPI();
    static void registerSystemAPI();
};

} // namespace ArtifactCore
