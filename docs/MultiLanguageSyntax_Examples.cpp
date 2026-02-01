// 多言語風文法サポート - エクスプレッションエンジンの使用例

/*
このエクスプレッションエンジンは複数の言語風の文法を同時にサポートします。

サポートされる言語スタイル:
- JavaScript/C風 (デフォルト)
- Python風
- Flexible (複数スタイルを同時に許容)

使用例:

import Script.Engine.BuiltinVM;
import Script.Expression.Value;

using namespace ArtifactCore;

int main() {
    BuiltinScriptVM vm;
    
    // ========================================
    // JavaScript/C風の文法
    // ========================================
    
    // 三項演算子
    auto result1 = vm.evaluate("x > 0 ? 100 : -100");
    
    // 論理演算子
    auto result2 = vm.evaluate("(x > 0 && y < 10) || z == 5");
    
    // べき乗（通常は関数呼び出し）
    auto result3 = vm.evaluate("pow(2, 8)");  // 256
    
    
    // ========================================
    // Python風の文法
    // ========================================
    
    // if-else式（後置）
    vm.setVariable("score", ExpressionValue(85.0));
    auto result4 = vm.evaluate("'Pass' if score >= 60 else 'Fail'");
    // 結果: "Pass"
    
    // 英語キーワードの論理演算子
    auto result5 = vm.evaluate("x > 0 and y < 10 or z == 5");
    auto result6 = vm.evaluate("not (x == 0)");
    
    // べき乗演算子
    auto result7 = vm.evaluate("2 ** 8");  // 256
    auto result8 = vm.evaluate("3 ** 2 ** 2");  // 3^(2^2) = 81 (右結合)
    
    // 整数除算
    auto result9 = vm.evaluate("7 // 2");  // 3 (floor division)
    auto result10 = vm.evaluate("7 / 2");   // 3.5 (通常の除算)
    
    // 複合条件式
    vm.setVariable("temperature", ExpressionValue(25.0));
    auto result11 = vm.evaluate("'Hot' if temperature > 30 else 'Warm' if temperature > 20 else 'Cold'");
    // 結果: "Warm"
    
    
    // ========================================
    // 混在使用（Flexibleモード）
    // ========================================
    
    // Python風とJS風を混在
    auto result12 = vm.evaluate("(x > 0 ? 100 : 0) + (10 if y > 5 else 5)");
    
    // 複数スタイルの論理演算子
    auto result13 = vm.evaluate("x > 0 && (y < 10 or z == 5)");  // && と or を混在
    
    
    // ========================================
    // 実用的なエクスプレッション例
    // ========================================
    
    // アニメーション: バウンス効果（Python風）
    vm.setVariable("time", ExpressionValue(2.5));
    auto bounce = vm.evaluate("abs(sin(time * 3)) if time < 5 else 0");
    
    // 色の補間（Python風）
    vm.setVariable("progress", ExpressionValue(0.3));
    auto color = vm.evaluate("[255, 0, 0] if progress < 0.5 else [0, 0, 255]");
    
    // スコア判定（複数条件、Python風）
    vm.setVariable("score", ExpressionValue(92.0));
    auto grade = vm.evaluate(
        "'A' if score >= 90 else "
        "'B' if score >= 80 else "
        "'C' if score >= 70 else "
        "'D' if score >= 60 else "
        "'F'"
    );
    // 結果: "A"
    
    // 数学計算（Python風演算子）
    auto result14 = vm.evaluate("2 ** 10");  // 1024
    auto result15 = vm.evaluate("sqrt(2 ** 8)");  // 16.0
    
    // ベクトル条件（混在）
    vm.setVariable("velocity", ExpressionValue(100.0, 50.0));
    auto limited = vm.evaluate(
        "velocity if length(velocity) <= 100 else normalize(velocity) * 100"
    );
    
    // 範囲チェック（Python風）
    vm.setVariable("value", ExpressionValue(150.0));
    auto clamped = vm.evaluate(
        "0 if value < 0 else 100 if value > 100 else value"
    );
    // 結果: 100
    
    
    // ========================================
    // 言語スタイルの切り替え
    // ========================================
    
    // 通常はFlexibleモードで動作（複数スタイル許容）
    // 必要に応じて厳格なモードに切り替え可能
    
    // parser.setLanguageStyle(ExpressionLanguageStyle::JavaScript);  // JS風のみ
    // parser.setLanguageStyle(ExpressionLanguageStyle::Python);      // Python風のみ
    // parser.setLanguageStyle(ExpressionLanguageStyle::Flexible);    // 混在OK（デフォルト）
    
    return 0;
}


// ========================================
// サポートされる構文の比較
// ========================================

条件式:
JavaScript/C:  condition ? true_val : false_val
Python:        true_val if condition else false_val
両方同時:      OK（Flexibleモード）

論理演算子:
JavaScript/C:  &&  ||  !
Python:        and or not
両方同時:      OK

べき乗:
JavaScript/C:  pow(x, y)
Python:        x ** y
両方同時:      OK

整数除算:
JavaScript/C:  floor(x / y)
Python:        x // y
両方同時:      OK


// ========================================
// 演算子の優先順位
// ========================================

1. **          (べき乗、右結合)
2. -, not      (単項)
3. *, /, //    (乗除)
4. +, -        (加減)
5. ==, !=, <, <=, >, >=  (比較)
6. and (&&)    (論理AND)
7. or (||)     (論理OR)
8. ? :         (三項演算子)
9. if-else     (Python風条件式)


// ========================================
// よくある使用パターン
// ========================================

// 1. 値の範囲制限（Python風）
"min_val if value < min_val else max_val if value > max_val else value"

// 2. 段階的な値（Python風チェイン）
"0 if x < 0 else 50 if x < 0.5 else 100"

// 3. 安全な除算（短絡評価）
"0 if denominator == 0 else numerator / denominator"

// 4. ベクトルの正規化（条件付き）
"vec if length(vec) < 0.001 else normalize(vec)"

// 5. アニメーションカーブ（混在）
"start + (end - start) * (easeOut(t) if t < 0.5 else easeIn(t))"

// 6. 複数条件の組み合わせ（Python and/or）
"active and visible and opacity > 0"

// 7. べき乗の計算（Python演算子）
"2 ** bits"  // ビット数からサイズ計算

// 8. 配列インデックスの計算（整数除算）
"array[time // frame_duration]"


// ========================================
// パフォーマンスのヒント
// ========================================

1. 条件式は最初にマッチした時点で評価を停止（短絡評価）
2. Python風 if-else は内部的に同じ条件ノードに変換されるため、性能差はなし
3. べき乗 ** は pow() 関数と同等
4. 整数除算 // は floor(x/y) と同等だが、わずかに高速


// ========================================
// デバッグのヒント
// ========================================

// エラーチェック
auto result = vm.evaluate("invalid syntax here");
if (vm.hasError()) {
    std::cout << "Error: " << vm.getError() << std::endl;
}

// 変数の確認
if (vm.hasVariable("myVar")) {
    auto val = vm.getVariable("myVar");
    std::cout << "myVar = " << val.toString() << std::endl;
}

// 結果の型チェック
auto result = vm.evaluate("some_expression");
if (result.isNumber()) { ... }
else if (result.isVector()) { ... }
else if (result.isArray()) { ... }


// ========================================
// 制限事項
// ========================================

1. 変数の代入はサポートされていません（評価専用）
2. ループ構文はありません（単一式のみ）
3. 関数定義はできません（ビルトイン関数のみ）
4. Ruby風の unless/then/end は未サポート
5. VB風の IIf() は未サポート（三項演算子を使用）

*/
