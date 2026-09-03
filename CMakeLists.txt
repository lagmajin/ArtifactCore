cmake_minimum_required(VERSION 3.30)
project(ArtifactCore)

set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "")

# ---------------------------------------------------------------------------
# Explicit source manifest
# ---------------------------------------------------------------------------
# Do not discover or classify source files during configure.  In particular,
# reading every .cppm to find `export module` made an unrelated new file force
# a full source-tree scan.  Add new sources to cmake/ArtifactCoreSources.cmake
# and place public .cppm units in ARTIFACTCORE_MODULE_SOURCES explicitly.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArtifactCoreSources.cmake")
if(MSVC)
    add_compile_options(
        [=[/reference;std=CMakeFiles\\__cmake_cxx23.dir\\artifact_std.ifc]=])
    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/include/Render/RenderStatics.ixx"
        PROPERTIES COMPILE_OPTIONS
        [=[/reference;std=CMakeFiles\\__cmake_cxx23.dir\\artifact_std.ifc]=])
    set_source_files_properties(
          "${CMAKE_CURRENT_SOURCE_DIR}/include/Common/ThreadPool.ixx"
          PROPERTIES COMPILE_OPTIONS
          [=[/reference;std=CMakeFiles\\__cmake_cxx23.dir\\artifact_std.ifc]=])
endif()


set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Script/CSharpScriptEngine.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
    "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
    "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Script/CSharpScriptEngine.cppm" APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/NamedVector.ixx.obj"
    "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")

# AbstractProperty is registered by multiple split-target setup blocks.  Keep
# its implementation references attached after those blocks so later source
# property assignments cannot discard the required IFCs.
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Property/AbstractProperty.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Property.Abstract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Property.Abstract.ifc"
    "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
    "/reference;Script.Expression.Evaluator=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Evaluator.ifc"
    "/reference;Script.Expression.Value=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Value.ifc"
    "/reference;Script.Engine.Context=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Engine.Context.ifc"
    "/reference;Math.Interpolate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Interpolate.ifc"
    "/reference;EnvironmentVariable.Expansion=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEnvironment.dir/EnvironmentVariable.Expansion.ifc"
    "/reference;Audio.Modulation.Router=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.Modulation.Router.ifc")
set(CORE_MODULES ${ARTIFACTCORE_MODULE_SOURCES})
set(CORE_IMPL ${ARTIFACTCORE_IMPL_SOURCES})

# The generated manifest is rooted at this file's `cmake/` directory and
# therefore contains path spellings such as `cmake/../include/...`.  The
# extracted domain lists below are rooted at ArtifactCore itself.  Normalize
# both manifest lists before the domain split so list(REMOVE_ITEM) compares
# the same paths and a module is owned by exactly one target.
set(_artifact_normalized_core_modules)
foreach(_artifact_source IN LISTS CORE_MODULES)
    file(REAL_PATH "${_artifact_source}" _artifact_normalized_source)
    list(APPEND _artifact_normalized_core_modules "${_artifact_normalized_source}")
endforeach()
set(CORE_MODULES ${_artifact_normalized_core_modules})

# A CXX_MODULES file set must consist of sources that CMake schedules for C++
# compilation.  Keep the language explicit for both .ixx and exported .cppm
# units so Visual Studio generators do not classify module-only sources as
# non-compilable project items before the file set is generated.
set_source_files_properties(${CORE_MODULES} PROPERTIES LANGUAGE CXX)

set(_artifact_normalized_core_impl)
foreach(_artifact_source IN LISTS CORE_IMPL)
    file(REAL_PATH "${_artifact_source}" _artifact_normalized_source)
    list(APPEND _artifact_normalized_core_impl "${_artifact_normalized_source}")
endforeach()
set(CORE_IMPL ${_artifact_normalized_core_impl})

# These are implementation units for interface partitions already present in
# the CXX_MODULES file set.  CMake's scanner treats the repeated partition
# name as a second provider and emits duplicate IFC rules, so keep the source
# private and let MSVC compile it without a separate dyndep scan.
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/File/FileInfo.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Image/OpenEXR.cppm"
    PROPERTIES CXX_SCAN_FOR_MODULES OFF
)
if(MSVC)
    if(CMAKE_GENERATOR MATCHES "Ninja")
        set(_artifact_object_dir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir")
    else()
        set(_artifact_object_dir "${CMAKE_CURRENT_BINARY_DIR}/ArtifactCore.dir/$<CONFIG>")
    endif()
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ImageProcessing/BroadcastColors.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ImageProcessing/ChromaKey.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ImageProcessing/ChromaSpread.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ImageProcessing/ChromaSpreadGlow.cppm"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Color.GamutConversion=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.GamutConversion.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc"
            "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
            "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
            "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
            "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
            "/reference;ImageProcessing:Duotone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Duotone.ifc"
            "/reference;ImageProcessing:Echo=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Echo.ifc"
            "/reference;ImageProcessing:EdgeEcho=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-EdgeEcho.ifc"
            "/reference;ImageProcessing:Emboss=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Emboss.ifc"
            "/reference;ImageProcessing:BroadcastColors=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-BroadcastColors.ifc"
            "/reference;ImageProcessing:ChromaKey=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-ChromaKey.ifc"
            "/reference;ImageProcessing:ChromaSpread=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-ChromaSpread.ifc"
            "/reference;ImageProcessing:ChromaSpreadGlow=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-ChromaSpreadGlow.ifc"
            "/reference;ImageProcessing.BroadcastColorsCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.BroadcastColorsCS.ifc"
            "/reference;ImageProcessing.ChromaKeyCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ChromaKeyCS.ifc"
            "/reference;ImageProcessing.ColorTransform.ChannelMixer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.ChannelMixer.ifc"
            "/reference;ImageProcessing.ColorTransform.ColorBalance=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.ColorBalance.ifc"
            "/reference;ImageProcessing.ColorTransform.Colorama=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Colorama.ifc"
            "/reference;ImageProcessing.ColorTransform.Fill=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Fill.ifc"
            "/reference;ImageProcessing.ColorTransform.GradientRamp=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.GradientRamp.ifc"
            "/reference;ImageProcessing.ColorTransform.LevelsCurves=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.LevelsCurves.ifc"
            "/reference;ImageProcessing.ColorTransform.PhotoFilter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.PhotoFilter.ifc"
            "/reference;ImageProcessing.ColorTransform.SelectiveColor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.SelectiveColor.ifc"
            "/reference;ImageProcessing.ColorTransform.Tritone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Tritone.ifc"
            "/reference;ImageProcessing.EmbossCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.EmbossCS.ifc"
            "/reference;ImageProcessing.LeaveColorCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.LeaveColorCS.ifc"
            "/reference;ImageProcessing.NegateCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.NegateCS.ifc"
            "/reference;ImageProcessing.ProceduralTexture=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ProceduralTexture.ifc"
            "/reference;ImageProcessing.ScatterCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ScatterCS.ifc"
            "/reference;ImageProcessing.SimpleChokerCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.SimpleChokerCS.ifc"
            "/reference;ImageProcessing:AffineTransform=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AffineTransform.ifc"
            "/reference;ImageProcessing:AnamorphicFlare=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AnamorphicFlare.ifc"
            "/reference;ImageProcessing:AnisotropicFlowBlur=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AnisotropicFlowBlur.ifc"
            "/reference;ImageProcessing:AntiAliasing=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AntiAliasing.ifc"
            "/reference;Particle=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Particle.ifc")
    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/File/FileInfo.cppm"
        PROPERTIES
            OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreFile.dir/include/File/FileInfo.ixx.obj"
    )
    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Image/OpenEXR.cppm"
        PROPERTIES
            COMPILE_OPTIONS "/reference;Image:OpenEXR=$(IntDir)/include/Image/OpenEXR.ixx.ifc;/reference;Image.DeepImageBuffer=$(IntDir)/include/Image/DeepImageBuffer.ixx.ifc"
            OBJECT_DEPENDS "${_artifact_object_dir}/include/Image/OpenEXR.ixx.obj;${_artifact_object_dir}/include/Image/DeepImageBuffer.ixx.obj"
    )
endif()

