#include "Inspector.h"
#include <sstream>
#include <thread>

namespace STPyV8 {

Inspector::Channel::Channel(Inspector& inspector) : inspector_(inspector) {}

void Inspector::Channel::sendResponse(int callId,
                                    std::unique_ptr<v8_inspector::StringBuffer> message) {
    std::string msg(
        reinterpret_cast<const char*>(message->string().characters8()),
        message->string().length()
    );
    inspector_.QueueMessage(msg);
}

void Inspector::Channel::sendNotification(std::unique_ptr<v8_inspector::StringBuffer> message) {
    std::string msg(
        reinterpret_cast<const char*>(message->string().characters8()),
        message->string().length()
    );
    inspector_.QueueMessage(msg);
}

void Inspector::Channel::flushProtocolNotifications() {}

Inspector::Inspector(v8::Isolate* isolate, v8::Local<v8::Context> context)
    : isolate_(isolate)
    , context_(isolate, context)
    , running_nested_loop_(false)
    , context_group_id_(1)
{
    v8::HandleScope handle_scope(isolate_);

    // Create V8 Inspector instance
    inspector_ = v8_inspector::V8Inspector::create(isolate_, this);

    // Create the debugging context
    v8_inspector::StringView context_name(
        reinterpret_cast<const uint8_t*>("STPyV8 Context"),
        13
    );

    // Create the inspector session
    inspector_->contextCreated(v8_inspector::V8ContextInfo(
        context,
        context_group_id_,
        context_name
    ));

    channel_ = std::make_unique<Channel>(*this);
}

Inspector::~Inspector() {
    if (session_) {
        session_.reset();
    }

    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> local_context = context_.Get(isolate_);
    inspector_->contextDestroyed(local_context);
}

void Inspector::ConnectDebugger(int port) {
    v8::HandleScope handle_scope(isolate_);

    if (!session_) {
        session_ = inspector_->connect(
            context_group_id_,
            channel_.get(),
            v8_inspector::StringView(),
            v8_inspector::V8Inspector::kFullyTrusted//,
            //v8_inspector::V8InspectorSession::WaitForDebuggerOnStart
        );

        // Enable required domains
        const char* enable_debugger = "{\"id\":1,\"method\":\"Debugger.enable\"}";
        const char* enable_runtime = "{\"id\":2,\"method\":\"Runtime.enable\"}";
        const char* enable_console = "{\"id\":3,\"method\":\"Console.enable\"}";

        SendMessage(enable_debugger);
        SendMessage(enable_runtime);
        SendMessage(enable_console);
    }
}

void Inspector::DisconnectDebugger() {
    if (session_) {
        session_.reset();
    }
}

void Inspector::SendMessage(const std::string& message) {
    if (session_) {
        v8::HandleScope handle_scope(isolate_);
        v8_inspector::StringView message_view(
            reinterpret_cast<const uint8_t*>(message.c_str()),
            message.length()
        );
        session_->dispatchProtocolMessage(message_view);
    }
}

void Inspector::QueueMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(message_mutex_);
    message_queue_.push(message);
}

std::string Inspector::GetNextMessage() {
    std::lock_guard<std::mutex> lock(message_mutex_);
    if (message_queue_.empty()) {
        return "";
    }
    std::string message = message_queue_.front();
    message_queue_.pop();
    return message;
}

void Inspector::runMessageLoopOnPause(int contextGroupId) {
    if (running_nested_loop_) return;

    running_nested_loop_ = true;
    while (running_nested_loop_) {
        {
            v8::SealHandleScope seal_handle_scope(isolate_);
            v8::Local<v8::Context> context = context_.Get(isolate_);
            v8::MicrotaskQueue* microtask_queue = context->GetMicrotaskQueue();
            if (microtask_queue) {
                microtask_queue->PerformCheckpoint(isolate_);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Inspector::quitMessageLoopOnPause() {
    running_nested_loop_ = false;
}

void Inspector::runIfWaitingForDebugger(int contextGroupId) {
    running_nested_loop_ = false;
}

} // namespace STPyV8