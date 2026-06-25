import os
import re

SERVICES_SRC_DIR = os.path.join("src", "emu", "services", "src")
SERVICES_INC_DIR = os.path.join("src", "emu", "services", "include", "services")

COVERAGE_HEADER_NAME = "ngage_coverage.h"
COVERAGE_HEADER_CONTENT = """#pragma once
#include <fstream>
#include <mutex>
#include <string>

class NgageCoverageLogger {
private:
    inline static std::mutex mtx;
    inline static std::ofstream out;
    inline static bool initialized = false;

public:
    static void Log(const char* file, const char* func) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!initialized) {
            out.open("ngage_coverage_log.txt", std::ios::out | std::ios::trunc);
            initialized = true;
        }
        if (out.is_open()) {
            std::string f(file);
            size_t pos = f.find_last_of("/\\\\");
            if (pos != std::string::npos) f = f.substr(pos + 1);
            out << f << " | " << func << "\\n";
        }
    }
};

#define NGAGE_COVERAGE_LOG() NgageCoverageLogger::Log(__FILE__, __func__)
"""

def create_header():
    os.makedirs(SERVICES_INC_DIR, exist_ok=True)
    header_path = os.path.join(SERVICES_INC_DIR, COVERAGE_HEADER_NAME)
    with open(header_path, "w", encoding="utf-8") as f:
        f.write(COVERAGE_HEADER_CONTENT)
    print(f"[+] Created logger header at {header_path}")

def strip_strings_comments_and_macros(text):
    """Replaces strings, comments, and preprocessor macros with spaces to ensure safe parsing."""
    result = list(text)
    in_string = False
    in_char = False
    in_line_comment = False
    in_block_comment = False
    in_macro = False
    
    i = 0
    while i < len(result):
        if in_line_comment:
            if result[i] == '\n':
                in_line_comment = False
            else:
                result[i] = ' '
        elif in_block_comment:
            if result[i] == '*' and i+1 < len(result) and result[i+1] == '/':
                result[i] = ' '
                result[i+1] = ' '
                in_block_comment = False
                i += 1
            elif result[i] != '\n':
                result[i] = ' '
        elif in_macro:
            if result[i] == '\\' and i+1 < len(result) and result[i+1] == '\n':
                result[i] = ' '
                i += 1 # Preserve newline
            elif result[i] == '\n':
                in_macro = False
            else:
                result[i] = ' '
        elif in_string:
            if result[i] == '\\':
                result[i] = ' '
                i += 1
                if i < len(result) and result[i] != '\n': 
                    result[i] = ' '
            elif result[i] == '"':
                in_string = False
                result[i] = ' '
            elif result[i] != '\n':
                result[i] = ' '
        elif in_char:
            if result[i] == '\\':
                result[i] = ' '
                i += 1
                if i < len(result) and result[i] != '\n': 
                    result[i] = ' '
            elif result[i] == "'":
                in_char = False
                result[i] = ' '
            elif result[i] != '\n':
                result[i] = ' '
        else:
            if result[i] == '/' and i+1 < len(result):
                if result[i+1] == '/':
                    in_line_comment = True
                    result[i] = ' '
                    result[i+1] = ' '
                    i += 1
                elif result[i+1] == '*':
                    in_block_comment = True
                    result[i] = ' '
                    result[i+1] = ' '
                    i += 1
            elif result[i] == '"':
                in_string = True
                result[i] = ' '
            elif result[i] == "'":
                in_char = True
                result[i] = ' '
            elif result[i] == '#':
                # Check if it's a preprocessor directive by tracing back to newline
                j = i - 1
                is_start_of_line = True
                while j >= 0 and result[j] != '\n':
                    if result[j] not in (' ', '\t'):
                        is_start_of_line = False
                        break
                    j -= 1
                if is_start_of_line:
                    in_macro = True
                    result[i] = ' '
        i += 1
        
    return "".join(result)

def is_function_definition(context):
    context = context.strip()
    if not context:
        return False
        
    depth_paren = 0
    depth_angle = 0
    depth_square = 0
    
    for char in context:
        if char == '(': depth_paren += 1
        elif char == ')': depth_paren -= 1
        elif char == '<': depth_angle += 1
        elif char == '>': depth_angle -= 1
        elif char == '[': depth_square += 1
        elif char == ']': depth_square -= 1
        elif char == '=' and depth_paren == 0 and depth_angle == 0 and depth_square == 0:
            return False 
            
    if depth_paren != 0 or depth_angle != 0 or depth_square != 0:
        return False
        
    if '(' not in context or ')' not in context:
        return False
        
    top_level_open_paren_idx = -1
    d = 0
    for idx, char in enumerate(context):
        if char == '(':
            if d == 0:
                top_level_open_paren_idx = idx
                break
            d += 1
        elif char == ')':
            d -= 1
            
    if top_level_open_paren_idx == -1:
        return False
        
    before_paren = context[:top_level_open_paren_idx].strip()
    words = re.findall(r'[a-zA-Z_]\w*', before_paren)
    if not words:
        return False
        
    last_word_before_paren = words[-1]
    
    skip_keywords = {
        'if', 'while', 'for', 'switch', 'catch', 'template', 'sizeof', 
        'alignas', 'decltype', 'case', 'return', 'do', 'new'
    }
    if last_word_before_paren in skip_keywords:
        return False
        
    if re.search(r'\b(class|struct|enum|union|namespace)\b', before_paren):
        return False
        
    return True

def inject_into_cpp(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        text = f.read()

    clean_text = strip_strings_comments_and_macros(text)
    
    out_text = ""
    last_idx = 0
    last_stmt_end = 0
    injected_count = 0
    
    for i, char in enumerate(clean_text):
        if char in (';', '{', '}'):
            if char == '{':
                context = clean_text[last_stmt_end:i]
                if is_function_definition(context):
                    insert_pos = i + 1
                    out_text += text[last_idx:insert_pos]
                    out_text += "\n  NGAGE_COVERAGE_LOG();"
                    last_idx = insert_pos
                    injected_count += 1
            last_stmt_end = i + 1
            
    out_text += text[last_idx:]
    
    if injected_count > 0:
        out_text = '#include <services/ngage_coverage.h>\n' + out_text
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(out_text)
        print(f"[+] Injected {injected_count} logs into {file_path}")

def main():
    create_header()
    for root, dirs, files in os.walk(SERVICES_SRC_DIR):
        for file in files:
            if file.endswith(".cpp") or file.endswith(".cc"):
                inject_into_cpp(os.path.join(root, file))

if __name__ == "__main__":
    main()