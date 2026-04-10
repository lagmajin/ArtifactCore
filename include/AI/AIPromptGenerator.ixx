module;
#include <utility>
#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>

export module Core.AI.PromptGenerator;

import Core.AI.Describable;
import Core.AI.CommandSandbox;
import Artifact.AI.WorkspaceAutomation;

export namespace ArtifactCore {

/**
 * @brief Generates optimized prompts for LLM AI agents based on app metadata
 */
class AIPromptGenerator {
public:
    /**
     * @brief Generate a comprehensive System Prompt for the AI
     * 
     * This includes:
     * 1. Basic App Identity
     * 2. Available Tools/Classes (from DescriptionRegistry)
     * 3. Interaction Rules
     */
    static QString generateSystemPrompt(DescriptionLanguage lang = DescriptionLanguage::English) {
        CommandSandbox::ensureRegistered();
        WorkspaceAutomation::ensureRegistered();
        QString prompt;
        
        // 1. Header
        prompt += "# ArtifactStudio AI Assistant System Prompt\n";
        prompt += QString("Generated at: %1\n\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
        
        // 2. Identity & Role
        prompt += "## Role\n";
        if (lang == DescriptionLanguage::Japanese) {
            prompt += "あなたは ArtifactStudio の強力な AI アシスタントです。ArtifactStudio はモーショングラフィックス、動画編集、コンポジット、レイヤー編集のためのアプリです。\n";
            prompt += "この文脈では「composition」はプロジェクト内のコンポジションを指し、音楽の作曲ではありません。\n";
            prompt += "提供されたツールと情報を活用し、具体的かつ技術的に正確なアドバイスや操作提案を行ってください。\n\n";
        } else {
            prompt += "You are the powerful AI Assistant for ArtifactStudio. ArtifactStudio is a motion-graphics, video-editing, compositing, and layer-editing application.\n";
            prompt += "In this context, \"composition\" means a project composition inside ArtifactStudio, not a musical composition.\n";
            prompt += "Utilize the provided tools and information to give specific and technically accurate advice or operation proposals.\n\n";
        }
        
        // 3. Application Capabilities (The Core Metadata)
        prompt += "## Available Application Components & Tools\n";
        if (lang == DescriptionLanguage::Japanese) {
            prompt += "以下は、あなたが現在操作または参照できるアプリケーションのコンポーネント定義です：\n\n";
        } else {
            prompt += "Below are the application component definitions you can currently interact with or reference:\n\n";
        }
        const auto classes = DescriptionRegistry::instance().registeredClasses();
        if (classes.isEmpty()) {
            prompt += "(component registry is currently empty)\n\n";
        } else {
            prompt += QString("Registered component count: %1\n").arg(classes.size());
            prompt += "Registered components:\n";
            for (const auto &className : classes) {
                prompt += QStringLiteral("- %1\n").arg(className);
            }
            prompt += "\n";
        }
        
        // 4. Interaction Guidelines
        prompt += "\n## Interaction Guidelines\n";
        if (lang == DescriptionLanguage::Japanese) {
            prompt += "1. テクニカルな質問には、上記のコンポーネント名やプロパティ名を使用して正確に答えてください。\n";
            prompt += "2. 複雑なタスクには、ステップバイステップの実行プランを提案してください。\n";
            prompt += "3. ソースコードや JSON 形式での出力が必要な場合は、Markdown 形式で記述してください。\n";
            prompt += "4. 不具合調査や曖昧な相談では、最初に最有力仮説、根拠、確認すべきファイルや値、最小の次アクションを提示してください。\n";
            prompt += "5. 情報が不足していても、ただ質問で止まらず、現時点の文脈から最も可能性が高い説明を先に返してください。\n";
        } else {
            prompt += "1. For technical questions, answer accurately using the component and property names defined above.\n";
            prompt += "2. For complex tasks, propose a step-by-step execution plan.\n";
            prompt += "3. If source code or JSON output is required, use Markdown blocks.\n";
            prompt += "4. For bugs or ambiguous reports, start with the most likely hypothesis, the evidence, the files or values to inspect, and the smallest next action.\n";
            prompt += "5. If context is incomplete, do not stop at a clarification question; first provide the best explanation available from the current context.\n";
        }
        
        return prompt;
    }
    
    /**
     * @brief Generate a concise JSON-formatted tool schema for function calling
     */
    static QByteArray generateToolSchemaJson() {
        CommandSandbox::ensureRegistered();
        WorkspaceAutomation::ensureRegistered();
        const auto &registry = DescriptionRegistry::instance();
        const QJsonObject components = registry.describeAllAsJson(DescriptionLanguage::English);

        QJsonArray tools;
        for (auto it = components.constBegin(); it != components.constEnd(); ++it) {
            const QString componentName = it.key().trimmed();
            if (componentName.isEmpty()) {
                continue;
            }
            const QJsonObject component = it.value().toObject();
            for (const auto &methodValue : component.value(QStringLiteral("methods")).toArray()) {
                const QJsonObject method = methodValue.toObject();
                const QString methodName = method.value(QStringLiteral("name")).toString().trimmed();
                if (methodName.isEmpty()) {
                    continue;
                }
                QJsonObject tool;
                tool[QStringLiteral("component")] = componentName;
                tool[QStringLiteral("method")] = methodName;
                tool[QStringLiteral("description")] = method.value(QStringLiteral("description")).toString();
                tool[QStringLiteral("returnType")] = method.value(QStringLiteral("returnType")).toString();
                tool[QStringLiteral("parameters")] = method.value(QStringLiteral("parameters")).toArray();
                tools.append(tool);
            }
        }

        QJsonObject schema;
        schema[QStringLiteral("generatedAt")] =
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        schema[QStringLiteral("language")] = QStringLiteral("en");
        schema[QStringLiteral("componentCount")] = components.size();
        schema[QStringLiteral("components")] = components;
        schema[QStringLiteral("tools")] = tools;
        return QJsonDocument(schema).toJson(QJsonDocument::Compact);
    }
};

} // namespace ArtifactCore
