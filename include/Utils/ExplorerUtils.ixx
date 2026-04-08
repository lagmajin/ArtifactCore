module;
#include <utility>
#include <memory>
export module Utils.ExplorerUtils;

import Utils.String.UniString;

export namespace ArtifactCore {
// path: file or folder path
// select: open file in selection mode when true (Win/Mac only)
export void openInExplorer(const UniString& path, bool select = false);
}