# Private .cppm implementation units use `module Name;` to attach to the
# public interface.  MSVC needs an explicit BMI reference for those units;
# CMake emits per-source Visual Studio metadata only when the source property
# is set before the source is attached to its target.  Keep the precomputed
# mapping explicit rather than reading every source during CMake configure.
set(_artifact_scanned_impl ${CORE_IMPL})
list(REMOVE_ITEM _artifact_scanned_impl
    "${CMAKE_CURRENT_SOURCE_DIR}/src/File/FileInfo.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Image/OpenEXR.cppm"
)
set_source_files_properties(${_artifact_scanned_impl} PROPERTIES CXX_SCAN_FOR_MODULES ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArtifactCoreModuleReferences.cmake")
foreach(_artifact_module_reference IN LISTS ARTIFACTCORE_IMPLEMENTATION_MODULE_REFERENCES)
    string(REPLACE "|" ";" _artifact_module_fields "${_artifact_module_reference}")
    list(GET _artifact_module_fields 0 _artifact_impl_relative)
    list(GET _artifact_module_fields 1 _artifact_module_name)
    list(GET _artifact_module_fields 2 _artifact_interface_relative)
    set(_artifact_impl_file "${CMAKE_CURRENT_SOURCE_DIR}/${_artifact_impl_relative}")
    # The dependency scanner does not model a primary-interface attachment
    # (`module Name;`) reliably.  Let the explicit reference below be the
    # single source of truth for these implementation units.
    set_source_files_properties("${_artifact_impl_file}" PROPERTIES CXX_SCAN_FOR_MODULES OFF)
    set_property(SOURCE "${_artifact_impl_file}" APPEND_STRING PROPERTY
        COMPILE_FLAGS " /TP")
    # APIKeyManager.cppm is compiled with scanner support disabled.  Its
    # primary interface BMI uses the module name as its output, rather than
    # the source path spelling used by the generic mapping below.
    if(_artifact_impl_relative STREQUAL "src/AI/CloudAgent.cppm")
        # This implementation is currently emitted in the base ArtifactCore
        # target.  Use concrete IFC paths so the rule remains valid under
        # Ninja/MSVC, where $(IntDir) is not expanded by CMake-generated
        # compile rules.
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Core.AI.CloudAgent=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.AI.CloudAgent.ifc"
                "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
                "/reference;Core.AI.Context=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.AI.Context.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/AI/LlamaLocalAgent.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Core.AI.LlamaAgent=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.AI.LlamaAgent.ifc"
                "/reference;Core.AI.Context=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.AI.Context.ifc"
                "/reference;Core.AI.LocalAgent=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.AI.LocalAgent.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Diagnostics/Logger.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Diagnostics.Logger=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Diagnostics.Logger.ifc"
                "/reference;Event.Bus=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEvent.dir/Event.Bus.ifc"
                "/reference;Core.Diagnostics.Recorder=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Diagnostics.Recorder.ifc"
                "/reference;Core.Diagnostics.Snapshot=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Diagnostics.Snapshot.ifc"
                "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
                "/reference;Utils.Result=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Result.ifc"
                "/reference;Utils.Optional=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Optional.ifc"
                "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
                "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc"
                "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Animation/AnimatableTransform2D.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Animation.Transform2D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Animation.Transform2D.ifc"
                "/reference;Animation.Value=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Animation.Value.ifc"
                "/reference;Frame.Position=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Position.ifc"
                "/reference;Math.Interpolate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Interpolate.ifc"
                "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc"
                "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
                "/reference;Frame.Rate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc"
                "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc"
                "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
                "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Animation/AnimatableTransform3D.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
                "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
                "/reference;Animation.Transform3D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Animation.Transform3D.ifc"
                "/reference;Animation.Value=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Animation.Value.ifc"
                "/reference;Frame.Position=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Position.ifc"
                "/reference;Math.Interpolate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Interpolate.ifc"
                "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc"
                "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
                "/reference;Frame.Rate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc"
                "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc"
                "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Codec/EncoderSetting.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Codec.Encoder.Setting=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Codec.Encoder.Setting.ifc"
                "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Codec/GStreamerDecoder.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Video.GStreamerDecoder=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Video.GStreamerDecoder.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Codec/MFEncoder.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Codec.MFEncoder=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Codec.MFEncoder.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Animation/AnimationDynamics.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Animation.Dynamics=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Animation.Dynamics.ifc"
                "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Channel/Channel.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Channel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Channel.ifc"
                "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
                "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Codec/MFFrameExtractor.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Codec.MFFrameExtractor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Codec.MFFrameExtractor.ifc"
                "/reference;Memory.TrackedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.TrackedPtr.ifc"
                "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
                "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
                "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc"
                "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Color/ColorBlendMode.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Color.BlendMode=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.BlendMode.ifc"
                "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc"
                "/reference;Color.Conversion=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Conversion.ifc"
                "/reference;Color.Luminance=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Luminance.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Color/ColorConversion.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Color.Conversion=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Conversion.ifc"
                "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Color/ColorHarmonizer.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Color.Harmonizer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Harmonizer.ifc"
                "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc"
                "/reference;Color.Conversion=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Conversion.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Color/ColorLuminance.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Color.Luminance=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Luminance.ifc"
                "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Color/ColorLUT.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Color.LUT=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.LUT.ifc"
                "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
                "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
                "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Color/ColorSpace.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Color.ColorSpace=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.ColorSpace.ifc"
                "/reference;Color.GamutConversion=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.GamutConversion.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Color/FloatColor.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc"
                "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Color/FloatRGBA.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
                "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Color/LabColor.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Color.Lab=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Lab.ifc"
                "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc"
                "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Color/LUTWriter.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Color.LUTWriter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.LUTWriter.ifc"
                "/reference;Color.LUT=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.LUT.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Color/OCIOConfig.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Color.OCIOConfig=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.OCIOConfig.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Color/Sarturation.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Color.Saturation=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Saturation.ifc"
                "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Color/XYZColor.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Color.XYZ=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.XYZ.ifc"
                "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc"
                "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Common/Point2D.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Core.Point2D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Point2D.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Core/AspectRatio.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Core.AspectRatio=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.AspectRatio.ifc"
                "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
                "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Core/FastSettingsStore.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Core.FastSettingsStore=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.FastSettingsStore.ifc"
                [=[/reference;std=CMakeFiles\\__cmake_cxx23.dir\\artifact_std.ifc]=])
    elseif(_artifact_impl_relative STREQUAL "src/Color/ColorSwatch.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Color.Swatch=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Swatch.ifc"
                "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc"
                "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
                "/reference;Memory.TrackedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.TrackedPtr.ifc"
                "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
                "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
                "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Color/AutoColorMatch.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Color.AutoMatch=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.AutoMatch.ifc"
                "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc"
                "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
                "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
                "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
                "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
                "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
                "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
                "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
                "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Diagnostics/CrashHandler.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Diagnostics.CrashHandler=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Diagnostics.CrashHandler.ifc"
                [=[/reference;std=CMakeFiles\\__cmake_cxx23.dir\\artifact_std.ifc]=])
    elseif(_artifact_impl_relative STREQUAL "src/Core/Position2D.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Core.Position2D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Position2D.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Core/KeyFrame.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Core.KeyFrame=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.KeyFrame.ifc"
                "/reference;Frame.Position=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Position.ifc"
                "/reference;Math.Interpolate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Interpolate.ifc"
                "/reference;Frame.Rate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc"
                "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Core/UniformScale.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Core.UniformScale=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.UniformScale.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Core/Opacity.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Core.Opacity=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Opacity.ifc"
                "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Core/Parallel.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Frame/FrameSkipTracker.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Frame.SkipTracker=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.SkipTracker.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Frame/FramePosition.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Frame.Position=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Position.ifc"
                "/reference;Frame.Rate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc"
                "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc"
                "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Frame/FrameTime.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Frame.Time=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Time.ifc"
                "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/GPU/GPUInfo.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Geometry/LayerBoundsCalculator.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Geometry.LayerBounds=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Geometry.LayerBounds.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Geometry/LayerAlignment.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Geometry.LayerAlignment=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Geometry.LayerAlignment.ifc"
                "/reference;Geometry.LayerBounds=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Geometry.LayerBounds.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Geometry/ResolutionRemap.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Geometry.ResolutionRemap=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Geometry.ResolutionRemap.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Geometry/BezierPathSampler.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Math.Bezier.Sampler=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Bezier.Sampler.ifc"
                "/reference;Math.Bezier=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Bezier.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Geometry/BezierCalculator.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Math.Bezier=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Bezier.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Geometry/MeshImporter.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;MeshImporter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/MeshImporter.ifc"
                "/reference;Mesh=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Mesh.ifc"
                "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
                "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/Compute.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc"
                "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
                "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/GPUComputeContext.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
                "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/GPUTexture.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Graphics.Texture=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Texture.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Frame/FrameRate.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Frame.Rate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc"
                "/reference;Serialization.JsonAdapter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.JsonAdapter.ifc"
                "/reference;Serialization.SchemaMigration=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.SchemaMigration.ifc"
                "/reference;Serialization.ISerializable=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.ISerializable.ifc"
                "/reference;Serialization.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.Registry.ifc"
                "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Diagnostics/ProjectDiagnostic.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
                "/reference;Core.Diagnostics.ProjectDiagnostic=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Diagnostics.ProjectDiagnostic.ifc"
                "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Diagnostics/DiagnosticEngine.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Core.Diagnostics.DiagnosticEngine=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Diagnostics.DiagnosticEngine.ifc"
                "/reference;Core.Diagnostics.ProjectDiagnostic=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Diagnostics.ProjectDiagnostic.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Diagnostics/Logger.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Diagnostics.Logger=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Diagnostics.Logger.ifc"
                "/reference;Event.Bus=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEvent.dir/Event.Bus.ifc"
                "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
                "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
                "/reference;Core.Diagnostics.Recorder=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Diagnostics.Recorder.ifc"
                "/reference;Core.Diagnostics.Snapshot=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Diagnostics.Snapshot.ifc"
                "/reference;Utils.Result=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Result.ifc"
                "/reference;Utils.Optional=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Optional.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Frame/FrameOffset.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Frame.Offset=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Offset.ifc"
                "/reference;Frame.Rate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc"
                "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc"
                "/reference;Time.Code=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Code.ifc"
                "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
                "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Frame/FrameRange.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Frame.Range=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Range.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
            "/reference;Frame.Position=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Position.ifc"
            "/reference;Frame.Offset=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Offset.ifc"
            "/reference;Frame.Rate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc"
            "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc"
            "/reference;Serialization.JsonAdapter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.JsonAdapter.ifc"
            "/reference;Serialization.SchemaMigration=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.SchemaMigration.ifc"
            "/reference;Serialization.ISerializable=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.ISerializable.ifc"
            "/reference;Serialization.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.Registry.ifc"
            "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
            "/reference;Time.Code=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Code.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
            "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc"
            "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Math/NoiseGenerator.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Math.Noise=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Noise.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Math/Noise.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/NamedVector.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Geometry/Procedural3DGenerators.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Procedural3DGenerators=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Procedural3DGenerators.ifc"
                "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
                "/reference;Math.Noise=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Noise.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Geometry/Fracture.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Geometry.Fracture=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Geometry.Fracture.ifc"
            "/reference;Memory.TrackedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.TrackedPtr.ifc"
            "/reference;Physics.Fracture=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Physics.Fracture.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Math.Random=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Random.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
            "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Mesh=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Mesh.ifc"
            "/reference;Particle=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Particle.ifc"
            "/reference;Utils.Id=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Id.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/Compute/LUT3DComputer.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Graphics.Compute.LUT3DComputer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.LUT3DComputer.ifc"
                "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc"
                "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
                "/reference;Graphics.Shader.Compute.HLSL.LUT3D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.Compute.HLSL.LUT3D.ifc"
                "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc"
                "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            OBJECT_DEPENDS
                "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Graphics/Shader/Compute/HLSL/LUT3D.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/BoidsCompute.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Graphics.BoidsCompute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.BoidsCompute.ifc"
                "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
                "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
                "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc"
                "/reference;Graphics.ParticleData=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.ParticleData.ifc")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/RayTracingManager.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Graphics.RayTracingManager=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.RayTracingManager.ifc"
                "/reference;Graphics=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.ifc"
                "/reference;Graphics:GraphicsHelper=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics-GraphicsHelper.ifc"
                "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc"
                "/reference;Graphics.CBuffer.Constants.Helper=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.CBuffer.Constants.Helper.ifc"
                "/reference;Graphics.CBuffer.Constants.PixelShader=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.CBuffer.Constants.PixelShader.ifc"
                "/reference;Graphics.CBuffer.Constants.VertexShader=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.CBuffer.Constants.VertexShader.ifc"
                "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc"
                "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc"
                "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
                "/reference;Graphics.Effect.Creative=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.ifc"
                "/reference;Graphics.ImageCache=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.ImageCache.ifc"
                "/reference;Graphics.Helper.PSO=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Helper.PSO.ifc"
                "/reference;Graphics.Helpler.GLM=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Helpler.GLM.ifc"
                "/reference;Graphics.Resource.PSOAndSRB=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Resource.PSOAndSRB.ifc"
                "/reference;Graphics.Shader.Compute.MaskCutout=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.Compute.MaskCutout.ifc"
                "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
                "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc"
                "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
                "/reference;Graphics.Shader.HLSL.Basics.Pixel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.HLSL.Basics.Pixel.ifc"
                "/reference;Graphics.Shader.HLSL.Basics.Vertex=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.HLSL.Basics.Vertex.ifc"
                "/reference;Graphics.Shader.Set=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.Set.ifc"
                "/reference;Math.Noise=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Noise.ifc"
                "/reference;Layer.Matte=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Layer.Matte.ifc"
                "/reference;Graphics.Effect.Creative.ColorVibrance=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.ColorVibrance.ifc"
                "/reference;Graphics.Effect.Creative.DepthMelt=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.DepthMelt.ifc"
                "/reference;Graphics.Effect.Creative.EdgeEcho=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.EdgeEcho.ifc"
                "/reference;Graphics.Effect.Creative.Factory=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Factory.ifc"
                "/reference;Graphics.Effect.Creative.Fisheye=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Fisheye.ifc"
                "/reference;Graphics.Effect.Creative.Glitch=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Glitch.ifc"
                "/reference;Graphics.Effect.Creative.Halftone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Halftone.ifc"
                "/reference;Graphics.Effect.Creative.Kaleidoscope=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Kaleidoscope.ifc"
                "/reference;Graphics.Effect.Creative.LightPressure=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.LightPressure.ifc"
                "/reference;Graphics.Effect.Creative.Manager=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Manager.ifc"
                "/reference;Graphics.Effect.Creative.Mirror=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Mirror.ifc"
                "/reference;Graphics.Effect.Creative.OldTV=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.OldTV.ifc"
                "/reference;Graphics.Effect.Creative.PigmentSeparation=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.PigmentSeparation.ifc"
                "/reference;Graphics.Effect.Creative.Pixelate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Pixelate.ifc"
                "/reference;Graphics.Effect.Creative.Posterize=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Posterize.ifc"
                "/reference;Graphics.Effect.Creative.SurfaceMemory=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.SurfaceMemory.ifc"
                "/reference;Graphics.Effect.Creative.TemporalFossil=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.TemporalFossil.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/GPUTextureCacheManager.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Graphics.ImageCache=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.ImageCache.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/PSOCache.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Graphics.PSO.Cache=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.PSO.Cache.ifc"
                "/reference;Utils.Path=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Path.ifc"
                "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/Shader/BasicGeometryShader.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Graphics.Shader.HLSL.Geometry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.HLSL.Geometry.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Diagnostics/FallbackPolicy.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Core.Diagnostics.FallbackPolicy=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Diagnostics.FallbackPolicy.ifc"
            "/reference;Utils.Optional=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Optional.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
            "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/Effect/ChromaticAberrationEffect.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Graphics.Effect.Creative.ChromaticAberration=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.ChromaticAberration.ifc"
            "/reference;Graphics.Effect.Creative=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.ifc"
            "/reference;Channel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Channel.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/SandGPUCompute.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Graphics.SandGPUCompute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SandGPUCompute.ifc"
                "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
                "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc"
                "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc"
                "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/MpmCompute.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Graphics.MpmCompute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.MpmCompute.ifc"
                "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
                "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc"
                "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/Shader/Compute/GPUCompressionPipeline.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Graphics.GPUCompressionPipeline=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUCompressionPipeline.ifc"
                "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc"
                "/reference;Graphics.Shader.Compute.HLSL.Compression=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.Compute.HLSL.Compression.ifc"
                "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
                "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/Texture/TextureManager.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Graphics.Texture.Manager=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Texture.Manager.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/DirectCompute/SinglePassShader.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.Shader=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.Shader.ifc"
            "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc"
            "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
            "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc"
            "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/DirectCompute/BroadcastColorsCS.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/DirectCompute/ChromaKeyCS.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/DirectCompute/EmbossCS.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/DirectCompute/LeaveColorCS.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/DirectCompute/LumaKeyCS.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/DirectCompute/MedianCS.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/DirectCompute/NegateCS.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/DirectCompute/ScatterCS.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/DirectCompute/SimpleChokerCS.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/DirectCompute/ThresholdCS.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Channel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Channel.ifc"
            "/reference;Image=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ifc"
            "/reference;Image.ImageF32x4RGBAWithCache=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4RGBAWithCache.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Image.ImageYUV420=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageYUV420.ifc"
            "/reference;Image.MultiChannelImage=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.MultiChannelImage.ifc"
            "/reference;Image.PSDDocument=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.PSDDocument.ifc"
            "/reference;Image.Raw=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Raw.ifc"
            "/reference;Image.GpuImageUpload=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.GpuImageUpload.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;Image.ImageF16x4=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF16x4.ifc"
            "/reference;ImageProcessing.BroadcastColorsCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.BroadcastColorsCS.ifc"
           "/reference;ImageProcessing.ChromaKeyCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ChromaKeyCS.ifc"
            "/reference;ImageProcessing.EmbossCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.EmbossCS.ifc"
            "/reference;ImageProcessing.LeaveColorCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.LeaveColorCS.ifc"
            "/reference;ImageProcessing.LumaKeyCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.LumaKeyCS.ifc"
            "/reference;ImageProcessing.MedianCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.MedianCS.ifc"
            "/reference;ImageProcessing.NegateCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.NegateCS.ifc"
            "/reference;ImageProcessing.ScatterCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ScatterCS.ifc"
            "/reference;ImageProcessing.SimpleChokerCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.SimpleChokerCS.ifc"
            "/reference;ImageProcessing.ThresholdCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ThresholdCS.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/DirectCompute/GlowCS.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Graphics=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.ifc"
            "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
            "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc"
            "/reference;GraphicsHelper=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/GraphicsHelper.ifc"
            "/reference;Graphics.Resource.PSOAndSRB=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Resource.PSOAndSRB.ifc"
            "/reference;Graphics.ImageCache=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.ImageCache.ifc"
            "/reference;Graphics.Helper.PSO=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Helper.PSO.ifc"
            "/reference;Graphics.Helpler.GLM=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Helpler.GLM.ifc"
            "/reference;Graphics.Shader.Set=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.Set.ifc"
            "/reference;Graphics.Shader.Compute.MaskCutout=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.Compute.MaskCutout.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Graphics.Shader.HLSL.Basics.Vertex=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.HLSL.Basics.Vertex.ifc"
            "/reference;Graphics.Shader.HLSL.Basics.Pixel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.HLSL.Basics.Pixel.ifc"
            "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc"
            "/reference;Graphics:GraphicsHelper=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics-GraphicsHelper.ifc"
            "/reference;Graphics.Effect.Creative=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.ifc"
            "/reference;Graphics.Effect.Creative.Manager=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Manager.ifc"
            "/reference;Graphics.Effect.Creative.Mirror=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Mirror.ifc"
            "/reference;Graphics.Effect.Creative.LightPressure=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.LightPressure.ifc"
            "/reference;Graphics.Effect.Creative.OldTV=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.OldTV.ifc"
            "/reference;Graphics.Effect.Creative.PigmentSeparation=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.PigmentSeparation.ifc"
            "/reference;Graphics.CBuffer.Constants.Helper=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.CBuffer.Constants.Helper.ifc"
            "/reference;Graphics.CBuffer.Constants.PixelShader=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.CBuffer.Constants.PixelShader.ifc"
            "/reference;Graphics.CBuffer.Constants.VertexShader=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.CBuffer.Constants.VertexShader.ifc"
            "/reference;Graphics.Effect.Creative.ColorVibrance=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.ColorVibrance.ifc"
            "/reference;Graphics.Effect.Creative.DepthMelt=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.DepthMelt.ifc"
            "/reference;Graphics.Effect.Creative.EdgeEcho=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.EdgeEcho.ifc"
            "/reference;Graphics.Effect.Creative.Factory=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Factory.ifc"
            "/reference;Graphics.Effect.Creative.Fisheye=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Fisheye.ifc"
            "/reference;Graphics.Effect.Creative.Glitch=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Glitch.ifc"
            "/reference;Graphics.Effect.Creative.Halftone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Halftone.ifc"
            "/reference;Graphics.Effect.Creative.Kaleidoscope=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Kaleidoscope.ifc"
            "/reference;Math.Noise=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Noise.ifc"
            "/reference;Layer.Matte=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Layer.Matte.ifc"
            "/reference;ImageProcessing.GlowCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.GlowCS.ifc"
            "/reference;Graphics.Effect.Creative.Pixelate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Pixelate.ifc"
            "/reference;Graphics.Effect.Creative.Posterize=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Posterize.ifc"
            "/reference;Graphics.Effect.Creative.SurfaceMemory=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.SurfaceMemory.ifc"
            "/reference;Graphics.Effect.Creative.TemporalFossil=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.TemporalFossil.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/Draw.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Draw=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Draw.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/FaceDetectionEngine.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ArtifactCore.ImageProcessing.FaceDetection=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ArtifactCore.ImageProcessing.FaceDetection.ifc"
            "/reference;CvUtils=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/CvUtils.ifc"
            "/reference;Memory.TrackedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.TrackedPtr.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
            "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/ImageProcessing/OpenCV/CvUtils.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/ImageProcessing/OpenCV/FaceDetectionEngine.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/NamedVector.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/FaceTracker.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ArtifactCore.ImageProcessing.FaceTracker=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ArtifactCore.ImageProcessing.FaceTracker.ifc"
            "/reference;ArtifactCore.ImageProcessing.FaceDetection=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ArtifactCore.ImageProcessing.FaceDetection.ifc"
            "/reference;Memory.TrackedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.TrackedPtr.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
            "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/GlitchCV.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.GlitchCV=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.GlitchCV.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;Math.Random=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Random.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/Glow.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Glow=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Glow.ifc"
            "/reference;Image=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ifc"
            "/reference;Image.GpuImageUpload=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.GpuImageUpload.ifc"
            "/reference;Image.ImageF16x4=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF16x4.ifc"
            "/reference;Math.Random=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Random.ifc"
            "/reference;Image.ImageF32x4RGBAWithCache=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4RGBAWithCache.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;Particle=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Particle.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Image.ImageYUV420=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageYUV420.ifc"
            "/reference;Image.MultiChannelImage=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.MultiChannelImage.ifc"
            "/reference;Image.PSDDocument=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.PSDDocument.ifc"
            "/reference;Image.Raw=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Raw.ifc"
            "/reference;Channel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Channel.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/Noise.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Noise=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Noise.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;Math.Random=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Random.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/OpenCVPuppetEngine.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ArtifactCore.ImageProcessing.OpenCV.PuppetEngine=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ArtifactCore.ImageProcessing.OpenCV.PuppetEngine.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/LiftCV.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;LiftCV=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/LiftCV.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/SepiaCV.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/VHS_CV.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/SpectralGlowCV.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/SharpenDirectionalBlur.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;${_artifact_module_name}=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/${_artifact_module_name}.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;Math.Random=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Random.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/Shader/BasicShaders.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Graphics.Shader.HLSL.Basics.Pixel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.HLSL.Basics.Pixel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/Shader/BasicVertexShader.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Graphics.Shader.HLSL.Basics.Vertex=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.HLSL.Basics.Vertex.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Image/BitmapImage.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Image.Bitmap=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Bitmap.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Image/ImageF32x4.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageF32x4=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageF32x4.ifc"
            "/reference;CvUtils=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/CvUtils.ifc")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/ImageProcessing/OpenCV/CvUtils.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Image/DeepImageBuffer.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Image.DeepImageBuffer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.DeepImageBuffer.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Image/PNGImage.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Image.Png=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Png.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Image/Cryptomatte/CryptoPixel.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Image.Cryptomatte.Pixel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Cryptomatte.Pixel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Image/Cryptomatte/CryptoImage.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Image.Cryptomatte.Image=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Cryptomatte.Image.ifc"
            "/reference;Image.Cryptomatte.Pixel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Cryptomatte.Pixel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Image/FFmpegAudioEncoder.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Media.Encoder.FFmpegAudioEncoder=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Media.Encoder.FFmpegAudioEncoder.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Image/ImageYUV420.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Image.ImageYUV420=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageYUV420.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
            "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Image/CompressQuality.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;CompressQuality=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/CompressQuality.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Image/Cryptomatte/CryptoExrWriter.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Image.Cryptomatte.ExrWriter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Cryptomatte.ExrWriter.ifc"
            "/reference;Image.Cryptomatte.Image=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Cryptomatte.Image.ifc"
            "/reference;Image.Cryptomatte.Pixel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Cryptomatte.Pixel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/RayTracingManager.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Graphics.RayTracingManager=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.RayTracingManager.ifc"
            "/reference;Graphics=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.ifc"
            "/reference;GraphicsHelper=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics-GraphicsHelper.ifc"
            "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc"
            "/reference;Graphics.CBuffer.Constants.Helper=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.CBuffer.Constants.Helper.ifc"
            "/reference;Graphics.CBuffer.Constants.PixelShader=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.CBuffer.Constants.PixelShader.ifc"
            "/reference;Graphics.CBuffer.Constants.VertexShader=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.CBuffer.Constants.VertexShader.ifc"
            "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc"
            "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc"
            "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
            "/reference;Graphics.Effect.Creative=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.ifc"
            "/reference;Graphics.ImageCache=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.ImageCache.ifc"
            "/reference;Graphics.Helper.PSO=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Helper.PSO.ifc"
            "/reference;Graphics.Helpler.GLM=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Helpler.GLM.ifc"
            "/reference;Graphics.Resource.PSOAndSRB=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Resource.PSOAndSRB.ifc"
            "/reference;Graphics.Shader.Compute.MaskCutout=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.Compute.MaskCutout.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
            "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Graphics.Shader.HLSL.Basics.Pixel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.HLSL.Basics.Pixel.ifc"
            "/reference;Graphics.Shader.HLSL.Basics.Vertex=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.HLSL.Basics.Vertex.ifc"
            "/reference;Graphics.Shader.Set=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.Set.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Render/RenderStatics.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Render.Statics=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Statics.ifc"
            [=[/reference;std=CMakeFiles\\__cmake_cxx23.dir\\artifact_std.ifc]=])
    elseif(_artifact_impl_relative STREQUAL "src/Render/RenderJobModel.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Render.JobModel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.JobModel.ifc"
            "/reference;Utils.Id=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Id.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
            "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/NamedVector.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Script/AngelScript/AngelScriptBehaviour.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Script.AngelScript.Behaviour=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.AngelScript.Behaviour.ifc"
            "/reference;Script.AngelScript.Engine=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.AngelScript.Engine.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Script/Python/PythonEngine.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Script.Python.Engine=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Python.Engine.ifc"
             "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
             "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
             "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
             "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Script/Python/CorePythonAPI.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Script.Python.CoreAPI=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Python.CoreAPI.ifc"
            "/reference;Script.Python.Engine=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Python.Engine.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Script/Engine/Enviroment/EnvironmentManager.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Script.Enviroment=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Enviroment.ifc"
            "/reference;Script.Engine.Value=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Engine.Value.ifc")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Script/Engine/Value/Value.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/ShaderNode/ArtifactShaderNode.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Artifact.ShaderNode.Core=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Artifact.ShaderNode.Core.ifc"
            "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Script/Expression/ExpressionValue.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Script.Expression.Value=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Value.ifc"
            "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
            "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Transform/ZoomScale.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Core.Scale.Zoom=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Scale.Zoom.ifc"
            "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Transform/Light.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Core.Light=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Light.ifc"
            "/reference;Float3=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Float3.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Diagnostics/Logger.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEvent.dir/include/Event/EventBus.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/src/Memory/SharedPtr.cppm.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Transform/ViewportTransformer.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Core.Transform.Viewport=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Transform.Viewport.ifc"
            "/reference;Core.Scale.Zoom=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Scale.Zoom.ifc"
            "/reference;Diagnostics.Logger=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Diagnostics.Logger.ifc"
            "/reference;Core.Diagnostics.Recorder=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Diagnostics.Recorder.ifc"
            "/reference;Core.Diagnostics.Snapshot=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Diagnostics.Snapshot.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Transform/Camera.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Core.Camera=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Camera.ifc"
            "/reference;Float3=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Float3.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/ParticleCompute.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Graphics.ParticleCompute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.ParticleCompute.ifc"
            "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc"
            "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
            "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc"
            "/reference;Particle=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Particle.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/LayerBlendPipeline.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Graphics.LayerBlendPipeline=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.LayerBlendPipeline.ifc"
            "/reference;Artifact.Render.PointwiseEffectFusion=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Artifact.Render.PointwiseEffectFusion.ifc"
            "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc"
            "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
            "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc"
            "/reference;Graphics.Shader.Compute.HLSL.Blend=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.Compute.HLSL.Blend.ifc"
            "/reference;Graphics.Shader.Compute.HLSL.MatteTrack=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.Compute.HLSL.MatteTrack.ifc"
            "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
            "/reference;Layer.Blend=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Layer.Blend.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/ParticleRenderer.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Graphics.ParticleRenderer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.ParticleRenderer.ifc"
            [=[/reference;std=CMakeFiles\\__cmake_cxx23.dir\\artifact_std.ifc]=]
            "/reference;Frame.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Debug.ifc"
            "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
            "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc"
            "/reference;Graphics.ParticleData=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.ParticleData.ifc"
            "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/MeshRenderer.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Graphics.MeshRenderer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.MeshRenderer.ifc"
            [=[/reference;std=CMakeFiles\\__cmake_cxx23.dir\\artifact_std.ifc]=]
            "/reference;Core.Light=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Light.ifc"
            "/reference;Frame.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Debug.ifc"
            "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
            "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc"
            "/reference;Graphics.ParticleData=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.ParticleData.ifc"
            "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc"
            "/reference;Float3=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Float3.ifc"
            "/reference;IO.ImageImporter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/IO.ImageImporter.ifc"
            "/reference;Image.Raw=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Raw.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/NoiseImageGenerator.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Generator=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Generator.ifc"
            "/reference;Generator:Image=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Generator-Image.ifc"
            "/reference;Generator:Noise=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Generator-Noise.ifc"
            "/reference;Generator:SolidColor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Generator-SolidColor.ifc"
            "/reference;Math.Noise=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Noise.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;Size=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Size.ifc"
            "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
            "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/AnisotropicFlowBlur.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ifc"
            "/reference;ImageProcessing:AffineTransform=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AffineTransform.ifc"
            "/reference;ImageProcessing:AnamorphicFlare=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AnamorphicFlare.ifc"
            "/reference;ImageProcessing:AnisotropicFlowBlur=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AnisotropicFlowBlur.ifc"
            "/reference;ImageProcessing:AntiAliasing=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AntiAliasing.ifc"
            "/reference;ImageProcessing:BroadcastColors=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-BroadcastColors.ifc"
            "/reference;ImageProcessing:AffineTransform=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AffineTransform.ifc"
            "/reference;ImageProcessing:AnamorphicFlare=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AnamorphicFlare.ifc"
            "/reference;ImageProcessing:AnisotropicFlowBlur=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AnisotropicFlowBlur.ifc"
            "/reference;ImageProcessing:AntiAliasing=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AntiAliasing.ifc"
            "/reference;ImageProcessing:ChromaKey=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-ChromaKey.ifc"
            "/reference;ImageProcessing:ChromaSpread=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-ChromaSpread.ifc"
            "/reference;ImageProcessing:ChromaSpreadGlow=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-ChromaSpreadGlow.ifc"
            "/reference;ImageProcessing:Duotone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Duotone.ifc"
            "/reference;ImageProcessing:Echo=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Echo.ifc"
            "/reference;ImageProcessing:EdgeEcho=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-EdgeEcho.ifc"
            "/reference;ImageProcessing:Emboss=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Emboss.ifc"
            "/reference;ImageProcessing:Halftone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Halftone.ifc"
            "/reference;ImageProcessing:IBKKeyer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-IBKKeyer.ifc"
            "/reference;ImageProcessing:LeaveColor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-LeaveColor.ifc"
            "/reference;ImageProcessing:LumaKey=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-LumaKey.ifc"
            "/reference;ImageF32x4=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageF32x4.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;ImageProcessing:Median=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Median.ifc"
            "/reference;ImageProcessing:Monochrome=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Monochrome.ifc"
            "/reference;ImageProcessing:MotionTrail=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-MotionTrail.ifc"
            "/reference;ImageProcessing:NegativeCV=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-NegativeCV.ifc"
            "/reference;ImageProcessing:NeuromorphicDisplace=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-NeuromorphicDisplace.ifc"
            "/reference;ImageProcessing:Posterize=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Posterize.ifc"
            "/reference;ImageProcessing:PosterizeTime=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-PosterizeTime.ifc"
            "/reference;ImageProcessing:Scatter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Scatter.ifc"
            "/reference;ImageProcessing:SimpleChoker=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-SimpleChoker.ifc"
            "/reference;ImageProcessing:StrobeLight=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-StrobeLight.ifc"
            "/reference;ImageProcessing:StructureTensor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-StructureTensor.ifc"
            "/reference;ImageProcessing:Threshold=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Threshold.ifc"
            "/reference;ImageProcessing:TiltShift=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-TiltShift.ifc"
            "/reference;ImageProcessing:VectorFlowGlitch=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-VectorFlowGlitch.ifc"
            "/reference;ImageProcessing.AutoContrast.CS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.AutoContrast.CS.ifc"
            "/reference;ImageProcessing.AutoContrast.CV=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.AutoContrast.CV.ifc"
            "/reference;ImageProcessing.BroadcastColorsCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.BroadcastColorsCS.ifc"
            "/reference;ImageProcessing.ChromaKeyCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ChromaKeyCS.ifc"
            "/reference;ImageProcessing.ScatterCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ScatterCS.ifc"
            "/reference;ImageProcessing.SimpleChokerCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.SimpleChokerCS.ifc"
            "/reference;ImageProcessing.ColorTransform.ChannelMixer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.ChannelMixer.ifc"
            "/reference;ImageProcessing.ColorTransform.ColorBalance=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.ColorBalance.ifc"
            "/reference;ImageProcessing.ColorTransform.Colorama=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Colorama.ifc"
            "/reference;ImageProcessing.ColorTransform.Fill=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Fill.ifc"
            "/reference;ImageProcessing.ColorTransform.GradientRamp=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.GradientRamp.ifc"
            "/reference;ImageProcessing.ColorTransform.LevelsCurves=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.LevelsCurves.ifc"
            "/reference;ImageProcessing.ColorTransform.PhotoFilter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.PhotoFilter.ifc"
            "/reference;ImageProcessing.ColorTransform.SelectiveColor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.SelectiveColor.ifc"
            "/reference;ImageProcessing.ColorTransform.Tritone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Tritone.ifc"
            "/reference;ImageProcessing.EmbossCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.EmbossCS.ifc"
            "/reference;ImageProcessing.LeaveColorCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.LeaveColorCS.ifc"
            "/reference;ImageProcessing.NegateCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.NegateCS.ifc"
            "/reference;ImageProcessing.ProceduralTexture=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ProceduralTexture.ifc"
            "/reference;Particle=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Particle.ifc"
            "/reference;Color.GamutConversion=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.GamutConversion.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
            "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
            "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Image/RawImage.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Image.Raw=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Raw.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Image/PSDDocument.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Image.PSDDocument=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.PSDDocument.ifc"
            "/reference;Image.Raw=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Raw.ifc"
            "/reference;Layer.Blend=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Layer.Blend.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Image/ImageF32x4_With_Cache.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Image.ImageF32x4RGBAWithCache=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4RGBAWithCache.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
            "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Physics/Physics2D.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Physics2D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Physics2D.ifc"
            "/reference;Utils.Id=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Id.ifc"
             "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
             "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
             "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc"
             "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Physics/MpmSolver2D.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Physics.Mpm2D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Physics.Mpm2D.ifc"
                "/reference;Graphics.MpmCompute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.MpmCompute.ifc"
                "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
                "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc"
                "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc"
                "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Physics/PhysicsSystem.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Physics.System=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Physics.System.ifc"
                "/reference;Physics.Fluid=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Physics.Fluid.ifc"
                "/reference;Physics2D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Physics2D.ifc"
                "/reference;Physics.SoftBody=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Physics.SoftBody.ifc"
                "/reference;Physics.Mpm2D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Physics.Mpm2D.ifc"
                "/reference;Core.Simulation.Pyro=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreSimulation.dir/Core.Simulation.Pyro.ifc"
                "/reference;Graphics.ParticleData=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.ParticleData.ifc"
                "/reference;Graphics.BoidsCompute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.BoidsCompute.ifc")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreSimulation.dir/Core.Simulation.Pyro.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/IO/Image/ImageImporter.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;IO.ImageImporter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/IO.ImageImporter.ifc"
            "/reference;Image.Raw=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Raw.ifc"
            "/reference;Image.PSDDocument=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.PSDDocument.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Layer/Layer2D.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Layer2D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Layer2D.ifc"
            "/reference;Channel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Channel.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Image=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ifc"
            "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
            "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
            "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc"
            "/reference;Image.GpuImageUpload=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.GpuImageUpload.ifc"
            "/reference;Image.ImageF16x4=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF16x4.ifc"
            "/reference;Image.ImageF32x4RGBAWithCache=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4RGBAWithCache.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Image.ImageYUV420=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageYUV420.ifc"
            "/reference;Image.MultiChannelImage=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.MultiChannelImage.ifc"
            "/reference;Image.PSDDocument=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.PSDDocument.ifc"
            "/reference;Image.Raw=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Raw.ifc"
            "/reference;ImageF32x4=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageF32x4.ifc"
            "/reference;Layer.Blend=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Layer.Blend.ifc"
            "/reference;Layer.Matte=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Layer.Matte.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
            "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc"
            "/reference;Memory.TrackedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.TrackedPtr.ifc"
            "/reference;Transform=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Transform.ifc"
            "/reference;Transform.Scale2D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Transform.Scale2D.ifc"
            "/reference;Transform._2D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Transform._2D.ifc"
            "/reference;Utils.Id=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Id.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/IO/Image/AsyncImageWriterManager.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;IO.Async.ImageWriterManager=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/IO.Async.ImageWriterManager.ifc"
            "/reference;IO.ImageExporter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/IO.ImageExporter.ifc"
            "/reference;Channel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Channel.ifc"
            "/reference;Image=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ifc"
            "/reference;Image.ExportOptions=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ExportOptions.ifc"
            "/reference;Image.GpuImageUpload=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.GpuImageUpload.ifc"
            "/reference;Image.ImageF16x4=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF16x4.ifc"
            "/reference;Image.ImageF32x4RGBAWithCache=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4RGBAWithCache.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Image.ImageYUV420=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageYUV420.ifc"
            "/reference;Image.MultiChannelImage=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.MultiChannelImage.ifc"
            "/reference;Image.PSDDocument=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.PSDDocument.ifc"
            "/reference;Image.Raw=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Raw.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/IO/Image/ImageExporter.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;IO.ImageExporter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/IO.ImageExporter.ifc"
            "/reference;Image.ExportOptions=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ExportOptions.ifc"
            "/reference;Image.Utils=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Utils.ifc"
            "/reference;Image.MultiChannelImage=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.MultiChannelImage.ifc"
            "/reference;Channel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Channel.ifc"
            "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Mesh/Mesh.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
             "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
             "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
             "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc"
             "/reference;Mesh=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Mesh.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Image/OpenCV/ImageTransformCV.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Image=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ifc"
            "/reference;Image:ImageTransform=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image-ImageTransform.ifc"
            "/reference;Image.GpuImageUpload=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.GpuImageUpload.ifc"
            "/reference;Image.ImageF16x4=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF16x4.ifc"
            "/reference;Image.ImageF32x4RGBAWithCache=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4RGBAWithCache.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Image.ImageYUV420=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageYUV420.ifc"
            "/reference;Image.MultiChannelImage=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.MultiChannelImage.ifc"
            "/reference;Image.PSDDocument=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.PSDDocument.ifc"
            "/reference;Image.Raw=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Raw.ifc"
            "/reference;Channel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Channel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Utils/ExplorerUtils.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Utils.ExplorerUtils=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.ExplorerUtils.ifc"
            "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Utils/UniString.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc"
            "/reference;Utils.Convertor.String=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Convertor.String.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Utils/NameGenerator.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Utils.NameGenerator=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.NameGenerator.ifc"
            "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
            "/reference;Container.NameMap=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NameMap.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NameMap.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Image/GpuImageUpload.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Image.GpuImageUpload=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.GpuImageUpload.ifc"
            "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/Distortion.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.Distortion=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.Distortion.ifc"
            "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc"
            "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ProceduralTexture.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.ProceduralTexture=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ProceduralTexture.ifc"
            "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
            "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc"
            "/reference;Graphics.Shader.Compute.HLSL.ProceduralTexture=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.Compute.HLSL.ProceduralTexture.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;Memory.TrackedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.TrackedPtr.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
            "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
            "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc"
            "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Image/ImageF16x4.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Image.ImageF16x4=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF16x4.ifc"
            "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc"
            "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
            "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Image/ImageF32x4_RGBA.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
            "/reference;CvUtils=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/CvUtils.ifc"
            "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
            "/reference;Size=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Size.ifc"
            "/reference;Image.SurfacePixelConversion=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.SurfacePixelConversion.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc"
            "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Image/SurfacePixelConversion.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Mesh/Mesh.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebug.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/NamedVector.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Physics/MpmSolver2D.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Common/Parallel.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Graphics/Compute.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Graphics/GPUInfo.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Graphics/GPUComputeContext.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Physics/Physics2D.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Utils/Id.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/NamedVector.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebug.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Property/AbstractProperty.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEnvironment.dir/include/EnvironmentVariable/TokenExpansion.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/include/Audio/Modulation/Router.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Animation/Transform3D.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Color/Float.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Frame/FramePosition.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Property/PropertyTypes.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Time/RationalTime.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Utils/UniString.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/include/Audio/Modulation/Modulator.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Layer/LayerMatte.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Layer.Matte=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Layer.Matte.ifc"
            [=[/reference;std=CMakeFiles\\__cmake_cxx23.dir\\artifact_std.ifc]=]
             "/reference;Color.Luminance=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Luminance.ifc"
             "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
             "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
             "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
             "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc"
             "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;Utils.Id=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Id.ifc"
            "/reference;Serialization.JsonAdapter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.JsonAdapter.ifc"
            "/reference;Serialization.SchemaMigration=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.SchemaMigration.ifc"
            "/reference;Serialization.ISerializable=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.ISerializable.ifc"
            "/reference;Serialization.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.Registry.ifc"
            "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc"
            "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/Shader/Compute/MaskCutoutPipeline.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Graphics.Shader.Compute.MaskCutout=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.Compute.MaskCutout.ifc"
            "/reference;Color.Luminance=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Luminance.ifc"
            "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc"
            "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
            "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc"
            "/reference;Graphics.Shader.Compute.HLSL.MaskCutout=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.Compute.HLSL.MaskCutout.ifc"
            "/reference;Layer.Matte=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Layer.Matte.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/Shader/Compute/MaskPathRasterizerPipeline.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Graphics.Shader.Compute.MaskPathRasterizer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.Compute.MaskPathRasterizer.ifc"
            "/reference;Graphics.Compute=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Compute.ifc"
            "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
            "/reference;Graphics.Shader.Compute.HLSL.MaskPathRasterizer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Shader.Compute.HLSL.MaskPathRasterizer.ifc"
            "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ColorTransform/ChannelMixer.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.ColorTransform.ChannelMixer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.ChannelMixer.ifc"
            "/reference;Color.GamutConversion=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.GamutConversion.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ColorTransform/ColorBalance.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.ColorTransform.ColorBalance=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.ColorBalance.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ColorTransform/Fill.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.ColorTransform.Fill=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Fill.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ColorTransform/FilmCurve.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.ColorTransform.FilmCurve=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.FilmCurve.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ColorTransform/GradientRamp.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.ColorTransform.GradientRamp=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.GradientRamp.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ColorTransform/HueSaturation.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.ColorTransform.HueSaturation=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.HueSaturation.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ColorTransform/HueVsCurves.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.ColorTransform.HueVsCurves=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.HueVsCurves.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ColorTransform/LevelsCurves.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.ColorTransform.LevelsCurves=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.LevelsCurves.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ColorTransform/PhotoFilter.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.ColorTransform.PhotoFilter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.PhotoFilter.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ColorTransform/SelectiveColor.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.ColorTransform.SelectiveColor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.SelectiveColor.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ColorTransform/TonalSplit.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.ColorTransform.TonalSplit=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.TonalSplit.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ColorTransform/TriChromaticShift.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.ColorTransform.TriChromaticShift=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.TriChromaticShift.ifc"
            "/reference;Particle=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Particle.ifc"
            "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ColorTransform/Tritone.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.ColorTransform.Tritone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Tritone.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ColorTransform/Colorama.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.ColorTransform.Colorama=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Colorama.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ColorTransform/TriChromaticShift.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.ColorTransform.TriChromaticShift=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.TriChromaticShift.ifc"
            "/reference;Particle=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Particle.ifc"
            "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/FluidVisualizer.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing.FluidVisualizer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.FluidVisualizer.ifc"
            "/reference;Particle=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Particle.ifc"
            "/reference;Physics.Fluid=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Physics.Fluid.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Render/VolumePostProcess.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Render.VolumePostProcess=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.VolumePostProcess.ifc"
            "/reference;Render.ImageBuffer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.ImageBuffer.ifc"
            "/reference;Render.Vector3D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Vector3D.ifc"
            "/reference;Render.Ray=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Ray.ifc"
            "/reference;Render.Material=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Material.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Render/ProgressAggregator.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Render.Farm.Progress=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Farm.Progress.ifc"
            "/reference;Render.Farm.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Farm.Types.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Render/LogCollector.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Render.Farm.Log=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Farm.Log.ifc"
             "/reference;Render.Farm.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Farm.Types.ifc"
             "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
             "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
             "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Render/AtmosphereFog.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Render.AtmosphereFog=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.AtmosphereFog.ifc"
            "/reference;Render.Camera=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Camera.ifc"
            "/reference;Render.ImageBuffer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.ImageBuffer.ifc"
            "/reference;Render.Ray=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Ray.ifc"
            "/reference;Render.Vector3D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Vector3D.ifc"
            "/reference;Render.VolumeRenderer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.VolumeRenderer.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Render/MeshToVolume.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Render.MeshToVolume=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.MeshToVolume.ifc"
            "/reference;Render.Triangle=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Triangle.ifc"
            "/reference;Render.Vector3D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Vector3D.ifc"
            "/reference;Render.Ray=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Ray.ifc"
            "/reference;Render.VolumeRenderer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.VolumeRenderer.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Render/NoiseField.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Render.NoiseField=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.NoiseField.ifc"
            "/reference;Render.Vector3D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Vector3D.ifc"
            "/reference;Render.VolumeRenderer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.VolumeRenderer.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Render/VolumeRenderer.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Render.VolumeRenderer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.VolumeRenderer.ifc"
            "/reference;Render.Camera=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Camera.ifc"
            "/reference;Render.ImageBuffer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.ImageBuffer.ifc"
            "/reference;Render.Ray=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Ray.ifc"
            "/reference;Render.Vector3D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Vector3D.ifc"
            "/reference;Render.Ray=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Ray.ifc"
            "/reference;Render.Material=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Material.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Render/VolumeModifier.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Render.VolumeModifier=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.VolumeModifier.ifc"
            "/reference;Render.NoiseField=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.NoiseField.ifc"
            "/reference;Render.Vector3D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Vector3D.ifc"
            "/reference;Render.VolumeRenderer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.VolumeRenderer.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Render/CheckpointStore.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Render.Farm.Checkpoint=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Farm.Checkpoint.ifc"
            "/reference;Render.Farm.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Farm.Types.ifc"
            "/reference;Utils.Optional=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Optional.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Image/FFmpegEncoder.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Encoder.FFmpegEncoder=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Encoder.FFmpegEncoder.ifc"
            "/reference;Image=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;Image.GpuImageUpload=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.GpuImageUpload.ifc"
            "/reference;Image.ImageF16x4=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF16x4.ifc"
            "/reference;Image.ImageF32x4RGBAWithCache=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4RGBAWithCache.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Image.ImageYUV420=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageYUV420.ifc"
            "/reference;Image.MultiChannelImage=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.MultiChannelImage.ifc"
            "/reference;Image.PSDDocument=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.PSDDocument.ifc"
            "/reference;Image.Raw=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Raw.ifc"
            "/reference;Channel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Channel.ifc"
            "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
            "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
            "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc"
            "/reference;Time.Code=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Code.ifc"
            "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Render/RenderFarmMaster.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Render.Farm.Master=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Farm.Master.ifc"
            "/reference;Render.Farm.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Farm.Types.ifc"
            "/reference;Render.Farm.Checkpoint=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Farm.Checkpoint.ifc"
            "/reference;Core.ThreadPool=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ThreadPool.ifc"
             "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
             "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
             "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc"
             "/reference;NetworkRPCServer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreNetwork.dir/NetworkRPCServer.ifc"
            "/reference;Utils.Id=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Id.ifc"
            "/reference;Utils.Optional=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Optional.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Utils/Tag.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Utils.Tag=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Tag.ifc"
            "/reference;Utils.String.Like=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.Like.ifc"
            "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Utils/Path.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Utils.Path=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Path.ifc"
            "/reference;Utils.String.Like=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.Like.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Render/RendererQueueSetting.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;RendererQueueSetting=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/RendererQueueSetting.ifc"
            "/reference;Utils=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.ifc"
            "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc"
            "/reference;Utils.String.Like=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.Like.ifc"
            "/reference;Utils.Buffer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Buffer.ifc"
            "/reference;Utils.Handle=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Handle.ifc"
            "/reference;Utils.Id=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Id.ifc"
            "/reference;Utils.Numeric=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Numeric.ifc"
            "/reference;Utils.Optional=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Optional.ifc"
            "/reference;Utils.Path=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Path.ifc"
            "/reference;Utils.Result=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Result.ifc"
            "/reference;Utils.Size.Like=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Size.Like.ifc"
            "/reference;Utils.String.View=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.View.ifc"
            "/reference;Utils.Timer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Timer.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Render/RendererQueueManager.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Render.Queue.Manager=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Queue.Manager.ifc"
            "/reference;Render.Settings=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Settings.ifc"
            "/reference;Render.JobModel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.JobModel.ifc"
            "/reference;RendererQueueSetting=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/RendererQueueSetting.ifc"
             "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
             "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
             "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc"
             "/reference;Utils.Id=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Id.ifc"
            "/reference;Memory.TrackedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.TrackedPtr.ifc"
            "/reference;Core.ThreadPool=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ThreadPool.ifc"
            "/reference;Core.Render.MFR.Dispatcher=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Render.MFR.Dispatcher.ifc"
            "/reference;Core.Render.MFR.Job=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Render.MFR.Job.ifc"
            "/reference;Utils.Path=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Path.ifc"
            "/reference;Utils.Timer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Timer.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Script/ArtifactScript/ArtifactScript.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Script.ArtifactScript=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.ArtifactScript.ifc"
            "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Utils.Optional=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Optional.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
            "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Script/Engine/BultinScriptVM/BuiltinScriptVM.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Script.Engine.BuiltinVM=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Engine.BuiltinVM.ifc"
            "/reference;Script.Expression.Value=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Value.ifc"
            "/reference;Script.Expression.Evaluator=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Evaluator.ifc"
            "/reference;Script.Engine.Context=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Engine.Context.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Script.Expression.Parser=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Parser.ifc"
            "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Script/Engine/Context/ScriptContext.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Script.Engine.Context=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Engine.Context.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Script.Expression.Parser=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Parser.ifc"
            "/reference;Script.Expression.Value=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Value.ifc"
            "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Script/Engine/Syntax/ASTNode.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ASTNode=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ASTNode.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Script/Expression/ExpressionEvaluator.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Script.Expression.Parser=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Parser.ifc"
            "/reference;Script.Expression.Value=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Value.ifc"
            "/reference;Script.Expression.Evaluator=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Evaluator.ifc"
            "/reference;Math.Noise=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Noise.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/AbstractImageEffect.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;ImageF32x4=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageF32x4.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;ImageProcessing:AffineTransform=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AffineTransform.ifc"
            "/reference;ImageProcessing:AnamorphicFlare=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AnamorphicFlare.ifc"
            "/reference;ImageProcessing:AnisotropicFlowBlur=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AnisotropicFlowBlur.ifc"
            "/reference;ImageProcessing:AntiAliasing=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AntiAliasing.ifc"
            "/reference;ImageProcessing:BroadcastColors=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-BroadcastColors.ifc"
            "/reference;ImageProcessing:ChromaKey=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-ChromaKey.ifc"
            "/reference;ImageProcessing:ChromaSpread=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-ChromaSpread.ifc"
            "/reference;ImageProcessing:ChromaSpreadGlow=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-ChromaSpreadGlow.ifc"
            "/reference;ImageProcessing:Duotone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Duotone.ifc"
            "/reference;ImageProcessing:Echo=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Echo.ifc"
            "/reference;ImageProcessing:EdgeEcho=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-EdgeEcho.ifc"
            "/reference;ImageProcessing:Emboss=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Emboss.ifc"
            "/reference;ImageProcessing:Halftone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Halftone.ifc"
            "/reference;ImageProcessing:IBKKeyer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-IBKKeyer.ifc"
            "/reference;ImageProcessing:LeaveColor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-LeaveColor.ifc"
            "/reference;ImageProcessing:LumaKey=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-LumaKey.ifc"
            "/reference;ImageProcessing:Median=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Median.ifc"
            "/reference;ImageProcessing:Monochrome=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Monochrome.ifc"
            "/reference;ImageProcessing:MotionTrail=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-MotionTrail.ifc"
            "/reference;ImageProcessing:NegativeCV=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-NegativeCV.ifc"
            "/reference;ImageProcessing:NeuromorphicDisplace=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-NeuromorphicDisplace.ifc"
            "/reference;ImageProcessing:Posterize=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Posterize.ifc"
            "/reference;ImageProcessing:PosterizeTime=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-PosterizeTime.ifc"
            "/reference;ImageProcessing:Scatter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Scatter.ifc"
            "/reference;ImageProcessing:SimpleChoker=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-SimpleChoker.ifc"
            "/reference;ImageProcessing:StrobeLight=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-StrobeLight.ifc"
            "/reference;ImageProcessing:StructureTensor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-StructureTensor.ifc"
            "/reference;ImageProcessing:Threshold=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Threshold.ifc"
            "/reference;ImageProcessing:TiltShift=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-TiltShift.ifc"
            "/reference;ImageProcessing:VectorFlowGlitch=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-VectorFlowGlitch.ifc"
            "/reference;ImageProcessing.ColorTransform.SelectiveColor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.SelectiveColor.ifc"
            "/reference;ImageProcessing.ColorTransform.Tritone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Tritone.ifc"
            "/reference;ImageProcessing.EmbossCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.EmbossCS.ifc"
            "/reference;ImageProcessing.LeaveColorCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.LeaveColorCS.ifc"
            "/reference;ImageProcessing.NegateCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.NegateCS.ifc"
            "/reference;ImageProcessing.ProceduralTexture=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ProceduralTexture.ifc"
            "/reference;ImageProcessing.ScatterCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ScatterCS.ifc"
            "/reference;ImageProcessing.SimpleChokerCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.SimpleChokerCS.ifc"
            "/reference;ImageProcessing.BroadcastColorsCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.BroadcastColorsCS.ifc"
            "/reference;ImageProcessing.ChromaKeyCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ChromaKeyCS.ifc"
            "/reference;ImageProcessing.ColorTransform.ChannelMixer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.ChannelMixer.ifc"
            "/reference;ImageProcessing.ColorTransform.ColorBalance=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.ColorBalance.ifc"
            "/reference;ImageProcessing.ColorTransform.Colorama=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Colorama.ifc"
            "/reference;ImageProcessing.ColorTransform.Fill=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Fill.ifc"
            "/reference;ImageProcessing.ColorTransform.GradientRamp=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.GradientRamp.ifc"
            "/reference;ImageProcessing.ColorTransform.LevelsCurves=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.LevelsCurves.ifc"
            "/reference;ImageProcessing.ColorTransform.PhotoFilter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.PhotoFilter.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
            "/reference;Color.GamutConversion=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.GamutConversion.ifc"
            "/reference;Particle=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Particle.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ChromaKey.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ifc"
            "/reference;ImageProcessing:ChromaKey=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-ChromaKey.ifc"
            "/reference;ImageF32x4=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageF32x4.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;ImageProcessing:AffineTransform=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AffineTransform.ifc"
            "/reference;ImageProcessing:AnamorphicFlare=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AnamorphicFlare.ifc"
            "/reference;ImageProcessing:AntiAliasing=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AntiAliasing.ifc"
            "/reference;ImageProcessing:Halftone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Halftone.ifc"
            "/reference;ImageProcessing:IBKKeyer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-IBKKeyer.ifc"
            "/reference;ImageProcessing:LeaveColor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-LeaveColor.ifc"
            "/reference;ImageProcessing:LumaKey=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-LumaKey.ifc"
            "/reference;ImageProcessing:Median=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Median.ifc"
            "/reference;ImageProcessing:Monochrome=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Monochrome.ifc"
            "/reference;ImageProcessing:MotionTrail=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-MotionTrail.ifc"
            "/reference;ImageProcessing:NegativeCV=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-NegativeCV.ifc"
            "/reference;ImageProcessing:Posterize=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Posterize.ifc"
            "/reference;ImageProcessing:PosterizeTime=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-PosterizeTime.ifc"
            "/reference;ImageProcessing:Scatter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Scatter.ifc"
            "/reference;ImageProcessing:SimpleChoker=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-SimpleChoker.ifc"
            "/reference;ImageProcessing:StrobeLight=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-StrobeLight.ifc"
            "/reference;ImageProcessing:StructureTensor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-StructureTensor.ifc"
            "/reference;ImageProcessing:Threshold=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Threshold.ifc"
            "/reference;ImageProcessing:TiltShift=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-TiltShift.ifc"
            "/reference;ImageProcessing:VectorFlowGlitch=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-VectorFlowGlitch.ifc"
            "/reference;ImageProcessing:AffineTransform=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AffineTransform.ifc"
            "/reference;ImageProcessing:AnamorphicFlare=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AnamorphicFlare.ifc"
            "/reference;ImageProcessing:AnisotropicFlowBlur=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AnisotropicFlowBlur.ifc"
            "/reference;ImageProcessing:AntiAliasing=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AntiAliasing.ifc"
            "/reference;ImageProcessing.BroadcastColorsCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.BroadcastColorsCS.ifc"
            "/reference;ImageProcessing.ChromaKeyCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ChromaKeyCS.ifc"
            "/reference;ImageProcessing.EmbossCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.EmbossCS.ifc"
            "/reference;ImageProcessing.LeaveColorCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.LeaveColorCS.ifc"
            "/reference;ImageProcessing.NegateCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.NegateCS.ifc"
            "/reference;ImageProcessing.ProceduralTexture=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ProceduralTexture.ifc"
            "/reference;ImageProcessing.ScatterCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ScatterCS.ifc"
            "/reference;ImageProcessing.SimpleChokerCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.SimpleChokerCS.ifc"
            "/reference;Particle=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Particle.ifc"
            "/reference;Color.GamutConversion=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.GamutConversion.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;ImageProcessing.BroadcastColorsCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.BroadcastColorsCS.ifc"
            "/reference;ImageProcessing.ChromaKeyCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ChromaKeyCS.ifc"
            "/reference;ImageProcessing.EmbossCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.EmbossCS.ifc"
            "/reference;ImageProcessing.LeaveColorCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.LeaveColorCS.ifc"
            "/reference;ImageProcessing.NegateCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.NegateCS.ifc"
            "/reference;ImageProcessing.ScatterCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ScatterCS.ifc"
            "/reference;ImageProcessing.SimpleChokerCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.SimpleChokerCS.ifc"
            "/reference;ImageProcessing.ProceduralTexture=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ProceduralTexture.ifc"
            "/reference;ImageProcessing.ColorTransform.ChannelMixer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.ChannelMixer.ifc"
            "/reference;ImageProcessing.ColorTransform.ColorBalance=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.ColorBalance.ifc"
            "/reference;ImageProcessing.ColorTransform.Colorama=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Colorama.ifc"
            "/reference;ImageProcessing.ColorTransform.Fill=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Fill.ifc"
            "/reference;ImageProcessing.ColorTransform.GradientRamp=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.GradientRamp.ifc"
            "/reference;ImageProcessing.ColorTransform.LevelsCurves=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.LevelsCurves.ifc"
            "/reference;ImageProcessing.ColorTransform.PhotoFilter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.PhotoFilter.ifc"
            "/reference;ImageProcessing.ColorTransform.SelectiveColor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.SelectiveColor.ifc"
            "/reference;ImageProcessing.ColorTransform.Tritone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Tritone.ifc"
            "/reference;Particle=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Particle.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/BroadcastColors.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ifc"
            "/reference;ImageProcessing:BroadcastColors=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-BroadcastColors.ifc"
            "/reference;ImageF32x4=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageF32x4.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;ImageProcessing:EdgeEcho=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-EdgeEcho.ifc"
            "/reference;ImageProcessing:Emboss=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Emboss.ifc"
            "/reference;ImageProcessing:Halftone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Halftone.ifc"
            "/reference;ImageProcessing:IBKKeyer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-IBKKeyer.ifc"
            "/reference;ImageProcessing:LeaveColor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-LeaveColor.ifc"
            "/reference;ImageProcessing:LumaKey=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-LumaKey.ifc"
            "/reference;ImageProcessing:Median=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Median.ifc"
            "/reference;ImageProcessing:Monochrome=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Monochrome.ifc"
            "/reference;ImageProcessing:MotionTrail=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-MotionTrail.ifc"
            "/reference;ImageProcessing:NegativeCV=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-NegativeCV.ifc"
            "/reference;ImageProcessing:Posterize=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Posterize.ifc"
            "/reference;ImageProcessing:PosterizeTime=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-PosterizeTime.ifc"
            "/reference;ImageProcessing:Scatter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Scatter.ifc"
            "/reference;ImageProcessing:SimpleChoker=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-SimpleChoker.ifc"
            "/reference;ImageProcessing:StrobeLight=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-StrobeLight.ifc"
            "/reference;ImageProcessing:StructureTensor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-StructureTensor.ifc"
            "/reference;ImageProcessing:Threshold=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Threshold.ifc"
            "/reference;ImageProcessing:TiltShift=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-TiltShift.ifc"
            "/reference;ImageProcessing:VectorFlowGlitch=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-VectorFlowGlitch.ifc"
            "/reference;ImageProcessing.BroadcastColorsCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.BroadcastColorsCS.ifc"
            "/reference;ImageProcessing.ChromaKeyCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ChromaKeyCS.ifc"
            "/reference;ImageProcessing.ColorTransform.ChannelMixer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.ChannelMixer.ifc"
            "/reference;ImageProcessing.ColorTransform.ColorBalance=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.ColorBalance.ifc"
            "/reference;ImageProcessing.ColorTransform.Colorama=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Colorama.ifc"
            "/reference;ImageProcessing.ColorTransform.Fill=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Fill.ifc"
            "/reference;ImageProcessing.ColorTransform.GradientRamp=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.GradientRamp.ifc"
            "/reference;ImageProcessing.ColorTransform.LevelsCurves=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.LevelsCurves.ifc"
            "/reference;ImageProcessing.ColorTransform.PhotoFilter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.PhotoFilter.ifc"
            "/reference;ImageProcessing.ColorTransform.SelectiveColor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.SelectiveColor.ifc"
            "/reference;ImageProcessing.ColorTransform.Tritone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Tritone.ifc"
            "/reference;ImageProcessing.EmbossCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.EmbossCS.ifc"
            "/reference;ImageProcessing.LeaveColorCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.LeaveColorCS.ifc"
            "/reference;ImageProcessing.NegateCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.NegateCS.ifc"
            "/reference;ImageProcessing.ProceduralTexture=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ProceduralTexture.ifc"
            "/reference;ImageProcessing.ScatterCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ScatterCS.ifc"
            "/reference;ImageProcessing.SimpleChokerCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.SimpleChokerCS.ifc"
            "/reference;ImageProcessing:AffineTransform=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AffineTransform.ifc"
            "/reference;ImageProcessing:AnamorphicFlare=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AnamorphicFlare.ifc"
            "/reference;ImageProcessing:AnisotropicFlowBlur=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AnisotropicFlowBlur.ifc"
            "/reference;ImageProcessing:AntiAliasing=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AntiAliasing.ifc"
            "/reference;Particle=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Particle.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ChromaSpread.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ifc"
            "/reference;ImageProcessing:ChromaSpread=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-ChromaSpread.ifc"
            "/reference;ImageF32x4=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageF32x4.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;ImageProcessing:Duotone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Duotone.ifc"
            "/reference;ImageProcessing:Echo=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Echo.ifc"
            "/reference;ImageProcessing:EdgeEcho=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-EdgeEcho.ifc"
            "/reference;ImageProcessing:Emboss=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Emboss.ifc"
            "/reference;ImageProcessing:Halftone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Halftone.ifc"
            "/reference;ImageProcessing:IBKKeyer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-IBKKeyer.ifc"
            "/reference;ImageProcessing:LeaveColor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-LeaveColor.ifc"
            "/reference;ImageProcessing:LumaKey=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-LumaKey.ifc"
            "/reference;ImageProcessing:Median=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Median.ifc"
            "/reference;ImageProcessing:Monochrome=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Monochrome.ifc"
            "/reference;ImageProcessing:MotionTrail=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-MotionTrail.ifc"
            "/reference;ImageProcessing:NegativeCV=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-NegativeCV.ifc"
            "/reference;ImageProcessing:Posterize=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Posterize.ifc"
            "/reference;ImageProcessing:PosterizeTime=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-PosterizeTime.ifc"
            "/reference;ImageProcessing:Scatter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Scatter.ifc"
            "/reference;ImageProcessing:SimpleChoker=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-SimpleChoker.ifc"
            "/reference;ImageProcessing:StrobeLight=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-StrobeLight.ifc"
            "/reference;ImageProcessing:StructureTensor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-StructureTensor.ifc"
            "/reference;ImageProcessing:Threshold=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Threshold.ifc"
            "/reference;ImageProcessing:TiltShift=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-TiltShift.ifc"
            "/reference;ImageProcessing:VectorFlowGlitch=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-VectorFlowGlitch.ifc"
            "/reference;ImageProcessing.BroadcastColorsCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.BroadcastColorsCS.ifc"
            "/reference;ImageProcessing.ChromaKeyCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ChromaKeyCS.ifc"
            "/reference;ImageProcessing.EmbossCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.EmbossCS.ifc"
            "/reference;ImageProcessing.LeaveColorCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.LeaveColorCS.ifc"
            "/reference;ImageProcessing.NegateCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.NegateCS.ifc"
            "/reference;ImageProcessing.ProceduralTexture=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ProceduralTexture.ifc"
            "/reference;ImageProcessing.ScatterCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ScatterCS.ifc"
            "/reference;ImageProcessing.SimpleChokerCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.SimpleChokerCS.ifc"
            "/reference;ImageProcessing.ColorTransform.ChannelMixer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.ChannelMixer.ifc"
            "/reference;ImageProcessing.ColorTransform.ColorBalance=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.ColorBalance.ifc"
            "/reference;ImageProcessing.ColorTransform.Colorama=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Colorama.ifc"
            "/reference;ImageProcessing.ColorTransform.Fill=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Fill.ifc"
            "/reference;ImageProcessing.ColorTransform.GradientRamp=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.GradientRamp.ifc"
            "/reference;ImageProcessing.ColorTransform.LevelsCurves=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.LevelsCurves.ifc"
            "/reference;ImageProcessing.ColorTransform.PhotoFilter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.PhotoFilter.ifc"
            "/reference;ImageProcessing.ColorTransform.SelectiveColor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.SelectiveColor.ifc"
            "/reference;ImageProcessing.ColorTransform.Tritone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Tritone.ifc"
            "/reference;ImageProcessing:AffineTransform=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AffineTransform.ifc"
            "/reference;ImageProcessing:AnamorphicFlare=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AnamorphicFlare.ifc"
            "/reference;ImageProcessing:AnisotropicFlowBlur=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AnisotropicFlowBlur.ifc"
            "/reference;ImageProcessing:AntiAliasing=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AntiAliasing.ifc"
            "/reference;ImageProcessing:BroadcastColors=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-BroadcastColors.ifc"
            "/reference;ImageProcessing:ChromaKey=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-ChromaKey.ifc"
            "/reference;ImageProcessing:ChromaSpreadGlow=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-ChromaSpreadGlow.ifc"
            "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
            "/reference;Particle=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Particle.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/OpenCVRotoBrushEngine.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ArtifactCore.ImageProcessing.OpenCV.RotoBrushEngine=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ArtifactCore.ImageProcessing.OpenCV.RotoBrushEngine.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ChromaSpreadGlow.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/AffineCV.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/Duotone.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/Echo.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/EdgeEcho.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/Emboss.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/Keying/IBKKeyer.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/Halftone.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/LumaKey.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/LeaveColor.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/MotionTrail.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/Median.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/NegativeCV.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/NeuromorphicDisplace.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/Monochrome.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/Posterize.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/PosterizeTime.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/Scatter.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/SimpleChoker.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/StructureTensor.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/StrobeLight.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/Threshold.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/TiltShift.cppm" OR
           _artifact_impl_relative STREQUAL "src/ImageProcessing/VectorFlowGlitch.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing:NeuromorphicDisplace=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-NeuromorphicDisplace.ifc")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Transform._2D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Transform._2D.ifc")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;ImageProcessing=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ifc"
            "/reference;ImageProcessing:ChromaSpreadGlow=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-ChromaSpreadGlow.ifc"
            "/reference;ImageProcessing.BroadcastColorsCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.BroadcastColorsCS.ifc"
            "/reference;ImageProcessing.ChromaKeyCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ChromaKeyCS.ifc"
            "/reference;ImageProcessing.ColorTransform.ChannelMixer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.ChannelMixer.ifc"
            "/reference;ImageProcessing.ColorTransform.ColorBalance=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.ColorBalance.ifc"
            "/reference;ImageProcessing.ColorTransform.Colorama=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Colorama.ifc"
            "/reference;ImageProcessing.ColorTransform.Fill=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Fill.ifc"
            "/reference;ImageProcessing.ColorTransform.GradientRamp=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.GradientRamp.ifc"
            "/reference;ImageProcessing.ColorTransform.LevelsCurves=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.LevelsCurves.ifc"
            "/reference;ImageProcessing.ColorTransform.PhotoFilter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.PhotoFilter.ifc"
            "/reference;ImageProcessing.ColorTransform.SelectiveColor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.SelectiveColor.ifc"
            "/reference;ImageProcessing.ColorTransform.Tritone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ColorTransform.Tritone.ifc"
            "/reference;ImageProcessing.EmbossCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.EmbossCS.ifc"
            "/reference;ImageProcessing.LeaveColorCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.LeaveColorCS.ifc"
            "/reference;ImageProcessing.NegateCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.NegateCS.ifc"
            "/reference;ImageProcessing.ProceduralTexture=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ProceduralTexture.ifc"
            "/reference;ImageProcessing.ScatterCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.ScatterCS.ifc"
            "/reference;ImageProcessing.SimpleChokerCS=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing.SimpleChokerCS.ifc"
            "/reference;ImageProcessing:AffineTransform=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AffineTransform.ifc"
            "/reference;ImageProcessing:AnamorphicFlare=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AnamorphicFlare.ifc"
            "/reference;ImageProcessing:AnisotropicFlowBlur=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AnisotropicFlowBlur.ifc"
            "/reference;ImageProcessing:AntiAliasing=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-AntiAliasing.ifc"
            "/reference;ImageF32x4=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageF32x4.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
            "/reference;ImageProcessing:Duotone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Duotone.ifc"
            "/reference;ImageProcessing:Echo=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Echo.ifc"
            "/reference;ImageProcessing:EdgeEcho=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-EdgeEcho.ifc"
            "/reference;ImageProcessing:Emboss=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Emboss.ifc"
            "/reference;ImageProcessing:Halftone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Halftone.ifc"
            "/reference;ImageProcessing:IBKKeyer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-IBKKeyer.ifc"
            "/reference;ImageProcessing:LeaveColor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-LeaveColor.ifc"
            "/reference;ImageProcessing:LumaKey=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-LumaKey.ifc"
            "/reference;ImageProcessing:Median=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Median.ifc"
            "/reference;ImageProcessing:Monochrome=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Monochrome.ifc"
            "/reference;ImageProcessing:MotionTrail=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-MotionTrail.ifc"
            "/reference;ImageProcessing:NegativeCV=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-NegativeCV.ifc"
            "/reference;ImageProcessing:Posterize=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Posterize.ifc"
            "/reference;ImageProcessing:PosterizeTime=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-PosterizeTime.ifc"
            "/reference;ImageProcessing:Scatter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Scatter.ifc"
            "/reference;ImageProcessing:SimpleChoker=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-SimpleChoker.ifc"
            "/reference;ImageProcessing:StrobeLight=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-StrobeLight.ifc"
            "/reference;ImageProcessing:StructureTensor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-StructureTensor.ifc"
            "/reference;ImageProcessing:Threshold=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-Threshold.ifc"
            "/reference;ImageProcessing:TiltShift=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-TiltShift.ifc"
            "/reference;ImageProcessing:VectorFlowGlitch=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-VectorFlowGlitch.ifc"
            "/reference;ImageProcessing:BroadcastColors=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-BroadcastColors.ifc"
            "/reference;ImageProcessing:ChromaKey=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-ChromaKey.ifc"
            "/reference;ImageProcessing:ChromaSpread=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageProcessing-ChromaSpread.ifc"
            "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
            "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
            "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
            "/reference;Memory.TrackedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.TrackedPtr.ifc"
            "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc"
            "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;Color.GamutConversion=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.GamutConversion.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
            "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc"
            "/reference;Math.Random=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Random.ifc"
            "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc"
            "/reference;Particle=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Particle.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Script/Expression/ExpressionParser.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Script.Expression.Parser=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Parser.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
             "/reference;Script.Expression.Value=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Value.ifc"
             "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
             "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
             "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Property/PropertyGroup.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Property.Group=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Property.Group.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Frame.Rate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc"
            "/reference;Property.Abstract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Property.Abstract.ifc"
            "/reference;Property.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Property.Types.ifc"
            "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc"
            "/reference;Math.Interpolate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Interpolate.ifc"
            "/reference;Script.Expression.Evaluator=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Evaluator.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Script/ScriptRuntime.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Script.Runtime=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Runtime.ifc"
            "/reference;Script.Expression.Evaluator=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Evaluator.ifc"
            "/reference;Script.Expression.Parser=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Parser.ifc"
            "/reference;Script.Expression.Value=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Value.ifc"
            "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
             "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
             "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
             "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
             "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebug.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Transform/TransformHelper.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Transform.Hlper=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Transform.Hlper.ifc"
            "/reference;Transform=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Transform.ifc"
            "/reference;Transform._2D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Transform._2D.ifc"
            "/reference;Transform.Scale2D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Transform.Scale2D.ifc"
            "/reference;Math.Matrix4x4=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Matrix4x4.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Particle/ParticleSystem.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Particle=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Particle.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Mesh=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Mesh.ifc"
            "/reference;Physics.Fluid=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Physics.Fluid.ifc"
            "/reference;Physics2D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Physics2D.ifc"
            "/reference;Math.Noise=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Noise.ifc"
            "/reference;Shape=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Shape.ifc"
            "/reference;Particle.System=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Particle.System.ifc"
            "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
            "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
            "/reference;Utils.Id=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Id.ifc")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Property/PropertyLinkManager.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Property.LinkManager=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Property.LinkManager.ifc"
            "/reference;Property.Abstract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Property.Abstract.ifc"
            "/reference;Frame.Rate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc"
            "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc"
            "/reference;Math.Interpolate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Interpolate.ifc"
            "/reference;Script.Expression.Evaluator=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Evaluator.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Render/GPURayTracer.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Render.GPURayTracer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.GPURayTracer.ifc"
            "/reference;Render.Camera=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Camera.ifc"
            "/reference;Render.Hittable=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Hittable.ifc"
            "/reference;Render.IRayTracer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.IRayTracer.ifc"
            "/reference;Render.SoftwareRayTracer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.SoftwareRayTracer.ifc"
            "/reference;Render.ImageBuffer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.ImageBuffer.ifc"
            "/reference;Render.Vector3D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Vector3D.ifc"
            "/reference;Render.Ray=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Ray.ifc"
            "/reference;Render.Material=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Material.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Graphics.GPUcomputeContext=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPUcomputeContext.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            "/reference;Graphics.GPU.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.GPU.Info.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Render/RenderFarmWorker.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Render.Farm.Worker=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Farm.Worker.ifc"
            "/reference;Render.Farm.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Render.Farm.Types.ifc"
            "/reference;Core.ThreadPool=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ThreadPool.ifc"
            "/reference;Utils.Id=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Id.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Property/AbstractProperty.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Property.Abstract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Property.Abstract.ifc"
            "/reference;Animation.Transform3D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Animation.Transform3D.ifc"
            "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc"
            "/reference;Frame.Position=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Position.ifc"
            "/reference;Math.Interpolate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Interpolate.ifc"
            "/reference;Property.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Property.Types.ifc"
            "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
            "/reference;Script.Engine.Context=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Engine.Context.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Script.Expression.Evaluator=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Evaluator.ifc"
            "/reference;Script.Expression.Value=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Value.ifc"
            "/reference;Script.Expression.Parser=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Parser.ifc"
            "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc"
            "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc"
            "/reference;Audio.Modulation.Router=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.Modulation.Router.ifc"
            "/reference;EnvironmentVariable.Expansion=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEnvironment.dir/EnvironmentVariable.Expansion.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Time/TimeCode.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Time.Code=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Code.ifc"
            "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc"
            "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Time/TimeCodeRange.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;TimeCodeRange=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/TimeCodeRange.ifc"
            "/reference;Time.Code=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Code.ifc"
            "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc")
    elseif(_artifact_impl_relative MATCHES "^src/Graphics/Effect/" )
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY COMPILE_OPTIONS
            "/reference;${_artifact_module_name}=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/${_artifact_module_name}.ifc"
            "/reference;Channel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Channel.ifc"
            "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
            "/reference;Core.Diagnostics.FallbackPolicy=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Diagnostics.FallbackPolicy.ifc"
            "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
            [=[/reference;std=CMakeFiles\\__cmake_cxx23.dir\\artifact_std.ifc]=]
            "/reference;Graphics.Effect.Creative=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.ifc"
            "/reference;Math.Noise=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Noise.ifc"
            "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
            "/reference;Graphics.Effect.Creative.ChromaticAberration=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.ChromaticAberration.ifc"
            "/reference;Graphics.Effect.Creative.ColorVibrance=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.ColorVibrance.ifc"
            "/reference;Graphics.Effect.Creative.DepthMelt=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.DepthMelt.ifc"
            "/reference;Graphics.Effect.Creative.EdgeEcho=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.EdgeEcho.ifc"
            "/reference;Graphics.Effect.Creative.Emboss=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Emboss.ifc"
            "/reference;Graphics.Effect.Creative.Fisheye=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Fisheye.ifc"
            "/reference;Graphics.Effect.Creative.Glitch=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Glitch.ifc"
            "/reference;Graphics.Effect.Creative.Halftone=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Halftone.ifc"
            "/reference;Graphics.Effect.Creative.Kaleidoscope=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Kaleidoscope.ifc"
            "/reference;Graphics.Effect.Creative.LightPressure=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.LightPressure.ifc"
            "/reference;Graphics.Effect.Creative.Mirror=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Mirror.ifc"
            "/reference;Graphics.Effect.Creative.OldTV=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.OldTV.ifc"
            "/reference;Graphics.Effect.Creative.PigmentSeparation=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.PigmentSeparation.ifc"
            "/reference;Graphics.Effect.Creative.Pixelate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Pixelate.ifc"
            "/reference;Graphics.Effect.Creative.Posterize=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Posterize.ifc"
            "/reference;Graphics.Effect.Creative.Solarize=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.Solarize.ifc"
            "/reference;Graphics.Effect.Creative.SurfaceMemory=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.SurfaceMemory.ifc"
            "/reference;Graphics.Effect.Creative.TemporalFossil=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.Effect.Creative.TemporalFossil.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Scene/SceneNode.cppm")
        # SceneNode is emitted by ArtifactCoreScene, not the aggregate
        # ArtifactCore target.  Do not attach the implementation to the
        # aggregate IFC produced by the generic fallback below.
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Scene.SceneNode=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreScene.dir/Scene.SceneNode.ifc")
    elseif(_artifact_impl_relative MATCHES "^src/Audio/" OR
           _artifact_impl_relative STREQUAL "src/Codec/FFMpegAudioDecoder.cppm" OR
           _artifact_impl_relative STREQUAL "src/CLAP/CLAPHost.cppm" OR
           _artifact_impl_relative STREQUAL "src/Animation/KeyframeEditingTools.cppm")
        # Audio is a split target.  Its scanner-disabled implementation units
        # must use the Audio target's primary IFC and imported Audio-domain
        # interfaces, not the aggregate Core output directory.
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;${_artifact_module_name}=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/${_artifact_module_name}.ifc"
                "/reference;Audio.Segment=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.Segment.ifc"
                "/reference;Audio.Effect=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.Effect.ifc"
                "/reference;Audio.Backend=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.Backend.ifc"
                "/reference;Audio.Backend.WASAPI=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.Backend.WASAPI.ifc"
                "/reference;Audio.Backend.ASIOStub=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.Backend.ASIOStub.ifc"
                "/reference;Audio.Backend.Qt=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.Backend.Qt.ifc"
                "/reference;Audio.Analyze=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.Analyze.ifc"
                "/reference;Audio.Panner=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.Panner.ifc"
                "/reference;Audio.Effect.Compressor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.Effect.Compressor.ifc"
                "/reference;Audio.Decibels=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.Decibels.ifc"
                "/reference;Audio.FormantExtractor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.FormantExtractor.ifc"
                "/reference;Audio.SimpleWav=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.SimpleWav.ifc"
                "/reference;Audio.RingBuffer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.RingBuffer.ifc"
                "/reference;Audio.DownMixer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.DownMixer.ifc"
                "/reference;Audio.Bus=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.Bus.ifc"
                "/reference;Audio.BufferQueue=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.BufferQueue.ifc"
                "/reference;Memory.TrackedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.TrackedPtr.ifc"
                "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
                "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
                "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
                "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
                "/reference;Utils.Id=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Id.ifc"
                "/reference;Utils.Size.Like=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Size.Like.ifc"
                "/reference;Math.Interpolate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Interpolate.ifc"
                "/reference;Utils.String.Like=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.Like.ifc"
                "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc"
                "/reference;ArtifactCore.Utils.PerformanceProfiler=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ArtifactCore.Utils.PerformanceProfiler.ifc"
                "/reference;Math.Random=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Random.ifc")
    elseif(_artifact_impl_relative MATCHES "^src/Composition/")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;${_artifact_module_name}=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreComposition.dir/${_artifact_module_name}.ifc"
                "/reference;Composition.TemplateLock=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreComposition.dir/Composition.TemplateLock.ifc"
                "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc"
                "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc"
                "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
                "/reference;Serialization.JsonAdapter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.JsonAdapter.ifc"
                "/reference;Serialization.ISerializable=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.ISerializable.ifc"
                "/reference;Serialization.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.Registry.ifc"
                "/reference;Serialization.SchemaMigration=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.SchemaMigration.ifc"
                "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
                "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
                "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
                "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
                "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
                "/reference;Utils.Optional=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Optional.ifc"
                "/reference;Frame.Position=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Position.ifc"
                "/reference;Utils.Id=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Id.ifc")
    elseif(_artifact_impl_relative MATCHES "^src/Text/")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;${_artifact_module_name}=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreText.dir/${_artifact_module_name}.ifc"
                "/reference;Text.Style=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreText.dir/Text.Style.ifc"
                "/reference;Text.LayoutContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreText.dir/Text.LayoutContract.ifc"
                "/reference;Text.ShapingBackend=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreText.dir/Text.ShapingBackend.ifc"
                "/reference;Text.GlyphLayout=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreText.dir/Text.GlyphLayout.ifc"
                "/reference;Font.FreeFont=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreText.dir/Font.FreeFont.ifc"
                "/reference;Font.Descriptor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreText.dir/Font.Descriptor.ifc"
                "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc"
                "/reference;Utils.Optional=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Optional.ifc"
                "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc"
                "/reference;Shape.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreShape.dir/Shape.Types.ifc"
                "/reference;Property.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Property.Types.ifc"
                "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/File/FileInfo.cppm")
        # FileInfo is compiled by the split File target.  Its partition IFC
        # must not resolve to the aggregate Core output directory.
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;File:Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreFile.dir/File-Info.ifc")
    elseif(_artifact_impl_relative MATCHES "^src/Configuration/" OR
           _artifact_impl_relative STREQUAL "src/Application/ArtifactAppSettings.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;${_artifact_module_name}=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreConfiguration.dir/${_artifact_module_name}.ifc"
                "/reference;Configuration.ConfigLayer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreConfiguration.dir/Configuration.ConfigLayer.ifc"
                "/reference;Configuration.ConfigSchema=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreConfiguration.dir/Configuration.ConfigSchema.ifc"
                "/reference;Configuration.LayeredConfigStore=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreConfiguration.dir/Configuration.LayeredConfigStore.ifc"
                "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc"
                "/reference;Core.FastSettingsStore=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.FastSettingsStore.ifc"
                "/reference;Artifact.Grid.System=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreGrid.dir/Artifact.Grid.System.ifc")
    elseif(_artifact_impl_relative MATCHES "^src/Asset/")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;${_artifact_module_name}=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAsset.dir/${_artifact_module_name}.ifc"
                "/reference;AssetType=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAsset.dir/AssetType.ifc"
                "/reference;Asset.File=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAsset.dir/Asset.File.ifc"
                "/reference;Asset.Database=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAsset.dir/Asset.Database.ifc"
                "/reference;Asset.VectorImport=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAsset.dir/Asset.VectorImport.ifc"
                "/reference;Asset.Import.Setting=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAsset.dir/Asset.Import.Setting.ifc"
                "/reference;Asset=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAsset.dir/Asset.ifc"
                "/reference;ImageAsset=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAsset.dir/ImageAsset.ifc"
                "/reference;AssetConverter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAsset.dir/AssetConverter.ifc"
                "/reference;File.TypeDetector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreFile.dir/File.TypeDetector.ifc"
                "/reference;Configuration.ConfigLayer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreConfiguration.dir/Configuration.ConfigLayer.ifc"
                "/reference;Configuration.LayeredConfigStore=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreConfiguration.dir/Configuration.LayeredConfigStore.ifc"
                "/reference;IO.ImageImporter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/IO.ImageImporter.ifc"
                "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
                "/reference;Utils.Id=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Id.ifc"
                "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc"
                "/reference;Container.MultiIndex=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.MultiIndex.ifc"
                "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
                "/reference;Utils.Optional=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Optional.ifc"
                "/reference;Data.CsvParser=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreData.dir/Data.CsvParser.ifc"
                "/reference;Data.DataTable=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreData.dir/Data.DataTable.ifc"
                "/reference;Data.ColumnType=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreData.dir/Data.ColumnType.ifc"
                "/reference;Data.TypeInference=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreData.dir/Data.TypeInference.ifc"
                "/reference;Image.Raw=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Raw.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Codec/FFMpegVideoDecoder.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;${_artifact_module_name}=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/${_artifact_module_name}.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Video/MediaTimeStamp.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;${_artifact_module_name}=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMedia.dir/${_artifact_module_name}.ifc")
    elseif(_artifact_impl_relative MATCHES "^src/Video/")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;${_artifact_module_name}=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/${_artifact_module_name}.ifc")
    elseif(_artifact_impl_relative MATCHES "^src/Media/")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;${_artifact_module_name}=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMedia.dir/${_artifact_module_name}.ifc")
    elseif(_artifact_impl_relative MATCHES "^src/Network/")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;${_artifact_module_name}=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreNetwork.dir/${_artifact_module_name}.ifc")
    elseif(_artifact_impl_relative MATCHES "^src/Command/")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;${_artifact_module_name}=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreCommand.dir/${_artifact_module_name}.ifc")
    elseif(_artifact_impl_relative MATCHES "^src/Configuration/")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;${_artifact_module_name}=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreConfiguration.dir/${_artifact_module_name}.ifc")
    elseif(_artifact_impl_relative MATCHES "^src/Mask/")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;${_artifact_module_name}=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMask.dir/${_artifact_module_name}.ifc"
                "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
                "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
                "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(_artifact_impl_relative STREQUAL "src/Mask/RotoMask.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Core.Mask.RotoMask=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMask.dir/Core.Mask.RotoMask.ifc"
                "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
                "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
                "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
    elseif(NOT _artifact_impl_relative STREQUAL "src/AI/APIKeyManager.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;${_artifact_module_name}=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/${_artifact_module_name}.ifc")
    else()
        # Scanner-disabled primary units still need explicit references for
        # every imported BMI.  These modules are produced by their owning
        # targets before the AI implementation is compiled.
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY
            COMPILE_OPTIONS
                "/reference;Core.AI.APIKeyManager=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAI.dir/Core.AI.APIKeyManager.ifc"
                "/reference;Core.AI.Context=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAI.dir/Core.AI.Context.ifc"
                "/reference;Core.AI.CloudAgent=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAI.dir/Core.AI.CloudAgent.ifc"
                "/reference;Configuration.ConfigLayer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Configuration.ConfigLayer.ifc"
                "/reference;Configuration.LayeredConfigStore=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Configuration.LayeredConfigStore.ifc")
    endif()
    set(_artifact_dependency_target_dir "ArtifactCore.dir")
    file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${_artifact_interface_relative}" _artifact_expected_interface)
    set(_artifact_interface_owner_found OFF)
    get_cmake_property(_artifact_core_variables VARIABLES)
    foreach(_artifact_domain_var IN LISTS _artifact_core_variables)
        if(_artifact_domain_var MATCHES "^ARTIFACTCORE_(.+)_MODULES$")
            foreach(_artifact_domain_interface IN LISTS ${_artifact_domain_var})
                file(REAL_PATH "${_artifact_domain_interface}" _artifact_actual_interface)
                if(_artifact_actual_interface STREQUAL _artifact_expected_interface)
                    set(_artifact_interface_owner_found ON)
                    string(REGEX REPLACE "^ARTIFACTCORE_(.+)_MODULES$" "\\1" _artifact_domain "${_artifact_domain_var}")
                    set(_artifact_dependency_target_dir "ArtifactCore${_artifact_domain}.dir")
                    break()
                endif()
            endforeach()
            if(NOT _artifact_dependency_target_dir STREQUAL "ArtifactCore.dir")
                break()
            endif()
        endif()
    endforeach()
    string(REGEX REPLACE "^src/([^/]+)/.*$" "\\1" _artifact_source_domain "${_artifact_impl_relative}")
    string(TOUPPER "${_artifact_source_domain}" _artifact_source_domain_upper)
    if(DEFINED ARTIFACTCORE_${_artifact_source_domain_upper}_IMPL)
        set(_artifact_dependency_target_dir "ArtifactCore${_artifact_source_domain}.dir")
    endif()
    get_cmake_property(_artifact_core_variables VARIABLES)
    foreach(_artifact_domain_var IN LISTS _artifact_core_variables)
        if(_artifact_domain_var MATCHES "^ARTIFACTCORE_(.+)_IMPL$")
            string(REGEX REPLACE "^ARTIFACTCORE_(.+)_IMPL$" "\\1" _artifact_domain "${_artifact_domain_var}")
            string(TOUPPER "${_artifact_domain}" _artifact_domain_upper)
            if(_artifact_domain_upper STREQUAL _artifact_source_domain_upper)
                set(_artifact_dependency_target_dir "ArtifactCore${_artifact_domain}.dir")
                break()
            endif()
        endif()
    endforeach()
    if(_artifact_impl_relative MATCHES "^src/AI/")
        set(_artifact_dependency_target_dir "ArtifactCoreAI.dir")
    elseif(_artifact_impl_relative MATCHES "^src/Analyze/")
        set(_artifact_dependency_target_dir "ArtifactCoreAnalyze.dir")
    elseif(_artifact_impl_relative MATCHES "^src/Configuration/" OR
           _artifact_impl_relative MATCHES "^src/Application/")
        set(_artifact_dependency_target_dir "ArtifactCoreConfiguration.dir")
    elseif(_artifact_impl_relative MATCHES "^src/Asset/")
        set(_artifact_dependency_target_dir "ArtifactCoreAsset.dir")
    elseif(_artifact_impl_relative MATCHES "^src/Audio/")
        set(_artifact_dependency_target_dir "ArtifactCoreAudio.dir")
    elseif(_artifact_impl_relative STREQUAL "src/Codec/FFMpegAudioDecoder.cppm")
        set(_artifact_dependency_target_dir "ArtifactCoreAudio.dir")
    elseif(_artifact_impl_relative MATCHES "^src/Particle/")
        set(_artifact_dependency_target_dir "ArtifactCoreAudio.dir")
    elseif(_artifact_interface_relative MATCHES "^include/Composition/")
        set(_artifact_dependency_target_dir "ArtifactCoreComposition.dir")
    elseif(_artifact_interface_relative MATCHES "^include/Event/")
        set(_artifact_dependency_target_dir "ArtifactCoreEvent.dir")
    elseif(_artifact_interface_relative MATCHES "^include/Media/")
        set(_artifact_dependency_target_dir "ArtifactCoreMedia.dir")
    elseif(_artifact_interface_relative MATCHES "^include/NLE/")
        set(_artifact_dependency_target_dir "ArtifactCoreNLE.dir")
    elseif(_artifact_interface_relative MATCHES "^include/Playback/")
        set(_artifact_dependency_target_dir "ArtifactCorePlayback.dir")
    elseif(_artifact_interface_relative MATCHES "^include/Video/")
        set(_artifact_dependency_target_dir "ArtifactCoreVideo.dir")
    elseif(_artifact_interface_relative MATCHES "^include/Scene/")
        set(_artifact_dependency_target_dir "ArtifactCoreScene.dir")
    elseif(_artifact_interface_relative MATCHES "^include/Control/")
        set(_artifact_dependency_target_dir "ArtifactCoreControl.dir")
    elseif(_artifact_interface_relative MATCHES "^include/Database/")
        set(_artifact_dependency_target_dir "ArtifactCoreDatabase.dir")
    elseif(_artifact_interface_relative MATCHES "^include/Export/")
        set(_artifact_dependency_target_dir "ArtifactCoreExport.dir")
    elseif(_artifact_interface_relative MATCHES "^include/Rig/")
        set(_artifact_dependency_target_dir "ArtifactCoreRig.dir")
    elseif(_artifact_interface_relative MATCHES "^include/IPC/")
        set(_artifact_dependency_target_dir "ArtifactCoreIPC.dir")
    elseif(_artifact_interface_relative MATCHES "^include/Light/")
        set(_artifact_dependency_target_dir "ArtifactCoreLight.dir")
    elseif(_artifact_interface_relative MATCHES "^include/Plugin/")
        set(_artifact_dependency_target_dir "ArtifactCorePlugin.dir")
    elseif(_artifact_interface_relative MATCHES "^include/IO/")
        set(_artifact_dependency_target_dir "ArtifactCoreShape.dir")
    elseif(_artifact_interface_relative MATCHES "^include/Text/")
        set(_artifact_dependency_target_dir "ArtifactCoreText.dir")
    elseif(_artifact_interface_relative MATCHES "^include/Simulation/")
        set(_artifact_dependency_target_dir "ArtifactCoreSimulation.dir")
    elseif(_artifact_interface_relative MATCHES "^include/Source/")
        set(_artifact_dependency_target_dir "ArtifactCoreSource.dir")
    elseif(_artifact_interface_relative MATCHES "^include/Track/")
        set(_artifact_dependency_target_dir "ArtifactCoreTrack.dir")
    elseif(_artifact_interface_relative MATCHES "^include/UI/")
        set(_artifact_dependency_target_dir "ArtifactCoreUI.dir")
    endif()
    # Resolve the owning split target from its explicit implementation list.
    # This keeps the BMI/object dependency aligned when a domain is moved out
    # of the aggregate target, without maintaining a second path-based map.
    get_cmake_property(_artifact_core_variables VARIABLES)
    foreach(_artifact_domain_var IN LISTS _artifact_core_variables)
        if(_artifact_domain_var MATCHES "^ARTIFACTCORE_(.+)_IMPL$")
            foreach(_artifact_domain_source IN LISTS ${_artifact_domain_var})
                file(RELATIVE_PATH _artifact_domain_relative
                    "${CMAKE_CURRENT_SOURCE_DIR}" "${_artifact_domain_source}")
                if(_artifact_domain_relative STREQUAL _artifact_impl_relative)
                    string(REGEX REPLACE "^ARTIFACTCORE_(.+)_IMPL$" "\\1" _artifact_domain "${_artifact_domain_var}")
                    set(_artifact_dependency_target_dir "ArtifactCore${_artifact_domain}.dir")
                    break()
                endif()
            endforeach()
            if(NOT _artifact_dependency_target_dir STREQUAL "ArtifactCore.dir")
                break()
            endif()
        endif()
    endforeach()
    # The aggregate target still needs its interface object as an explicit
    # order-only dependency.  Split targets receive ordering from their own
    # CXX_MODULES file set and must not reference ArtifactCore.dir objects.
    if(_artifact_dependency_target_dir STREQUAL "ArtifactCore.dir")
        set_property(SOURCE "${_artifact_impl_file}" PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/${_artifact_interface_relative}.obj")
    else()
        set_property(SOURCE "${_artifact_impl_file}" PROPERTY OBJECT_DEPENDS "")
    endif()
endforeach()

# Split-domain targets emit their primary-interface BMIs under their own
# target directory.  Override the aggregate-target fallback for these
# scanner-disabled implementation units and retain imported BMI references.
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/NLE/Core.cppm" PROPERTY COMPILE_OPTIONS
    "/reference;NLE.Core=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreNLE.dir/NLE.Core.ifc"
    "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc"
    "/reference;Time.Code=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Code.ifc"
    "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc"
    "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
    "/reference;Core.Defaults=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Defaults.ifc"
    "/reference;Frame.Position=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Position.ifc"
    "/reference;Frame.Range=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Range.ifc"
    "/reference;Frame.Rate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc"
    "/reference;Frame.Offset=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Offset.ifc"
    "/reference;Serialization.ISerializable=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.ISerializable.ifc"
    "/reference;Serialization.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.Registry.ifc"
    "/reference;Serialization.JsonAdapter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.JsonAdapter.ifc"
    "/reference;Serialization.SchemaMigration=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.SchemaMigration.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Event/EventBus.cppm" PROPERTY COMPILE_OPTIONS
    "/reference;Event.Bus=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEvent.dir/Event.Bus.ifc"
    "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
    "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
    "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
    "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Event/EventBusDebugger.cppm" PROPERTY COMPILE_OPTIONS
    "/reference;ArtifactCore.Event.EventBusDebugger=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEvent.dir/ArtifactCore.Event.EventBusDebugger.ifc"
    "/reference;Event.Bus=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEvent.dir/Event.Bus.ifc"
    "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
    "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
    "/reference;Utils.Text.String=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Text.String.ifc"
    "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
    "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
    "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/AbstractEncoder.cppm" PROPERTY COMPILE_OPTIONS
    "/reference;Codec.AbstractEncoder=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Codec.AbstractEncoder.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/PlaybackManager.cppm" PROPERTY COMPILE_OPTIONS
    "/reference;Video.PlaybackManager=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Video.PlaybackManager.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/TransitionFactory.cppm" PROPERTY COMPILE_OPTIONS
    "/reference;Video.TransitionFactory=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Video.TransitionFactory.ifc"
    "/reference;Video.AbstractTransition=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Video.AbstractTransition.ifc"
    "/reference;Video.VideoFrame=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Video.VideoFrame.ifc"
    "/reference;NLE.Core=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreNLE.dir/NLE.Core.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Transitions/CubeTransition.cppm" PROPERTY COMPILE_OPTIONS
    "/reference;Video.Transitions.CubeTransition=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Video.Transitions.CubeTransition.ifc"
    "/reference;NLE.Core=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreNLE.dir/NLE.Core.ifc"
    "/reference;Video.TransitionFactory=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Video.TransitionFactory.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/NLE/OTIO.cppm" PROPERTY COMPILE_OPTIONS
    "/reference;NLE.OTIO=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreNLE.dir/NLE.OTIO.ifc"
    "/reference;NLE.Core=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreNLE.dir/NLE.Core.ifc"
    "/reference;Frame.Position=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Position.ifc"
    "/reference;Frame.Rate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc"
    "/reference;Frame.Offset=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Offset.ifc"
    "/reference;Frame.Range=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Range.ifc"
    "/reference;Time.Code=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Code.ifc"
    "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc"
    "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Thread/PreciseTicker.cppm" PROPERTY COMPILE_OPTIONS
    "/reference;Thread.PreciseTicker=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreThread.dir/Thread.PreciseTicker.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Thread/ThreadHelper.cppm" PROPERTY COMPILE_OPTIONS
    "/reference;Thread.Helper=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreThread.dir/Thread.Helper.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Thread/ThreadHelper.cppm"
    APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreThread.dir/include/Thread/ThreadHelper.ixx.obj")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Stabilizer.cppm" PROPERTY COMPILE_OPTIONS
    "/reference;Video.Stabilizer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Video.Stabilizer.ifc"
    "/reference;Frame.Position=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Position.ifc"
    "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
    "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc"
    "/reference;Frame.Rate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc"
    "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")

