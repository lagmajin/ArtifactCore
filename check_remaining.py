import os
import re

def fix_file(file_path):
    with open(file_path, 'r', encoding='latin-1') as f:
        lines = f.readlines()

    module_semi_idx = -1
    for i, line in enumerate(lines):
        if re.match(r'^\s*module\s*;\s*$', line):
            module_semi_idx = i
            break
    
    if module_semi_idx == -1:
        return False, "module ; not found"

    first_code_idx = -1
    in_block_comment = False
    for i in range(len(lines)):
        line = lines[i].strip()
        if not line: continue
        if line.startswith('//'): continue
        if '/*' in line:
            in_block_comment = True
        if '*/' in line:
            if in_block_comment:
                in_block_comment = False
                continue
            else:
                # Malformed? Skip
                pass
        if not in_block_comment:
            first_code_idx = i
            break
    
    if first_code_idx == module_semi_idx:
        return False, "Already first statement"

    if first_code_idx < module_semi_idx:
        # We need to move module ; to first_code_idx
        module_line = lines.pop(module_semi_idx)
        lines.insert(first_code_idx, module_line)
        
        with open(file_path, 'w', encoding='latin-1') as f:
            f.writelines(lines)
        return True, f"Fixed: moved from {module_semi_idx} to {first_code_idx}"

    return False, f"Not fixed: first_code_idx={first_code_idx}, module_semi_idx={module_semi_idx}"

def main():
    check_files = [
        r"X:\dev\ArtifactStudio\ArtifactCore\include\Composition\AbstractCompositionBuffer2D.ixx",
        r"X:\dev\ArtifactStudio\ArtifactCore\include\Graphics\Shader\ShaderCompileTask.ixx",
        r"X:\dev\ArtifactStudio\ArtifactCore\include\Graphics\TextureFactory.ixx",
        r"X:\dev\ArtifactStudio\ArtifactCore\include\Image\Transform\ImageTransformCV.ixx",
        r"X:\dev\ArtifactStudio\ArtifactCore\include\ImageProcessing\Halide\HalideTest.ixx",
        r"X:\dev\ArtifactStudio\ArtifactCore\include\ImageProcessing\OpenCV\Grading\BrightnessContrast.ixx",
        r"X:\dev\ArtifactStudio\ArtifactCore\include\ImageProcessing\OpenCV\BloomCV.ixx",
        r"X:\dev\ArtifactStudio\ArtifactCore\include\ImageProcessing\OpenCV\EdgeCV.ixx",
        r"X:\dev\ArtifactStudio\ArtifactCore\include\Layer\LayerStrip.ixx",
        r"X:\dev\ArtifactStudio\ArtifactCore\include\Script\Engine\Value\Value.ixx",
        r"X:\dev\ArtifactStudio\ArtifactCore\include\Video\AbstractEncoder.ixx",
        r"X:\dev\ArtifactStudio\ArtifactCore\include\Video\Stabilizer.ixx",
        r"X:\dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\Halide\MonochromeHalide.cppm",
        r"X:\dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\OpenCV\AddBlockNoise.cppm",
        r"X:\dev\ArtifactStudio\ArtifactCore\src\ImageProcessing\OpenCV\Monochrome.ixx",
        r"X:\dev\ArtifactStudio\ArtifactCore\src\Time\TimeCodeRange.cppm",
        r"X:\dev\ArtifactStudio\ArtifactCore\src\Video\Stabilizer.cppm"
    ]
    
    for f in check_files:
        if os.path.exists(f):
            success, msg = fix_file(f)
            print(f"{f}: {msg}")
        else:
            print(f"{f}: Not found")

if __name__ == "__main__":
    main()
