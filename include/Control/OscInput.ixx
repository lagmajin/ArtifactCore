module;
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <QObject>
#include <QString>
#include <wobjectdefs.h>

export module Control.OSC.Input;

export namespace ArtifactCore {

/**
 * @brief OSC (Open Sound Control) 入力レシーバー
 *
 * QUdpSocket で指定ポートをリッスンし、最小限の OSC 1.0 パーサーで
 * メッセージをデコードする。受信した値を ExternalControlManager に
 * ルーティングするためのシグナルを提供。
 */
class OscInput : public QObject {
    W_OBJECT(OscInput)

public:
    explicit OscInput(QObject* parent = nullptr);
    ~OscInput() override;

    /// 指定ポートで UDP サーバーを開始
    bool startServer(uint16_t port);

    /// サーバーを停止
    void stopServer();

    /// サーバーが実行中か
    bool isRunning() const;

    /// 現在のポート番号 (0 = 停止中)
    uint16_t port() const;

    /// OSC メッセージを受信したときのシグナル
    /// @param address OSC アドレスパス (例: "/filter/cutoff")
    /// @param value   float 値 (OSC タイプ 'f' のみ対応)
    void messageReceived(const QString& address, float value)
        W_SIGNAL(messageReceived, address, value);

private:
    class Impl;
    Impl* impl_;
};

} // namespace ArtifactCore