# Procedural3DGenerators.cppm is the implementation unit for the explicitly
# registered public generator module above.
list(FIND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Geometry/Procedural3DGenerators.cppm" _procedural_3d_impl_idx)
if(_procedural_3d_impl_idx EQUAL -1)
    list(APPEND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Geometry/Procedural3DGenerators.cppm")
endif()

# LUT3DComputer.cppm is the implementation unit for the interface declared in
# include/Graphics/Compute/LUT3DComputer.ixx. Keep it as a private source so it
# does not generate a second module interface unit.
list(FIND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Compute/LUT3DComputer.cppm" _lut3d_computer_impl_idx)
if(_lut3d_computer_impl_idx EQUAL -1)
    list(APPEND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Compute/LUT3DComputer.cppm")
endif()

list(FIND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Simulation/PyroSimulation.cppm" _pyro_simulation_impl_idx)
if(_pyro_simulation_impl_idx EQUAL -1)
    list(APPEND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Simulation/PyroSimulation.cppm")
endif()

list(FIND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Physics/SandSim2D.cppm" _sand_sim_impl_idx)
if(_sand_sim_impl_idx EQUAL -1)
    list(APPEND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Physics/SandSim2D.cppm")
endif()

list(FIND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/SandGPUCompute.cppm" _sand_gpu_impl_idx)
if(_sand_gpu_impl_idx EQUAL -1)
    list(APPEND CORE_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/SandGPUCompute.cppm")
endif()

# Audio is a large, mostly self-contained domain.  Keep the two non-Audio
# modules that directly import Audio in the same pack so the base Core target
# does not retain an edge back into the extracted module set.
set(ARTIFACTCORE_AUDIO_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/ASIOBackendStub.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioAnalyzer.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioBackend.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioBassTreble.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioBufferQueue.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioBus.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioCache.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioChorus.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioCompressor.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioDecibels.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioDelay.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioDownMixer.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioFormat.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioFrame.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioFrameToQAudioFormatConverter.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioHighLowPass.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioMixer.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioModule.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioPanner.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioParametricEQ.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioProvider.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioRasterizer.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioRenderer.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioReverb.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioRingBuffer.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioSegment.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioSpectrum.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioStereoMixer.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioTone.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioVolume.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioWaveform.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/AudioWriter.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/DSP/AllPassFilter.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/DSP/DelayLine.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/DSP/LFO.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/Modulation/Modulator.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/Modulation/Router.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/Spatial/SpatialParams.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/Spatial/SpatialMath.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/Spatial/SpatialRenderer.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/Spatial/HrtfData.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/FormantExtractor.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/LipSyncTrack.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/SimpleWav.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/WASAPIBackend.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/QtAudioBackend.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Codec/FFMpegAudioDecoder.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Particle/ParticleSystem.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/CLAP/CLAPHost.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Animation/KeyframeEditingTools.ixx"
)
set(ARTIFACTCORE_AUDIO_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/ASIOBackendStub.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioAnalyzer.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioBassTreble.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioBus.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioCache.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioChorus.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioCompressor.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioDecibels.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioDelay.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioDownMixer.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioHighLowPass.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioMixer.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioPanner.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioParametricEQ.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioRasterizer.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioRenderer.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioReverb.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioRingBuffer.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioSpectrum.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioStereoMixer.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioTone.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioVolume.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioWaveform.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioWriter.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/Spatial/SpatialRenderer.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/FormantExtractor.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/LipSyncTrack.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/SimpleWav.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/WASAPIBackend.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Codec/FFMpegAudioDecoder.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Particle/ParticleSystem.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CLAP/CLAPHost.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Animation/KeyframeEditingTools.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_AUDIO_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_AUDIO_IMPL})
list(REMOVE_ITEM ARTIFACTCORE_AUDIO_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Particle/ParticleSystem.ixx")
list(REMOVE_ITEM ARTIFACTCORE_AUDIO_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Particle/ParticleSystem.cppm")

# AI modules form a leaf domain in Core: the static import audit found no
# non-AI Core module importing an AI module.  Keep their public descriptions
# and optional local/cloud agent implementations in a separate target.
set(ARTIFACTCORE_AI_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/AIActionContext.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/AIContext.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/AIPromptGenerator.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/AIToolExecutor.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/APIKeyManager.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/CommandIR.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/CommandSandbox.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/ICloudAIAgent.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/IDescribable.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/ImageSegmenter.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/LlamaLocalAgent.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/LocalAIAgent.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/McpBridge.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/McpTransport.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/ObjectDetector.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/OnnxDmlLocalAgent.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/TieredAIManager.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/AI/ToolBridge.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/3DLightingDescriptions.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/AIAnalysisDescriptions.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/AnimationUIDescriptions.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/CoreDescriptions.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/DataAssetDescriptions.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/DescriptionExamples.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/EffectsUtilityDescriptions.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/ExportImportDescriptions.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/FinalUtilitiesDescriptions.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/FluidMaskingDescriptions.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/ImageMathDescriptions.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/LayerDescriptions.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/MoreDescriptions.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/RenderAudioColorDescriptions.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/TransitionsGeneratorDescriptions.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/UIWidgetsDescriptions.cppm"
)
set(ARTIFACTCORE_AI_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/APIKeyManager.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/CloudAgent.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/LlamaLocalAgent.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/ObjectDetector.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/OnnxDmlLocalAgent.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/TieredAIManager.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_AI_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_AI_IMPL})

# Video is another mostly leaf domain.  Media playback/editor modules and the
# FFmpeg video decoder are included here because they directly import
# Video.VideoFrame; this keeps the base Core target free of reverse Video
# module edges.
set(ARTIFACTCORE_VIDEO_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/AbstractDecoder.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/AbstractEncoder.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/AbstractTransition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/CpuFrameView.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/EncoderSetting.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/FFMpegEncoder.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/FFMpegVideoDecoder.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/FFmpegAudioEncoder.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/GStreamerDecoder.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/GStreamerEncoder.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/PlaybackManager.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Stabilizer.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/TransitionFactory.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Transitions/BlockDissolveTransition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Transitions/CrossfadeTransition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Transitions/CubeTransition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Transitions/DissolveTransition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Transitions/DoorsTransition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Transitions/FlipTransition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Transitions/GlitchDisplaceTransition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Transitions/GradientWipeTransition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Transitions/IrisWipeTransition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Transitions/LightLeakTransition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Transitions/LinearWipeTransition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Transitions/RadialWipeTransition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Transitions/SlideTransition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Transitions/SpinTransition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Transitions/WipeTransition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Transitions/ZoomTransition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/Video.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Video/VideoFrame.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Image/FFmpegEncoder.Helpers.cppm"
)
set(ARTIFACTCORE_VIDEO_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Image/FFmpegEncoder.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/AbstractEncoder.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/PlaybackManager.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Stabilizer.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/TransitionFactory.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Transitions/BlockDissolveTransition.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Transitions/CrossfadeTransition.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Transitions/CubeTransition.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Transitions/DissolveTransition.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Transitions/DoorsTransition.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Transitions/FlipTransition.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Transitions/GlitchDisplaceTransition.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Transitions/GradientWipeTransition.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Transitions/IrisWipeTransition.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Transitions/LightLeakTransition.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Transitions/LinearWipeTransition.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Transitions/RadialWipeTransition.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Transitions/SlideTransition.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Transitions/WipeTransition.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/Transitions/ZoomTransition.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Codec/FFMpegVideoDecoder.cppm"
)
# Split Video transition implementations need explicit references to the
# shared transition contract and frame view under MSVC.
set_property(SOURCE ${ARTIFACTCORE_VIDEO_IMPL} APPEND PROPERTY COMPILE_OPTIONS
    "/reference;NLE.Core=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreNLE.dir/NLE.Core.ifc"
    "/reference;Core.Diagnostics.Recorder=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Diagnostics.Recorder.ifc"
    "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
    "/reference;Utils.Result=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Result.ifc"
    "/reference;Video.AbstractTransition=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Video.AbstractTransition.ifc"
    "/reference;Video.CpuFrameView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Video.CpuFrameView.ifc"
    "/reference;Video.TransitionFactory=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Video.TransitionFactory.ifc"
    "/reference;Video.VideoFrame=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Video.VideoFrame.ifc")
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_VIDEO_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_VIDEO_IMPL})

set(ARTIFACTCORE_MEDIA_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Media/ISource.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Media/ImageSequenceSource.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Media/Media.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Media/MediaAudioDecoder.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Media/MediaEditorController.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Media/MediaFrame.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Media/MediaImageFrameDecoder.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Media/MediaInfo.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Media/MediaMetaData.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Media/MediaPlaybackController.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Media/MediaProbe.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Media/MediaReader.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Media/MediaSource.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Media/MediaTimeStamp.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Media/SourceInterpret.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Codec/FFmpegThumbnailExtractor.ixx"
)
set(ARTIFACTCORE_MEDIA_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/ImageSequenceSource.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/MediaAudioDecoder.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/MediaEditorController.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/MediaFrame.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/MediaImageFrameDecoder.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/MediaInfo.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/MediaPlaybackController.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/MediaProbe.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/MediaReader.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/MediaSource.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Video/MediaTimeStamp.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Codec/FFmpegThumbnailExtractor.cppm"
)
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/ImageSequenceSource.cppm"
    PROPERTY COMPILE_OPTIONS
        "/reference;Media.ImageSequenceSource=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMedia.dir/Media.ImageSequenceSource.ifc"
        "/reference;Media.ISource=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMedia.dir/Media.ISource.ifc"
        "/reference;Thread.Helper=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreThread.dir/Thread.Helper.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/ImageSequenceSource.cppm"
    APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMedia.dir/include/Media/ISource.ixx.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreThread.dir/include/Thread/ThreadHelper.ixx.obj")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/MediaAudioDecoder.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
    "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
    "/reference;Core.Diagnostics.Recorder=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Diagnostics.Recorder.ifc"
    "/reference;Utils.Result=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Result.ifc"
    "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/MediaEditorController.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;MediaPlaybackController=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMedia.dir/MediaPlaybackController.ifc"
    "/reference;Video.VideoFrame=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Video.VideoFrame.ifc"
    "/reference;Playback.State=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCorePlayback.dir/Playback.State.ifc"
    "/reference;Media.MetaData=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMedia.dir/Media.MetaData.ifc"
    "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/MediaImageFrameDecoder.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Video.VideoFrame=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Video.VideoFrame.ifc"
    "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/MediaPlaybackController.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Codec.MFFrameExtractor=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Codec.MFFrameExtractor.ifc"
    "/reference;Media.MetaData=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMedia.dir/Media.MetaData.ifc"
    "/reference;MediaAudioDecoder=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMedia.dir/MediaAudioDecoder.ifc"
    "/reference;MediaImageFrameDecoder=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMedia.dir/MediaImageFrameDecoder.ifc"
    "/reference;MediaReader=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMedia.dir/MediaReader.ifc"
    "/reference;MediaSource=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMedia.dir/MediaSource.ifc"
    "/reference;Playback.State=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCorePlayback.dir/Playback.State.ifc"
    "/reference;Video.VideoFrame=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/Video.VideoFrame.ifc"
    "/reference;ArtifactCore.Utils.PerformanceProfiler=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ArtifactCore.Utils.PerformanceProfiler.ifc"
    "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
    "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc"
    "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/MediaProbe.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Media.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMedia.dir/Media.Info.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/MediaSource.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Core.Diagnostics.Recorder=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Diagnostics.Recorder.ifc"
    "/reference;Core.Diagnostics.Snapshot=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Diagnostics.Snapshot.ifc"
    "/reference;Utils.Result=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Result.ifc"
    "/reference;Utils.Optional=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Optional.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Media/MediaReader.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;MediaSource=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMedia.dir/MediaSource.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Network/NetworkRPCServer.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
    "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Command/LayerLockManager.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Event.Bus=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEvent.dir/Event.Bus.ifc"
    "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Mask/VolumeMask.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Mask/PathMorph.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
    "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Codec/FFmpegThumbnailExtractor.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Codec.Thumbnail.FFmpeg=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMedia.dir/Codec.Thumbnail.FFmpeg.ifc"
    "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc"
    "/reference;Media.Info=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMedia.dir/Media.Info.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Codec/FFmpegThumbnailExtractor.cppm"
    APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMedia.dir/include/Codec/FFmpegThumbnailExtractor.ixx.obj")
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_MEDIA_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_MEDIA_IMPL})

# Composition is a leaf Core domain: no remaining Core interface imports a
# Composition module.  Keep the buffer, pre-compose, template, and final
# effect contracts together so project/composition changes do not rebuild the
# entire base target.
set(ARTIFACTCORE_COMPOSITION_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Composition/AbstractCompositionBuffer2D.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Composition/CompositionBuffer2D.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Composition/CompositionContext.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Composition/CompositionFinalEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Composition/ExportMatrix.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Composition/ParametricComposition.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Composition/PreCompose.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Composition/TemplateLock.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Composition/TemplateSlot.ixx"
)
set(ARTIFACTCORE_COMPOSITION_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Composition/CompositionBuffer2D.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Composition/CompositionFinalEffect.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Composition/ExportMatrix.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Composition/OpenCVCompositionBuffer2D.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Composition/ParametricComposition.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Composition/PreCompose.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Composition/TemplateLock.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Composition/TemplateSlot.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_COMPOSITION_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_COMPOSITION_IMPL})
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Composition/PreCompose.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Composition.PreCompose=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreComposition.dir/Composition.PreCompose.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Composition/PreCompose.cppm" APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreComposition.dir/include/Composition/PreCompose.ixx.obj")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Composition/CompositionFinalEffect.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
    "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Configuration/ConfigSchema.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
    "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/InputOperator.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
    "/reference;Event.Bus=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEvent.dir/Event.Bus.ifc"
    "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Asset/AbstractAssetFile.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
    "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Rig/Rig2D.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
    "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Text/TextAnimator.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Script.Expression.Evaluator=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Evaluator.ifc"
    "/reference;Script.Expression.Parser=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Parser.ifc"
    "/reference;Script.Expression.Value=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Value.ifc"
    "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
    "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Text/TextAnimator.cppm" APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Script/Expression/ExpressionParser.ixx.obj")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Text/TextShapingBackend.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
    "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Application/ArtifactAppSettings.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Event.Bus=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEvent.dir/Event.Bus.ifc"
    "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")

set(ARTIFACTCORE_UI_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/Context.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/DragOperator.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/GizmoMode.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/InputEvent.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/InteractiveActions.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/InputOperator.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/InputOperatorManager.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/KeyMap.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/LayoutState.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/RotateOperator.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/RotoMaskEditor.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/ScaleOperator.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/SelectOperator.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/SelectionManager.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/ShortcutBindings.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/StandardActions.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/TransformControls.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/ViewOrientationNavigator.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/UI/ViewportOperator.ixx"
)
set(ARTIFACTCORE_UI_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/InputOperator.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/InputOperatorManager.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/LayoutState.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/RotoMaskEditor.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/SelectionManager.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/ShortcutBindings.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/ViewOrientationNavigator.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/ViewportOperator.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_UI_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_UI_IMPL})

set(ARTIFACTCORE_DATA_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Data/ColumnType.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Data/CsvDataSource.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Data/CsvParser.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Data/Data.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Data/DataCache.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Data/DataPropertyBridge.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Data/DataSource.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Data/DataTable.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Data/ExpressionDataFunctions.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Data/FileWatcher.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Data/TypedColumn.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Data/TypeInference.ixx"
)
set(ARTIFACTCORE_DATA_IMPL)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_DATA_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_DATA_IMPL})

