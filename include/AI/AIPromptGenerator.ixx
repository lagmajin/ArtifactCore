module;
#include <utility>
#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>

export module Core.AI.PromptGenerator;

import Core.AI.Describable;

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
        QString prompt;
        
        // 1. Header
        prompt += "# ArtifactStudio AI Assistant System Prompt\n";
        prompt += QString("Generated at: %1\n\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
        
        // 2. Identity & Role
        prompt += "## Role\n";
        if (lang == DescriptionLanguage::Japanese) {
            prompt += "あなたは ArtifactStudio の強力な AI アシスタントです。ユーザーの創作活動（画像処理、動画編集、AI解析）をサポートするのがあなたの役割です。\n";
            prompt += "提供されたツールと情報を活用し、具体的かつ技術的に正確なアドバイスや操作提案を行ってください。\n\n";
        } else {
            prompt += "You are the powerful AI Assistant for ArtifactStudio. Your role is to support the user's creative activities including image processing, video editing, and AI analysis.\n";
            prompt += "Utilize the provided tools and information to give specific and technically accurate advice or operation proposals.\n\n";
        }
        
        // 3. Application Capabilities (The Core Metadata)
        prompt += "## Available Application Components & Tools\n";
        if (lang == DescriptionLanguage::Japanese) {
            prompt += "以下は、あなたが現在操作または参照できるアプリケーションのコンポーネント定義です：\n\n";
        } else {
            prompt += "Below are the application component definitions you can currently interact with or reference:\n\n";
        }
        prompt += "(component registry unavailable in this build configuration)\n\n";
        
        // 4. Interaction Guidelines
        prompt += "\n## Interaction Guidelines\n";
        if (lang == DescriptionLanguage::Japanese) {
            prompt += "1. テクニカルな質問には、上記のコンポーネント名やプロパティ名を使用して正確に答えてください。\n";
            prompt += "2. 複雑なタスクには、ステップバイステップの実行プランを提案してください。\n";
            prompt += "3. ソースコードや JSON 形式での出力が必要な場合は、Markdown 形式で記述してください。\n";
        } else {
            prompt += "1. For technical questions, answer accurately using the component and property names defined above.\n";
            prompt += "2. For complex tasks, propose a step-by-step execution plan.\n";
            prompt += "3. If source code or JSON output is required, use Markdown blocks.\n";
        }
        
        return prompt;
    }
    
    /**
     * @brief Generate a concise JSON-formatted tool schema for function calling
     */
    static QByteArray generateToolSchemaJson() {
        return QByteArrayLiteral("{\"components\":[],\"tools\":[]}");
    }
};

} // namespace ArtifactCore
