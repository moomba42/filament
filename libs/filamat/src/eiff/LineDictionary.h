/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TNT_FILAMAT_LINEDICTIONARY_H
#define TNT_FILAMAT_LINEDICTIONARY_H

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace utils::io {
class ostream;
}

namespace filamat {

/**
 * LineDictionary parses and deduplicates a text shader into a minimal set of optimal string tokens.
 *
 * Encoding Schema (Multi-Base Variable-Length Interleaved Token Stream):
 * ----------------------------------------------------------------------
 * Once the dictionary determines the global frequency of tokens, MaterialTextChunk encodes the shader
 * text into a binary integer stream according to the following variable-length layout, intentionally
 * designed to minimize mid-tail permutations natively while heavily anchoring Zlib (LZ77) sliding windows:
 *
 *   Tokens with frequency-sorted dictionary ID < 240:
 *      [ 1 byte ]: The exact dictionary ID representing the token (0 to 239).
 *
 *   Tokens with frequency-sorted dictionary ID < 4080:
 *      [ 2 bytes ]: `<240 to 254> <uint8_t(LSB)>`.
 *      The first byte natively dedicates 15 statically bound permutation slots to provide 4 bits
 *      (nibble) of spatial MSB data without bitwise-scrambling the Zlib entropy signatures.
 *
 *   Tokens with frequency-sorted dictionary ID >= 4080:
 *      [ 3 bytes ]: `<255> <uint16_t(ID - 4080)>`.
 *      The `255` byte acts as a literal escape hatch for the absolute monolithic long-tail.
 *
 * The matching decoder loop reverses this inside `MaterialChunk::getTextShader()`.
 */
class LineDictionary {
public:
    using index_t = uint32_t;

    LineDictionary();
    ~LineDictionary() noexcept;

    // Due to the presence of unique_ptr, disallow copy construction but allow move construction.
    LineDictionary(LineDictionary const&) = delete;
    LineDictionary(LineDictionary&&) = default;

    // Adds text to the dictionary, parsing it into lines.
    void addText(std::string_view text) noexcept;

    // Sorts the dictionary entries by frequency and reassigns their indices
    void resolve() noexcept;

    // Returns the total number of unique lines stored in the dictionary.
    size_t getDictionaryLineCount() const {
        return mStrings.size();
    }

    // Checks if the dictionary is empty.
    bool isEmpty() const noexcept {
        return mStrings.empty();
    }

    // Retrieves a string by its index.
    std::string const& getString(index_t index) const noexcept;

    // Retrieves the indices of lines that match the given string view.
    std::vector<index_t> getIndices(std::string_view const& line) const noexcept;

    // Prints statistics about the dictionary to the given output stream.
    void printStatistics(utils::io::ostream& stream) const noexcept;

    // conveniences...
    size_t size() const {
        return getDictionaryLineCount();
    }

    bool empty() const noexcept {
        return isEmpty();
    }

    std::string const& operator[](index_t const index) const noexcept {
        return getString(index);
    }

private:
    // Adds a single line to the dictionary.
    void addLine(std::string_view line) noexcept;

    // Trims leading whitespace from a string view.
    static std::string_view ltrim(std::string_view s);

    // Splits a string view into a vector of string views based on delimiters.
    static std::vector<std::string_view> splitString(std::string_view line);

    // Finds a pattern within a string view starting from an offset.
    static std::pair<size_t, size_t> findPattern(std::string_view line, size_t offset);

    struct LineInfo {
        index_t index;
        uint32_t count;
    };

    std::unordered_map<std::string_view, LineInfo> mLineIndices;
    std::vector<std::unique_ptr<std::string>> mStrings;
};

} // namespace filamat
#endif // TNT_FILAMAT_LINEDICTIONARY_H
