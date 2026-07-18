#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <memory>

// Forward-declare the preprocessor types so the VM header does not pull
// in the full preprocessor implementation. This keeps compile times low
// for translation units that only need the VM interface.
struct PreprocessedSource;
class  Preprocessor;

class VM {
public:
    uint8_t memory[16];          // The VM's 16-byte address space.
    uint8_t pc;                  // Program counter (0..15).
    uint8_t accumulator;         // 8-bit accumulator register.
    uint8_t output_reg;          // Last value written by OUT.
    bool    halted;              // True after HLT executes.
    bool    carry_flag;          // Set when ADD produces a carry (result > 255).
    bool    zero_flag;           // Set when the accumulator is zero after ALU op.

    std::vector<std::string> console_output;

    VM();

    void reset();

    // Load (assemble + write into `memory`) a program given as raw .bincode
    // source text. This runs Pass 0 (preprocessor) followed by the existing
    // two-pass assembler. Diagnostics are silently discarded; use
    // `analyzeProgram()` if you need them.
    void loadProgram(const std::string& binary_text);

    // Execute exactly one instruction from the current PC. Honours `halted`.
    void step();

    // Run the assembler and return its diagnostics WITHOUT touching the
    // VM's `memory` array. The editor calls this every frame for live
    // linting. Diagnostics carry *original* (pre-expansion) line numbers
    // so the red squiggles point at the user's source, not the expanded
    // macro text.
    struct Diagnostic {
        int  line_number;   // 1-based original source line. -1 = file-level.
        bool is_error;      // true -> red squiggle; false -> warning.
        std::string message;
    };
    std::vector<Diagnostic> analyzeProgram(const std::string& binary_text);

    // Run the VM at ~2 instructions per second until halted.
    void run();

    // Expose the preprocessor so the UI can display parsed macros and the
    // expanded source for debugging. The returned reference is valid until
    // the next call to loadProgram() / analyzeProgram().
    //
    // Returns nullptr if no preprocessing has happened yet.
    const PreprocessedSource* lastPreprocessed() const { return last_pp_.get(); }

private:
    // Internal: parses an 8-character binary string into a byte.
    uint8_t stringToByte(const std::string& str);

    // Cached result of the most recent preprocess() call, used to support
    // lastPreprocessed(). Stored as a pointer so we don't drag the full
    // PreprocessedSource definition into this header.
    std::shared_ptr<PreprocessedSource> last_pp_;
};
