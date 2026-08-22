module;

#include <QString>
#include <QChar>
#include <functional>

#include <algorithm>

#include "../Define/DllExportMacro.hpp"

export module EnvironmentVariable.Expansion;

import EnvironmentVariable;

export namespace ArtifactCore {

/// トークン展開のコンテキスト。
struct ExpansionContext {
    /// $F / $F<n> で使うフレーム番号 (0 始まりでも 1 始まりでもなく「そのまま」)。
    double frame = 0.0;
    double fps = 30.0;
    /// env マネージャより優先される解決器 (プロジェクト変数等)。nullptr 可。
    std::function<QString(const QString&)> customResolver;
};

/// 入力に展開マーカー ($ ) を含むか。ホットパスのガード用。
inline bool containsExpansionMarker(const QString& input)
{
    return input.contains(QChar('$'));
}

/// 文字列中のトークンを展開する。
///
/// ルール:
///   $$          → リテラル $
///   ${NAME}     / $NAME (NAME = [A-Za-z_][A-Za-z0-9_]*)
///               → customResolver → EnvironmentVariableManager の順で解決
///   $F / $F<n>  → frame を幅 n (既定1, 上限10) で 0 埋め
///   未解決変数は元のまま残す (空化しない。Houdini 挙動)
LIBRARY_DLL_API QString expandTokens(const QString& input, const ExpansionContext& ctx);

// ---------------------------------------------------------------------
// 実装 (header-only)
// ---------------------------------------------------------------------

namespace detail {

inline bool isNameStartChar(QChar c)
{
    return (c >= QChar(u'a') && c <= QChar(u'z'))
        || (c >= QChar(u'A') && c <= QChar(u'Z'))
        || c == QChar(u'_');
}

inline bool isNameChar(QChar c)
{
    return isNameStartChar(c) || (c >= QChar(u'0') && c <= QChar(u'9'));
}

inline QString resolveVariable(const QString& name, const ExpansionContext& ctx)
{
    if (ctx.customResolver) {
        const QString custom = ctx.customResolver(name);
        if (!custom.isNull()) {
            return custom;
        }
    }
    if (EnvironmentVariableManager::instance()->hasVariable(name)) {
        return EnvironmentVariableManager::instance()->getVariable(name).toString();
    }
    return QString(); // null = 未解決
}

} // namespace detail

inline QString expandTokens(const QString& input, const ExpansionContext& ctx)
{
    if (!containsExpansionMarker(input)) {
        return input;
    }

    QString out;
    out.reserve(input.size() + 16);

    int i = 0;
    const int n = input.size();
    while (i < n) {
        const QChar c = input.at(i);
        if (c != QChar('$')) {
            out.append(c);
            ++i;
            continue;
        }

        // "$" の次が無い → リテラルとして残す
        if (i + 1 >= n) {
            out.append(c);
            break;
        }
        const QChar next = input.at(i + 1);

        // $$ エスケープ
        if (next == QChar('$')) {
            out.append(QChar('$'));
            i += 2;
            continue;
        }

        // ${NAME}
        if (next == QChar('{')) {
            const int close = input.indexOf(QChar('}'), i + 2);
            if (close < 0) {
                // 閉じ無し → リテラルとして残す
                out.append(c);
                ++i;
                continue;
            }
            const QString name = input.mid(i + 2, close - (i + 2));
            const QString resolved = name.isEmpty()
                ? QString()
                : detail::resolveVariable(name, ctx);
            if (resolved.isNull()) {
                out.append(input.mid(i, close - i + 1)); // 未解決は元のまま
            } else {
                out.append(resolved);
            }
            i = close + 1;
            continue;
        }

        // $F<n> (フレーム幅指定)。$F 単体も可。
        // 注意: $FRAME 等 F で始まる変数名との競合を避けるため、
        // 「F の直後が数字」または「F で終端」の場合のみフレーム扱いする。
        if (next == QChar('F')) {
            const bool atEnd = (i + 2 >= n);
            const bool nameFollows = !atEnd && detail::isNameStartChar(input.at(i + 2));
            if (!nameFollows) {
                int width = 1;
                int j = i + 2;
                while (j < n && input.at(j).isDigit()) {
                    width = std::clamp(width * 10 + input.at(j).digitValue(), 1, 10);
                    ++j;
                }
                const int frameNumber = static_cast<int>(ctx.frame);
                out.append(QString::number(frameNumber).rightJustified(width, QChar(u'0')));
                i = j;
                continue;
            }
            // $FRAME 等 → 下の $NAME フローへ落ちる (i はそのまま)
        }

        // $NAME
        if (detail::isNameStartChar(next)) {
            int j = i + 1;
            while (j < n && detail::isNameChar(input.at(j))) {
                ++j;
            }
            const QString name = input.mid(i + 1, j - (i + 1));
            const QString resolved = detail::resolveVariable(name, ctx);
            if (resolved.isNull()) {
                out.append(input.mid(i, j - i)); // 未解決は元のまま
            } else {
                out.append(resolved);
            }
            i = j;
            continue;
        }

        // その他 ($1, $- 等) → リテラルとして残す
        out.append(c);
        ++i;
    }
    return out;
}

} // namespace ArtifactCore
