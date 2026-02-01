// After Effects風エクスプレッションエンジンの使用例

/*
使用例:

#include <iostream>
import Script.Engine.BuiltinVM;
import Script.Expression.Value;

using namespace ArtifactCore;

int main() {
    BuiltinScriptVM vm;
    
    // 基本的な数式評価
    auto result1 = vm.evaluate("2 + 3 * 4");
    std::cout << "2 + 3 * 4 = " << result1.asNumber() << std::endl;  // 14
    
    // ベクトル演算
    auto result2 = vm.evaluate("[1, 2, 3] + [4, 5, 6]");
    std::cout << "Vector addition = " << result2.toString() << std::endl;  // [5, 7, 9]
    
    // 変数の使用
    vm.setVariable("time", ExpressionValue(5.0));
    vm.setVariable("position", ExpressionValue(100.0, 200.0));
    auto result3 = vm.evaluate("position + [sin(time) * 50, cos(time) * 50]");
    std::cout << "Position with wiggle = " << result3.toString() << std::endl;
    
    // 条件分岐（三項演算子）
    vm.setVariable("opacity", ExpressionValue(0.7));
    auto result4 = vm.evaluate("opacity > 0.5 ? 100 : 0");
    std::cout << "Conditional = " << result4.asNumber() << std::endl;  // 100
    
    // ビルトイン関数の使用
    auto result5 = vm.evaluate("clamp(150, 0, 100)");
    std::cout << "Clamp = " << result5.asNumber() << std::endl;  // 100
    
    auto result6 = vm.evaluate("linear(0.5, 0, 100)");
    std::cout << "Linear interpolation = " << result6.asNumber() << std::endl;  // 50
    
    // ベクトルの長さ
    auto result7 = vm.evaluate("length([3, 4])");
    std::cout << "Vector length = " << result7.asNumber() << std::endl;  // 5
    
    // 配列操作
    auto result8 = vm.evaluate("sum([1, 2, 3, 4, 5])");
    std::cout << "Sum = " << result8.asNumber() << std::endl;  // 15
    
    // ランダム値
    auto result9 = vm.evaluate("random(0, 100)");
    std::cout << "Random = " << result9.asNumber() << std::endl;
    
    // エラーチェック
    auto result10 = vm.evaluate("undefined_var + 10");
    if (vm.hasError()) {
        std::cout << "Error: " << vm.getError() << std::endl;
    }
    
    return 0;
}

サポートされているAEスタイルのエクスプレッション例:

1. 位置アニメーション（ウィグル）:
   position + [sin(time * 5) * 20, cos(time * 5) * 20]

2. スケールのバウンス:
   bounce = abs(sin(time * 3));
   [100 + bounce * 20, 100 + bounce * 20]

3. 透明度のフェード:
   time < 1 ? time * 100 : 100

4. 色の補間:
   linear(time, [255, 0, 0, 255], [0, 0, 255, 255])

5. 回転の加速:
   time * time * 360

6. ランダムな揺れ:
   [random(-10, 10), random(-10, 10)]

7. 配列の操作:
   points = [[0, 0], [100, 0], [100, 100], [0, 100]];
   points[floor(time) % 4]

8. ベクトルの正規化:
   velocity = [100, 50];
   normalize(velocity) * speed

9. クランプ:
   clamp(position[0], 0, 1920)

10. イージング:
    ease(time, startValue, endValue)

サポートされている演算子:
- 算術: +, -, *, /
- 比較: ==, !=, <, <=, >, >=
- 論理: && (未実装), || (未実装), !
- 三項: condition ? true_value : false_value
- 配列アクセス: array[index]

サポートされているビルトイン関数:
数学関数:
- sin, cos, tan
- sqrt, pow
- abs, floor, ceil, round
- min, max, clamp

ベクトル関数:
- length (ベクトルの長さ)
- normalize (正規化)
- dot (内積)
- cross (外積)

補間:
- linear (線形補間)
- ease, easeIn, easeOut (イージング)

ランダム:
- random (ランダム値)
- noise (ノイズ、簡易版)

配列:
- sum (合計)
- average (平均)

*/