set(ARTIFACTCORE_ASSET_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Asset/AbstractAssetFile.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Asset/AssetConverter.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Asset/AssetDatabase.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Asset/AssetImportSetting.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Asset/AssetImporter.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Asset/AssetMetaFile.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Asset/AssetSequence.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Asset/AssetType.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Asset/DataAssetFile.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Asset/ImageAsset.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Asset/VectorImport.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Utils/AssetManager.ixx"
)
set(ARTIFACTCORE_ASSET_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Asset/AbstractAssetFile.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Asset/AssetConverter.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Asset/AssetDatabase.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Asset/AssetImporter.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Asset/AssetManager.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Asset/AssetMetaFile.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Asset/ImageAsset.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_ASSET_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_ASSET_IMPL})

set(ARTIFACTCORE_COMMAND_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Command/CommandModule.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Command/CommonCommands.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Command/EditSession.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Command/EditSessionManager.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Command/LambdaCommand.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Command/LayerLockManager.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Command/SerializableCommand.ixx"
)
set(ARTIFACTCORE_COMMAND_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Command/LambdaCommand.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Command/LayerLockManager.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_COMMAND_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_COMMAND_IMPL})

set(ARTIFACTCORE_ACOUSTIC_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Acoustic/Acoustic.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Acoustic/AcousticSystem.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Acoustic/FrictionModel.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Acoustic/ModalResonator.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Acoustic/RainModel.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Acoustic/SpatialSystem.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Acoustic/WindModel.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Diagnostic/DiagnosticRegistry.cppm"
)
set(ARTIFACTCORE_ACOUSTIC_IMPL)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_ACOUSTIC_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_ACOUSTIC_IMPL})

