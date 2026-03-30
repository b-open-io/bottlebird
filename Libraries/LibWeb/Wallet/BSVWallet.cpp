/*
 * Copyright (c) 2026, the Bottlebird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <LibGC/Function.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/Realm.h>
#include <LibWeb/Bindings/BSVWalletPrototype.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/Page/Page.h>
#include <LibWeb/Wallet/BSVWallet.h>
#include <LibWeb/WebIDL/DOMException.h>
#include <LibWeb/WebIDL/Promise.h>

namespace Web::Wallet {

static JS::Value json_value_to_js(JS::VM& vm, JsonValue const& value);

static JS::Object* json_object_to_js(JS::VM& vm, JsonObject const& json_object)
{
    auto& realm = *vm.current_realm();
    auto object = JS::Object::create(realm, realm.intrinsics().object_prototype());
    json_object.for_each_member([&](auto& key, auto& val) {
        object->define_direct_property(Utf16String::from_utf8(key), json_value_to_js(vm, val), JS::default_attributes);
    });
    return object;
}

static JS::Array* json_array_to_js(JS::VM& vm, JsonArray const& json_array)
{
    auto& realm = *vm.current_realm();
    auto array = MUST(JS::Array::create(realm, 0));
    size_t index = 0;
    json_array.for_each([&](auto& val) {
        array->define_direct_property(index++, json_value_to_js(vm, val), JS::default_attributes);
    });
    return array;
}

static JS::Value json_value_to_js(JS::VM& vm, JsonValue const& value)
{
    if (value.is_object())
        return JS::Value(json_object_to_js(vm, value.as_object()));
    if (value.is_array())
        return JS::Value(json_array_to_js(vm, value.as_array()));
    if (value.is_null())
        return JS::js_null();
    if (auto double_value = value.get_double_with_precision_loss(); double_value.has_value())
        return JS::Value(double_value.value());
    if (value.is_string())
        return JS::PrimitiveString::create(vm, value.as_string());
    if (value.is_bool())
        return JS::Value(value.as_bool());
    return JS::js_undefined();
}

GC_DEFINE_ALLOCATOR(BSVWallet);

GC::Ref<BSVWallet> BSVWallet::create(JS::Realm& realm)
{
    return realm.create<BSVWallet>(realm);
}

BSVWallet::BSVWallet(JS::Realm& realm)
    : PlatformObject(realm)
{
}

BSVWallet::~BSVWallet() = default;

void BSVWallet::initialize(JS::Realm& realm)
{
    WEB_SET_PROTOTYPE_FOR_INTERFACE(BSVWallet);
    Base::initialize(realm);
}

void BSVWallet::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
}

GC::Ref<WebIDL::Promise> BSVWallet::send_wallet_operation(String const& operation, String const& params)
{
    auto& realm = this->realm();
    auto promise = WebIDL::create_promise(realm);

    auto& window = as<HTML::Window>(realm.global_object());
    auto& page = window.page();

    page.request_wallet_operation(operation, params,
        GC::create_function(realm.heap(), [&realm, promise](String result) {
            HTML::TemporaryExecutionContext context(realm, HTML::TemporaryExecutionContext::CallbacksEnabled::Yes);

            auto parsed = JsonValue::from_string(result);
            if (parsed.is_error()) {
                WebIDL::reject_promise(realm, *promise, JS::TypeError::create(realm, "Invalid wallet operation response"sv));
                return;
            }

            auto const& json = parsed.value();

            // Check for error response
            if (json.is_object() && json.as_object().has_string("error"sv)) {
                auto error_msg = json.as_object().get_string("error"sv).value();
                WebIDL::reject_promise(realm, *promise, WebIDL::NotSupportedError::create(realm, Utf16String::from_utf8(error_msg)));
                return;
            }

            // Convert JSON to JS value
            auto js_value = json_value_to_js(realm.vm(), json);
            WebIDL::resolve_promise(realm, *promise, js_value);
        }));

    return promise;
}

GC::Ref<WebIDL::Promise> BSVWallet::get_public_key(WalletKeyOptions const& options)
{
    JsonObject params;
    params.set("protocolID"sv, options.protocol_id);
    params.set("keyID"sv, options.key_id);
    params.set("forSelf"sv, options.for_self);
    return send_wallet_operation("getPublicKey"_string, params.serialized());
}

GC::Ref<WebIDL::Promise> BSVWallet::create_action(WalletActionOptions const& options)
{
    JsonObject params;
    params.set("description"sv, options.description);
    return send_wallet_operation("createAction"_string, params.serialized());
}

GC::Ref<WebIDL::Promise> BSVWallet::sign_action(WalletSignOptions const& options)
{
    JsonObject params;
    params.set("reference"sv, options.reference);
    return send_wallet_operation("signAction"_string, params.serialized());
}

GC::Ref<WebIDL::Promise> BSVWallet::list_outputs(WalletListOptions const& options)
{
    JsonObject params;
    params.set("basket"sv, options.basket);
    params.set("limit"sv, static_cast<i64>(options.limit));
    params.set("offset"sv, static_cast<i64>(options.offset));
    return send_wallet_operation("listOutputs"_string, params.serialized());
}

GC::Ref<WebIDL::Promise> BSVWallet::list_actions(WalletListOptions const& options)
{
    JsonObject params;
    params.set("basket"sv, options.basket);
    params.set("limit"sv, static_cast<i64>(options.limit));
    params.set("offset"sv, static_cast<i64>(options.offset));
    return send_wallet_operation("listActions"_string, params.serialized());
}

}
