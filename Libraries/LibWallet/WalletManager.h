/*
 * Copyright (c) 2026, the Bottlebird contributors.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteString.h>
#include <AK/Error.h>
#include <AK/String.h>
#include <AK/StringView.h>
#include <AK/Types.h>

namespace Wallet {

class WalletManager {
public:
    static WalletManager& the();
    ~WalletManager();

    // Wallet lifecycle
    ErrorOr<String> create_wallet();
    ErrorOr<void> import_from_mnemonic(StringView mnemonic);
    ErrorOr<void> import_from_wif(StringView wif);
    bool is_initialized() const { return m_initialized; }

    // Key derivation
    ErrorOr<String> get_receive_address();
    ErrorOr<String> get_identity_pubkey();
    ErrorOr<String> get_bap_id();
    ErrorOr<String> get_wif();

    // BRC-42/43 key derivation
    ErrorOr<String> get_derived_public_key(StringView protocol_id, StringView key_id, u8 security_level, bool for_self, StringView counterparty);

    // BRC-100 wallet operations via remote backend
    ErrorOr<String> fetch_balance(StringView backend_url);
    ErrorOr<String> fetch_ordinals(StringView backend_url);
    ErrorOr<String> fetch_tokens(StringView backend_url);

    // Transaction building via remote backend
    ErrorOr<String> create_action(StringView backend_url, StringView args_json);
    ErrorOr<String> sign_action(StringView backend_url, StringView args_json);
    ErrorOr<String> inscribe_file(StringView backend_url, ReadonlyBytes content, StringView content_type, StringView app_name);

    // Persistence
    ErrorOr<void> save_to_disk();
    ErrorOr<void> load_from_disk();

private:
    WalletManager() = default;

    ErrorOr<void> derive_keys_from_seed(unsigned char const* seed, size_t seed_len);
    ErrorOr<void> compute_bap_id();
    ByteString wallet_key_path() const;
    ByteString wallet_enc_path() const;

    bool m_initialized { false };
    u8 m_root_privkey[32] {};
    u8 m_identity_pubkey[33] {};
    String m_receive_address;
    String m_identity_hex;
    String m_bap_id;
};

}
