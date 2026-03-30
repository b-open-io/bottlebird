/*
 * Copyright (c) 2026, Bottlebird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/NonnullRefPtr.h>
#include <LibCore/Forward.h>
#include <LibCore/Socket.h>

namespace WebView {

class MCPToolRegistry;

constexpr inline u16 default_mcp_port = 3322;

class MCPServer {
public:
    static ErrorOr<NonnullOwnPtr<MCPServer>> create(u16 port);
    ~MCPServer();

private:
    explicit MCPServer(NonnullRefPtr<Core::TCPServer>, NonnullOwnPtr<MCPToolRegistry>);

    ErrorOr<void> on_new_client();
    ErrorOr<void> handle_http_request(Core::BufferedTCPSocket&);

    JsonValue handle_jsonrpc(JsonObject const& request);
    JsonValue handle_initialize(JsonValue const& id);
    JsonValue handle_tools_list(JsonValue const& id);
    JsonValue handle_tools_call(JsonValue const& id, JsonObject const& params);

    static JsonValue make_error_response(JsonValue const& id, int code, StringView message);
    static JsonValue make_success_response(JsonValue const& id, JsonValue result);

    NonnullRefPtr<Core::TCPServer> m_server;
    NonnullOwnPtr<MCPToolRegistry> m_tools;
};

}
