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

    # Check if there is any code before module_semi_idx
    has_code_before = False
    code_lines_indices = []
    
    in_block_comment = False
    for i in range(module_semi_idx):
        line = lines[i].strip()
        if not line:
            continue
        
        # This is a very basic comment check.
        if line.startswith('//'):
            continue
        
        # Check for block comments (basic)
        if '/*' in line:
            in_block_comment = True
        if '*/' in line:
            in_block_comment = False
            continue
        
        if not in_block_comment:
            has_code_before = True
            code_lines_indices.append(i)

    if not has_code_before:
        return False, "module ; is already the first statement"

    # Perform fix: 
    # Move all lines (including comments) that precede module ; to after it.
    # Wait, if there are license comments at the top, we might want to keep them at the top.
    # But the simplest approach that satisfies "module ; must be the very first statement" 
    # is to move module ; to the top.
    
    # Actually, "move any headers or other code that currently precedes 'module ;' to be AFTER 'module ;'".
    # Let's identify the first line of code.
    
    first_code_idx = -1
    in_block_comment = False
    for i in range(len(lines)):
        line = lines[i].strip()
        if not line: continue
        if line.startswith('//'): continue
        if '/*' in line:
            in_block_comment = True
        if '*/' in line:
            in_block_comment = False
            continue
        if not in_block_comment:
            first_code_idx = i
            break
    
    if first_code_idx == module_semi_idx:
        return False, "module ; is already the first statement (verified)"

    # We need to extract the 'module ;' line and put it before the first code line.
    module_line = lines.pop(module_semi_idx)
    lines.insert(first_code_idx, module_line)
    
    with open(file_path, 'w', encoding='latin-1') as f:
        f.writelines(lines)
    
    return True, "Fixed"

def main():
    root_dir = r"X:\dev\ArtifactStudio\ArtifactCore"
    fixed_files = []
    for root, dirs, files in os.walk(root_dir):
        for file in files:
            if file.endswith('.cppm') or file.endswith('.ixx'):
                full_path = os.path.join(root, file)
                try:
                    success, msg = fix_file(full_path)
                    if success:
                        fixed_files.append(full_path)
                        print(f"Fixed: {full_path}")
                except Exception as e:
                    print(f"Error processing {full_path}: {e}")
    
    print(f"\nTotal fixed: {len(fixed_files)}")

if __name__ == "__main__":
    main()
