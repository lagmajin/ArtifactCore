module;

#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QUuid>

export module Collaborate.Protocol;

import Reactive.Events;

namespace ArtifactCore {

struct CollaborationMessage {
    enum class Type {
        RuleAdded,
        RuleRemoved,
        RuleUpdated,
        RuleExecuted,
        UserJoined,
        UserLeft,
        Ping,
        Pong
    };
    
    QString type;
    QString ruleId;
    QString userId;
    QString userName;
    QString payload;
    qint64 timestamp;
    QString sessionId;
    
    CollaborationMessage() : timestamp(QDateTime::currentMSecsSinceEpoch()) {}
};

class CollaborationProtocol {
public:
    static CollaborationMessage wrapRule(const ReactiveRule& rule, CollaborationMessage::Type msgType) {
        CollaborationMessage msg;
        msg.type = toString(msgType);
        msg.ruleId = rule.id;
        msg.payload = rule.toJson();
        return msg;
    }
    
    static ReactiveRule unwrapRule(const CollaborationMessage& msg) {
        return ReactiveRule::fromJson(msg.payload);
    }
    
    static QString serialize(const CollaborationMessage& msg) {
        QJsonObject obj;
        obj["type"] = msg.type;
        obj["ruleId"] = msg.ruleId;
        obj["userId"] = msg.userId;
        obj["userName"] = msg.userName;
        obj["payload"] = QJsonDocument::fromJson(msg.payload.toUtf8()).object();
        obj["timestamp"] = static_cast<qint64>(msg.timestamp);
        obj["sessionId"] = msg.sessionId;
        return QString(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }
    
    static CollaborationMessage deserialize(const QString& json) {
        CollaborationMessage msg;
        auto doc = QJsonDocument::fromJson(json.toUtf8());
        if (!doc.isObject()) return msg;
        
        auto obj = doc.object();
        msg.type = obj["type"].toString();
        msg.ruleId = obj["ruleId"].toString();
        msg.userId = obj["userId"].toString();
        msg.userName = obj["userName"].toString();
        msg.timestamp = obj["timestamp"].toVariant().toLongLong();
        msg.sessionId = obj["sessionId"].toString();
        
        if (obj.contains("payload") && obj["payload"].isObject()) {
            msg.payload = QJsonDocument(obj["payload"].toObject()).toJson(QJsonDocument::Compact);
        }
        return msg;
    }
    
    static QString generateSessionId() {
        return QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    
private:
    static QString toString(CollaborationMessage::Type type) {
        switch (type) {
            case CollaborationMessage::Type::RuleAdded: return "rule_added";
            case CollaborationMessage::Type::RuleRemoved: return "rule_removed";
            case CollaborationMessage::Type::RuleUpdated: return "rule_updated";
            case CollaborationMessage::Type::RuleExecuted: return "rule_executed";
            case CollaborationMessage::Type::UserJoined: return "user_joined";
            case CollaborationMessage::Type::UserLeft: return "user_left";
            case CollaborationMessage::Type::Ping: return "ping";
            case CollaborationMessage::Type::Pong: return "pong";
            default: return "unknown";
        }
    }
};

} // namespace ArtifactCore
