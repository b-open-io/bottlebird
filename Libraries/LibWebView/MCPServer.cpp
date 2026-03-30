/*
 * Copyright (c) 2026, Bottlebird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteBuffer.h>
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>
#include <LibCore/Socket.h>
#include <LibCore/TCPServer.h>
#include <LibWebView/MCPServer.h>
#include <LibWebView/MCPTools.h>

namespace WebView {

ErrorOr<NonnullOwnPtr<MCPServer>> MCPServer::create(u16 port)
{
    auto server = TRY(Core::TCPServer::try_create());
    TRY(server->listen(IPv4Address::from_string("127.0.0.1"sv).release_value(), port, Core::TCPServer::AllowAddressReuse::Yes));

    auto tools = make<MCPToolRegistry>();

    dbgln("MCP server listening on http://127.0.0.1:{}", port);

    return adopt_own(*new MCPServer(move(server), move(tools)));
}

MCPServer::MCPServer(NonnullRefPtr<Core::TCPServer> server, NonnullOwnPtr<MCPToolRegistry> tools)
    : m_server(move(server))
    , m_tools(move(tools))
{
    m_server->on_ready_to_accept = [this]() {
        if (auto result = on_new_client(); result.is_error())
            warnln("MCP: Failed to accept client: {}", result.error());
    };
}

MCPServer::~MCPServer() = default;

ErrorOr<void> MCPServer::on_new_client()
{
    auto client = TRY(m_server->accept());

    // Set the socket to blocking mode so we can read the full HTTP request.
    // This is acceptable since MCP requests come from localhost tooling and
    // should complete quickly.
    TRY(client->set_blocking(true));

    auto buffered = TRY(Core::BufferedTCPSocket::create(move(client)));

    if (auto result = handle_http_request(*buffered); result.is_error())
        warnln("MCP: Error handling request: {}", result.error());

    return {};
}

ErrorOr<void> MCPServer::handle_http_request(Core::BufferedTCPSocket& socket)
{
    // Read the HTTP request line and headers.
    // We only care about POST requests with a JSON body.
    ByteBuffer line_buffer;
    line_buffer.resize(4096);

    auto request_line = TRY(socket.read_line(line_buffer));

    bool is_post = request_line.starts_with("POST "sv);
    bool is_options = request_line.starts_with("OPTIONS "sv);

    // Read headers to find Content-Length.
    size_t content_length = 0;
    while (true) {
        auto header_line = TRY(socket.read_line(line_buffer));
        if (header_line.is_empty() || header_line == "\r"sv)
            break;

        auto trimmed = header_line.trim_whitespace();
        if (trimmed.starts_with("Content-Length:"sv, CaseSensitivity::CaseInsensitive)) {
            auto value = trimmed.substring_view(15).trim_whitespace();
            content_length = value.to_number<size_t>().value_or(0);
        }
    }

    // Handle CORS preflight.
    if (is_options) {
        StringBuilder response;
        response.append("HTTP/1.1 204 No Content\r\n"sv);
        response.append("Access-Control-Allow-Origin: *\r\n"sv);
        response.append("Access-Control-Allow-Methods: POST, OPTIONS\r\n"sv);
        response.append("Access-Control-Allow-Headers: Content-Type\r\n"sv);
        response.append("Content-Length: 0\r\n"sv);
        response.append("\r\n"sv);
        TRY(socket.write_until_depleted(response.string_view().bytes()));
        return {};
    }

    if (!is_post) {
        // Return 405 Method Not Allowed for non-POST requests.
        auto body = "{\"error\":\"Method not allowed. Use POST.\"}"sv;
        StringBuilder response;
        response.append("HTTP/1.1 405 Method Not Allowed\r\n"sv);
        response.append("Content-Type: application/json\r\n"sv);
        response.append("Access-Control-Allow-Origin: *\r\n"sv);
        response.appendff("Content-Length: {}\r\n", body.length());
        response.append("\r\n"sv);
        response.append(body);
        TRY(socket.write_until_depleted(response.string_view().bytes()));
        return {};
    }

    // Read the JSON body.
    ByteBuffer body_buffer;
    if (content_length > 0) {
        body_buffer.resize(content_length);
        TRY(socket.read_until_filled(body_buffer));
    }

    auto body_view = StringView { body_buffer };
    auto maybe_json = JsonValue::from_string(body_view);

    JsonValue json_response;
    if (maybe_json.is_error() || !maybe_json.value().is_object()) {
        json_response = make_error_response(JsonValue {}, -32700, "Parse error"sv);
    } else {
        json_response = handle_jsonrpc(maybe_json.value().as_object());
    }

    auto response_body = json_response.serialized();

    StringBuilder http_response;
    http_response.append("HTTP/1.1 200 OK\r\n"sv);
    http_response.append("Content-Type: application/json\r\n"sv);
    http_response.append("Access-Control-Allow-Origin: *\r\n"sv);
    http_response.appendff("Content-Length: {}\r\n", response_body.byte_count());
    http_response.append("\r\n"sv);
    http_response.append(response_body);

    TRY(socket.write_until_depleted(http_response.string_view().bytes()));

    return {};
}

JsonValue MCPServer::handle_jsonrpc(JsonObject const& request)
{
    auto jsonrpc = request.get_string("jsonrpc"sv);
    if (!jsonrpc.has_value() || *jsonrpc != "2.0"sv) {
        return make_error_response(JsonValue {}, -32600, "Invalid Request: missing or wrong jsonrpc version"sv);
    }

    auto method = request.get_string("method"sv);
    if (!method.has_value()) {
        return make_error_response(JsonValue {}, -32600, "Invalid Request: missing method"sv);
    }

    auto id = request.get("id"sv);
    JsonValue id_value = id.has_value() ? *id : JsonValue {};

    auto params = request.get("params"sv);
    JsonObject params_obj;
    if (params.has_value() && params->is_object())
        params_obj = params->as_object();

    if (*method == "initialize"sv)
        return handle_initialize(id_value);
    if (*method == "tools/list"sv)
        return handle_tools_list(id_value);
    if (*method == "tools/call"sv)
        return handle_tools_call(id_value, params_obj);

    return make_error_response(id_value, -32601, "Method not found"sv);
}

JsonValue MCPServer::handle_initialize(JsonValue const& id)
{
    JsonObject capabilities;

    JsonObject tools_cap;
    tools_cap.set("listChanged"sv, false);
    capabilities.set("tools"sv, move(tools_cap));

    JsonObject server_info;
    server_info.set("name"sv, "bottlebird-mcp"sv);
    server_info.set("version"sv, "0.1.0"sv);

    JsonObject result;
    result.set("protocolVersion"sv, "2024-11-05"sv);
    result.set("capabilities"sv, move(capabilities));
    result.set("serverInfo"sv, move(server_info));

    return make_success_response(id, JsonValue(move(result)));
}

JsonValue MCPServer::handle_tools_list(JsonValue const& id)
{
    JsonArray tools_array;

    for (auto const& tool : m_tools->tools()) {
        JsonObject tool_obj;
        tool_obj.set("name"sv, JsonValue(tool.name));
        tool_obj.set("description"sv, JsonValue(tool.description));
        tool_obj.set("inputSchema"sv, tool.input_schema);
        tools_array.must_append(move(tool_obj));
    }

    JsonObject result;
    result.set("tools"sv, move(tools_array));

    return make_success_response(id, JsonValue(move(result)));
}

JsonValue MCPServer::handle_tools_call(JsonValue const& id, JsonObject const& params)
{
    auto name = params.get_string("name"sv);
    if (!name.has_value())
        return make_error_response(id, -32602, "Missing parameter: name"sv);

    auto arguments = params.get("arguments"sv);
    JsonObject args;
    if (arguments.has_value() && arguments->is_object())
        args = arguments->as_object();

    auto result = m_tools->call_tool(*name, args);
    if (result.is_error()) {
        JsonArray content;
        JsonObject text_content;
        text_content.set("type"sv, "text"sv);
        text_content.set("text"sv, MUST(String::formatted("Error: {}", result.error())));
        content.must_append(move(text_content));

        JsonObject response_result;
        response_result.set("content"sv, move(content));
        response_result.set("isError"sv, true);
        return make_success_response(id, JsonValue(move(response_result)));
    }

    JsonArray content;
    JsonObject text_content;
    text_content.set("type"sv, "text"sv);
    text_content.set("text"sv, result.value().serialized());
    content.must_append(move(text_content));

    JsonObject response_result;
    response_result.set("content"sv, move(content));
    response_result.set("isError"sv, false);

    return make_success_response(id, JsonValue(move(response_result)));
}

JsonValue MCPServer::make_error_response(JsonValue const& id, int code, StringView message)
{
    JsonObject error;
    error.set("code"sv, JsonValue(static_cast<i64>(code)));
    error.set("message"sv, message);

    JsonObject response;
    response.set("jsonrpc"sv, "2.0"sv);
    response.set("id"sv, id);
    response.set("error"sv, move(error));
    return JsonValue(move(response));
}

JsonValue MCPServer::make_success_response(JsonValue const& id, JsonValue result)
{
    JsonObject response;
    response.set("jsonrpc"sv, "2.0"sv);
    response.set("id"sv, id);
    response.set("result"sv, move(result));
    return JsonValue(move(response));
}

}
