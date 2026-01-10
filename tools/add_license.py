import os

HEADER_C = """/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 */

"""

HEADER_PY = """# Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
# All rights reserved.

"""

EXCLUDE_DIRS = {
    'build', '.git', '.vscode', 'CMSIS_5', 'bsp', '__pycache__', '.venv', '.ruff_cache', '.pytest_cache'
}

def is_target_file(filename):
    if filename == "CMakeLists.txt":
        return True
    _, ext = os.path.splitext(filename)
    return ext in {'.c', '.h', '.py', '.cmake'}

def get_header(filename):
    if filename == "CMakeLists.txt" or filename.endswith(".cmake") or filename.endswith(".py"):
        return HEADER_PY
    return HEADER_C

def process_file(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        print(f"Skipping {filepath} (binary or non-utf8)")
        return

    if "Copyright (c) 2026 Advance Instrumentation 'n' Control Systems" in content:
        print(f"Skipping {filepath} (already has header)")
        return

    header = get_header(filepath)
    
    if filepath.endswith(".py") and content.startswith("#!"):
        # Insert after shebang
        lines = content.splitlines(keepends=True)
        # Find index after shebang
        insert_idx = 1
        # Check if empty lines follow shebang
        if len(lines) > 1 and lines[1].strip() == "":
             # Keep the empty line after shebang if present, or just insert header
             pass
        
        lines.insert(insert_idx, header)
        new_content = "".join(lines)
    else:
        new_content = header + content

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(new_content)
    print(f"Updated {filepath}")

def main():
    root_dir = "."
    for dirpath, dirnames, filenames in os.walk(root_dir):
        # Modify dirnames in-place to exclude directories
        dirnames[:] = [d for d in dirnames if d not in EXCLUDE_DIRS]
        
        for filename in filenames:
            if is_target_file(filename):
                process_file(os.path.join(dirpath, filename))

if __name__ == "__main__":
    main()