set(ARTIFACTCORE_SHAPE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Shape/AeOperators.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Shape/Repeater.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Shape/ShapeGroup.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Shape/ShapeLayer.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Shape/ShapeOperator.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Shape/ShapePath.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Shape/ShapeTypes.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Shape/TrimPaths.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/IO/VectorExport.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/IO/IO.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Shape/ShapeGroup.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Shape/ShapeLayer.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Shape/ShapePath.cppm"
)
set(ARTIFACTCORE_SHAPE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/IO/VectorExport.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_SHAPE_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_SHAPE_IMPL})

set(ARTIFACTCORE_PLATFORM_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Platform/Platform.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Platform/PlatformHint.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Platform/Power.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Platform/ProcessHelperWin.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Platform/ShellUtils.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Platform/SystemUsage.ixx"
)
set(ARTIFACTCORE_PLATFORM_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Platform/Platform.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Platform/Power.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Platform/ProcessHelperWin.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Platform/ShellUtils.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_PLATFORM_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_PLATFORM_IMPL})

set(ARTIFACTCORE_THREAD_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Thread/BackgroundTaskRuntime.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Thread/BackgroundTaskSamples.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Thread/BackgroundTaskWorkerPool.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Thread/PreciseTicker.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Thread/ThreadHelper.ixx"
)
set(ARTIFACTCORE_THREAD_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Thread/PreciseTicker.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Thread/ThreadHelper.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_THREAD_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_THREAD_IMPL})

set(ARTIFACTCORE_ANALYZE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Analyze/Analyze.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Analyze/Histgram.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Analyze/ImageAnalyzer.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Analyze/OpenCV/OptifalFlowResult.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Analyze/SmartPaletteAnalyzer.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Time/TimeRemap.ixx"
)
set(ARTIFACTCORE_ANALYZE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Analyze/Histgram.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Analyze/ImageAnalyzer.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Analyze/OpenCV/OptifalFlowResult.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Analyze/SmartPaletteAnalyzer.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Time/TimeRemap.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_ANALYZE_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_ANALYZE_IMPL})

set(ARTIFACTCORE_TRACKING_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Tracking/MotionTracker.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Tracking/PlanarTracker.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Tracking/CameraTracker.cppm"
)
set(ARTIFACTCORE_TRACKING_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Tracking/MotionTracker.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Tracking/PlanarTracker.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_TRACKING_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_TRACKING_IMPL})

set(ARTIFACTCORE_IPC_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/IPC/IPCChannel.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/IPC/RenderFarmSharedBuffer.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/IPC/SharedMemoryRingBuffer.ixx"
)
set(ARTIFACTCORE_IPC_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/IPC/IPCChannel.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/IPC/RenderFarmSharedBuffer.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/IPC/SharedMemoryRingBuffer.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_IPC_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_IPC_IMPL})

set(ARTIFACTCORE_NLE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/NLE/Core.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/NLE/OTIO.ixx"
)
set(ARTIFACTCORE_NLE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/NLE/Core.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/NLE/OTIO.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_NLE_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_NLE_IMPL})

set(ARTIFACTCORE_PLAYBACK_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Playback/PlaybackClock.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Playback/PlaybackState.ixx"
)
set(ARTIFACTCORE_PLAYBACK_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Playback/PlaybackClock.cppm"
)
set_source_files_properties(${ARTIFACTCORE_PLAYBACK_IMPL} PROPERTIES CXX_SCAN_FOR_MODULES OFF)
set_property(SOURCE ${ARTIFACTCORE_PLAYBACK_IMPL} APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Playback.Clock=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCorePlayback.dir/Playback.Clock.ifc"
    "/reference;Playback.State=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCorePlayback.dir/Playback.State.ifc"
    "/reference;Event.Bus=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEvent.dir/Event.Bus.ifc"
    "/reference;Frame.Position=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Position.ifc"
    "/reference;Frame.Rate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc"
    "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc"
    "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
    "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
set_property(SOURCE ${ARTIFACTCORE_PLAYBACK_IMPL} APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCorePlayback.dir/include/Playback/PlaybackClock.ixx.obj")
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_PLAYBACK_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_PLAYBACK_IMPL})

set(ARTIFACTCORE_PREVIEW_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Preview/PreviewQuality.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Preview/PreviewSettings.ixx"
)
set(ARTIFACTCORE_PREVIEW_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Preview/PreviewQuality.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Preview/PreviewSettings.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_PREVIEW_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_PREVIEW_IMPL})

set(ARTIFACTCORE_EXPORT_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Export/Lottie/LottieExporter.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Export/Lottie/LottieRigExporter.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Export/Lottie/LottieTypes.ixx"
)
set(ARTIFACTCORE_EXPORT_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Export/Lottie/LottieExporter.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Export/Lottie/LottieRigExporter.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_EXPORT_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_EXPORT_IMPL})

set(ARTIFACTCORE_VST3_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/VST3/VST3Interfaces.ixx"
)
set(ARTIFACTCORE_VST3_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/VST3/VST3Loader.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_VST3_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_VST3_IMPL})

set(ARTIFACTCORE_LOCALIZATION_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Localization/LocaleFormatting.ixx"
)
set(ARTIFACTCORE_LOCALIZATION_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Localization/Localization.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_LOCALIZATION_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_LOCALIZATION_IMPL})

set(ARTIFACTCORE_COORDINATE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Coordinate/CoordinateProfile.ixx"
)
set(ARTIFACTCORE_COORDINATE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Coordinate/CoordinateProfile.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_COORDINATE_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_COORDINATE_IMPL})

set(ARTIFACTCORE_EVENT_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Event/Event.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Event/EventBus.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Event/EventBusDebugger.ixx"
)
set(ARTIFACTCORE_EVENT_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Event/EventBus.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Event/EventBusDebugger.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_EVENT_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_EVENT_IMPL})

set(ARTIFACTCORE_FILE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/File/FileInfo.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/File/FileTypeDetector.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/File/PreviewDescriptor.ixx"
)
set(ARTIFACTCORE_FILE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/File/FileInfo.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/File/FileTypeDetector.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_FILE_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_FILE_IMPL})

set(ARTIFACTCORE_PLUGIN_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Plugin/ILayerPlugin.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Plugin/PluginCommon.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Plugin/PluginRegistry.ixx"
)
set(ARTIFACTCORE_PLUGIN_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Plugin/PluginCommon.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Plugin/PluginRegistry.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_PLUGIN_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_PLUGIN_IMPL})

set(ARTIFACTCORE_CONTROL_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Control/MidiInput.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Control/OscInput.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Control/ArtifactExternalControlManager.cppm"
)
set(ARTIFACTCORE_CONTROL_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Control/MidiInput.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Control/OscInput.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_CONTROL_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_CONTROL_IMPL})

set(ARTIFACTCORE_DATABASE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Database/Database.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Database/DatabaseTypes.ixx"
)
set(ARTIFACTCORE_DATABASE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Database/Database.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_DATABASE_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_DATABASE_IMPL})

set(ARTIFACTCORE_MASK_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Mask/DepthMask.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Mask/PathMorph.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Mask/RotoMask.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Mask/VolumeMask.ixx"
)
set(ARTIFACTCORE_MASK_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Mask/DepthMask.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Mask/PathMorph.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Mask/RotoMask.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Mask/VolumeMask.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_MASK_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_MASK_IMPL})

set(ARTIFACTCORE_CONFIGURATION_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Configuration/ConfigLayer.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Configuration/ConfigSchema.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Configuration/LayeredConfigStore.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Application/ArtifactAppSettings.ixx"
)
set(ARTIFACTCORE_CONFIGURATION_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Configuration/ConfigSchema.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Configuration/LayeredConfigStore.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Application/ArtifactAppSettings.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_CONFIGURATION_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_CONFIGURATION_IMPL})

