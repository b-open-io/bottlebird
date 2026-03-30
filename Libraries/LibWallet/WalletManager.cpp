/*
 * Copyright (c) 2026, the Bottlebird contributors.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/StringBuilder.h>
#include <LibWallet/WalletManager.h>

#include <bsvz.h>

namespace Wallet {

WalletManager& WalletManager::the()
{
    static WalletManager s_instance;
    return s_instance;
}

ErrorOr<String> WalletManager::create_wallet()
{
    // Generate a 12-word BIP39 mnemonic
    char mnemonic_buf[256] {};
    size_t mnemonic_len = sizeof(mnemonic_buf);

    int rc = bsvz_mnemonic_generate(128, mnemonic_buf, &mnemonic_len);
    if (rc != 0)
        return Error::from_string_literal("Failed to generate mnemonic");

    auto mnemonic = StringView { mnemonic_buf, mnemonic_len };

    // Derive seed from mnemonic (empty passphrase)
    unsigned char seed[64] {};
    rc = bsvz_mnemonic_to_seed(mnemonic_buf, mnemonic_len, "", 0, seed);
    if (rc != 0)
        return Error::from_string_literal("Failed to derive seed from mnemonic");

    // Derive keys from seed
    TRY(derive_keys_from_seed(seed, sizeof(seed)));

    return TRY(String::from_utf8(mnemonic));
}

ErrorOr<void> WalletManager::import_from_mnemonic(StringView mnemonic)
{
    auto words = mnemonic.split_view(' ');
    if (words.size() != 12 && words.size() != 24)
        return Error::from_string_literal("Expected 12 or 24 words in mnemonic");

    // Derive seed from mnemonic (empty passphrase)
    unsigned char seed[64] {};
    int rc = bsvz_mnemonic_to_seed(
        mnemonic.characters_without_null_termination(), mnemonic.length(),
        "", 0,
        seed);
    if (rc != 0)
        return Error::from_string_literal("Failed to derive seed from mnemonic");

    TRY(derive_keys_from_seed(seed, sizeof(seed)));
    return {};
}

ErrorOr<void> WalletManager::import_from_wif(StringView wif)
{
    int rc = bsvz_wif_to_privkey(
        wif.characters_without_null_termination(), wif.length(),
        m_root_privkey);
    if (rc != 0)
        return Error::from_string_literal("Invalid WIF key");

    // Derive public key from private key
    rc = bsvz_privkey_to_pubkey(m_root_privkey, m_identity_pubkey);
    if (rc != 0)
        return Error::from_string_literal("Failed to derive public key");

    // Derive address from public key
    char addr_buf[40] {};
    size_t addr_len = sizeof(addr_buf);
    rc = bsvz_pubkey_to_address(m_identity_pubkey, sizeof(m_identity_pubkey), addr_buf, &addr_len);
    if (rc != 0)
        return Error::from_string_literal("Failed to derive address");

    m_receive_address = TRY(String::from_utf8({ addr_buf, addr_len }));

    // Hex-encode the identity pubkey
    StringBuilder hex;
    for (size_t i = 0; i < sizeof(m_identity_pubkey); ++i)
        TRY(hex.try_appendff("{:02x}", m_identity_pubkey[i]));
    m_identity_hex = hex.to_string_without_validation();

    m_initialized = true;
    return {};
}

ErrorOr<String> WalletManager::get_receive_address()
{
    if (!m_initialized)
        return Error::from_string_literal("Wallet not initialized");
    return m_receive_address;
}

ErrorOr<String> WalletManager::get_identity_pubkey()
{
    if (!m_initialized)
        return Error::from_string_literal("Wallet not initialized");
    return m_identity_hex;
}

ErrorOr<String> WalletManager::get_wif()
{
    if (!m_initialized)
        return Error::from_string_literal("Wallet not initialized");

    char wif_buf[60] {};
    size_t wif_len = sizeof(wif_buf);
    int rc = bsvz_privkey_to_wif(m_root_privkey, wif_buf, &wif_len);
    if (rc != 0)
        return Error::from_string_literal("Failed to encode WIF");

    return TRY(String::from_utf8({ wif_buf, wif_len }));
}

ErrorOr<void> WalletManager::derive_keys_from_seed(unsigned char const* seed, size_t seed_len)
{
    // Create master HD key from seed
    char xprv_buf[120] {};
    size_t xprv_len = sizeof(xprv_buf);
    int rc = bsvz_hd_from_seed(seed, seed_len, xprv_buf, &xprv_len);
    if (rc != 0)
        return Error::from_string_literal("Failed to create HD key from seed");

    // Derive the identity key at m/0'/0'/0'
    char derived_buf[120] {};
    size_t derived_len = sizeof(derived_buf);
    static constexpr char identity_path[] = "m/0'/0'/0'";
    rc = bsvz_hd_derive_path(xprv_buf, xprv_len, identity_path, sizeof(identity_path) - 1, derived_buf, &derived_len);
    if (rc != 0)
        return Error::from_string_literal("Failed to derive identity key");

    // Extract raw 32-byte private key
    rc = bsvz_hd_privkey_bytes(derived_buf, derived_len, m_root_privkey);
    if (rc != 0)
        return Error::from_string_literal("Failed to extract private key bytes");

    // Derive public key from private key
    rc = bsvz_privkey_to_pubkey(m_root_privkey, m_identity_pubkey);
    if (rc != 0)
        return Error::from_string_literal("Failed to derive public key");

    // Derive address from public key
    char addr_buf[40] {};
    size_t addr_len = sizeof(addr_buf);
    rc = bsvz_pubkey_to_address(m_identity_pubkey, sizeof(m_identity_pubkey), addr_buf, &addr_len);
    if (rc != 0)
        return Error::from_string_literal("Failed to derive address");

    m_receive_address = TRY(String::from_utf8({ addr_buf, addr_len }));

    // Hex-encode the identity pubkey
    StringBuilder hex;
    for (size_t i = 0; i < sizeof(m_identity_pubkey); ++i)
        TRY(hex.try_appendff("{:02x}", m_identity_pubkey[i]));
    m_identity_hex = hex.to_string_without_validation();

    m_initialized = true;
    return {};
}

}
