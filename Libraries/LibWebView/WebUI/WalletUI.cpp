/*
 * Copyright (c) 2025, the Ladybird contributors.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/JsonObject.h>
#include <LibURL/Parser.h>
#include <LibWebView/Application.h>
#include <LibWebView/WebUI/WalletUI.h>

namespace WebView {

void WalletUI::register_interfaces()
{
    register_interface("loadWalletStatus"sv, [this](auto const&) {
        load_wallet_status();
    });
    register_interface("getBalance"sv, [this](auto const&) {
        get_balance();
    });
    register_interface("getReceiveAddress"sv, [this](auto const&) {
        get_receive_address();
    });
    register_interface("sendPayment"sv, [this](auto const& data) {
        send_payment(data);
    });
    register_interface("setWalletEnabled"sv, [this](auto const& data) {
        set_wallet_enabled(data);
    });
    register_interface("setWalletBackendURL"sv, [this](auto const& data) {
        set_wallet_backend_url(data);
    });
}

void WalletUI::load_wallet_status()
{
    auto const& settings = WebView::Application::settings();

    JsonObject status;
    status.set("enabled"sv, settings.wallet_enabled());
    status.set("backendURL"sv, settings.wallet_backend_url().serialize());
    status.set("connected"sv, false);

    async_send_message("walletStatus"sv, move(status));
}

void WalletUI::get_balance()
{
    // FIXME: Implement actual balance fetching from the wallet backend.
    JsonObject balance;
    balance.set("confirmed"sv, 0);
    balance.set("unconfirmed"sv, 0);

    async_send_message("walletBalance"sv, move(balance));
}

void WalletUI::get_receive_address()
{
    // FIXME: Implement actual address derivation from the wallet.
    JsonObject address;
    address.set("address"sv, ""sv);

    async_send_message("receiveAddress"sv, move(address));
}

void WalletUI::send_payment(JsonValue const& data)
{
    // FIXME: Implement actual payment sending via the wallet backend.
    if (!data.is_object())
        return;

    auto recipient = data.as_object().get_string("recipient"sv);
    auto amount = data.as_object().get_integer<u64>("amount"sv);

    if (!recipient.has_value() || !amount.has_value()) {
        JsonObject error;
        error.set("error"sv, "Invalid payment parameters"sv);
        async_send_message("paymentResult"sv, move(error));
        return;
    }

    JsonObject result;
    result.set("error"sv, "Wallet backend not yet connected"sv);
    async_send_message("paymentResult"sv, move(result));
}

void WalletUI::set_wallet_enabled(JsonValue const& data)
{
    if (!data.is_bool())
        return;

    WebView::Application::settings().set_wallet_enabled(data.as_bool());
    load_wallet_status();
}

void WalletUI::set_wallet_backend_url(JsonValue const& data)
{
    if (!data.is_string())
        return;

    auto parsed_url = URL::Parser::basic_parse(data.as_string());
    if (!parsed_url.has_value())
        return;

    WebView::Application::settings().set_wallet_backend_url(parsed_url.release_value());
    load_wallet_status();
}

}