set(ARTIFACTCORE_TEXT_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Font/FontDescriptor.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Font/FontLicenseRegistry.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Font/FreeFont.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Text/GlyphAtlas.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Text/GlyphLayout.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Text/TextAnimator.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Text/TextLayoutContract.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Text/TextShapingBackend.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Text/TextStyle.ixx"
)
set(ARTIFACTCORE_TEXT_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Text/GlyphAtlas.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Text/GlyphLayout.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Text/TextAnimator.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Text/TextShapingBackend.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Text/TextStyle.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_TEXT_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_TEXT_IMPL})
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/include/Text/GlyphLayout.ixx" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Shape.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreShape.dir/Shape.Types.ifc"
    "/reference;Property.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Property.Types.ifc")

set(ARTIFACTCORE_GENERATE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Generate/GenerateTestImage.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Generate/StarfieldGenerator.ixx"
)
set(ARTIFACTCORE_GENERATE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Generate/GenerateTestImage.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Generate/StarfieldGenerator.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_GENERATE_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_GENERATE_IMPL})

set(ARTIFACTCORE_SIMULATION_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Simulation/OpenVDBVolumeReference.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Simulation/PyroSimulation.ixx"
)
set(ARTIFACTCORE_SIMULATION_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Simulation/OpenVDBVolumeReference.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Simulation/PyroSimulation.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_SIMULATION_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_SIMULATION_IMPL})

set(ARTIFACTCORE_TRACK_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Track/LayerTrack.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Track/NccTracker.ixx"
)
set(ARTIFACTCORE_TRACK_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Track/NccTracker.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_TRACK_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_TRACK_IMPL})

set(ARTIFACTCORE_SOURCE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Source/ISource.ixx"
)
set(ARTIFACTCORE_SOURCE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Source/ISource.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_SOURCE_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_SOURCE_IMPL})

set(ARTIFACTCORE_PROJECT_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Project/MetadataCollector.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Project/ProjectVisitor.ixx"
)
set(ARTIFACTCORE_PROJECT_IMPL)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_PROJECT_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_PROJECT_IMPL})

set(ARTIFACTCORE_SCENE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Scene/SceneNode.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Scene/SimulationSettings.ixx"
)
set(ARTIFACTCORE_SCENE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Scene/SceneNode.cppm"
)
set_property(SOURCE ${ARTIFACTCORE_SCENE_IMPL} APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Scene.SceneNode=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreScene.dir/Scene.SceneNode.ifc"
    "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
    "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
    "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
    "/reference;Mesh=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Mesh.ifc"
    "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_SCENE_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_SCENE_IMPL})

set(ARTIFACTCORE_RIG_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Rig/Rig2D.ixx"
)
set(ARTIFACTCORE_RIG_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Rig/Rig2D.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_RIG_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_RIG_IMPL})

set(ARTIFACTCORE_GRID_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Grid/ArtifactGridSystem.ixx"
)
set(ARTIFACTCORE_GRID_IMPL)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_GRID_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_GRID_IMPL})

set(ARTIFACTCORE_COLORCOLLECTION_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ColorCollection/ColorGrading.ixx"
)
set(ARTIFACTCORE_COLORCOLLECTION_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ColorCollection/ColorGrading.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_COLORCOLLECTION_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_COLORCOLLECTION_IMPL})

set(ARTIFACTCORE_SOUND_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Sound/SoundTrack.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Sound/SoundType.ixx"
)
set(ARTIFACTCORE_SOUND_IMPL)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_SOUND_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_SOUND_IMPL})

set(ARTIFACTCORE_SEQUENCE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Sequence/SequenceGroup.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Sequence/StepSequencer.ixx"
)
set(ARTIFACTCORE_SEQUENCE_IMPL)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_SEQUENCE_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_SEQUENCE_IMPL})

set(ARTIFACTCORE_MATERIAL_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Material/Material.ixx")
set(ARTIFACTCORE_MATERIAL_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Material/Material.cppm")
set_property(SOURCE ${ARTIFACTCORE_MATERIAL_IMPL} PROPERTY COMPILE_OPTIONS
    "/reference;Material.Material=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMaterial.dir/Material.Material.ifc"
    "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_MATERIAL_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_MATERIAL_IMPL})

set(ARTIFACTCORE_ENVIRONMENT_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EnvironmentVariable/EnvironmentVariable.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EnvironmentVariable/TokenExpansion.ixx")
set(ARTIFACTCORE_ENVIRONMENT_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/EnvironmentVariable/EnvironmentVariable.cppm")
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_ENVIRONMENT_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_ENVIRONMENT_IMPL})

set(ARTIFACTCORE_LIGHT_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Light/IESProfile.ixx")
set(ARTIFACTCORE_LIGHT_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Light/IESProfile.cppm")
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_LIGHT_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_LIGHT_IMPL})

set(ARTIFACTCORE_CROWD_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Crowd/BoidsSwarmSystem.ixx")
set(ARTIFACTCORE_CROWD_IMPL)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_CROWD_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_CROWD_IMPL})

set(ARTIFACTCORE_DOMAIN_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Domain/LayerTypes.ixx")
set(ARTIFACTCORE_DOMAIN_IMPL)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_DOMAIN_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_DOMAIN_IMPL})

set(ARTIFACTCORE_FILESYSTEM_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/FileSystem/DirectoryBuilder.ixx")
set(ARTIFACTCORE_FILESYSTEM_IMPL)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_FILESYSTEM_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_FILESYSTEM_IMPL})

set(ARTIFACTCORE_ICON_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Icon/SvgToIcon.ixx")
set(ARTIFACTCORE_ICON_IMPL)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_ICON_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_ICON_IMPL})

set(ARTIFACTCORE_VST_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/VST/VSTEffect.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/VST/VSTHost.ixx"
)
set(ARTIFACTCORE_VST_IMPL)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_VST_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_VST_IMPL})

set(ARTIFACTCORE_NETWORK_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/NetworkRPCClient.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/NetworkRPCServer.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Network/CollaborationWebSocket.ixx"
)
set(ARTIFACTCORE_NETWORK_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Network/CollaborationWebSocket.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Network/NetworkRPCClient.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Network/NetworkRPCServer.cppm"
)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_NETWORK_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_NETWORK_IMPL})

set(ARTIFACTCORE_COLLABORATE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Collaborate/CollaborationProtocol.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Collaborate/CollaborationSession.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Collaborate/CollabReview.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Collaborate/CollabOperations.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Collaborate/CollaborationSessionAdapter.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Collaborate/CollabSchema.cppm"
)
set(ARTIFACTCORE_COLLABORATE_IMPL)
list(REMOVE_ITEM CORE_MODULES ${ARTIFACTCORE_COLLABORATE_MODULES})
list(REMOVE_ITEM CORE_IMPL ${ARTIFACTCORE_COLLABORATE_IMPL})


# ---------------------------------------------------------------------------
# ターゲット定義
# ---------------------------------------------------------------------------

# Finalize implementation-unit BMI ordering after all domain lists exist.
foreach(_artifact_module_reference IN LISTS ARTIFACTCORE_IMPLEMENTATION_MODULE_REFERENCES)
    string(REPLACE "|" ";" _artifact_module_fields "${_artifact_module_reference}")
    list(GET _artifact_module_fields 0 _artifact_impl_relative)
    list(GET _artifact_module_fields 2 _artifact_interface_relative)
    set(_artifact_impl_file "${CMAKE_CURRENT_SOURCE_DIR}/${_artifact_impl_relative}")
    file(REAL_PATH "${_artifact_impl_file}" _artifact_impl_real)
    set(_artifact_owner_target_dir "ArtifactCore.dir")
    get_cmake_property(_artifact_core_variables VARIABLES)
    foreach(_artifact_domain_var IN LISTS _artifact_core_variables)
        if(_artifact_domain_var MATCHES "^ARTIFACTCORE_(.+)_IMPL$")
            foreach(_artifact_domain_source IN LISTS ${_artifact_domain_var})
                file(REAL_PATH "${_artifact_domain_source}" _artifact_domain_source_real)
                if(_artifact_domain_source_real STREQUAL _artifact_impl_real)
                    string(REGEX REPLACE "^ARTIFACTCORE_(.+)_IMPL$" "\\1" _artifact_domain "${_artifact_domain_var}")
                    if(_artifact_domain STREQUAL "AI" OR _artifact_domain STREQUAL "UI" OR
                       _artifact_domain STREQUAL "IPC" OR _artifact_domain STREQUAL "NLE" OR
                       _artifact_domain STREQUAL "VST3")
                        set(_artifact_domain_target "${_artifact_domain}")
                    elseif(_artifact_domain STREQUAL "COLORCOLLECTION")
                        set(_artifact_domain_target "ColorCollection")
                    elseif(_artifact_domain STREQUAL "FILESYSTEM")
                        set(_artifact_domain_target "FileSystem")
                    else()
                        string(SUBSTRING "${_artifact_domain}" 0 1 _artifact_domain_initial)
                        string(SUBSTRING "${_artifact_domain}" 1 -1 _artifact_domain_tail)
                        string(TOLOWER "${_artifact_domain_tail}" _artifact_domain_tail_lower)
                        set(_artifact_domain_target "${_artifact_domain_initial}${_artifact_domain_tail_lower}")
                    endif()
                    set(_artifact_owner_target_dir "ArtifactCore${_artifact_domain_target}.dir")
                    break()
                endif()
            endforeach()
            if(NOT _artifact_owner_target_dir STREQUAL "ArtifactCore.dir")
                break()
            endif()
        endif()
    endforeach()
    file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${_artifact_interface_relative}" _artifact_expected_interface)
    get_cmake_property(_artifact_core_variables VARIABLES)
    foreach(_artifact_domain_var IN LISTS _artifact_core_variables)
        if(_artifact_domain_var MATCHES "^ARTIFACTCORE_(.+)_MODULES$")
            foreach(_artifact_domain_interface IN LISTS ${_artifact_domain_var})
                file(REAL_PATH "${_artifact_domain_interface}" _artifact_actual_interface)
                if(_artifact_actual_interface STREQUAL _artifact_expected_interface)
                    string(REGEX REPLACE "^ARTIFACTCORE_(.+)_MODULES$" "\\1" _artifact_domain "${_artifact_domain_var}")
                    if(_artifact_domain STREQUAL "AI" OR _artifact_domain STREQUAL "UI" OR
                       _artifact_domain STREQUAL "IPC" OR _artifact_domain STREQUAL "NLE" OR
                       _artifact_domain STREQUAL "VST3")
                        set(_artifact_domain_target "${_artifact_domain}")
                    elseif(_artifact_domain STREQUAL "COLORCOLLECTION")
                        set(_artifact_domain_target "ColorCollection")
                    elseif(_artifact_domain STREQUAL "FILESYSTEM")
                        set(_artifact_domain_target "FileSystem")
                    else()
                        string(SUBSTRING "${_artifact_domain}" 0 1 _artifact_domain_initial)
                        string(SUBSTRING "${_artifact_domain}" 1 -1 _artifact_domain_tail)
                        string(TOLOWER "${_artifact_domain_tail}" _artifact_domain_tail_lower)
                        set(_artifact_domain_target "${_artifact_domain_initial}${_artifact_domain_tail_lower}")
                    endif()
                    set(_artifact_owner_target_dir "ArtifactCore${_artifact_domain_target}.dir")
                    break()
                endif()
            endforeach()
            if(NOT _artifact_owner_target_dir STREQUAL "ArtifactCore.dir")
                break()
            endif()
        endif()
    endforeach()
    if(NOT _artifact_interface_owner_found AND
       _artifact_interface_relative MATCHES "^include/Utils/Localization\\.ixx$")
        set(_artifact_owner_target_dir "ArtifactCore.dir")
    endif()
    if(_artifact_interface_relative MATCHES "^include/AI/")
        set(_artifact_owner_target_dir "ArtifactCoreAI.dir")
    elseif(_artifact_interface_relative MATCHES "^include/Configuration/")
        set(_artifact_owner_target_dir "ArtifactCoreConfiguration.dir")
    endif()
    set_property(SOURCE "${_artifact_impl_file}" PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${_artifact_owner_target_dir}/${_artifact_interface_relative}.obj")
    if(_artifact_impl_relative STREQUAL "src/Animation/AnimatableTransform2D.cppm")
        # Keep this attachment unscanned (MSVC/CMake otherwise forms a dyndep
        # cycle through Animation.Transform2D), but also order its imported
        # Animation.Value BMI before compilation.
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Animation/AnimatableValue.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Animation/AnimatableTransform3D.cppm")
        # Keep imported interface BMIs available before this unscanned
        # implementation is compiled without adding a dyndep cycle.
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Animation/AnimatableValue.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/NamedVector.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Diagnostics/Logger.cppm")
        # Logger imports NamedVector, whose BMI also requires the container
        # debug interfaces when MSVC resolves the transitive import set.
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Diagnostics/Logger.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEvent.dir/include/Event/EventBus.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Diagnostics/CoreDiagnosticRecorder.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Diagnostics/CoreDiagnosticSnapshot.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/src/Memory/SharedPtr.cppm.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Utils/Result.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Utils/Optional.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/NamedVector.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Color/ColorSpace.cppm")
        # Non-scanned implementation units need explicit build ordering for
        # imported interface BMIs in addition to their /reference options.
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Color/ColorGamutConversion.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Codec/MFFrameExtractor.cppm")
        # Memory.TrackedPtr is an interface-bearing .cppm, so make its object
        # (and therefore its BMI) available before this attachment compiles.
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/src/Memory/TrackedPtr.cppm.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Color/ColorSwatch.cppm")
        # ColorSwatch's attached interface uses the container debug registry
        # while MSVC instantiates its implementation-side standard containers.
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Diagnostics/FallbackPolicy.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/Compute/LUT3DComputer.cppm")
        # Preserve the HLSL source module ordering after the generic
        # implementation-interface dependency assignment above.
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Graphics/Shader/Compute/HLSL/LUT3D.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Frame/FrameRate.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Serialization/JsonSerializableAdapter.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Serialization/SchemaMigration.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/Shader/Compute/GPUCompressionPipeline.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Graphics/Shader/Compute/HLSL/Compression.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/PSOCache.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Utils/Path.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/RayTracingManager.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Graphics/Graphics.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/DirectCompute/SinglePassShader.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Core/ArtifactString.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Image/ImageYUV420.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Image/ImageF32x4_RGBA.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Image/ImageF32x4_RGBA.cppm")
        # The implementation imports SurfacePixelConversion.  Keep its
        # interface object ahead of this compile after the generic dependency
        # assignment above, which otherwise overwrites earlier attachments.
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Image/SurfacePixelConversion.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/EdgeEcho.cppm")
        # EdgeEcho imports the DirectCompute SimpleChoker interface.  The
        # compile option alone does not order its IFC producer in MSVC.
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/ImageProcessing/DirectCompute/SimpleChokerCS.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/NeuromorphicDisplace.cppm")
        # This implementation unit explicitly imports its own partition;
        # order the partition interface object before compiling it.
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/ImageProcessing/NeuromorphicDisplace.ixx.obj")
    elseif(_artifact_impl_relative MATCHES "^src/Composition/")
        set(_artifact_composition_interface_objects)
        foreach(_artifact_composition_interface IN LISTS ARTIFACTCORE_COMPOSITION_MODULES)
            if(_artifact_composition_interface MATCHES "\\.ixx$")
                file(RELATIVE_PATH _artifact_composition_interface_relative
                    "${CMAKE_CURRENT_SOURCE_DIR}" "${_artifact_composition_interface}")
                list(APPEND _artifact_composition_interface_objects
                    "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreComposition.dir/${_artifact_composition_interface_relative}.obj")
            endif()
        endforeach()
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            ${_artifact_composition_interface_objects}
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Color/FloatColor.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Color/ColorTransferFunction.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Color/FloatRGBA.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Serialization/JsonSerializableAdapter.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Serialization/ISerializable.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Serialization/SerializationRegistry.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Serialization/SchemaMigration.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Image/ImageF32x4_RGBA.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Image/ImageInterface.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Image/ImageSurfaceView.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Graphics/SurfaceColorContract.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/src/Memory/SharedPtr.cppm.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Utils/Optional.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Frame/FramePosition.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Utils/Id.ixx.obj")
    elseif(_artifact_impl_relative MATCHES "^src/Text/")
        set(_artifact_text_interface_objects)
        foreach(_artifact_text_interface IN LISTS ARTIFACTCORE_TEXT_MODULES)
            if(_artifact_text_interface MATCHES "\\.ixx$")
                file(RELATIVE_PATH _artifact_text_interface_relative
                    "${CMAKE_CURRENT_SOURCE_DIR}" "${_artifact_text_interface}")
                list(APPEND _artifact_text_interface_objects
                    "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreText.dir/${_artifact_text_interface_relative}.obj")
            endif()
        endforeach()
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            ${_artifact_text_interface_objects}
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreText.dir/include/Font/FontDescriptor.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Utils/UniString.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Utils/Optional.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Color/FloatColor.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Color/FloatRGBA.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreShape.dir/include/Shape/ShapeTypes.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Property/PropertyTypes.ixx.obj")
    elseif(_artifact_impl_relative MATCHES "^src/Configuration/" OR
           _artifact_impl_relative STREQUAL "src/Application/ArtifactAppSettings.cppm")
        set(_artifact_configuration_interface_objects)
        foreach(_artifact_configuration_interface IN LISTS ARTIFACTCORE_CONFIGURATION_MODULES)
            if(_artifact_configuration_interface MATCHES "\\.ixx$")
                file(RELATIVE_PATH _artifact_configuration_interface_relative
                    "${CMAKE_CURRENT_SOURCE_DIR}" "${_artifact_configuration_interface}")
                list(APPEND _artifact_configuration_interface_objects
                    "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreConfiguration.dir/${_artifact_configuration_interface_relative}.obj")
            endif()
        endforeach()
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            ${_artifact_configuration_interface_objects}
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Core/FastSettingsStore.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreGrid.dir/include/Grid/ArtifactGridSystem.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Color/FloatColor.ixx.obj")
    elseif(_artifact_impl_relative MATCHES "^src/Asset/")
        set(_artifact_asset_interface_objects)
        foreach(_artifact_asset_interface IN LISTS ARTIFACTCORE_ASSET_MODULES)
            if(_artifact_asset_interface MATCHES "\\.ixx$")
                file(RELATIVE_PATH _artifact_asset_interface_relative
                    "${CMAKE_CURRENT_SOURCE_DIR}" "${_artifact_asset_interface}")
                list(APPEND _artifact_asset_interface_objects
                    "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAsset.dir/${_artifact_asset_interface_relative}.obj")
            endif()
        endforeach()
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            ${_artifact_asset_interface_objects}
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreFile.dir/include/File/FileTypeDetector.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreConfiguration.dir/include/Configuration/ConfigLayer.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreConfiguration.dir/include/Configuration/LayeredConfigStore.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/IO/Image/ImageImporter.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/src/Memory/SharedPtr.cppm.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Utils/Id.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Utils/UniString.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Mask/RotoMask.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMask.dir/include/Mask/RotoMask.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/NamedVector.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebug.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Common/Parallel.ixx.obj")
    elseif(_artifact_impl_relative MATCHES "^src/Audio/" OR
           _artifact_impl_relative STREQUAL "src/Codec/FFMpegAudioDecoder.cppm" OR
           _artifact_impl_relative STREQUAL "src/CLAP/CLAPHost.cppm" OR
           _artifact_impl_relative STREQUAL "src/Animation/KeyframeEditingTools.cppm")
        # Order every Audio-domain interface before scanner-disabled Audio
        # implementations.  This keeps all imported Audio IFCS in the split
        # target and avoids races against the aggregate Core target.
        set(_artifact_audio_interface_objects)
        foreach(_artifact_audio_interface IN LISTS ARTIFACTCORE_AUDIO_MODULES)
            if(_artifact_audio_interface MATCHES "\\.ixx$")
                file(RELATIVE_PATH _artifact_audio_interface_relative
                    "${CMAKE_CURRENT_SOURCE_DIR}" "${_artifact_audio_interface}")
                list(APPEND _artifact_audio_interface_objects
                    "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/${_artifact_audio_interface_relative}.obj")
            endif()
        endforeach()
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            ${_artifact_audio_interface_objects}
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebug.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Utils/Id.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Utils/SizeLike.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Geometry/Interpolate.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/FaceDetectionEngine.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/ImageProcessing/OpenCV/CvUtils.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/NamedVector.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/FaceTracker.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/NamedVector.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/OpenCV/Glow.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Image/Image.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/ImageProcessing/ProceduralTexture.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Graphics/Shader/Compute/HLSL/ProceduralTexture.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebug.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Layer/Layer2D.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebug.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/IO/Image/ImageExporter.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Image/ImageUtils.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Script/Engine/Enviroment/EnvironmentManager.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Script/Engine/Value/Value.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Transform/ViewportTransformer.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Diagnostics/Logger.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Utils/UniString.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Utils/StringConvertor.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Image/OpenCV/ImageTransformCV.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Image/Transform/ImageTransformCV.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Graphics/LayerBlendPipeline.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Graphics/Shader/Compute/HLSL/MatteTrack.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/NLE/Core.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Core/Defaults.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Time/RationalTime.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Utils/UniString.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Frame/FramePosition.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Frame/FrameRange.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Frame/FrameRate.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Frame/FrameOffset.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Serialization/JsonSerializableAdapter.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Serialization/SchemaMigration.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Event/EventBus.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/src/Memory/SharedPtr.cppm.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebug.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Event/EventBusDebugger.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEvent.dir/include/Event/EventBus.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/src/Memory/SharedPtr.cppm.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Core/ArtifactString.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Utils/Text/String.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebug.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Video/Transitions/CubeTransition.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreNLE.dir/include/NLE/Core.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/include/Video/TransitionFactory.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/NLE/OTIO.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreNLE.dir/include/NLE/Core.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Frame/FrameRange.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Frame/FrameOffset.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Thread/PreciseTicker.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreThread.dir/include/Thread/PreciseTicker.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Frame/FrameRange.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/NamedVector.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Frame/FramePosition.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Frame/FrameOffset.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Frame/FrameRate.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Time/RationalTime.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Serialization/JsonSerializableAdapter.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Serialization/SchemaMigration.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Layer/LayerMatte.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_BINARY_DIR}/CMakeFiles/__cmake_cxx23.dir/artifact_std.ifc"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Color/ColorLuminance.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Color/FloatRGBA.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Image/ImageF32x4_RGBA.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebug.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Serialization/JsonSerializableAdapter.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Serialization/SchemaMigration.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Video/Stabilizer.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Frame/FramePosition.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Common/Parallel.ixx.obj")
    elseif(_artifact_impl_relative STREQUAL "src/Render/LogCollector.cppm")
        set_property(SOURCE "${_artifact_impl_file}" APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebug.ixx.obj"
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
    endif()
endforeach()

add_library(ArtifactCore STATIC)

set_target_properties(ArtifactCore PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# import std is provided by the explicit MSVC std.ifc producer declared at the
# top level; do not trigger CMake's incomplete 4.3 standard-module target.
set_property(TARGET ArtifactCore PROPERTY CXX_MODULE_STD OFF)
if(TARGET ArtifactStdModule)
    add_dependencies(ArtifactCore ArtifactStdModule)
endif()

# MSVC specific C++23 modules settings
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCore PRIVATE /WX- /wd5050)
    # Enable C++23 standard (latest)
    target_compile_options(ArtifactCore PRIVATE /std:c++latest)
    # Enable modules support
    target_compile_options(ArtifactCore PRIVATE /experimental:module)
    # Enable full C++23 library support
    target_compile_options(ArtifactCore PRIVATE /Zc:__cplusplus /permissive-)
    target_compile_options(ArtifactCore PRIVATE
        "/FI${CMAKE_CURRENT_SOURCE_DIR}/include/Compat/QtListForward.h")
endif()

set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Property/AbstractProperty.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;EnvironmentVariable.Expansion=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEnvironment.dir/EnvironmentVariable.Expansion.ifc"
    "/reference;Audio.Modulation.Router=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.Modulation.Router.ifc"
    "/reference;Animation.Transform3D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Animation.Transform3D.ifc"
    "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc"
    "/reference;Frame.Position=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Position.ifc"
    "/reference;Property.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Property.Types.ifc"
    "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc"
    "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc"
    "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Property/AbstractProperty.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Script.Expression.Parser=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Script.Expression.Parser.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Property/AbstractProperty.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Audio.Modulation.Modulator=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/Audio.Modulation.Modulator.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Property/AbstractProperty.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;EnvironmentVariable=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEnvironment.dir/EnvironmentVariable.ifc"
    "/reference;Frame.Rate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Property/AbstractProperty.cppm" APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEnvironment.dir/include/EnvironmentVariable/TokenExpansion.ixx.obj"
    "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAudio.dir/include/Audio/Modulation/Router.ixx.obj")
