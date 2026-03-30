/*
 * Copyright (c) 2026, Bottlebird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <LibURL/Parser.h>
#include <LibWallet/WalletManager.h>
#include <LibWebView/Application.h>
#include <LibWebView/MCPTools.h>
#include <LibWebView/ViewImplementation.h>

namespace WebView {

MCPToolRegistry::MCPToolRegistry()
{
    register_wallet_tools();
    register_browser_tools();
}

void MCPToolRegistry::register_tool(MCPToolDefinition definition, MCPToolHandler handler)
{
    auto name = definition.name;
    m_tools.append(move(definition));
    m_handlers.set(name, move(handler));
}

ErrorOr<JsonValue> MCPToolRegistry::call_tool(StringView name, JsonObject const& args)
{
    auto it = m_handlers.find(name);
    if (it == m_handlers.end())
        return Error::from_string_literal("Unknown tool");

    return it->value(args);
}

static JsonObject make_empty_schema()
{
    JsonObject schema;
    schema.set("type"sv, "object"sv);
    schema.set("properties"sv, JsonObject {});
    return schema;
}

void MCPToolRegistry::register_wallet_tools()
{
    // wallet_status
    {
        MCPToolDefinition def;
        def.name = MUST(String::from_utf8("wallet_status"sv));
        def.description = MUST(String::from_utf8("Returns wallet lock status"sv));
        def.input_schema = make_empty_schema();

        register_tool(move(def), [](JsonObject const&) -> ErrorOr<JsonValue> {
            auto& wallet = Wallet::WalletManager::the();
            auto const& settings = Application::settings();

            JsonObject result;
            if (!wallet.is_initialized())
                result.set("status"sv, "not_configured"sv);
            else if (!settings.wallet_enabled())
                result.set("status"sv, "disabled"sv);
            else
                result.set("status"sv, "active"sv);
            return JsonValue(move(result));
        });
    }

    // wallet_identity
    {
        MCPToolDefinition def;
        def.name = MUST(String::from_utf8("wallet_identity"sv));
        def.description = MUST(String::from_utf8("Returns wallet identity key and address"sv));
        def.input_schema = make_empty_schema();

        register_tool(move(def), [](JsonObject const&) -> ErrorOr<JsonValue> {
            auto& wallet = Wallet::WalletManager::the();
            JsonObject result;

            if (!wallet.is_initialized()) {
                result.set("error"sv, "Wallet not initialized"sv);
                return JsonValue(move(result));
            }

            auto bap_id = wallet.get_bap_id();
            if (!bap_id.is_error())
                result.set("bapId"sv, JsonValue(bap_id.release_value()));

            auto address = wallet.get_receive_address();
            if (!address.is_error())
                result.set("address"sv, JsonValue(address.release_value()));

            auto pubkey = wallet.get_identity_pubkey();
            if (!pubkey.is_error())
                result.set("identityKey"sv, JsonValue(pubkey.release_value()));

            return JsonValue(move(result));
        });
    }

    // wallet_balance
    {
        MCPToolDefinition def;
        def.name = MUST(String::from_utf8("wallet_balance"sv));
        def.description = MUST(String::from_utf8("Returns wallet balance in satoshis"sv));
        def.input_schema = make_empty_schema();

        register_tool(move(def), [](JsonObject const&) -> ErrorOr<JsonValue> {
            auto& wallet = Wallet::WalletManager::the();
            JsonObject result;

            if (!wallet.is_initialized()) {
                result.set("error"sv, "Wallet not initialized"sv);
                return JsonValue(move(result));
            }

            // Balance requires async HTTP fetch which is not available in this synchronous context.
            // The wallet UI page fetches balance directly via JavaScript fetch().
            result.set("confirmed"sv, JsonValue(static_cast<i64>(0)));
            result.set("unconfirmed"sv, JsonValue(static_cast<i64>(0)));
            result.set("note"sv, "Balance requires async HTTP; use the wallet UI page for live balance"sv);
            return JsonValue(move(result));
        });
    }
}

void MCPToolRegistry::register_browser_tools()
{
    // browser_navigate
    {
        JsonObject properties;
        JsonObject url_prop;
        url_prop.set("type"sv, "string"sv);
        url_prop.set("description"sv, "URL to navigate to"sv);
        properties.set("url"sv, move(url_prop));

        JsonArray required;
        required.must_append("url"sv);

        JsonObject schema;
        schema.set("type"sv, "object"sv);
        schema.set("properties"sv, move(properties));
        schema.set("required"sv, move(required));

        MCPToolDefinition def;
        def.name = MUST(String::from_utf8("browser_navigate"sv));
        def.description = MUST(String::from_utf8("Navigate the active tab to a URL"sv));
        def.input_schema = move(schema);

        register_tool(move(def), [](JsonObject const& args) -> ErrorOr<JsonValue> {
            auto url_string = args.get_string("url"sv);
            if (!url_string.has_value())
                return Error::from_string_literal("Missing required parameter: url");

            auto url = URL::Parser::basic_parse(*url_string);
            if (!url.has_value())
                return Error::from_string_literal("Invalid URL");

            auto view = Application::the().active_web_view();
            if (!view.has_value())
                return Error::from_string_literal("No active tab");

            view->load(*url);

            JsonObject result;
            result.set("success"sv, true);
            result.set("url"sv, JsonValue(url->to_string()));
            return JsonValue(move(result));
        });
    }

    // browser_get_url
    {
        MCPToolDefinition def;
        def.name = MUST(String::from_utf8("browser_get_url"sv));
        def.description = MUST(String::from_utf8("Returns the current tab URL"sv));
        def.input_schema = make_empty_schema();

        register_tool(move(def), [](JsonObject const&) -> ErrorOr<JsonValue> {
            auto view = Application::the().active_web_view();
            if (!view.has_value()) {
                JsonObject result;
                result.set("url"sv, JsonValue {});
                return JsonValue(move(result));
            }

            JsonObject result;
            result.set("url"sv, JsonValue(view->url().to_string()));
            return JsonValue(move(result));
        });
    }

    // browser_list_tabs
    {
        MCPToolDefinition def;
        def.name = MUST(String::from_utf8("browser_list_tabs"sv));
        def.description = MUST(String::from_utf8("Returns list of open tabs with id, url, and title"sv));
        def.input_schema = make_empty_schema();

        register_tool(move(def), [](JsonObject const&) -> ErrorOr<JsonValue> {
            JsonArray tabs;

            ViewImplementation::for_each_view([&](ViewImplementation& view) {
                JsonObject tab;
                tab.set("id"sv, JsonValue(static_cast<i64>(view.view_id())));
                tab.set("url"sv, JsonValue(view.url().to_string()));
                tab.set("title"sv, JsonValue(view.title().to_utf8()));
                tabs.must_append(move(tab));
                return IterationDecision::Continue;
            });

            JsonObject result;
            result.set("tabs"sv, move(tabs));
            return JsonValue(move(result));
        });
    }
}

}
