/*
 * Copyright (c) 2026, the Bottlebird contributors.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/String.h>
#include <AK/StringView.h>
#include <AK/Types.h>

namespace Wallet {

class WalletManager {
public:
    static WalletManager& the();

    // Wallet lifecycle
    ErrorOr<String> create_wallet();
    ErrorOr<void> import_from_mnemonic(StringView mnemonic);
    ErrorOr<void> import_from_wif(StringView wif);
    bool is_initialized() const { return m_initialized; }

    // Key derivation
    ErrorOr<String> get_receive_address();
    ErrorOr<String> get_identity_pubkey();
    ErrorOr<String> get_wif();

private:
    WalletManager() = default;

    ErrorOr<void> derive_keys_from_seed(unsigned char const* seed, size_t seed_len);

    bool m_initialized { false };
    u8 m_root_privkey[32] {};
    u8 m_identity_pubkey[33] {};
    String m_receive_address;
    String m_identity_hex;
};

}
