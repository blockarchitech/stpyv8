#pragma once

#include <memory>
#include <string>
#include <queue>
#include <mutex>
#include <v8.h>
#include <v8-inspector.h>

namespace STPyV8 {

    class Inspector : public v8_inspector::V8InspectorClient {
    public:
        explicit Inspector(v8::Isolate* isolate, v8::Local<v8::Context> context);
        ~Inspector() override;

        void ConnectDebugger(int port = 9229);
        void DisconnectDebugger();
        void SendMessage(const std::string& message);
        std::string GetNextMessage();
        void QueueMessage(const std::string& message);

        void runMessageLoopOnPause(int contextGroupId) override;
        void quitMessageLoopOnPause() override;
        void runIfWaitingForDebugger(int contextGroupId) override;

    private:
        class Channel : public v8_inspector::V8Inspector::Channel {
        public:
            explicit Channel(Inspector& inspector);
            void sendResponse(int callId,
                             std::unique_ptr<v8_inspector::StringBuffer> message) override;
            void sendNotification(std::unique_ptr<v8_inspector::StringBuffer> message) override;
            void flushProtocolNotifications() override;
        private:
            Inspector& inspector_;
        };

        v8::Isolate* isolate_;
        v8::Global<v8::Context> context_;
        std::unique_ptr<v8_inspector::V8Inspector> inspector_;
        std::unique_ptr<v8_inspector::V8InspectorSession> session_;
        bool running_nested_loop_;
        const int context_group_id_;

        std::queue<std::string> message_queue_;
        std::mutex message_mutex_;
        std::unique_ptr<Channel> channel_;
    };

} // namespace STPyV8