module;
#include <utility>
#include <string>
#include <string_view>
#include <vector>
#include <regex>
#include <algorithm>
#include <filesystem>
#include <cstdint>
#include <exception>
#include <map>

export module Asset.Sequence;

import Core.ArtifactString;
import Utils.Optional;
import Container.NamedVector;


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
    String prefix;            // "image_"   "shotA."   "render-v003-"
    int64_t     frame   = 0;  // 1, 42, 1001
    int         padding = 0;  // zero-padding width (4 → "0001")
    String suffix;            // ".png"  ".exr"  ".tif"
};

// ----------------------------------------------------------------
// SequenceGroup — one logical image sequence
// ----------------------------------------------------------------
struct SequenceGroup {
    String prefix;                // common prefix
    String suffix;                // common extension
    int         padding = 0;      // detected zero-padding
    int64_t     firstFrame = 0;
    int64_t     lastFrame  = 0;
    std::vector<String> filenames;       // sorted, all members

    /// Frame numbers absent between firstFrame and lastFrame.
    /// Populated only when detection ran with MissingFramePolicy::Preserve.
    std::vector<int64_t> missingFrames;

    /// Display name: e.g.  "image_[0001-0100].png  (100 frames)"
    String displayName() const
    {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "%s[%0*lld-%0*lld]%s  (%zu frames)",
            toStdString(prefix).c_str(),
            padding, (long long)firstFrame,
            padding, (long long)lastFrame,
            toStdString(suffix).c_str(),
            filenames.size());
        return String(buf);
    }

    /// Returns the representative filename (first frame).
    String representative() const
    {
        return filenames.front();
    }

    /// Returns the printf-style path pattern, e.g.  "image_%04lld.png"
    String pathPattern() const
    {
        char widthSpec[16];
        std::snprintf(widthSpec, sizeof(widthSpec), "%%0%dlld", padding);
        return String(toStdString(prefix) + widthSpec + toStdString(suffix));
    }
};

// ----------------------------------------------------------------
// Result from detectSequences()
// ----------------------------------------------------------------
struct SequenceDetectionResult {
    std::vector<SequenceGroup>  sequences;   // found groups (≥2 frames)
    std::vector<String>         singles;     // files that aren't in any group
};

// ----------------------------------------------------------------
// MissingFramePolicy — how gaps between detected frame numbers are
// reported.
//
//   Split    (default) : a gap ends the current run; each consecutive
//                        run becomes its own SequenceGroup.  Groups
//                        never contain missing frames.
//   Preserve           : the whole bucket stays one SequenceGroup and
//                        absent frame numbers are reported through
//                        SequenceGroup::missingFrames.
// ----------------------------------------------------------------
enum class MissingFramePolicy : std::uint8_t {
    Split,
    Preserve
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

inline Optional<FrameToken> parseFrameToken(const String& filename)
{
    // Pattern: (anything)(one-or-more-digits)(\.[a-zA-Z0-9]+)$
    static const std::regex kPattern(
        R"(^(.*?)(\d+)(\.[a-zA-Z0-9]+)$)",
        std::regex::ECMAScript | std::regex::optimize);

    std::smatch m;
    const std::string filenameStd = toStdString(filename);
    if (!std::regex_match(filenameStd, m, kPattern)) {
        return {};
    }

    FrameToken tok;
    tok.prefix  = String(m[1].str());
    tok.suffix  = String(m[3].str());
    const std::string digits = m[2].str();
    if (digits.size() > 18) {
        return {};
    }
    try {
        tok.frame = std::stoll(digits);
    } catch (const std::exception&) {
        return {};
    }
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
// @param policy       Gap handling; see MissingFramePolicy.
//
// Filenames that look like "image_0001.png" are grouped by
// (prefix, suffix, padding).  Groups with fewer than minFrames
// files are demoted back to singles.
// ----------------------------------------------------------------
inline SequenceDetectionResult detectSequences(
    const std::vector<String>& filenames,
    int minFrames = 2,
    MissingFramePolicy policy = MissingFramePolicy::Split)
{
    using namespace detail;
    minFrames = std::max(2, minFrames);

    // Map from GroupKey → [(frame, filename)]
    std::map<GroupKey, std::vector<std::pair<int64_t, std::string>>> buckets;
    NamedVector<std::string> unparsed{
        makeNamedVector<std::string>(ContainerName{"AssetSequenceUnparsedNames"})};

    for (const auto& fn : filenames) {
        auto tok = parseFrameToken(fn);
        if (!tok) {
            unparsed.append(toStdString(fn));
            continue;
        }
        GroupKey key{toStdString(tok->prefix), toStdString(tok->suffix), tok->padding};
        buckets[key].emplace_back(tok->frame, toStdString(fn));
    }

    SequenceDetectionResult result;

    for (auto& [key, frames] : buckets) {
        // Sort by frame number
        std::sort(frames.begin(), frames.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        if (static_cast<int>(frames.size()) < minFrames) {
            // Too few frames — treat as singles
            for (const auto& [frame, fn] : frames) {
                result.singles.emplace_back(fn);
            }
            continue;
        }

        // Preserve keeps the whole bucket as one group and reports the
        // absent frame numbers instead of splitting the run.
        if (policy == MissingFramePolicy::Preserve) {
            SequenceGroup grp;
            grp.prefix     = String(key.prefix);
            grp.suffix     = String(key.suffix);
            grp.padding    = key.padding;
            grp.firstFrame = frames.front().first;
            grp.lastFrame  = frames.back().first;
            grp.filenames.reserve(frames.size());
            int64_t expectedFrame = grp.firstFrame;
            for (const auto& [frame, fn] : frames) {
                while (expectedFrame < frame) {
                    grp.missingFrames.emplace_back(expectedFrame++);
                }
                expectedFrame = frame + 1;
                grp.filenames.emplace_back(fn);
            }
            result.sequences.push_back(std::move(grp));
            continue;
        }

        // Split on gaps so each reported group is a truly consecutive run.
        // This keeps the sequence contract deterministic for importers and
        // avoids presenting missing frames as available media.
        NamedVector<std::pair<int64_t, std::string>> run{
            makeNamedVector<std::pair<int64_t, std::string>>(ContainerName{"AssetSequenceFrameRun"})};
        run.reserve(frames.size());

        const auto flushRun = [&]() {
            if (static_cast<int>(run.size()) < minFrames) {
                for (const auto& [frame, fn] : run) {
                    result.singles.emplace_back(fn);
                }
                    run.clear();
                return;
            }

            SequenceGroup grp;
            grp.prefix     = String(key.prefix);
            grp.suffix     = String(key.suffix);
            grp.padding    = key.padding;
            grp.firstFrame = run.first()->first;
            grp.lastFrame  = run.last()->first;
            grp.filenames.reserve(run.size());
            for (const auto& [frame, fn] : run) {
                grp.filenames.emplace_back(fn);
            }
            result.sequences.push_back(std::move(grp));
            run.clear();
        };

        for (const auto& frame : frames) {
            if (!run.empty() && frame.first != run.last()->first + 1) {
                flushRun();
            }
            run.append(frame);
        }
        flushRun();
    }

    // Merge unparsed into singles
    for (auto& fn : unparsed) {
        result.singles.emplace_back(fn);
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
    NamedVector<std::string> names;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        names.push_back(entry.path().filename().string());
    }
    std::sort(names.begin(), names.end());
    NamedVector<String> coreNames;
    coreNames.reserve(names.size());
    for (const auto& name : names) {
        coreNames.emplace_back(name);
    }
    return detectSequences(coreNames.toStdVector(), minFrames);
}

} // namespace ArtifactCore
