/*
 * Copyright (c) 2026, Bottlebird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteBuffer.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>
#include <LibCore/Socket.h>
#include <LibCore/TCPServer.h>
#include <LibWallet/WalletManager.h>
#include <LibWebView/Application.h>
#include <LibWebView/WalletServer.h>

namespace WebView {

// All 28 BRC-100 WalletInterface method names.
static constexpr Array wallet_methods = {
    "createAction"sv,
    "signAction"sv,
    "abortAction"sv,
    "listActions"sv,
    "internalizeAction"sv,
    "listOutputs"sv,
    "relinquishOutput"sv,
    "getPublicKey"sv,
    "revealCounterpartyKeyLinkage"sv,
    "revealSpecificKeyLinkage"sv,
    "encrypt"sv,
    "decrypt"sv,
    "createHmac"sv,
    "verifyHmac"sv,
    "createSignature"sv,
    "verifySignature"sv,
    "acquireCertificate"sv,
    "listCertificates"sv,
    "proveCertificate"sv,
    "relinquishCertificate"sv,
    "discoverByIdentityKey"sv,
    "discoverByAttributes"sv,
    "isAuthenticated"sv,
    "waitForAuthentication"sv,
    "getHeight"sv,
    "getHeaderForHeight"sv,
    "getNetwork"sv,
    "getVersion"sv,
};

static bool is_known_wallet_method(StringView method)
{
    for (auto const& m : wallet_methods) {
        if (m == method)
            return true;
    }
    return false;
}

static void send_json_response(Core::BufferedTCPSocket& socket, StringView status_line, StringView body)
{
    StringBuilder response;
    response.append(status_line);
    response.append("Content-Type: application/json\r\n"sv);
    response.append("Access-Control-Allow-Origin: *\r\n"sv);
    response.append("Access-Control-Allow-Methods: POST, OPTIONS\r\n"sv);
    response.append("Access-Control-Allow-Headers: Content-Type\r\n"sv);
    response.appendff("Content-Length: {}\r\n", body.length());
    response.append("\r\n"sv);
    response.append(body);

    auto result = socket.write_until_depleted(response.string_view().bytes());
    if (result.is_error())
        warnln("WalletServer: Failed to write response: {}", result.error());
}

ErrorOr<NonnullOwnPtr<WalletServer>> WalletServer::create(u16 port)
{
    auto server = TRY(Core::TCPServer::try_create());
    TRY(server->listen(IPv4Address::from_string("127.0.0.1"sv).release_value(), port, Core::TCPServer::AllowAddressReuse::Yes));

    dbgln("BRC-100 wallet server listening on http://127.0.0.1:{}", port);

    return adopt_own(*new WalletServer(move(server)));
}

WalletServer::WalletServer(NonnullRefPtr<Core::TCPServer> server)
    : m_server(move(server))
{
    m_server->on_ready_to_accept = [this]() {
        if (auto result = on_new_client(); result.is_error())
            warnln("WalletServer: Failed to accept client: {}", result.error());
    };
}

WalletServer::~WalletServer() = default;

ErrorOr<void> WalletServer::on_new_client()
{
    auto client = TRY(m_server->accept());

    // Set the socket to blocking mode so we can read the full HTTP request.
    // This is acceptable since BRC-100 requests come from localhost dApps and
    // should complete quickly.
    TRY(client->set_blocking(true));

    auto buffered = TRY(Core::BufferedTCPSocket::create(move(client)));

    if (auto result = handle_http_request(*buffered); result.is_error())
        warnln("WalletServer: Error handling request: {}", result.error());

    return {};
}

ErrorOr<void> WalletServer::handle_http_request(Core::BufferedTCPSocket& socket)
{
    // Read the HTTP request line and headers.
    ByteBuffer line_buffer;
    line_buffer.resize(4096);

    auto request_line = TRY(socket.read_line(line_buffer));

    bool is_post = request_line.starts_with("POST "sv);
    bool is_options = request_line.starts_with("OPTIONS "sv);

    // Extract the path from the request line: "POST /methodName HTTP/1.1"
    StringView path;
    if (auto space1 = request_line.find(' '); space1.has_value()) {
        auto rest = request_line.substring_view(*space1 + 1);
        if (auto space2 = rest.find(' '); space2.has_value())
            path = rest.substring_view(0, *space2);
        else
            path = rest;
    }

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
        send_json_response(socket, "HTTP/1.1 405 Method Not Allowed\r\n"sv,
            "{\"error\":\"Method not allowed. Use POST.\"}"sv);
        return {};
    }

    // Read the JSON body.
    ByteBuffer body_buffer;
    if (content_length > 0) {
        body_buffer.resize(content_length);
        TRY(socket.read_until_filled(body_buffer));
    }

    auto body_view = StringView { body_buffer };

    // Strip the leading / from the path to get the method name.
    StringView method_name;
    if (path.starts_with('/'))
        method_name = path.substring_view(1);
    else
        method_name = path;

    if (!is_known_wallet_method(method_name)) {
        auto error_body = MUST(String::formatted("{{\"error\":\"Unknown endpoint: {}\"}}", path));
        send_json_response(socket, "HTTP/1.1 404 Not Found\r\n"sv, error_body);
        return {};
    }

    auto result = handle_wallet_method(method_name, body_view);
    if (result.is_error()) {
        auto error_body = MUST(String::formatted("{{\"error\":\"{}\"}}", result.error()));
        send_json_response(socket, "HTTP/1.1 400 Bad Request\r\n"sv, error_body);
        return {};
    }

    send_json_response(socket, "HTTP/1.1 200 OK\r\n"sv, result.value());

    return {};
}

ErrorOr<String> WalletServer::handle_wallet_method(StringView method, StringView body)
{
    auto& wallet = Wallet::WalletManager::the();

    // --- Simple methods that don't need wallet initialization ---

    if (method == "getVersion"sv) {
        return String::from_utf8("{\"version\":\"0.1.0\"}"sv);
    }

    if (method == "getNetwork"sv) {
        return String::from_utf8("{\"network\":\"mainnet\"}"sv);
    }

    if (method == "isAuthenticated"sv) {
        if (wallet.is_initialized())
            return String::from_utf8("{\"authenticated\":true}"sv);
        return String::from_utf8("{\"authenticated\":false}"sv);
    }

    if (method == "waitForAuthentication"sv) {
        // Return current auth state immediately; no blocking wait.
        if (wallet.is_initialized())
            return String::from_utf8("{\"authenticated\":true}"sv);
        return String::from_utf8("{\"authenticated\":false}"sv);
    }

    // --- Methods that require an initialized wallet ---

    if (!wallet.is_initialized())
        return Error::from_string_literal("Wallet is not initialized");

    if (method == "getPublicKey"sv) {
        // Parse optional args for key derivation
        auto parsed = JsonValue::from_string(body);
        if (!parsed.is_error() && parsed.value().is_object()) {
            auto const& args = parsed.value().as_object();
            auto protocol_id = args.get_string("protocolID"sv);
            auto key_id = args.get_string("keyID"sv);
            auto security_level = args.get_integer<u8>("securityLevel"sv);

            // If derivation parameters are provided, use derived key
            if (protocol_id.has_value() && key_id.has_value()) {
                auto for_self = !args.has("counterparty"sv);
                auto counterparty_opt = args.get_string("counterparty"sv);
                auto counterparty = counterparty_opt.has_value() ? *counterparty_opt : String {};
                auto level = security_level.value_or(1);

                auto pubkey = TRY(wallet.get_derived_public_key(*protocol_id, *key_id, level, for_self, counterparty));
                return TRY(String::formatted("{{\"publicKey\":\"{}\"}}", pubkey));
            }
        }

        // Default: return identity public key
        auto pubkey = TRY(wallet.get_identity_pubkey());
        return TRY(String::formatted("{{\"publicKey\":\"{}\"}}", pubkey));
    }

    if (method == "listOutputs"sv) {
        auto& settings = Application::settings();
        auto backend_url = settings.wallet_backend_url().serialize();

        // Parse args to determine basket
        auto parsed = JsonValue::from_string(body);
        StringView basket = "default"sv;
        if (!parsed.is_error() && parsed.value().is_object()) {
            auto const& args = parsed.value().as_object();
            auto basket_arg = args.get_string("basket"sv);
            if (basket_arg.has_value())
                basket = *basket_arg;
        }

        if (basket == "ordinals"sv)
            return wallet.fetch_ordinals(backend_url);
        if (basket == "tokens"sv)
            return wallet.fetch_tokens(backend_url);

        // Default: return balance as output summary
        return wallet.fetch_balance(backend_url);
    }

    if (method == "createAction"sv) {
        // createAction requires wiring through WalletManager to
        // bsvwallet_create_action_remote. Stubbed until WalletManager
        // exposes a create_action(StringView args_json) method.
        return TRY(String::from_utf8("{\"error\":\"Not yet implemented: createAction\"}"sv));
    }

    if (method == "signAction"sv) {
        // signAction requires wiring through WalletManager to
        // bsvwallet_sign_action_remote. Stubbed until WalletManager
        // exposes a sign_action(StringView args_json) method.
        return TRY(String::from_utf8("{\"error\":\"Not yet implemented: signAction\"}"sv));
    }

    // --- Stub methods ---

    if (method == "getHeight"sv
        || method == "getHeaderForHeight"sv
        || method == "abortAction"sv
        || method == "listActions"sv
        || method == "internalizeAction"sv
        || method == "relinquishOutput"sv
        || method == "revealCounterpartyKeyLinkage"sv
        || method == "revealSpecificKeyLinkage"sv
        || method == "encrypt"sv
        || method == "decrypt"sv
        || method == "createHmac"sv
        || method == "verifyHmac"sv
        || method == "createSignature"sv
        || method == "verifySignature"sv
        || method == "acquireCertificate"sv
        || method == "listCertificates"sv
        || method == "proveCertificate"sv
        || method == "relinquishCertificate"sv
        || method == "discoverByIdentityKey"sv
        || method == "discoverByAttributes"sv) {
        return TRY(String::formatted("{{\"error\":\"Not yet implemented: {}\"}}", method));
    }

    return Error::from_string_literal("Unknown wallet method");
}

}
