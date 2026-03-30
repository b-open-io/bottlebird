/*
 * Copyright (c) 2026, the Bottlebird contributors.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteBuffer.h>
#include <AK/LexicalPath.h>
#include <AK/StringBuilder.h>
#include <LibCore/Directory.h>
#include <LibCore/File.h>
#include <LibCore/StandardPaths.h>
#include <LibCore/System.h>
#include <LibWallet/WalletManager.h>

#include <bsvwallet.h>
#include <bsvz.h>
#include <openssl/evp.h>

namespace Wallet {

// Base58 alphabet (Bitcoin)
static constexpr StringView base58_alphabet = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"sv;

static String base58_encode(ReadonlyBytes data)
{
    // Count leading zeros
    size_t leading_zeros = 0;
    for (auto byte : data) {
        if (byte != 0)
            break;
        ++leading_zeros;
    }

    // Convert to base58
    Vector<u8> digits;
    for (auto byte : data) {
        int carry = byte;
        for (auto& digit : digits) {
            carry += 256 * digit;
            digit = carry % 58;
            carry /= 58;
        }
        while (carry > 0) {
            digits.append(carry % 58);
            carry /= 58;
        }
    }

    StringBuilder result;
    for (size_t i = 0; i < leading_zeros; ++i)
        result.append('1');
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i)
        result.append(base58_alphabet[digits[i]]);

    return result.to_string_without_validation();
}

WalletManager& WalletManager::the()
{
    static WalletManager s_instance;
    static bool s_tried_load = false;
    if (!s_tried_load) {
        s_tried_load = true;
        if (auto result = s_instance.load_from_disk(); result.is_error())
            dbgln("WalletManager: No saved wallet found ({})", result.error());
    }
    return s_instance;
}

ErrorOr<String> WalletManager::create_wallet()
{
    char mnemonic_buf[256] {};
    size_t mnemonic_len = sizeof(mnemonic_buf);

    int rc = bsvz_mnemonic_generate(128, mnemonic_buf, &mnemonic_len);
    if (rc != 0)
        return Error::from_string_literal("Failed to generate mnemonic");

    auto mnemonic = StringView { mnemonic_buf, mnemonic_len };

    unsigned char seed[64] {};
    rc = bsvz_mnemonic_to_seed(mnemonic_buf, mnemonic_len, "", 0, seed);
    if (rc != 0)
        return Error::from_string_literal("Failed to derive seed from mnemonic");

    TRY(derive_keys_from_seed(seed, sizeof(seed)));
    TRY(save_to_disk());

    return TRY(String::from_utf8(mnemonic));
}

ErrorOr<void> WalletManager::import_from_mnemonic(StringView mnemonic)
{
    auto words = mnemonic.split_view(' ');
    if (words.size() != 12 && words.size() != 24)
        return Error::from_string_literal("Expected 12 or 24 words in mnemonic");

    unsigned char seed[64] {};
    int rc = bsvz_mnemonic_to_seed(
        mnemonic.characters_without_null_termination(), mnemonic.length(),
        "", 0, seed);
    if (rc != 0)
        return Error::from_string_literal("Failed to derive seed from mnemonic");

    TRY(derive_keys_from_seed(seed, sizeof(seed)));
    TRY(save_to_disk());
    return {};
}

ErrorOr<void> WalletManager::import_from_wif(StringView wif)
{
    int rc = bsvz_wif_to_privkey(
        wif.characters_without_null_termination(), wif.length(),
        m_root_privkey);
    if (rc != 0)
        return Error::from_string_literal("Invalid WIF key");

    rc = bsvz_privkey_to_pubkey(m_root_privkey, m_identity_pubkey);
    if (rc != 0)
        return Error::from_string_literal("Failed to derive public key");

    char addr_buf[40] {};
    size_t addr_len = sizeof(addr_buf);
    rc = bsvz_pubkey_to_address(m_identity_pubkey, sizeof(m_identity_pubkey), addr_buf, &addr_len);
    if (rc != 0)
        return Error::from_string_literal("Failed to derive address");

    m_receive_address = TRY(String::from_utf8({ addr_buf, addr_len }));

    StringBuilder hex;
    for (size_t i = 0; i < sizeof(m_identity_pubkey); ++i)
        TRY(hex.try_appendff("{:02x}", m_identity_pubkey[i]));
    m_identity_hex = hex.to_string_without_validation();

    TRY(compute_bap_id());
    m_initialized = true;

    // Note: save_to_disk() is intentionally not called here.
    // Callers (create_wallet, import_from_mnemonic, WalletUI) are responsible for persistence.
    // load_from_disk() calls import_from_wif internally and must not re-save.
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

ErrorOr<String> WalletManager::get_bap_id()
{
    if (!m_initialized)
        return Error::from_string_literal("Wallet not initialized");
    return m_bap_id;
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
    // Master HD key from seed
    char xprv_buf[120] {};
    size_t xprv_len = sizeof(xprv_buf);
    int rc = bsvz_hd_from_seed(seed, seed_len, xprv_buf, &xprv_len);
    if (rc != 0)
        return Error::from_string_literal("Failed to create HD key from seed");

    // Derive identity key at m/0'/0'/0'
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

    // Derive public key
    rc = bsvz_privkey_to_pubkey(m_root_privkey, m_identity_pubkey);
    if (rc != 0)
        return Error::from_string_literal("Failed to derive public key");

    // Derive P2PKH address
    char addr_buf[40] {};
    size_t addr_len = sizeof(addr_buf);
    rc = bsvz_pubkey_to_address(m_identity_pubkey, sizeof(m_identity_pubkey), addr_buf, &addr_len);
    if (rc != 0)
        return Error::from_string_literal("Failed to derive address");

    m_receive_address = TRY(String::from_utf8({ addr_buf, addr_len }));

    // Hex-encode identity pubkey
    StringBuilder hex;
    for (size_t i = 0; i < sizeof(m_identity_pubkey); ++i)
        TRY(hex.try_appendff("{:02x}", m_identity_pubkey[i]));
    m_identity_hex = hex.to_string_without_validation();

    // Compute BAP ID
    TRY(compute_bap_id());

    m_initialized = true;
    return {};
}

ErrorOr<void> WalletManager::compute_bap_id()
{
    // BAP ID = base58(ripemd160(sha256(address_string)))
    // This matches bsv-bap's bapIdFromAddress()
    auto address_bytes = m_receive_address.bytes();

    // SHA256 of the address string (as UTF-8 bytes)
    u8 sha256_result[32] {};
    unsigned int sha256_len = sizeof(sha256_result);
    auto* sha256_md = EVP_sha256();
    EVP_Digest(address_bytes.data(), address_bytes.size(), sha256_result, &sha256_len, sha256_md, nullptr);

    // Hex-encode the SHA256 result (BAP uses hex string as input to RIPEMD160)
    StringBuilder sha256_hex;
    for (size_t i = 0; i < sha256_len; ++i)
        TRY(sha256_hex.try_appendff("{:02x}", sha256_result[i]));
    auto sha256_hex_str = sha256_hex.to_string_without_validation();
    auto hex_bytes = sha256_hex_str.bytes();

    // RIPEMD160 of the hex-encoded SHA256
    u8 ripemd_result[20] {};
    unsigned int ripemd_len = sizeof(ripemd_result);
    auto* ripemd_md = EVP_ripemd160();
    EVP_Digest(hex_bytes.data(), hex_bytes.size(), ripemd_result, &ripemd_len, ripemd_md, nullptr);

    // Base58-encode the RIPEMD160 result
    m_bap_id = base58_encode(ReadonlyBytes { ripemd_result, ripemd_len });

    return {};
}

ByteString WalletManager::wallet_key_path() const
{
    return ByteString::formatted("{}/Ladybird/wallet.key", Core::StandardPaths::config_directory());
}

ErrorOr<void> WalletManager::save_to_disk()
{
    if (!m_initialized)
        return Error::from_string_literal("Wallet not initialized");

    auto wif = TRY(get_wif());
    auto path = wallet_key_path();
    auto directory = LexicalPath { path }.parent();
    TRY(Core::Directory::create(directory, Core::Directory::CreateDirectories::Yes));

    auto file = TRY(Core::File::open(path, Core::File::OpenMode::Write));
    TRY(file->write_until_depleted(wif.bytes()));

    TRY(Core::System::chmod(path, 0600));

    return {};
}

ErrorOr<void> WalletManager::load_from_disk()
{
    auto path = wallet_key_path();

    auto file = Core::File::open(path, Core::File::OpenMode::Read);
    if (file.is_error()) {
        if (file.error().is_errno() && file.error().code() == ENOENT)
            return Error::from_string_literal("No wallet key file found");
        return file.release_error();
    }

    auto contents = TRY(file.value()->read_until_eof());
    auto wif = StringView { contents };

    // Strip any trailing whitespace/newlines
    wif = wif.trim_whitespace();
    if (wif.is_empty())
        return Error::from_string_literal("Wallet key file is empty");

    TRY(import_from_wif(wif));
    return {};
}

WalletManager::~WalletManager() = default;

ErrorOr<String> WalletManager::fetch_balance(StringView backend_url)
{
    if (!m_initialized)
        return Error::from_string_literal("Wallet not initialized");

    // Create a temporary wallet handle for this request.
    // Must be created and used on the same thread (background thread)
    // because the Zig allocator state isn't thread-safe.
    void* handle = nullptr;
    int rc = bsvwallet_create_remote(
        m_root_privkey, 0,
        backend_url.characters_without_null_termination(), backend_url.length(),
        &handle);

    if (rc != 0) {
        dbgln("WalletManager: bsvwallet_create_remote failed with rc={}", rc);
        return Error::from_string_literal("Failed to connect to backend");
    }

    dbgln("WalletManager: Connected to backend {}", backend_url);

    // Call listOutputs through the proper BRC-100 path
    constexpr size_t buf_capacity = 65536;
    auto response_buf = TRY(ByteBuffer::create_uninitialized(buf_capacity));
    size_t body_len = buf_capacity;

    static constexpr auto args = "{\"basket\":\"default\",\"limit\":100}"sv;

    rc = bsvwallet_list_outputs(
        handle,
        args.characters_without_null_termination(), args.length(),
        reinterpret_cast<char*>(response_buf.data()), &body_len);

    // Always destroy the handle on this thread
    bsvwallet_destroy(handle);

    if (rc != 0) {
        dbgln("WalletManager: bsvwallet_list_outputs failed with rc={}", rc);
        return Error::from_string_literal("Failed to list outputs from backend");
    }

    auto body = StringView { reinterpret_cast<char const*>(response_buf.data()), body_len };
    dbgln("WalletManager: listOutputs response: {} bytes", body_len);
    return TRY(String::from_utf8(body));
}

}
