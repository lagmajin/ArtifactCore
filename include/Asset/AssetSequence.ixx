module;
#include <string>
#include <string_view>
#include <vector>
#include <regex>
#include <algorithm>
#include <filesystem>
#include <optional>
#include <cstdint>
#include <map>

export module Asset.Sequence;

// ================================================================
// M-AB-2 Asset Browser Sequence Grouping — Phase 1: Detection Core
// ================================================================
// Design goals:
//   • Detect image sequence groups (foo_0001.png … foo_0100.png)
//   • Zero false-positives: only consecutive integer-padded runs
//   • Filesystem-agnostic: works on a flat list of filenames
//   • No Qt dependency — pure std, usable from ArtifactCore
// ================================================================

export namespace ArtifactCore {

// ----------------------------------------------------------------
// FrameToken — the decomposed parts of a sequenced filename
// ----------------------------------------------------------------
struct FrameToken {
    std::string prefix;       // "image_"   "shotA."   "render-v003-"
    int64_t     frame   = 0;  // 1, 42, 1001
    int         padding = 0;  // zero-padding width (4 → "0001")
    std::string suffix;       // ".png"  ".exr"  ".tif"
};

// ----------------------------------------------------------------
// SequenceGroup — one logical image sequence
// ----------------------------------------------------------------
struct SequenceGroup {
    std::string prefix;           // common prefix
    std::string suffix;           // common extension
    int         padding = 0;      // detected zero-padding
    int64_t     firstFrame = 0;
    int64_t     lastFrame  = 0;
    std::vector<std::string> filenames;  // sorted, all members

    /// Display name: e.g.  "image_[0001-0100].png  (100 frames)"
    std::string displayName() const
    {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "%s[%0*lld-%0*lld]%s  (%zu frames)",
            prefix.c_str(),
            padding, (long long)firstFrame,
            padding, (long long)lastFrame,
            suffix.c_str(),
            filenames.size());
        return buf;
    }

    /// Returns the representative filename (first frame).
    const std::string& representative() const
    {
        return filenames.front();
    }

    /// Returns the printf-style path pattern, e.g.  "image_%04lld.png"
    std::string pathPattern() const
    {
        char widthSpec[16];
        std::snprintf(widthSpec, sizeof(widthSpec), "%%0%dlld", padding);
        return prefix + widthSpec + suffix;
    }
};

// ----------------------------------------------------------------
// Result from detectSequences()
// ----------------------------------------------------------------
struct SequenceDetectionResult {
    std::vector<SequenceGroup>  sequences;   // found groups (≥2 frames)
    std::vector<std::string>    singles;     // files that aren't in any group
};

// ----------------------------------------------------------------
// Internal helpers
// ----------------------------------------------------------------
namespace detail {

// Regex to extract the last run of digits in a filename stem.
// Captures:  (prefix)(digits)(suffix_with_ext)
// Examples:
//   "image_0042.png"   → prefix="image_"  digits="0042"  ext=".png"
//   "shotA.0001.exr"   → prefix="shotA."  digits="0001"  ext=".exr"
//   "v003_render.tif"  → prefix="v003_render."  digits="" → no match (last digits are part of stem)
// We intentionally anchor to the LAST digit run before the extension.

inline std::optional<FrameToken> parseFrameToken(const std::string& filename)
{
    // Pattern: (anything)(one-or-more-digits)(\.[a-zA-Z0-9]+)$
    static const std::regex kPattern(
        R"(^(.*?)(\d+)(\.[a-zA-Z0-9]+)$)",
        std::regex::ECMAScript | std::regex::optimize);

    std::smatch m;
    if (!std::regex_match(filename, m, kPattern)) {
        return std::nullopt;
    }

    FrameToken tok;
    tok.prefix  = m[1].str();
    tok.suffix  = m[3].str();
    const std::string digits = m[2].str();
    tok.frame   = std::stoll(digits);
    tok.padding = static_cast<int>(digits.size());
    return tok;
}

// Group key = (prefix, suffix, padding)
struct GroupKey {
    std::string prefix;
    std::string suffix;
    int         padding;
    bool operator<(const GroupKey& o) const noexcept
    {
        if (prefix != o.prefix) return prefix < o.prefix;
        if (suffix != o.suffix) return suffix < o.suffix;
        return padding < o.padding;
    }
};

} // namespace detail

// ----------------------------------------------------------------
// detectSequences()
//
// @param filenames    Flat list of filenames (basenames only,
//                     NOT full paths — the caller prepends the dir).
// @param minFrames    Minimum number of frames to form a sequence (default 2).
//
// Filenames that look like "image_0001.png" are grouped by
// (prefix, suffix, padding).  Groups with fewer than minFrames
// files are demoted back to singles.
// ----------------------------------------------------------------
inline SequenceDetectionResult detectSequences(
    const std::vector<std::string>& filenames,
    int minFrames = 2)
{
    using namespace detail;

    // Map from GroupKey → [(frame, filename)]
    std::map<GroupKey, std::vector<std::pair<int64_t, std::string>>> buckets;
    std::vector<std::string> unparsed;

    for (const auto& fn : filenames) {
        auto tok = parseFrameToken(fn);
        if (!tok) {
            unparsed.push_back(fn);
            continue;
        }
        GroupKey key{tok->prefix, tok->suffix, tok->padding};
        buckets[key].emplace_back(tok->frame, fn);
    }

    SequenceDetectionResult result;

    for (auto& [key, frames] : buckets) {
        // Sort by frame number
        std::sort(frames.begin(), frames.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        if (static_cast<int>(frames.size()) < minFrames) {
            // Too few frames — treat as singles
            for (const auto& [frame, fn] : frames) {
                result.singles.push_back(fn);
            }
            continue;
        }

        SequenceGroup grp;
        grp.prefix     = key.prefix;
        grp.suffix     = key.suffix;
        grp.padding    = key.padding;
        grp.firstFrame = frames.front().first;
        grp.lastFrame  = frames.back().first;
        grp.filenames.reserve(frames.size());
        for (const auto& [frame, fn] : frames) {
            grp.filenames.push_back(fn);
        }
        result.sequences.push_back(std::move(grp));
    }

    // Merge unparsed into singles
    for (auto& fn : unparsed) {
        result.singles.push_back(fn);
    }

    return result;
}

// ----------------------------------------------------------------
// detectSequencesInDirectory()
//
// Convenience wrapper that reads a directory listing (sorted by name)
// and runs detectSequences().  Returns the same SequenceDetectionResult.
// The caller can prepend the directory path to each filename to get
// full paths.
//
// Only regular files are considered; subdirectories are skipped.
// ----------------------------------------------------------------
inline SequenceDetectionResult detectSequencesInDirectory(
    const std::filesystem::path& dir,
    int minFrames = 2)
{
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        names.push_back(entry.path().filename().string());
    }
    std::sort(names.begin(), names.end());
    return detectSequences(names, minFrames);
}

} // namespace ArtifactCore
