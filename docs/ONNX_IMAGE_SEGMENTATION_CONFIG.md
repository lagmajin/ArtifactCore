**最終更新:** 2026-09-04

# ONNX Image Segmentation Configuration

`OnnxImageSegmenter::loadOptionsFromJson()` は、モデル本体とは別に入力・出力
契約を読み込む。実モデルを配置する際は、モデルと同じフォルダに次のような
設定 JSON を置く。

```json
{
  "inputWidth": 320,
  "inputHeight": 320,
  "outputIndex": 0,
  "foregroundChannel": 0,
  "preserveAspectRatio": true,
  "inputPaddingValue": 0.0,
  "inputScale": 1.0,
  "inputColorOrder": "rgb",
  "inputRedMean": 0.0,
  "inputGreenMean": 0.0,
  "inputBlueMean": 0.0,
  "inputRedStdDev": 1.0,
  "inputGreenStdDev": 1.0,
  "inputBlueStdDev": 1.0,
  "outputActivation": "sigmoid",
  "preferDirectML": true
}
```

`inputColorOrder` は `rgb` または `bgr`、`outputActivation` は `none`、`sigmoid`、
`softmax` を指定できる。2チャンネル以上の出力では、foreground に対応する
`foregroundChannel` を設定する。

この設定はモデルの配布仕様に従って作成する。推測した値を既定値として配布しない。
モデル本体のライセンス、入力サイズ、正規化、出力レイアウトは導入時に実機検証する。
