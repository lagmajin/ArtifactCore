module;
#include <llama.h>
#include <QString>
#include <QDebug>
#include <vector>
#include <string>
#include <memory>

module Core.AI.LlamaAgent;

import std;
import Core.AI.Context;

namespace ArtifactCore {

class LlamaLocalAgent::Impl {
public:
    llama_model* model = nullptr;
    const llama_vocab* vocab = nullptr;
    llama_context* ctx = nullptr;
    int n_predict = 128;
    float temp = 0.7f;

    Impl() {
        llama_backend_init();
    }

    ~Impl() {
        if (ctx) llama_free(ctx);
        if (model) llama_model_free(model);
        llama_backend_free();
    }

    QString generate(const std::string& prompt) {
        if (!model || !ctx) return "Model not initialized";
        if (!vocab) return "Vocabulary not initialized";

        // 非常に簡略化した推論ループ
        // ※ 実際の実装では llama_batch や KV cache の適切な管理が必要ですが、
        // 基盤としてのデモ用に同期的な最小限のコードを記述します。
        
        std::vector<llama_token> tokens = tokenize(prompt, true);
        llama_batch batch = llama_batch_init(static_cast<int32_t>(tokens.size()), 0, 1);
        batch.n_tokens = static_cast<int32_t>(tokens.size());
        for (int32_t i = 0; i < static_cast<int32_t>(tokens.size()); ++i) {
            batch.token[i] = tokens[static_cast<size_t>(i)];
            batch.pos[i] = i;
            batch.n_seq_id[i] = 1;
            batch.seq_id[i][0] = 0;
            batch.logits[i] = (i == static_cast<int32_t>(tokens.size()) - 1) ? 1 : 0;
        }

        if (llama_decode(ctx, batch) != 0) {
            llama_batch_free(batch);
            return "Inference failed";
        }

        std::string result;
        // 次のトークンをサンプリング（ここでは貪欲法で簡略化）
        for (int i = 0; i < n_predict; ++i) {
            auto* logits = llama_get_logits_ith(ctx, batch.n_tokens - 1);
            
            // シンプルなサンプリング（実際はもっと複雑なロジックが必要）
            llama_token next_token = 0;
            float max_logit = -1e10;
            for (int id = 0; id < llama_vocab_n_tokens(vocab); ++id) {
                if (logits[id] > max_logit) {
                    max_logit = logits[id];
                    next_token = id;
                }
            }

            if (next_token == llama_vocab_eos(vocab)) break;

            char buf[128];
            int n = llama_token_to_piece(vocab, next_token, buf, sizeof(buf), 0, false);
            if (n > 0) result.append(buf, n);

            // 次の入力を準備
            llama_batch next_batch = llama_batch_init(1, 0, 1);
            next_batch.n_tokens = 1;
            next_batch.token[0] = next_token;
            next_batch.pos[0] = tokens.size() + i;
            next_batch.n_seq_id[0] = 1;
            next_batch.seq_id[0][0] = 0;
            next_batch.logits[0] = 1;
            
            if (llama_decode(ctx, next_batch) != 0) {
                llama_batch_free(next_batch);
                break;
            }
            llama_batch_free(batch);
            batch = next_batch;
        }

        llama_batch_free(batch);
        return QString::fromStdString(result);
    }

    std::vector<llama_token> tokenize(const std::string& text, bool add_bos) {
        int n_tokens = text.length() + (add_bos ? 1 : 0);
        std::vector<llama_token> res(n_tokens);
        n_tokens = llama_tokenize(vocab, text.c_str(), static_cast<int32_t>(text.length()), res.data(), n_tokens, add_bos, false);
        res.resize(n_tokens);
        return res;
    }
};

LlamaLocalAgent::LlamaLocalAgent() : impl_(std::make_unique<Impl>()) {}
LlamaLocalAgent::~LlamaLocalAgent() = default;

bool LlamaLocalAgent::initialize(const QString& modelPath) {
    llama_model_params model_params = llama_model_default_params();
    // model_params.n_gpu_layers = 32; // GPU加速を使う場合はここを調整

    impl_->model = llama_load_model_from_file(modelPath.toStdString().c_str(), model_params);
    if (!impl_->model) {
        qWarning() << "[LlamaLocalAgent] Failed to load model:" << modelPath;
        return false;
    }
    impl_->vocab = llama_model_get_vocab(impl_->model);
    if (!impl_->vocab) {
        qWarning() << "[LlamaLocalAgent] Failed to get vocab:" << modelPath;
        return false;
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 2048;
    impl_->ctx = llama_init_from_model(impl_->model, ctx_params);
    
    return impl_->ctx != nullptr;
}

QString LlamaLocalAgent::analyzeContext(const AIContext& context) {
    std::string prompt = "Context: " + context.toJsonString().toStdString() + "\nSummarize the current user activity in one short sentence:";
    return impl_->generate(prompt);
}

QString LlamaLocalAgent::predictParameter(const QString& targetProperty, const AIContext& context) {
    std::string prompt = "Task: Predict " + targetProperty.toStdString() + " based on context.\nContext: " + context.toJsonString().toStdString() + "\nResult (JSON only):";
    return impl_->generate(prompt);
}

bool LlamaLocalAgent::requiresCloudEscalation(const QString& userPrompt, const AIContext& context) {
    // 簡易的な判定ロジック
    // 「複雑なスクリプト」「新しいアルゴリズム」などの単語が含まれていたらクラウドへ
    if (userPrompt.contains("script") || userPrompt.contains("algorithm") || userPrompt.contains("coding")) {
        return true;
    }
    return false;
}

void LlamaLocalAgent::setMaxTokens(int maxTokens) { impl_->n_predict = maxTokens; }
void LlamaLocalAgent::setTemperature(float temperature) { impl_->temp = temperature; }

} // namespace ArtifactCore
