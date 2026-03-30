/*
 * Copyright (c) 2026, Bottlebird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/Function.h>
#include <AK/HashMap.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/String.h>
#include <AK/StringView.h>
#include <AK/Vector.h>

namespace WebView {

struct MCPToolDefinition {
    String name;
    String description;
    JsonObject input_schema;
};

using MCPToolHandler = Function<ErrorOr<JsonValue>(JsonObject const& args)>;

class MCPToolRegistry {
public:
    MCPToolRegistry();

    void register_tool(MCPToolDefinition definition, MCPToolHandler handler);

    Vector<MCPToolDefinition> const& tools() const { return m_tools; }
    ErrorOr<JsonValue> call_tool(StringView name, JsonObject const& args);

private:
    void register_wallet_tools();
    void register_browser_tools();

    Vector<MCPToolDefinition> m_tools;
    HashMap<String, MCPToolHandler> m_handlers;
};

}
