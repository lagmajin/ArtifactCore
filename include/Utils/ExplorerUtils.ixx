module;
#include <memory>
export module Utils.ExplorerUtils;

import Utils.String.UniString;

export namespace ArtifactCore {
// path: ファイルまたはフォルダのパス
// select: trueならファイルを選択状態で開く（Win/Macのみ対応）
export void openInExplorer(const UniString& path, bool select = false);
}
