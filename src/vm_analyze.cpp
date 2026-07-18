std::vector<VM::Diagnostic> VM::analyzeProgram(const std::string& binary_text) {
    std::vector<Diagnostic> diagnostics;
    std::istringstream iss(binary_text);
    std::string line;
    int line_num = 1;
    
    bool found_hlt = false;
    bool acc_loaded = false;
    int instruction_count = 0;

    while (std::getline(iss, line)) {
        size_t hash_pos = line.find('#');
        if (hash_pos != std::string::npos) {
            line = line.substr(0, hash_pos);
        }

        std::string bits = "";
        for (char c : line) {
            if (c != '0' && c != '1' && c != ' ' && c != '\t' && c != '\r') {
                diagnostics.push_back({line_num, true, "Syntax Error: Invalid character (only 0, 1, and spaces allowed)"});
                break;
            }
            if (c == '0' || c == '1') bits += c;
        }

        if (bits.length() > 0) {
            if (bits.length() != 8) {
                diagnostics.push_back({line_num, true, "Syntax Error: Instruction must be exactly 8 bits. Found " + std::to_string(bits.length())});
            } else {
                std::string opcode_str = bits.substr(0, 4);
                
                if (opcode_str == "0001") acc_loaded = true;
                
                if (opcode_str == "1110" && !acc_loaded) {
                    diagnostics.push_back({line_num, false, "Assistant Warning: You are trying to output (OUT) the Accumulator, but you haven't loaded any data into it yet (LDA)."});
                }
                
                if (opcode_str == "1111") found_hlt = true;

                if (opcode_str != "0000" && opcode_str != "0001" && opcode_str != "0010" && 
                    opcode_str != "0011" && opcode_str != "0100" && opcode_str != "1110" && 
                    opcode_str != "1111") {
                    diagnostics.push_back({line_num, true, "Syntax Error: Unknown Opcode '" + opcode_str + "'."});
                }
                
                instruction_count++;
            }
        }
        line_num++;
    }

    if (instruction_count > 0 && !found_hlt) {
        diagnostics.push_back({-1, false, "Assistant Warning: Missing HLT (1111) instruction. The CPU might try to execute your raw data as code and crash."});
    }

    return diagnostics;
}