# The aggregate manifest can still receive a path spelling from the generated
# source manifest after domain removals. Enforce the split boundary immediately
# before the target is declared.
list(FILTER CORE_MODULES EXCLUDE REGEX "(^|[/\\\\])FFMpegAudioDecoder\\.ixx$")
list(FILTER CORE_IMPL EXCLUDE REGEX "(^|[/\\\\])FFMpegAudioDecoder\\.cppm$")
list(FILTER CORE_MODULES EXCLUDE REGEX "(^|[/\\\\])DecoderPoolManager\\.ixx$")
list(FILTER CORE_MODULES EXCLUDE REGEX "(^|[/\\\\])ParticleSystem\\.ixx$")
list(FILTER CORE_IMPL EXCLUDE REGEX "(^|[/\\\\])OpenEXR\\.cppm$")
list(FILTER CORE_IMPL EXCLUDE REGEX "(^|[/\\\\])FFmpegEncoder\\.Helpers\\.cppm$")
list(APPEND CORE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Particle/ParticleSystem.ixx"
)
list(APPEND CORE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Particle/ParticleSystem.cppm"
)
# Keep split-domain interfaces out of the aggregate target even if a later
# manifest append reintroduces them.  Duplicate module producers can make
# MSVC resolve a split implementation against a stale aggregate IFC.
list(REMOVE_ITEM CORE_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Scene/SceneNode.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EnvironmentVariable/EnvironmentVariable.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EnvironmentVariable/TokenExpansion.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Playback/PlaybackClock.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Playback/PlaybackState.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Codec/FFmpegThumbnailExtractor.ixx")
list(REMOVE_ITEM CORE_IMPL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EnvironmentVariable/EnvironmentVariable.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Playback/PlaybackClock.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Codec/FFmpegThumbnailExtractor.cppm")
target_sources(ArtifactCore
    PRIVATE ${CORE_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${CORE_MODULES}
)

target_include_directories(ArtifactCore
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/Composition"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/Core"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/Image"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/Grid"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/Render"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/Color"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Acoustic"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Artifact/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Common/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Primitives/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/DiligentEngine/DiligentCore/Platforms/interface"
)

if(MSVC)
    get_filename_component(_artifact_msvc_toolchain_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_artifact_msvc_toolchain_dir "${_artifact_msvc_toolchain_dir}" DIRECTORY)
    get_filename_component(_artifact_msvc_toolchain_dir "${_artifact_msvc_toolchain_dir}" DIRECTORY)
    get_filename_component(_artifact_msvc_toolchain_dir "${_artifact_msvc_toolchain_dir}" DIRECTORY)
    target_include_directories(ArtifactCore SYSTEM BEFORE PUBLIC "${_artifact_msvc_toolchain_dir}/include")

    set(_artifact_windows_sdk_root "C:/Program Files (x86)/Windows Kits/10")
    if(EXISTS "${_artifact_windows_sdk_root}/Include")
        file(GLOB _artifact_windows_sdk_versions LIST_DIRECTORIES true "${_artifact_windows_sdk_root}/Include/*")
        list(SORT _artifact_windows_sdk_versions COMPARE NATURAL ORDER DESCENDING)
        list(GET _artifact_windows_sdk_versions 0 _artifact_windows_sdk_version_dir)
        target_include_directories(ArtifactCore SYSTEM BEFORE PUBLIC
            "${_artifact_windows_sdk_version_dir}/ucrt"
            "${_artifact_windows_sdk_version_dir}/shared"
            "${_artifact_windows_sdk_version_dir}/um"
            "${_artifact_windows_sdk_version_dir}/winrt"
        )
    endif()
endif()

# DiligentEngine
set(DILIGENT_CORE_LIBS "")
set(DILIGENT_CORE_COMPILE_DEFINITIONS "")
if(TARGET Diligent-GraphicsEngineD3D12-static)
    list(APPEND DILIGENT_CORE_LIBS Diligent-GraphicsEngineD3D12-static)
elseif(TARGET Diligent-GraphicsEngineD3D12-shared)
    list(APPEND DILIGENT_CORE_LIBS Diligent-GraphicsEngineD3D12-shared)
    list(APPEND DILIGENT_CORE_COMPILE_DEFINITIONS DILIGENT_D3D12_SHARED=1)
endif()
if(TARGET Diligent-GraphicsEngineD3DBase)
    list(APPEND DILIGENT_CORE_LIBS Diligent-GraphicsEngineD3DBase)
endif()
if(TARGET Diligent-Common)
    list(APPEND DILIGENT_CORE_LIBS Diligent-Common)
endif()
if(DILIGENT_CORE_LIBS)
    target_link_libraries(ArtifactCore PUBLIC ${DILIGENT_CORE_LIBS})
endif()
if(DILIGENT_CORE_COMPILE_DEFINITIONS)
    list(REMOVE_DUPLICATES DILIGENT_CORE_COMPILE_DEFINITIONS)
    target_compile_definitions(ArtifactCore PUBLIC ${DILIGENT_CORE_COMPILE_DEFINITIONS})
endif()

# ---------------------------------------------------------------------------
# 外部ライブラリ探索
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebSockets Sql)
target_link_libraries(ArtifactCore PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::WebSockets Qt6::Sql)

find_package(OpenCV REQUIRED)
if(OpenCV_INCLUDE_DIRS)
    target_include_directories(ArtifactCore PUBLIC ${OpenCV_INCLUDE_DIRS})
endif()
target_link_libraries(ArtifactCore PUBLIC ${OpenCV_LIBS})

# Optional libvips backend.  Keep image-sequence and real-time paths on the
# existing OpenCV/QImage implementation unless libvips is explicitly
# available through the host package manager.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBVIPS QUIET IMPORTED_TARGET vips-cpp)
    if(LIBVIPS_FOUND)
        target_link_libraries(ArtifactCore PUBLIC PkgConfig::LIBVIPS)
        target_compile_definitions(ArtifactCore PUBLIC ARTIFACT_HAS_LIBVIPS=1)
        message(STATUS "libvips detected; optional image backend enabled")
    else()
        message(STATUS "libvips not detected; using existing image backend")
    endif()
else()
    message(STATUS "PkgConfig not detected; libvips backend disabled")
endif()

find_package(SDL2 REQUIRED)
target_link_libraries(ArtifactCore PUBLIC SDL2::SDL2)

# Boost (Asio, UUID, Iostreams)
find_package(Boost REQUIRED COMPONENTS system iostreams)
target_link_libraries(ArtifactCore PUBLIC Boost::boost Boost::system Boost::iostreams)

# Vulkan (headers only — volk handles dynamic loading; linking vulkan-1.lib
# would shadow volk's writable global symbols with read-only import thunks)
find_package(Vulkan REQUIRED)
target_link_libraries(ArtifactCore PUBLIC Vulkan::Headers)

find_package(OpenXR QUIET)
if(OpenXR_FOUND)
    target_link_libraries(ArtifactCore PUBLIC OpenXR::headers)
else()
    list(REMOVE_ITEM CORE_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/XR/OpenXR.ixx")
    message(STATUS "OpenXR not found; building without optional XR support")
endif()

find_package(box2d CONFIG REQUIRED)
target_link_libraries(ArtifactCore PUBLIC box2d::box2d)

# ggml-vulkan shader generation needs a real glslc executable.
# Some environments resolve Vulkan_GLSLC_EXECUTABLE to a vcpkg path that does
# not actually contain glslc.exe, so prefer a concrete SDK install if needed.
if(NOT EXISTS "${Vulkan_GLSLC_EXECUTABLE}")
    find_program(ARTIFACT_VULKAN_GLSLC_EXECUTABLE
        NAMES glslc
        HINTS
            ENV VulkanSDK
        PATHS
            "C:/VulkanSDK"
        PATH_SUFFIXES
            Bin
        REQUIRED
    )
    set(Vulkan_GLSLC_EXECUTABLE "${ARTIFACT_VULKAN_GLSLC_EXECUTABLE}" CACHE FILEPATH
        "Preferred glslc executable for llama.cpp Vulkan shader generation" FORCE)
endif()

# TBB
find_package(TBB REQUIRED)
target_link_libraries(ArtifactCore PUBLIC TBB::tbb)

# OpenImageIO
find_package(OpenImageIO REQUIRED)
target_link_libraries(ArtifactCore PUBLIC OpenImageIO::OpenImageIO)

# OpenColorIO is the authoritative color-management backend. Keep the
# existing matrix-based fallback available in code, but make the production
# dependency explicit instead of relying on OpenImageIO's transitive install.
find_package(OpenColorIO CONFIG REQUIRED)
target_link_libraries(ArtifactCore PUBLIC OpenColorIO::OpenColorIO)

# OpenVDB is optional. The native Pyro snapshot path remains available when
# OpenVDB is not installed, while VDB-backed volume sources can use it when
# the package is present.
find_package(OpenVDB CONFIG QUIET)
if(TARGET OpenVDB::openvdb)
    target_link_libraries(ArtifactCore PUBLIC OpenVDB::openvdb)
    target_compile_definitions(ArtifactCore PUBLIC ARTIFACTCORE_HAS_OPENVDB=1)
elseif(TARGET openvdb::openvdb)
    target_link_libraries(ArtifactCore PUBLIC openvdb::openvdb)
    target_compile_definitions(ArtifactCore PUBLIC ARTIFACTCORE_HAS_OPENVDB=1)
endif()

find_package(glm CONFIG REQUIRED)
target_link_libraries(ArtifactCore PUBLIC glm::glm)

# meshoptimizer provides vertex remapping, vertex-cache optimization, and
# meshlet generation for the Mesh LOD preparation path.
find_package(meshoptimizer CONFIG REQUIRED)
target_link_libraries(ArtifactCore PUBLIC meshoptimizer::meshoptimizer)

find_package(Stb REQUIRED)
target_include_directories(ArtifactCore PUBLIC ${Stb_INCLUDE_DIR})

find_package(EnTT CONFIG REQUIRED)
target_link_libraries(ArtifactCore PUBLIC EnTT::EnTT)

find_package(magic_enum CONFIG REQUIRED)
target_link_libraries(ArtifactCore PUBLIC magic_enum::magic_enum)

find_package(protobuf CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)

# Locate sentencepiece using explicit paths derived from the include dir to avoid
# the PATH_SUFFIXES bug where a Debug-build CMAKE_PREFIX_PATH causes the release
# find_library to resolve to the debug/lib folder.
unset(SENTENCEPIECE_INCLUDE_DIR       CACHE)
unset(SENTENCEPIECE_LIBRARY_RELEASE   CACHE)
unset(SENTENCEPIECE_LIBRARY_DEBUG     CACHE)

find_path(SENTENCEPIECE_INCLUDE_DIR
    NAMES sentencepiece_processor.h
)
if(SENTENCEPIECE_INCLUDE_DIR)
    # Derive the vcpkg triplet prefix from the include directory
    # (…/x64-windows/include → …/x64-windows)
    get_filename_component(_sp_prefix "${SENTENCEPIECE_INCLUDE_DIR}" DIRECTORY)
    find_library(SENTENCEPIECE_LIBRARY_RELEASE
        NAMES sentencepiece
        PATHS "${_sp_prefix}/lib"
        NO_DEFAULT_PATH
    )
    find_library(SENTENCEPIECE_LIBRARY_DEBUG
        NAMES sentencepiece
        PATHS "${_sp_prefix}/debug/lib"
        NO_DEFAULT_PATH
    )
endif()

if(SENTENCEPIECE_INCLUDE_DIR AND SENTENCEPIECE_LIBRARY_RELEASE AND SENTENCEPIECE_LIBRARY_DEBUG)
    add_library(sentencepiece::sentencepiece STATIC IMPORTED GLOBAL)
    set_target_properties(sentencepiece::sentencepiece PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SENTENCEPIECE_INCLUDE_DIR}"
        IMPORTED_LOCATION_RELEASE "${SENTENCEPIECE_LIBRARY_RELEASE}"
        IMPORTED_LOCATION_DEBUG   "${SENTENCEPIECE_LIBRARY_DEBUG}"
        INTERFACE_LINK_LIBRARIES  "protobuf::libprotobuf;absl::abseil_dll;absl::flags"
    )
    target_link_libraries(ArtifactCore PUBLIC sentencepiece::sentencepiece)
else()
    message(STATUS "sentencepiece not found; building without optional sentencepiece integration")
endif()

find_package(onnxruntime CONFIG QUIET)
if(TARGET onnxruntime::onnxruntime AND TARGET sentencepiece::sentencepiece)
    target_link_libraries(ArtifactCore PUBLIC onnxruntime::onnxruntime)
    target_compile_definitions(ArtifactCore PRIVATE ARTIFACT_HAS_ONNX_DML_BACKEND=1)
elseif(TARGET onnxruntime AND TARGET sentencepiece::sentencepiece)
    target_link_libraries(ArtifactCore PUBLIC onnxruntime)
    target_compile_definitions(ArtifactCore PRIVATE ARTIFACT_HAS_ONNX_DML_BACKEND=1)
else()
    message(STATUS "onnxruntime or sentencepiece not found; building ONNX DirectML backend stub")
endif()

# llama.cpp
set(ARTIFACT_LLAMA_CPP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../libs/llama.cpp")
option(ARTIFACT_USE_BUNDLED_LLAMA_CPP
    "Build the llama.cpp submodule instead of using the vcpkg llama-cpp package" OFF)
set(_artifact_using_bundled_llama_cpp FALSE)
if(ARTIFACT_USE_BUNDLED_LLAMA_CPP AND NOT TARGET llama AND EXISTS "${ARTIFACT_LLAMA_CPP_SOURCE_DIR}/CMakeLists.txt")
    set(_artifact_prev_scan_for_modules "${CMAKE_CXX_SCAN_FOR_MODULES}")
    set(_artifact_prev_build_shared_libs_defined FALSE)
    if(DEFINED BUILD_SHARED_LIBS)
        set(_artifact_prev_build_shared_libs "${BUILD_SHARED_LIBS}")
        set(_artifact_prev_build_shared_libs_defined TRUE)
    endif()
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

    set(LLAMA_BUILD_COMMON OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_WEBUI OFF CACHE BOOL "" FORCE)
    set(LLAMA_OPENSSL OFF CACHE BOOL "" FORCE)
    set(LLAMA_LLGUIDANCE OFF CACHE BOOL "" FORCE)
    set(GGML_BACKEND_DL ON CACHE BOOL "" FORCE)
    set(GGML_VULKAN OFF CACHE BOOL "" FORCE)
    # CUDA backend is intentionally disabled in the parent build to keep the
    # local verification loop stable on machines without a complete CUDA host toolchain.
    set(GGML_CUDA OFF CACHE BOOL "" FORCE)
    set(GGML_METAL OFF CACHE BOOL "" FORCE)
    set(GGML_WEBGPU OFF CACHE BOOL "" FORCE)
    set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
    # Build a single CPU variant to keep compile time and binary size down.
    set(GGML_CPU_ALL_VARIANTS OFF CACHE BOOL "" FORCE)
    set(GGML_CCACHE OFF CACHE BOOL "" FORCE)

    add_subdirectory("${ARTIFACT_LLAMA_CPP_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/llama.cpp" EXCLUDE_FROM_ALL)
    set(_artifact_using_bundled_llama_cpp TRUE)

    if(_artifact_prev_build_shared_libs_defined)
        set(BUILD_SHARED_LIBS "${_artifact_prev_build_shared_libs}" CACHE BOOL "" FORCE)
    else()
        unset(BUILD_SHARED_LIBS CACHE)
    endif()
    set(CMAKE_CXX_SCAN_FOR_MODULES "${_artifact_prev_scan_for_modules}")
endif()

if(_artifact_using_bundled_llama_cpp AND TARGET llama)
    if(MSVC)
        target_compile_options(llama PRIVATE /Zc:char8_t-)
    endif()

    function(_artifact_align_llama_output target_name)
        if(TARGET ${target_name})
            set_target_properties(${target_name} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
                LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
                ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
        endif()
    endfunction()

    _artifact_align_llama_output(llama)
    _artifact_align_llama_output(ggml)
    _artifact_align_llama_output(ggml-base)
    _artifact_align_llama_output(ggml-cpu)
    _artifact_align_llama_output(ggml-metal)
    _artifact_align_llama_output(ggml-cuda)

    foreach(_artifact_llama_target IN ITEMS llama ggml ggml-base ggml-cpu ggml-metal ggml-cuda)
        if(TARGET ${_artifact_llama_target})
            set_target_properties(${_artifact_llama_target} PROPERTIES
                VS_GLOBAL_VcpkgApplocalDeps false)
        endif()
    endforeach()
endif()

if(NOT TARGET llama)
    find_package(llama REQUIRED CONFIG)
endif()

target_link_libraries(ArtifactCore PUBLIC llama)

# ufbx (vendored)
if(TARGET ufbx)
    target_link_libraries(ArtifactCore PUBLIC ufbx)
endif()

# FFmpeg - Direct linking with explicit library paths
# These paths are set by the vcpkg integration and passed to CMake
# The libraries exist in: out/vcpkg_installed/x64-windows/lib and debug/lib

# Get the vcpkg installed directory from the toolchain file
# Usually: out/vcpkg_installed
if(NOT DEFINED VCPKG_INSTALLED_DIR)
    # Try to infer from CMAKE_TOOLCHAIN_FILE
    get_filename_component(_vcpkg_root_guess "${CMAKE_CURRENT_LIST_DIR}/../out/vcpkg_installed" ABSOLUTE)
    if(EXISTS "${_vcpkg_root_guess}")
        set(VCPKG_INSTALLED_DIR "${_vcpkg_root_guess}")
    else()
        # Fallback: use VCPKG_ROOT
        set(VCPKG_INSTALLED_DIR "${VCPKG_ROOT}/installed" CACHE PATH "vcpkg installed directory")
    endif()
endif()

message(STATUS "Using vcpkg directory: ${VCPKG_INSTALLED_DIR}")

# Determine the vcpkg triplet (x64-windows for MSVC on x64)
set(_vcpkg_triplet "x64-windows")

# Set lib and include directories
set(_vcpkg_lib_dir "${VCPKG_INSTALLED_DIR}/${_vcpkg_triplet}/lib")
set(_vcpkg_debug_lib_dir "${VCPKG_INSTALLED_DIR}/${_vcpkg_triplet}/debug/lib")
set(_vcpkg_include_dir "${VCPKG_INSTALLED_DIR}/${_vcpkg_triplet}/include")

message(STATUS "FFmpeg library dir: ${_vcpkg_lib_dir}")
message(STATUS "FFmpeg debug lib dir: ${_vcpkg_debug_lib_dir}")

# Link the FFmpeg libraries directly by full path.  Some vcpkg installations
# provide only the release import libraries, even for a Debug application.
# Do not emit references to non-existent debug libraries: Ninja/MSVC will
# otherwise compile ArtifactCore successfully and fail at the final link.
set(_artifact_ffmpeg_lib_names avcodec avformat avutil swscale swresample)
set(_artifact_ffmpeg_debug_libs_available TRUE)
foreach(_artifact_ffmpeg_lib_name IN LISTS _artifact_ffmpeg_lib_names)
    if(NOT EXISTS "${_vcpkg_debug_lib_dir}/${_artifact_ffmpeg_lib_name}.lib")
        set(_artifact_ffmpeg_debug_libs_available FALSE)
    endif()
endforeach()

if(_artifact_ffmpeg_debug_libs_available)
    target_link_libraries(ArtifactCore PUBLIC
        $<$<CONFIG:Debug>:${_vcpkg_debug_lib_dir}/avcodec.lib>
        $<$<CONFIG:Release>:${_vcpkg_lib_dir}/avcodec.lib>
        $<$<CONFIG:Debug>:${_vcpkg_debug_lib_dir}/avformat.lib>
        $<$<CONFIG:Release>:${_vcpkg_lib_dir}/avformat.lib>
        $<$<CONFIG:Debug>:${_vcpkg_debug_lib_dir}/avutil.lib>
        $<$<CONFIG:Release>:${_vcpkg_lib_dir}/avutil.lib>
        $<$<CONFIG:Debug>:${_vcpkg_debug_lib_dir}/swscale.lib>
        $<$<CONFIG:Release>:${_vcpkg_lib_dir}/swscale.lib>
        $<$<CONFIG:Debug>:${_vcpkg_debug_lib_dir}/swresample.lib>
        $<$<CONFIG:Release>:${_vcpkg_lib_dir}/swresample.lib>
    )
else()
    target_link_libraries(ArtifactCore PUBLIC
        "${_vcpkg_lib_dir}/avcodec.lib"
        "${_vcpkg_lib_dir}/avformat.lib"
        "${_vcpkg_lib_dir}/avutil.lib"
        "${_vcpkg_lib_dir}/swscale.lib"
        "${_vcpkg_lib_dir}/swresample.lib"
    )
    message(STATUS "FFmpeg debug import libraries not found; using release import libraries for all configurations")
endif()

# Add FFmpeg include directory
target_include_directories(ArtifactCore PUBLIC ${_vcpkg_include_dir})

message(STATUS "FFmpeg libraries linked: avcodec, avformat, avutil, swscale, swresample")

if(ARTIFACT_FRONTEND_MSVC)
    target_link_libraries(ArtifactCore PUBLIC
        mfplat
        mfreadwrite
        mfuuid
        mf
        winmm
    )
    target_compile_definitions(ArtifactCore PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

# Python scripting support (pybind11 embedded interpreter)
find_package(Python3 QUIET COMPONENTS Interpreter Development)
if(Python3_FOUND)
    find_package(pybind11 QUIET)
    if(pybind11_FOUND)
        target_link_libraries(ArtifactCore PUBLIC pybind11::headers Python3::Python)
        target_compile_definitions(ArtifactCore PRIVATE ARTIFACT_HAS_PYTHON=1)
        message(STATUS "Python scripting enabled: pybind11 + Python ${Python3_VERSION}")
    endif()
endif()

# AngelScript scripting support (Unity-MonoBehaviour-style layer components).
# Provided by vcpkg (angelscript[addons]). When the package is found, the
# ARTIFACT_HAS_ANGELSCRIPT macro enables the real implementation; otherwise
# AngelScriptEngine compiles as a no-op stub.
find_package(Angelscript CONFIG QUIET)
if(Angelscript_FOUND)
    target_link_libraries(ArtifactCore PUBLIC Angelscript::angelscript)
    target_compile_definitions(ArtifactCore PRIVATE ARTIFACT_HAS_ANGELSCRIPT=1)
    message(STATUS "AngelScript scripting enabled: Angelscript ${Angelscript_VERSION}")
else()
    message(STATUS "AngelScript not found (AngelScriptEngine will be a stub)")
endif()

# C# scripting support (hostfxr, dynamically loaded).
# CSharpScriptEngine keeps the hostfxr ABI declarations local, so no SDK
# headers or link library are required.  Enable the embedded path only when
# the .NET installation contains host/fxr; otherwise the external-process
# fallback remains available.
find_program(DOTNET_EXECUTABLE NAMES dotnet dotnet.exe)
if(DOTNET_EXECUTABLE)
    get_filename_component(_artifact_dotnet_root "${DOTNET_EXECUTABLE}" DIRECTORY)
    if(EXISTS "${_artifact_dotnet_root}/host/fxr")
        target_compile_definitions(ArtifactCore PRIVATE ARTIFACT_HAS_DOTNET=1)
        message(STATUS "C# scripting enabled: hostfxr found under ${_artifact_dotnet_root}")
    else()
        message(STATUS "C# scripting disabled: .NET found but host/fxr is missing")
    endif()
else()
    message(STATUS "C# scripting disabled: dotnet executable not found")
endif()

# Keep the large Audio domain in its own static library.  It links the base
# Core target because Audio uses foundational Core modules; the two direct
# Audio consumers moved above prevent a reverse module edge from Core back into
# this target.
add_library(ArtifactCoreAudio STATIC)
set_target_properties(ArtifactCoreAudio PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreAudio PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreAudio PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreAudio
    PRIVATE ${ARTIFACTCORE_AUDIO_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_AUDIO_MODULES}
)
target_link_libraries(ArtifactCoreAudio PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreAudio PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreAudio PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreAudio PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCoreAI STATIC)
set_target_properties(ArtifactCoreAI PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreAI PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreAI PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreAI
    PRIVATE ${ARTIFACTCORE_AI_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_AI_MODULES}
)
target_link_libraries(ArtifactCoreAI PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreAI PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreAI PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreAI PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCoreVideo STATIC)
set_target_properties(ArtifactCoreVideo PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreVideo PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreVideo PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreVideo
    PRIVATE ${ARTIFACTCORE_VIDEO_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_VIDEO_MODULES}
)
target_link_libraries(ArtifactCoreVideo PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreVideo PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreVideo PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreVideo PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCoreMedia STATIC)
set_target_properties(ArtifactCoreMedia PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreMedia PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreMedia PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreMedia
    PRIVATE ${ARTIFACTCORE_MEDIA_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_MEDIA_MODULES}
)
target_link_libraries(ArtifactCoreMedia PUBLIC ArtifactCore ArtifactCoreVideo ArtifactCoreThread)
target_include_directories(ArtifactCoreMedia PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreMedia PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreMedia PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCoreComposition STATIC)
set_target_properties(ArtifactCoreComposition PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreComposition PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreComposition PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreComposition
    PRIVATE ${ARTIFACTCORE_COMPOSITION_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_COMPOSITION_MODULES}
)
target_link_libraries(ArtifactCoreComposition PUBLIC ArtifactCore ArtifactCoreMedia)
target_include_directories(ArtifactCoreComposition PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreComposition PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreComposition PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCoreUI STATIC)
set_target_properties(ArtifactCoreUI PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreUI PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreUI PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreUI
    PRIVATE ${ARTIFACTCORE_UI_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_UI_MODULES}
)
add_dependencies(ArtifactCoreUI ArtifactCoreMask ArtifactCoreEvent)
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/SelectionManager.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Utils.Id=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Id.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/InputOperator.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;InputEvent=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreUI.dir/InputEvent.ifc"
        "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/InputOperator.cppm"
    APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreUI.dir/include/UI/InputEvent.ixx.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreUI.dir/include/UI/InputOperator.ixx.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/src/Memory/SharedPtr.cppm.obj")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/InputOperatorManager.cppm"
    PROPERTY COMPILE_OPTIONS
        "/reference;Input.Surface.Manager=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreUI.dir/Input.Surface.Manager.ifc"
        "/reference;Input.Operator=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreUI.dir/Input.Operator.ifc"
        "/reference;InputEvent=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreUI.dir/InputEvent.ifc"
        "/reference;Event.Bus=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEvent.dir/Event.Bus.ifc"
        "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/InputOperatorManager.cppm"
    APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreUI.dir/include/UI/InputOperatorManager.ixx.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreUI.dir/include/UI/InputOperator.ixx.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreUI.dir/include/UI/InputEvent.ixx.obj")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Rig/Rig2D.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Animation.Value=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Animation.Value.ifc"
        "/reference;Frame.Position=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Position.ifc"
        "/reference;Frame.Rate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc"
        "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc"
        "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
        "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc"
        "/reference;Utils.Id=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Id.ifc"
        "/reference;Math.Interpolate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Interpolate.ifc"
        "/reference;Serialization.JsonAdapter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.JsonAdapter.ifc"
        "/reference;Serialization.SchemaMigration=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.SchemaMigration.ifc"
        "/reference;Serialization.ISerializable=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.ISerializable.ifc"
        "/reference;Serialization.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.Registry.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Rig/Rig2D.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/ColorCollection/ColorGrading.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Color.ColorSpace=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.ColorSpace.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/ObjectDetector.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Core.AI.Describable=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAI.dir/Core.AI.Describable.ifc"
        "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
        "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
        "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
        "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
        "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
        "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc"
        "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc")
foreach(_artifact_ai_impl IN ITEMS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/TieredAIManager.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/OnnxDmlLocalAgent.cppm")
    set_property(SOURCE "${_artifact_ai_impl}" APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Core.AI.CloudAgent=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAI.dir/Core.AI.CloudAgent.ifc"
        "/reference;Core.AI.Context=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAI.dir/Core.AI.Context.ifc"
        "/reference;Core.AI.LocalAgent=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAI.dir/Core.AI.LocalAgent.ifc"
        "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
        "/reference;Memory.TrackedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.TrackedPtr.ifc"
        "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc")
endforeach()
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Time/TimeRemap.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Analyze.OpticalFlow=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAnalyze.dir/Analyze.OpticalFlow.ifc"
        "/reference;Frame.Position=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Position.ifc"
        "/reference;Frame.Rate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Rate.ifc"
        "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Time/TimeRemap.cppm"
    APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreAnalyze.dir/include/Analyze/OpenCV/OptifalFlowResult.ixx.obj")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Analyze/SmartPaletteAnalyzer.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Color.Float=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.Float.ifc"
        "/reference;Image.Raw=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.Raw.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Analyze/Histgram.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Tracking/MotionTracker.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
        "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Time/TimeRemap.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Control/MidiInput.cppm" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/IPC/RenderFarmSharedBuffer.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;IPC.SharedMemoryRingBuffer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreIPC.dir/IPC.SharedMemoryRingBuffer.ifc"
        "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
        "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
        "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
        "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
        "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
        "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/IPC/RenderFarmSharedBuffer.cppm"
    APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreIPC.dir/include/IPC/SharedMemoryRingBuffer.ixx.obj")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/IPC/IPCChannel.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;IPC.SharedMemoryRingBuffer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreIPC.dir/IPC.SharedMemoryRingBuffer.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/IPC/IPCChannel.cppm"
    APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreIPC.dir/include/IPC/SharedMemoryRingBuffer.ixx.obj")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Plugin/PluginCommon.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Plugin/PluginRegistry.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;ArtifactCore.Plugin.Common=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCorePlugin.dir/ArtifactCore.Plugin.Common.ifc"
        "/reference;Utils.Optional=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Optional.ifc"
        "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Export/Lottie/LottieExporter.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Export.Lottie.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreExport.dir/Export.Lottie.Types.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Export/Lottie/LottieRigExporter.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Export.Lottie.Exporter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Export.Lottie.Exporter.ifc"
        "/reference;ArtifactCore.Rig2D=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreRig.dir/ArtifactCore.Rig2D.ifc"
        "/reference;Export.Lottie.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreExport.dir/Export.Lottie.Types.ifc"
        "/reference;Utils.Id=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Id.ifc"
        "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
        "/reference;Time.Rational=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Time.Rational.ifc"
        "/reference;Frame.Position=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Frame.Position.ifc"
        "/reference;Math.Interpolate=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Interpolate.ifc"
        "/reference;Animation.Value=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Animation.Value.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Export/Lottie/LottieExporter.cppm"
    APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreExport.dir/include/Export/Lottie/LottieTypes.ixx.obj")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Export/Lottie/LottieRigExporter.cppm"
    APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreExport.dir/include/Export/Lottie/LottieTypes.ixx.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreRig.dir/include/Rig/Rig2D.ixx.obj")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/LayoutState.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Core.FastSettingsStore=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.FastSettingsStore.ifc"
        "/reference;Serialization.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.Registry.ifc"
        "/reference;Serialization.SchemaMigration=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.SchemaMigration.ifc"
        "/reference;Serialization.ISerializable=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.ISerializable.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Control/MidiInput.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
        "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
        "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Light/IESProfile.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
        "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
        "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/EnvironmentVariable/EnvironmentVariable.cppm"
    PROPERTY COMPILE_OPTIONS
        "/reference;EnvironmentVariable=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEnvironment.dir/EnvironmentVariable.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/EnvironmentVariable/EnvironmentVariable.cppm"
    APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreEnvironment.dir/include/EnvironmentVariable/EnvironmentVariable.ixx.obj")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/RenderFarmMaster.cppm"
    APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreNetwork.dir/NetworkRPCServer.ixx.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Script/Expression/ExpressionParser.cppm"
    APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebug.ixx.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Script/Expression/ExpressionValue.cppm"
    APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebug.ixx.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Render/RendererQueueManager.cppm"
    APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Render/MFR/MFRJob.ixx.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Render/MFR/MFRDispatcher.ixx.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Generate/GenerateTestImage.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Generate/StarfieldGenerator.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc"
        "/reference;Math.Random=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Math.Random.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/IO/VectorExport.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Shape.Layer=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreShape.dir/Shape.Layer.ifc"
        "/reference;Property.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Property.Types.ifc"
        "/reference;Shape.Group=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreShape.dir/Shape.Group.ifc"
        "/reference;Shape.Path=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreShape.dir/Shape.Path.ifc"
        "/reference;Shape.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreShape.dir/Shape.Types.ifc"
        "/reference;Shape.Operator=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreShape.dir/Shape.Operator.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Database/Database.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Core.Database.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Database.Types.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Simulation/OpenVDBVolumeReference.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Core.Simulation.Pyro=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Simulation.Pyro.ifc"
        "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Simulation/PyroSimulation.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Core.Simulation.Pyro=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Simulation.Pyro.ifc"
        "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
        "/reference;Float3=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Float3.ifc"
        "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.Parallel.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/VST3/VST3Loader.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Track/NccTracker.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Image.ImageF32x4_RGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageF32x4_RGBA.ifc"
        "/reference;ImageInterface=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/ImageInterface.ifc"
        "/reference;FloatRGBA=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/FloatRGBA.ifc"
        "/reference;Image.ImageSurfaceView=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Image.ImageSurfaceView.ifc"
        "/reference;Graphics.SurfaceColorContract=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Graphics.SurfaceColorContract.ifc"
        "/reference;Color.TransferFunction=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Color.TransferFunction.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Coordinate/CoordinateProfile.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Serialization.JsonAdapter=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.JsonAdapter.ifc"
        "/reference;Serialization.SchemaMigration=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.SchemaMigration.ifc"
        "/reference;Serialization.ISerializable=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.ISerializable.ifc"
        "/reference;Serialization.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Serialization.Registry.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Source/ISource.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Memory.SharedPtr=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Memory.SharedPtr.ifc"
        "/reference;Utils.Id=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.Id.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Localization/Localization.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Core.ArtifactString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Core.ArtifactString.ifc"
        "/reference;Utils.String.UniString=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Utils.String.UniString.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/RotoMaskEditor.cppm"
    APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Core.Mask.RotoMask=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreMask.dir/Core.Mask.RotoMask.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/ShortcutBindings.cppm"
    PROPERTY COMPILE_OPTIONS
        "/reference;UI.ShortcutBindings=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreUI.dir/UI.ShortcutBindings.ifc")
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/ShortcutBindings.cppm"
    APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreUI.dir/include/UI/ShortcutBindings.ixx.obj")
target_link_libraries(ArtifactCoreUI PUBLIC ArtifactCore ArtifactCoreCommand)
target_include_directories(ArtifactCoreUI PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreUI PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreUI PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCoreData STATIC)
set_target_properties(ArtifactCoreData PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreData PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreData PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreData
    PRIVATE ${ARTIFACTCORE_DATA_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_DATA_MODULES}
)
target_link_libraries(ArtifactCoreData PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreData PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreData PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreData PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCoreCommand STATIC)
set_target_properties(ArtifactCoreCommand PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreCommand PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreCommand PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreCommand
    PRIVATE ${ARTIFACTCORE_COMMAND_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_COMMAND_MODULES}
)
target_link_libraries(ArtifactCoreCommand PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreCommand PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreCommand PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreCommand PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCoreAsset STATIC)
set_target_properties(ArtifactCoreAsset PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
# import std is provided by the explicit MSVC std.ifc producer configured at
# the top level; avoid CMake's second standard-module reference on this target.
set_property(TARGET ArtifactCoreAsset PROPERTY CXX_MODULE_STD OFF)
target_sources(ArtifactCoreAsset
    PRIVATE ${ARTIFACTCORE_ASSET_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_ASSET_MODULES}
)
target_link_libraries(ArtifactCoreAsset PUBLIC ArtifactCore ArtifactCoreData)
target_include_directories(ArtifactCoreAsset PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreAsset PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreAsset PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCoreAcoustic STATIC)
set_target_properties(ArtifactCoreAcoustic PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreAcoustic PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreAcoustic PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreAcoustic
    PRIVATE ${ARTIFACTCORE_ACOUSTIC_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_ACOUSTIC_MODULES}
)
target_link_libraries(ArtifactCoreAcoustic PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreAcoustic PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreAcoustic PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreAcoustic PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCoreShape STATIC)
set_target_properties(ArtifactCoreShape PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreShape PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreShape PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreShape
    PRIVATE ${ARTIFACTCORE_SHAPE_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_SHAPE_MODULES}
)
target_link_libraries(ArtifactCoreShape PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreShape PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreShape PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreShape PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCorePlatform STATIC)
set_target_properties(ArtifactCorePlatform PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCorePlatform PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCorePlatform PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCorePlatform
    PRIVATE ${ARTIFACTCORE_PLATFORM_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_PLATFORM_MODULES}
)
target_link_libraries(ArtifactCorePlatform PUBLIC ArtifactCore)
target_include_directories(ArtifactCorePlatform PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCorePlatform PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCorePlatform PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCoreThread STATIC)
set_target_properties(ArtifactCoreThread PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
# The explicit MSVC std.ifc reference is supplied globally above.  Do not also
# enable CMake's experimental std module target for split targets, otherwise
# MSVC sees two IFCs for module `std`.
if(FALSE)
    set_property(TARGET ArtifactCoreThread PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreThread PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreThread
    PRIVATE ${ARTIFACTCORE_THREAD_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_THREAD_MODULES}
)
target_link_libraries(ArtifactCoreThread PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreThread PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreThread PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreThread PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCoreAnalyze STATIC)
set_target_properties(ArtifactCoreAnalyze PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreAnalyze PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreAnalyze PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreAnalyze
    PRIVATE ${ARTIFACTCORE_ANALYZE_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_ANALYZE_MODULES}
)
target_link_libraries(ArtifactCoreAnalyze PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreAnalyze PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreAnalyze PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreAnalyze PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCoreTracking STATIC)
set_target_properties(ArtifactCoreTracking PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreTracking PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreTracking PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreTracking
    PRIVATE ${ARTIFACTCORE_TRACKING_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_TRACKING_MODULES}
)
target_link_libraries(ArtifactCoreTracking PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreTracking PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreTracking PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreTracking PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCoreIPC STATIC)
set_target_properties(ArtifactCoreIPC PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreIPC PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreIPC PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreIPC
    PRIVATE ${ARTIFACTCORE_IPC_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_IPC_MODULES}
)
target_link_libraries(ArtifactCoreIPC PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreIPC PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreIPC PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreIPC PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCoreNLE STATIC)
set_target_properties(ArtifactCoreNLE PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreNLE PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreNLE PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreNLE
    PRIVATE ${ARTIFACTCORE_NLE_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_NLE_MODULES}
)
target_link_libraries(ArtifactCoreNLE PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreNLE PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreNLE PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreNLE PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCorePlayback STATIC)
set_target_properties(ArtifactCorePlayback PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCorePlayback PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCorePlayback PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCorePlayback
    PRIVATE ${ARTIFACTCORE_PLAYBACK_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_PLAYBACK_MODULES}
)
target_link_libraries(ArtifactCorePlayback PUBLIC ArtifactCore)
target_include_directories(ArtifactCorePlayback PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCorePlayback PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCorePlayback PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCorePreview STATIC)
set_target_properties(ArtifactCorePreview PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCorePreview PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCorePreview PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCorePreview
    PRIVATE ${ARTIFACTCORE_PREVIEW_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_PREVIEW_MODULES}
)
target_link_libraries(ArtifactCorePreview PUBLIC ArtifactCore)
target_include_directories(ArtifactCorePreview PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCorePreview PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCorePreview PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

target_link_libraries(ArtifactCoreVideo PUBLIC ArtifactCoreNLE)
target_link_libraries(ArtifactCoreMedia PUBLIC ArtifactCorePlayback)
add_library(ArtifactCoreExport STATIC)
set_target_properties(ArtifactCoreExport PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreExport PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreExport PROPERTY CXX_MODULE_STD OFF)
endif()

add_library(ArtifactCoreEvent STATIC)
set_target_properties(ArtifactCoreEvent PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreEvent PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreEvent PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreEvent
    PRIVATE ${ARTIFACTCORE_EVENT_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_EVENT_MODULES}
)
target_link_libraries(ArtifactCoreEvent PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreEvent PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreEvent PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreEvent PRIVATE PLATFORM_WIN32 _GUARDOVERFLOW_CRT_ALLOCATORS=1 _WIN32_WINNT=0x0A00 WINVER=0x0A00 _UNICODE UNICODE GLOG_USE_GLOG_EXPORT)
endif()

add_library(ArtifactCoreFile STATIC)
set_target_properties(ArtifactCoreFile PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreFile PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreFile PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreFile
    PRIVATE ${ARTIFACTCORE_FILE_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_FILE_MODULES}
)
target_link_libraries(ArtifactCoreFile PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreFile PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreFile PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreFile PRIVATE PLATFORM_WIN32 _GUARDOVERFLOW_CRT_ALLOCATORS=1 _WIN32_WINNT=0x0A00 WINVER=0x0A00 _UNICODE UNICODE GLOG_USE_GLOG_EXPORT)
endif()

add_library(ArtifactCorePlugin STATIC)
set_target_properties(ArtifactCorePlugin PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCorePlugin PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCorePlugin PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCorePlugin
    PRIVATE ${ARTIFACTCORE_PLUGIN_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_PLUGIN_MODULES}
)
target_link_libraries(ArtifactCorePlugin PUBLIC ArtifactCore)
target_include_directories(ArtifactCorePlugin PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCorePlugin PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCorePlugin PRIVATE PLATFORM_WIN32 _GUARDOVERFLOW_CRT_ALLOCATORS=1 _WIN32_WINNT=0x0A00 WINVER=0x0A00 _UNICODE UNICODE GLOG_USE_GLOG_EXPORT)
endif()

add_library(ArtifactCoreControl STATIC)
set_target_properties(ArtifactCoreControl PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreControl PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreControl PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreControl
    PRIVATE ${ARTIFACTCORE_CONTROL_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_CONTROL_MODULES}
)
target_link_libraries(ArtifactCoreControl PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreControl PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreControl PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreControl PRIVATE PLATFORM_WIN32 _GUARDOVERFLOW_CRT_ALLOCATORS=1 _WIN32_WINNT=0x0A00 WINVER=0x0A00 _UNICODE UNICODE GLOG_USE_GLOG_EXPORT)
endif()

add_library(ArtifactCoreDatabase STATIC)
set_target_properties(ArtifactCoreDatabase PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreDatabase PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreDatabase PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreDatabase
    PRIVATE ${ARTIFACTCORE_DATABASE_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_DATABASE_MODULES}
)
target_link_libraries(ArtifactCoreDatabase PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreDatabase PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreDatabase PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreDatabase PRIVATE PLATFORM_WIN32 _GUARDOVERFLOW_CRT_ALLOCATORS=1 _WIN32_WINNT=0x0A00 WINVER=0x0A00 _UNICODE UNICODE GLOG_USE_GLOG_EXPORT)
endif()

target_link_libraries(ArtifactCoreUI PUBLIC ArtifactCoreEvent)
target_link_libraries(ArtifactCorePlayback PUBLIC ArtifactCoreEvent)
target_link_libraries(ArtifactCoreAsset PUBLIC ArtifactCoreFile)

add_library(ArtifactCoreMask STATIC)
set_target_properties(ArtifactCoreMask PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreMask PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreMask PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreMask
    PRIVATE ${ARTIFACTCORE_MASK_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_MASK_MODULES}
)
target_link_libraries(ArtifactCoreMask PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreMask PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreMask PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreMask PRIVATE PLATFORM_WIN32 _GUARDOVERFLOW_CRT_ALLOCATORS=1 _WIN32_WINNT=0x0A00 WINVER=0x0A00 _UNICODE UNICODE GLOG_USE_GLOG_EXPORT)
endif()

add_library(ArtifactCoreConfiguration STATIC)
set_target_properties(ArtifactCoreConfiguration PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreConfiguration PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreConfiguration PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreConfiguration
    PRIVATE ${ARTIFACTCORE_CONFIGURATION_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_CONFIGURATION_MODULES}
)
target_link_libraries(ArtifactCoreConfiguration PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreConfiguration PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreConfiguration PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreConfiguration PRIVATE PLATFORM_WIN32 _GUARDOVERFLOW_CRT_ALLOCATORS=1 _WIN32_WINNT=0x0A00 WINVER=0x0A00 _UNICODE UNICODE GLOG_USE_GLOG_EXPORT)
endif()
target_sources(ArtifactCoreExport
    PRIVATE ${ARTIFACTCORE_EXPORT_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_EXPORT_MODULES}
)
target_link_libraries(ArtifactCoreExport PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreExport PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreExport PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreExport PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

target_link_libraries(ArtifactCoreUI PUBLIC ArtifactCoreMask)
target_link_libraries(ArtifactCoreAI PUBLIC ArtifactCoreConfiguration)
target_link_libraries(ArtifactCoreAsset PUBLIC ArtifactCoreConfiguration)

add_library(ArtifactCoreText STATIC)
set_target_properties(ArtifactCoreText PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
# The project supplies one explicit MSVC std.ifc globally; avoid CMake's
# experimental second std provider for the split text target.
if(FALSE)
    set_property(TARGET ArtifactCoreText PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreText PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreText
    PRIVATE ${ARTIFACTCORE_TEXT_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_TEXT_MODULES}
)
target_link_libraries(ArtifactCoreText PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreText PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreText PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreText PRIVATE PLATFORM_WIN32 _GUARDOVERFLOW_CRT_ALLOCATORS=1 _WIN32_WINNT=0x0A00 WINVER=0x0A00 _UNICODE UNICODE GLOG_USE_GLOG_EXPORT)
endif()

add_library(ArtifactCoreScene STATIC)
set_target_properties(ArtifactCoreScene PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
set_property(TARGET ArtifactCoreScene PROPERTY CXX_MODULE_STD OFF)
target_sources(ArtifactCoreScene PRIVATE ${ARTIFACTCORE_SCENE_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_SCENE_MODULES})
target_link_libraries(ArtifactCoreScene PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreScene PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreScene PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
endif()

add_library(ArtifactCoreRig STATIC)
set_target_properties(ArtifactCoreRig PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreRig PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreRig PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreRig PRIVATE ${ARTIFACTCORE_RIG_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_RIG_MODULES})
target_link_libraries(ArtifactCoreRig PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreRig PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreRig PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
endif()

add_library(ArtifactCoreGrid STATIC)
set_target_properties(ArtifactCoreGrid PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreGrid PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreGrid PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreGrid PRIVATE ${ARTIFACTCORE_GRID_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_GRID_MODULES})
target_link_libraries(ArtifactCoreGrid PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreGrid PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreGrid PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
endif()

add_library(ArtifactCoreColorCollection STATIC)
set_target_properties(ArtifactCoreColorCollection PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreColorCollection PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreColorCollection PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreColorCollection PRIVATE ${ARTIFACTCORE_COLORCOLLECTION_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_COLORCOLLECTION_MODULES})
target_link_libraries(ArtifactCoreColorCollection PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreColorCollection PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreColorCollection PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
endif()

add_library(ArtifactCoreSound STATIC)
set_target_properties(ArtifactCoreSound PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreSound PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreSound PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreSound PRIVATE ${ARTIFACTCORE_SOUND_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_SOUND_MODULES})
target_link_libraries(ArtifactCoreSound PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreSound PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")

add_library(ArtifactCoreSequence STATIC)
set_target_properties(ArtifactCoreSequence PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreSequence PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreSequence PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreSequence PRIVATE ${ARTIFACTCORE_SEQUENCE_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_SEQUENCE_MODULES})
target_link_libraries(ArtifactCoreSequence PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreSequence PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")

target_link_libraries(ArtifactCoreComposition PUBLIC ArtifactCoreScene)
target_link_libraries(ArtifactCoreExport PUBLIC ArtifactCoreRig)
target_link_libraries(ArtifactCoreConfiguration PUBLIC ArtifactCoreGrid)

add_library(ArtifactCoreMaterial STATIC)
set_target_properties(ArtifactCoreMaterial PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
# Use the project's explicit std.ifc provider for the material target.
if(FALSE)
    set_property(TARGET ArtifactCoreMaterial PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreMaterial PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreMaterial PRIVATE ${ARTIFACTCORE_MATERIAL_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_MATERIAL_MODULES})
target_link_libraries(ArtifactCoreMaterial PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreMaterial PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")

add_library(ArtifactCoreEnvironment STATIC)
set_target_properties(ArtifactCoreEnvironment PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreEnvironment PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreEnvironment PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreEnvironment PRIVATE ${ARTIFACTCORE_ENVIRONMENT_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_ENVIRONMENT_MODULES})
target_link_libraries(ArtifactCoreEnvironment PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreEnvironment PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")

add_library(ArtifactCoreLight STATIC)
set_target_properties(ArtifactCoreLight PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreLight PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreLight PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreLight PRIVATE ${ARTIFACTCORE_LIGHT_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_LIGHT_MODULES})
target_link_libraries(ArtifactCoreLight PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreLight PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")

add_library(ArtifactCoreCrowd STATIC)
set_target_properties(ArtifactCoreCrowd PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreCrowd PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreCrowd PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreCrowd PRIVATE ${ARTIFACTCORE_CROWD_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_CROWD_MODULES})
target_link_libraries(ArtifactCoreCrowd PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreCrowd PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")

add_library(ArtifactCoreDomain STATIC)
set_target_properties(ArtifactCoreDomain PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
target_sources(ArtifactCoreDomain PRIVATE ${ARTIFACTCORE_DOMAIN_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_DOMAIN_MODULES})
target_link_libraries(ArtifactCoreDomain PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreDomain PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")

add_library(ArtifactCoreFileSystem STATIC)
set_target_properties(ArtifactCoreFileSystem PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
target_sources(ArtifactCoreFileSystem PRIVATE ${ARTIFACTCORE_FILESYSTEM_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_FILESYSTEM_MODULES})
target_link_libraries(ArtifactCoreFileSystem PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreFileSystem PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")

add_library(ArtifactCoreIcon STATIC)
set_target_properties(ArtifactCoreIcon PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
target_sources(ArtifactCoreIcon PRIVATE ${ARTIFACTCORE_ICON_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_ICON_MODULES})
target_link_libraries(ArtifactCoreIcon PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreIcon PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")

add_library(ArtifactCoreVST STATIC)
set_target_properties(ArtifactCoreVST PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
target_sources(ArtifactCoreVST PRIVATE ${ARTIFACTCORE_VST_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_VST_MODULES})
target_link_libraries(ArtifactCoreVST PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreVST PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")


add_library(ArtifactCoreNetwork STATIC)
set_target_properties(ArtifactCoreNetwork PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreNetwork PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreNetwork PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreNetwork PRIVATE ${ARTIFACTCORE_NETWORK_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_NETWORK_MODULES})
target_link_libraries(ArtifactCoreNetwork PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreNetwork PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")

add_library(ArtifactCoreCollaborate STATIC)
set_target_properties(ArtifactCoreCollaborate PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreCollaborate PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreCollaborate PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreCollaborate PRIVATE ${ARTIFACTCORE_COLLABORATE_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_COLLABORATE_MODULES})
target_link_libraries(ArtifactCoreCollaborate PUBLIC ArtifactCore ArtifactCoreNetwork)
target_include_directories(ArtifactCoreCollaborate PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")

add_library(ArtifactCoreGenerate STATIC)
set_target_properties(ArtifactCoreGenerate PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreGenerate PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreGenerate PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreGenerate PRIVATE ${ARTIFACTCORE_GENERATE_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_GENERATE_MODULES})
target_link_libraries(ArtifactCoreGenerate PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreGenerate PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreGenerate PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
endif()

add_library(ArtifactCoreSimulation STATIC)
set_target_properties(ArtifactCoreSimulation PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreSimulation PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreSimulation PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreSimulation PRIVATE ${ARTIFACTCORE_SIMULATION_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_SIMULATION_MODULES})
target_link_libraries(ArtifactCoreSimulation PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreSimulation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreSimulation PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
endif()

add_library(ArtifactCoreTrack STATIC)
set_target_properties(ArtifactCoreTrack PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreTrack PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreTrack PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreTrack PRIVATE ${ARTIFACTCORE_TRACK_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_TRACK_MODULES})
target_link_libraries(ArtifactCoreTrack PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreTrack PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreTrack PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
endif()

add_library(ArtifactCoreSource STATIC)
set_target_properties(ArtifactCoreSource PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreSource PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreSource PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreSource PRIVATE ${ARTIFACTCORE_SOURCE_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_SOURCE_MODULES})
target_link_libraries(ArtifactCoreSource PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreSource PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreSource PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
endif()

add_library(ArtifactCoreProject STATIC)
set_target_properties(ArtifactCoreProject PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF FOLDER "ArtifactCore/Libraries")
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreProject PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreProject PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreProject PRIVATE ${ARTIFACTCORE_PROJECT_IMPL} PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_PROJECT_MODULES})
target_link_libraries(ArtifactCoreProject PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreProject PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreProject PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
endif()

add_library(ArtifactCoreVST3 STATIC)
set_target_properties(ArtifactCoreVST3 PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreVST3 PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreVST3 PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreVST3
    PRIVATE ${ARTIFACTCORE_VST3_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_VST3_MODULES}
)
target_link_libraries(ArtifactCoreVST3 PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreVST3 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreVST3 PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreVST3 PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCoreLocalization STATIC)
set_target_properties(ArtifactCoreLocalization PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreLocalization PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreLocalization PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreLocalization
    PRIVATE ${ARTIFACTCORE_LOCALIZATION_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_LOCALIZATION_MODULES}
)
target_link_libraries(ArtifactCoreLocalization PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreLocalization PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreLocalization PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreLocalization PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

add_library(ArtifactCoreCoordinate STATIC)
set_target_properties(ArtifactCoreCoordinate PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    FOLDER "ArtifactCore/Libraries"
)
if(TARGET __CMAKE::CXX23)
    set_property(TARGET ArtifactCoreCoordinate PROPERTY CXX_MODULE_STD ON)
else()
    set_property(TARGET ArtifactCoreCoordinate PROPERTY CXX_MODULE_STD OFF)
endif()
target_sources(ArtifactCoreCoordinate
    PRIVATE ${ARTIFACTCORE_COORDINATE_IMPL}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACTCORE_COORDINATE_MODULES}
)
target_link_libraries(ArtifactCoreCoordinate PUBLIC ArtifactCore)
target_include_directories(ArtifactCoreCoordinate PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party"
)
if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCoreCoordinate PRIVATE /WX- /wd5050 /std:c++latest /experimental:module /Zc:__cplusplus /permissive-)
    target_compile_definitions(ArtifactCoreCoordinate PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        _UNICODE
        UNICODE
        GLOG_USE_GLOG_EXPORT
    )
endif()

# Keep the cross-target interface reference attached after all split-target
# source property blocks; later assignments otherwise can replace it.
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/include/Text/GlyphLayout.ixx" APPEND PROPERTY COMPILE_OPTIONS
    "/reference;Shape.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreShape.dir/Shape.Types.ifc"
    "/reference;Property.Types=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Property.Types.ifc")

# These core implementations import NamedVector directly.  Keep their
# interface object dependency explicit so MSVC has the IFC before compiling
# the implementation unit.
foreach(_artifact_namedvector_consumer IN ITEMS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Diagnostics/ProjectDiagnostic.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Color/ColorLUT.cppm")
    set_property(SOURCE "${_artifact_namedvector_consumer}" APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Container.Debug.Registry=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.Registry.ifc")
    set_property(SOURCE "${_artifact_namedvector_consumer}" APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/NamedVector.ixx.obj")
endforeach()

# Fracture imports Math.Random from the aggregate core target.  Make the
# producing interface object explicit for MSVC's module build ordering.
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Geometry/Fracture.cppm"
    APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Math/Random.ixx.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/include/Container/ContainerDebugRegistry.ixx.obj")

foreach(_artifact_namedvector_impl IN ITEMS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/TieredAIManager.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/AI/ObjectDetector.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Analyze/Histgram.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Analyze/SmartPaletteAnalyzer.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Export/Lottie/LottieExporter.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Export/Lottie/LottieRigExporter.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Plugin/PluginRegistry.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Tracking/MotionTracker.cppm")
    set_property(SOURCE "${_artifact_namedvector_impl}" APPEND PROPERTY COMPILE_OPTIONS
        "/reference;Container.NamedVector=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.NamedVector.ifc"
        "/reference;Container.Debug=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCore.dir/Container.Debug.ifc")
endforeach()

# Video transition implementations import Video.TransitionFactory.  Keep the
# producing interface object ordered before those implementation units.
set(_artifact_video_transition_impls ${ARTIFACTCORE_VIDEO_IMPL})
list(FILTER _artifact_video_transition_impls INCLUDE REGEX
    "(^|[/\\\\])src/Video/Transitions/[^/\\\\]+\\.cppm$")
set_property(SOURCE ${_artifact_video_transition_impls} APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreVideo.dir/include/Video/TransitionFactory.ixx.obj")
