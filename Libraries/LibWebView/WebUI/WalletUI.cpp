/*
 * Copyright (c) 2026, the Bottlebird contributors.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/JsonObject.h>
#include <AK/Random.h>
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
    register_interface("createWallet"sv, [this](auto const&) {
        create_wallet();
    });
    register_interface("importWallet"sv, [this](auto const& data) {
        import_wallet(data);
    });
    register_interface("confirmWalletCreation"sv, [this](auto const&) {
        confirm_wallet_creation();
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
    // TODO: Fetch real balance from wallet backend via LibWallet
    JsonObject balance;
    balance.set("confirmed"sv, 0);
    balance.set("unconfirmed"sv, 0);

    async_send_message("walletBalance"sv, move(balance));
}

void WalletUI::get_receive_address()
{
    // TODO: Derive real address from wallet key via LibWallet
    JsonObject address;
    address.set("address"sv, ""sv);

    async_send_message("receiveAddress"sv, move(address));
}

void WalletUI::send_payment(JsonValue const& data)
{
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

    // TODO: Send via LibWallet
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

void WalletUI::create_wallet()
{
    // TODO: Use bsvz BIP39 mnemonic generation via LibWallet C ABI.
    // For now, generate a placeholder 12-word mnemonic to demonstrate the flow.
    // This will be replaced with real bsvz_mnemonic_generate() call.
    static constexpr Array placeholder_words = {
        "abandon"sv, "ability"sv, "able"sv, "about"sv, "above"sv, "absent"sv,
        "absorb"sv, "abstract"sv, "absurd"sv, "abuse"sv, "access"sv, "accident"sv,
        "acquire"sv, "across"sv, "act"sv, "action"sv, "actual"sv, "adapt"sv,
        "add"sv, "addict"sv, "address"sv, "adjust"sv, "admit"sv, "adult"sv,
    };

    StringBuilder mnemonic_builder;
    for (int i = 0; i < 12; i++) {
        if (i > 0)
            mnemonic_builder.append(' ');
        // Pick pseudo-random words for demo (NOT cryptographically secure - placeholder only)
        u32 random_value = 0;
        fill_with_random({ reinterpret_cast<u8*>(&random_value), sizeof(random_value) });
        mnemonic_builder.append(placeholder_words[random_value % placeholder_words.size()]);
    }

    m_pending_mnemonic = mnemonic_builder.to_string_without_validation();

    JsonObject result;
    result.set("mnemonic"sv, m_pending_mnemonic);
    async_send_message("walletCreated"sv, move(result));
}

void WalletUI::import_wallet(JsonValue const& data)
{
    if (!data.is_string()) {
        JsonObject error;
        error.set("error"sv, "Invalid mnemonic format"sv);
        async_send_message("walletImported"sv, move(error));
        return;
    }

    auto mnemonic = data.as_string();
    auto words = mnemonic.bytes_as_string_view().split_view(' ');

    if (words.size() != 12 && words.size() != 24) {
        JsonObject error;
        error.set("error"sv, MUST(String::formatted("Expected 12 or 24 words, got {}", words.size())));
        async_send_message("walletImported"sv, move(error));
        return;
    }

    // TODO: Validate mnemonic checksum via bsvz and derive keys via LibWallet.
    // For now, accept any 12/24 word input and enable the wallet.
    m_pending_mnemonic = mnemonic;

    WebView::Application::settings().set_wallet_enabled(true);

    JsonObject result;
    result.set("success"sv, true);
    async_send_message("walletImported"sv, move(result));
}

void WalletUI::confirm_wallet_creation()
{
    if (m_pending_mnemonic.is_empty())
        return;

    // TODO: Store mnemonic securely via SecureEnclave + Vault (LibWallet).
    // For now, just enable the wallet.
    WebView::Application::settings().set_wallet_enabled(true);
    m_pending_mnemonic = {};

    load_wallet_status();
}

}
