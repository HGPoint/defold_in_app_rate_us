#include <dmsdk/sdk.h>

#include <assert.h>
#include <limits.h>
#include <mutex>

#include "inapp_review_private.h"

#define EXTENSION_NAME InAppReview
#define LIB_NAME "InAppReview"
#define MODULE_NAME "inapp_review"
#define DLIB_LOG_DOMAIN LIB_NAME

namespace
{
    static std::mutex g_StateMutex;
    static bool g_Active = false;
    static bool g_Pending = false;
    static bool g_HasCompletion = false;
    static int g_RequestId = 0;
    static int g_NextRequestId = 0;
    static InAppReviewCompletion g_Completion = INAPP_REVIEW_REQUESTED;
    // Only the Defold thread reads or writes the Lua callback.
    static dmScript::LuaCallbackInfo* g_Callback = 0;

    static const char* ResultReason(InAppReviewResult result)
    {
        switch (result)
        {
            case INAPP_REVIEW_UNSUPPORTED: return "unsupported";
            case INAPP_REVIEW_BUSY: return "busy";
            case INAPP_REVIEW_NOT_VISIBLE: return "not_visible";
            default: return "platform_error";
        }
    }

    static const char* CompletionStatus(InAppReviewCompletion completion)
    {
        switch (completion)
        {
            case INAPP_REVIEW_REQUESTED: return "requested";
            case INAPP_REVIEW_FINISHED: return "finished";
            default: return "error";
        }
    }

    static const char* CompletionReason(InAppReviewCompletion completion)
    {
        switch (completion)
        {
            case INAPP_REVIEW_REQUEST_FAILED: return "request_failed";
            case INAPP_REVIEW_LAUNCH_FAILED: return "launch_failed";
            case INAPP_REVIEW_COMPLETION_NOT_VISIBLE: return "not_visible";
            default: return 0;
        }
    }

    static int PushResult(lua_State* L, InAppReviewResult result)
    {
        lua_pushboolean(L, result == INAPP_REVIEW_OK);
        if (result == INAPP_REVIEW_OK)
            lua_pushnil(L);
        else
            lua_pushstring(L, ResultReason(result));
        return 2;
    }

    static int IsSupported(lua_State* L)
    {
        lua_pushboolean(L, InAppReviewPlatform_IsSupported());
        return 1;
    }

    static int RequestReview(lua_State* L)
    {
        if (lua_gettop(L) > 1)
            return luaL_error(L, "request_review accepts at most one callback");
        if (!lua_isnoneornil(L, 1))
            luaL_checktype(L, 1, LUA_TFUNCTION);

        {
            std::lock_guard<std::mutex> lock(g_StateMutex);
            if (g_Pending)
                return PushResult(L, INAPP_REVIEW_BUSY);
        }

        dmScript::LuaCallbackInfo* callback = 0;
        if (!lua_isnoneornil(L, 1))
        {
            callback = dmScript::CreateCallback(L, 1);
            if (!callback)
                return PushResult(L, INAPP_REVIEW_PLATFORM_ERROR);
        }

        int request_id;
        {
            std::lock_guard<std::mutex> lock(g_StateMutex);
            if (g_NextRequestId == INT_MAX)
                g_NextRequestId = 0;
            request_id = ++g_NextRequestId;
            g_RequestId = request_id;
            g_Pending = true;
            g_HasCompletion = false;
        }
        g_Callback = callback;

        const InAppReviewResult result = InAppReviewPlatform_Request(request_id);
        if (result != INAPP_REVIEW_OK)
        {
            {
                std::lock_guard<std::mutex> lock(g_StateMutex);
                g_Pending = false;
                g_HasCompletion = false;
            }
            g_Callback = 0;
            if (callback)
                dmScript::DestroyCallback(callback);
        }
        return PushResult(L, result);
    }

    static const luaL_reg MODULE_METHODS[] = {
        {"is_supported", IsSupported},
        {"request_review", RequestReview},
        {0, 0},
    };

    static dmExtension::Result AppInitialize(dmExtension::AppParams*)
    {
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result AppFinalize(dmExtension::AppParams*)
    {
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Initialize(dmExtension::Params* params)
    {
        const int top = lua_gettop(params->m_L);
        luaL_register(params->m_L, MODULE_NAME, MODULE_METHODS);
        lua_pop(params->m_L, 1);
        assert(top == lua_gettop(params->m_L));

        std::lock_guard<std::mutex> lock(g_StateMutex);
        g_Active = true;
        g_Pending = false;
        g_HasCompletion = false;
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Finalize(dmExtension::Params*)
    {
        {
            std::lock_guard<std::mutex> lock(g_StateMutex);
            g_Active = false;
            g_Pending = false;
            g_HasCompletion = false;
        }
        InAppReviewPlatform_Finalize();
        if (g_Callback)
        {
            dmScript::DestroyCallback(g_Callback);
            g_Callback = 0;
        }
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Update(dmExtension::Params*)
    {
        InAppReviewCompletion completion;
        {
            std::lock_guard<std::mutex> lock(g_StateMutex);
            if (!g_HasCompletion)
                return dmExtension::RESULT_OK;
            completion = g_Completion;
            g_HasCompletion = false;
            g_Pending = false;
        }

        dmScript::LuaCallbackInfo* callback = g_Callback;
        g_Callback = 0;
        if (!callback)
            return dmExtension::RESULT_OK;

        if (dmScript::IsCallbackValid(callback))
        {
            lua_State* L = dmScript::GetCallbackLuaContext(callback);
            const int top = lua_gettop(L);
            if (dmScript::SetupCallback(callback))
            {
                lua_newtable(L);
                lua_pushstring(L, CompletionStatus(completion));
                lua_setfield(L, -2, "status");
                const char* reason = CompletionReason(completion);
                if (reason)
                {
                    lua_pushstring(L, reason);
                    lua_setfield(L, -2, "reason");
                }
                dmScript::PCall(L, 2, 0);
                dmScript::TeardownCallback(callback);
            }
            assert(top == lua_gettop(L));
        }
        dmScript::DestroyCallback(callback);
        return dmExtension::RESULT_OK;
    }
}

void InAppReview_Complete(int request_id, InAppReviewCompletion completion)
{
    std::lock_guard<std::mutex> lock(g_StateMutex);
    if (!g_Active || !g_Pending || g_RequestId != request_id || g_HasCompletion)
        return;
    g_Completion = completion;
    g_HasCompletion = true;
}

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInitialize, AppFinalize, Initialize, Update, 0, Finalize)
