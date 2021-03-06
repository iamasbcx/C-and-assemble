// server.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include"rpc_h.h"
#include"rpc_s.c"

#pragma comment(lib, "Rpcrt4.lib")


static TOKEN_USER* GetProcessUser() {
    BOOL        success;
    DWORD       length = 0;
    HANDLE      h_token;
    TOKEN_USER* result;

    success = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &h_token);

    if (!success) {
        return NULL;
    }

    GetTokenInformation(h_token, TokenUser, NULL, 0, &length);

    if (length <= 0) {
        return NULL;
    }

    result = (TOKEN_USER*)HeapAlloc(GetProcessHeap(),
        HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS,
        length);

    success = GetTokenInformation(h_token, TokenUser, result, length, &length);

    if (!success) {
        return NULL;
    }

    CloseHandle(h_token);

    return result;
}

RPC_STATUS CALLBACK RpcSecurityCallback(RPC_IF_HANDLE Interface, void* Context)
{
    RPC_STATUS        Status;
    BOOL              bAuthorized;
    HANDLE            h_client = Context;
    RPC_AUTHZ_HANDLE  privs = NULL;
    unsigned long     ulAuthenticationLevel,
        authentication_service,
        authorization_service;
    TOKEN_USER* client_user,
        * server_user = NULL;

    Status = RpcBindingInqAuthClient(h_client,
        &privs,
        NULL,
        &ulAuthenticationLevel,
        &authentication_service,
        &authorization_service);

    if (Status != RPC_S_OK) {
        return RPC_S_ACCESS_DENIED;
    }

    if (ulAuthenticationLevel < RPC_C_AUTHN_LEVEL_PKT_PRIVACY)
        /* Not secure enough - and it's impossible to get a lower level for
         * ncalrpc anyway */
        return RPC_S_ACCESS_DENIED;

    //切换到发送端的上下文环境
    Status = RpcImpersonateClient(h_client);

    if (Status != RPC_S_OK) {
        return RPC_S_ACCESS_DENIED;
    }

    client_user = GetProcessUser();

    Status = RpcRevertToSelf();

    if (Status != RPC_S_OK) {
        return RPC_S_ACCESS_DENIED;
    }

    server_user = GetProcessUser();

    if (client_user == NULL || server_user == NULL)
        return RPC_S_ACCESS_DENIED;

    bAuthorized = EqualSid(client_user->User.Sid, server_user->User.Sid);

    HeapFree(GetProcessHeap(), 0, client_user);
    HeapFree(GetProcessHeap(), 0, server_user);

    if (bAuthorized)
        return RPC_S_OK;
    else
        return RPC_S_ACCESS_DENIED;

    return RPC_S_OK;
}

int main()
{
    RPC_STATUS Status = RPC_S_OK;

    Status = RpcServerUseProtseqEp(
        RPC_WSTR(L"ncalrpc"),                      //使用LPC通信
        RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
        RPC_WSTR(L"{0AD3C2E4-C14D-48E1-9CAF-502CFD189EE0}"),
        NULL);
    if (Status != RPC_S_OK)
        return 0;

    Status = RpcServerRegisterIfEx(
        RpcService_v1_0_s_ifspec, // Interface to register.
        NULL,
        NULL, // Use the MIDL generated entry-point vector.
        RPC_IF_ALLOW_SECURE_ONLY,
        0,
        RpcSecurityCallback);

    if (Status != RPC_S_OK)
    {
        return 0;
    }

    Status = RpcServerListen(1, 20, FALSE);
    if (Status != RPC_S_OK)
    {
        return 0;
    }

    return 0;
}

HRESULT RpcCall(PParam lpParam)
{
    printf("RPC Call\n");
    return 1;
}

/* [async] */ void  RpcAsyncCall(
    /* [in] */ PRPC_ASYNC_STATE RpcAsyncCall_AsyncHandle,
    /* [out][in] */ PParam lpParam)
{
    printf("RPC Asysc Call\n");
    Sleep(4000);
    BOOL bRet = 1;

    lpParam->Type = 1;
    lpParam->uParam.Param.a = 2;
    RpcAsyncCompleteCall(RpcAsyncCall_AsyncHandle, &bRet);
    return;
}

void __RPC_FAR* __RPC_USER midl_user_allocate(size_t len)
{
    return(malloc(len));
}

void __RPC_USER  midl_user_free(void __RPC_FAR *ptr)
{
    free(ptr);
}
