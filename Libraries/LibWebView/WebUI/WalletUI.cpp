/*
 * Copyright (c) 2026, the Bottlebird contributors.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/JsonObject.h>
#include <LibURL/Parser.h>
#include <LibWallet/WalletManager.h>
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
    register_interface("importBackupFile"sv, [this](auto const& data) {
        import_backup_file(data);
    });
}

void WalletUI::load_wallet_status()
{
    auto const& settings = WebView::Application::settings();
    auto& wallet = Wallet::WalletManager::the();

    JsonObject status;
    status.set("initialized"sv, wallet.is_initialized());
    status.set("enabled"sv, settings.wallet_enabled());
    status.set("backendURL"sv, settings.wallet_backend_url().serialize());
    status.set("connected"sv, false);

    if (wallet.is_initialized()) {
        auto bap_id = wallet.get_bap_id();
        if (!bap_id.is_error())
            status.set("bapId"sv, bap_id.release_value());

        auto identity = wallet.get_identity_pubkey();
        if (!identity.is_error())
            status.set("identityPubkey"sv, identity.release_value());
    }

    async_send_message("walletStatus"sv, move(status));
}

void WalletUI::get_balance()
{
    // TODO: Fetch real balance from wallet backend
    JsonObject balance;
    balance.set("confirmed"sv, 0);
    balance.set("unconfirmed"sv, 0);

    async_send_message("walletBalance"sv, move(balance));
}

void WalletUI::get_receive_address()
{
    auto& wallet = Wallet::WalletManager::the();

    JsonObject address;
    if (wallet.is_initialized()) {
        auto addr = wallet.get_receive_address();
        if (!addr.is_error())
            address.set("address"sv, addr.release_value());
        else
            address.set("address"sv, ""sv);
    } else {
        address.set("address"sv, ""sv);
    }

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

    // TODO: Build and broadcast transaction via LibWallet
    JsonObject result;
    result.set("error"sv, "Transaction broadcasting not yet implemented"sv);
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
    auto& wallet = Wallet::WalletManager::the();
    auto result = wallet.create_wallet();

    if (result.is_error()) {
        JsonObject error;
        error.set("error"sv, MUST(String::formatted("Failed to create wallet: {}", result.error())));
        async_send_message("walletCreated"sv, move(error));
        return;
    }

    m_pending_mnemonic = result.release_value();

    JsonObject response;
    response.set("mnemonic"sv, m_pending_mnemonic);
    async_send_message("walletCreated"sv, move(response));
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
    auto& wallet = Wallet::WalletManager::the();
    auto result = wallet.import_from_mnemonic(mnemonic.bytes_as_string_view());

    if (result.is_error()) {
        JsonObject error;
        error.set("error"sv, MUST(String::formatted("{}", result.error())));
        async_send_message("walletImported"sv, move(error));
        return;
    }

    WebView::Application::settings().set_wallet_enabled(true);

    JsonObject response;
    response.set("success"sv, true);
    async_send_message("walletImported"sv, move(response));
}

void WalletUI::import_backup_file(JsonValue const& data)
{
    if (!data.is_object()) {
        JsonObject error;
        error.set("error"sv, "Invalid request"sv);
        async_send_message("walletImported"sv, move(error));
        return;
    }

    auto file_data = data.as_object().get_string("data"sv);
    auto password = data.as_object().get_string("password"sv);

    if (!file_data.has_value()) {
        JsonObject error;
        error.set("error"sv, "No file data provided"sv);
        async_send_message("walletImported"sv, move(error));
        return;
    }

    // Try to parse as unencrypted JSON and extract the key
    auto maybe_json = JsonValue::from_string(*file_data);
    if (!maybe_json.is_error() && maybe_json.value().is_object()) {
        auto const& obj = maybe_json.value().as_object();
        auto& wallet = Wallet::WalletManager::the();

        // Try WIF import
        if (obj.has_string("wif"sv)) {
            auto wif = obj.get_string("wif"sv);
            if (wif.has_value()) {
                auto result = wallet.import_from_wif(wif->bytes_as_string_view());
                if (!result.is_error()) {
                    WebView::Application::settings().set_wallet_enabled(true);
                    JsonObject response;
                    response.set("success"sv, true);
                    async_send_message("walletImported"sv, move(response));
                    return;
                }
            }
        }

        // Try mnemonic import
        if (obj.has_string("mnemonic"sv)) {
            auto mnemonic = obj.get_string("mnemonic"sv);
            if (mnemonic.has_value()) {
                auto result = wallet.import_from_mnemonic(mnemonic->bytes_as_string_view());
                if (!result.is_error()) {
                    WebView::Application::settings().set_wallet_enabled(true);
                    JsonObject response;
                    response.set("success"sv, true);
                    async_send_message("walletImported"sv, move(response));
                    return;
                }
            }
        }

        // Detect other backup types we recognize but haven't implemented
        if (obj.has_string("rootPk"sv) || obj.has_string("xprv"sv)) {
            JsonObject error;
            error.set("error"sv, "This backup format is not yet supported. Use recovery phrase import instead."sv);
            async_send_message("walletImported"sv, move(error));
            return;
        }
    }

    // If we got here with a password, it's encrypted but we can't decrypt yet
    if (password.has_value() && !password->is_empty()) {
        JsonObject error;
        error.set("error"sv, "Encrypted backup decryption not yet implemented. Use recovery phrase import instead."sv);
        async_send_message("walletImported"sv, move(error));
        return;
    }

    JsonObject error;
    error.set("error"sv, "Unrecognized backup format"sv);
    async_send_message("walletImported"sv, move(error));
}

void WalletUI::confirm_wallet_creation()
{
    if (m_pending_mnemonic.is_empty())
        return;

    // Keys were already derived during create_wallet(), just enable the wallet
    WebView::Application::settings().set_wallet_enabled(true);
    m_pending_mnemonic = {};

    load_wallet_status();
}

}
