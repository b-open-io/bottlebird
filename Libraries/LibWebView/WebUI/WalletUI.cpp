/*
 * Copyright (c) 2026, the Bottlebird contributors.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/LexicalPath.h>
#include <AK/StringBuilder.h>
#include <LibCore/Directory.h>
#include <LibCore/File.h>
#include <LibCore/StandardPaths.h>
#include <LibThreading/BackgroundAction.h>
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
    register_interface("listOrdinals"sv, [this](auto const&) {
        list_ordinals();
    });
    register_interface("listTokens"sv, [this](auto const&) {
        list_tokens();
    });
    register_interface("saveProfile"sv, [this](auto const& data) {
        save_profile(data);
    });
    register_interface("loadProfile"sv, [this](auto const&) {
        load_profile();
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
    auto& wallet = Wallet::WalletManager::the();
    auto const& settings = WebView::Application::settings();

    if (!wallet.is_initialized() || !settings.wallet_enabled()) {
        JsonObject balance;
        balance.set("confirmed"sv, 0);
        balance.set("unconfirmed"sv, 0);
        async_send_message("walletBalance"sv, move(balance));
        return;
    }

    auto backend_url = settings.wallet_backend_url().serialize();

    // Run the BRC-100 network call on a background thread to avoid blocking the UI.
    (void)Threading::BackgroundAction<String>::construct(
        [backend_url = move(backend_url)](auto&) -> ErrorOr<String> {
            return Wallet::WalletManager::the().fetch_balance(backend_url);
        },
        [this](String response) {
            auto response_json = JsonValue::from_string(response);

            if (!response_json.is_error() && response_json.value().is_object()) {
                auto const& obj = response_json.value().as_object();
                JsonObject balance;
                balance.set("confirmed"sv, obj.get_integer<i64>("confirmed"sv).value_or(0));
                balance.set("unconfirmed"sv, obj.get_integer<i64>("unconfirmed"sv).value_or(0));
                balance.set("connected"sv, true);
                async_send_message("walletBalance"sv, move(balance));
            } else {
                dbgln("WalletUI: Failed to parse balance JSON: {}", response);
                JsonObject balance;
                balance.set("confirmed"sv, 0);
                balance.set("unconfirmed"sv, 0);
                balance.set("connected"sv, false);
                balance.set("note"sv, "Failed to parse balance response"sv);
                async_send_message("walletBalance"sv, move(balance));
            }
        },
        [this](Error error) {
            dbgln("WalletUI: Balance fetch error: {}", error);
            JsonObject balance;
            balance.set("confirmed"sv, 0);
            balance.set("unconfirmed"sv, 0);
            balance.set("connected"sv, false);
            balance.set("note"sv, MUST(String::formatted("{}", error)));
            async_send_message("walletBalance"sv, move(balance));
        });
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

    // Transaction building requires a backend with signing capability
    JsonObject result;
    result.set("error"sv, "Transaction building requires a connected 1sat-stack backend with signing capability."sv);
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
                    (void)wallet.save_to_disk();
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

void WalletUI::list_ordinals()
{
    auto& wallet = Wallet::WalletManager::the();
    auto const& settings = WebView::Application::settings();

    if (!wallet.is_initialized() || !settings.wallet_enabled()) {
        JsonObject result;
        result.set("ordinals"sv, JsonArray {});
        result.set("note"sv, "Wallet not initialized or disabled"sv);
        async_send_message("ordinalsList"sv, move(result));
        return;
    }

    auto backend_url = settings.wallet_backend_url().serialize();

    (void)Threading::BackgroundAction<String>::construct(
        [backend_url = move(backend_url)](auto&) -> ErrorOr<String> {
            return Wallet::WalletManager::the().fetch_ordinals(backend_url);
        },
        [this](String response) {
            auto response_json = JsonValue::from_string(response);

            if (!response_json.is_error() && response_json.value().is_object()) {
                async_send_message("ordinalsList"sv, response_json.value().as_object());
            } else {
                dbgln("WalletUI: Failed to parse ordinals JSON: {}", response);
                JsonObject result;
                result.set("ordinals"sv, JsonArray {});
                result.set("error"sv, "Failed to parse ordinals response"sv);
                async_send_message("ordinalsList"sv, move(result));
            }
        },
        [this](Error error) {
            dbgln("WalletUI: Ordinals fetch error: {}", error);
            JsonObject result;
            result.set("ordinals"sv, JsonArray {});
            result.set("error"sv, MUST(String::formatted("{}", error)));
            async_send_message("ordinalsList"sv, move(result));
        });
}

void WalletUI::list_tokens()
{
    auto& wallet = Wallet::WalletManager::the();
    auto const& settings = WebView::Application::settings();

    if (!wallet.is_initialized() || !settings.wallet_enabled()) {
        JsonObject result;
        result.set("tokens"sv, JsonArray {});
        result.set("note"sv, "Wallet not initialized or disabled"sv);
        async_send_message("tokensList"sv, move(result));
        return;
    }

    auto backend_url = settings.wallet_backend_url().serialize();

    (void)Threading::BackgroundAction<String>::construct(
        [backend_url = move(backend_url)](auto&) -> ErrorOr<String> {
            return Wallet::WalletManager::the().fetch_tokens(backend_url);
        },
        [this](String response) {
            auto response_json = JsonValue::from_string(response);

            if (!response_json.is_error() && response_json.value().is_object()) {
                async_send_message("tokensList"sv, response_json.value().as_object());
            } else {
                dbgln("WalletUI: Failed to parse tokens JSON: {}", response);
                JsonObject result;
                result.set("tokens"sv, JsonArray {});
                result.set("error"sv, "Failed to parse tokens response"sv);
                async_send_message("tokensList"sv, move(result));
            }
        },
        [this](Error error) {
            dbgln("WalletUI: Tokens fetch error: {}", error);
            JsonObject result;
            result.set("tokens"sv, JsonArray {});
            result.set("error"sv, MUST(String::formatted("{}", error)));
            async_send_message("tokensList"sv, move(result));
        });
}

static ByteString profile_path()
{
    return ByteString::formatted("{}/Ladybird/wallet-profile.json", Core::StandardPaths::config_directory());
}

void WalletUI::save_profile(JsonValue const& data)
{
    if (!data.is_object()) {
        JsonObject error;
        error.set("error"sv, "Invalid profile data"sv);
        async_send_message("profileSaved"sv, move(error));
        return;
    }

    auto const& obj = data.as_object();
    JsonObject profile;
    profile.set("name"sv, obj.get_string("name"sv).value_or({}));
    profile.set("description"sv, obj.get_string("description"sv).value_or({}));
    profile.set("avatar"sv, obj.get_string("avatar"sv).value_or({}));

    StringBuilder json;
    profile.serialize(json);

    auto path = profile_path();
    auto directory = LexicalPath { path }.parent();
    auto dir_result = Core::Directory::create(directory, Core::Directory::CreateDirectories::Yes);
    if (dir_result.is_error()) {
        JsonObject error;
        error.set("error"sv, "Failed to create config directory"sv);
        async_send_message("profileSaved"sv, move(error));
        return;
    }

    auto file = Core::File::open(path, Core::File::OpenMode::Write);
    if (file.is_error()) {
        JsonObject error;
        error.set("error"sv, "Failed to open profile file for writing"sv);
        async_send_message("profileSaved"sv, move(error));
        return;
    }

    auto write_result = file.value()->write_until_depleted(json.string_view().bytes());
    if (write_result.is_error()) {
        JsonObject error;
        error.set("error"sv, "Failed to write profile data"sv);
        async_send_message("profileSaved"sv, move(error));
        return;
    }

    JsonObject response;
    response.set("success"sv, true);
    async_send_message("profileSaved"sv, move(response));
}

void WalletUI::load_profile()
{
    auto path = profile_path();

    auto file = Core::File::open(path, Core::File::OpenMode::Read);
    if (file.is_error()) {
        // No profile file yet — not an error, just empty
        JsonObject empty;
        empty.set("name"sv, ""sv);
        empty.set("description"sv, ""sv);
        empty.set("avatar"sv, ""sv);
        async_send_message("profileLoaded"sv, move(empty));
        return;
    }

    auto contents = file.value()->read_until_eof();
    if (contents.is_error()) {
        JsonObject empty;
        empty.set("name"sv, ""sv);
        empty.set("description"sv, ""sv);
        empty.set("avatar"sv, ""sv);
        async_send_message("profileLoaded"sv, move(empty));
        return;
    }

    auto json = JsonValue::from_string(StringView { contents.value() });
    if (json.is_error() || !json.value().is_object()) {
        JsonObject empty;
        empty.set("name"sv, ""sv);
        empty.set("description"sv, ""sv);
        empty.set("avatar"sv, ""sv);
        async_send_message("profileLoaded"sv, move(empty));
        return;
    }

    async_send_message("profileLoaded"sv, json.value().as_object());
}

}
