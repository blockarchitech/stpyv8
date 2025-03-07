#pragma once

#include <cassert>
#include <memory>

#include "Isolate.h"
#include "Platform.h"
#include "Wrapper.h"
#include "Utils.h"

namespace STPyV8 { class Inspector; }

class CContext
{
    py::object m_global;
    v8::Persistent<v8::Context> m_context;
    std::unique_ptr<STPyV8::Inspector> m_inspector;

public:
    CContext(v8::Handle<v8::Context> context);
    CContext(const CContext& context);
    CContext(py::object global);
    ~CContext();

    v8::Handle<v8::Context> Handle(void) const {
        return v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), m_context);
    }
    std::string get_debugger_message();


    py::object GetGlobal(void);

    py::str GetSecurityToken(void);
    void SetSecurityToken(py::str token);

    bool IsEntered(void) {
        return !m_context.IsEmpty();
    }
    void Enter(void) {
        v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
        Handle()->Enter();
    }
    void Leave(void) {
        v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
        Handle()->Exit();
    }

    py::object Evaluate(const std::string& src, const std::string name = std::string(),
                        int line = -1, int col = -1);
    py::object EvaluateW(const std::wstring& src, const std::wstring name = std::wstring(),
                         int line = -1, int col = -1);

    void InitDebugger(int port = 9229);
    void DisconnectDebugger();
    void SendDebuggerMessage(const std::string& message);
    STPyV8::Inspector* GetInspector() { return m_inspector.get(); }

    static py::object GetEntered(void);
    static py::object GetCurrent(void);
    static py::object GetCalling(void);
    static bool InContext(void) {
        return v8::Isolate::GetCurrent()->InContext();
    }

    static void Expose(void);
};