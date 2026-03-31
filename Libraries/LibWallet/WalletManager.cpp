/*
 * Copyright (c) 2026, the Bottlebird contributors.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteBuffer.h>
#include <AK/Hex.h>
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
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

ErrorOr<String> WalletManager::get_derived_public_key(StringView protocol_id, StringView key_id, u8 security_level, bool for_self, StringView counterparty)
{
    if (!m_initialized)
        return Error::from_string_literal("Wallet not initialized");

    // Step 1: Format BRC-43 invoice number from protocol_id, key_id, security_level
    char invoice_buf[1300] {};
    size_t invoice_len = sizeof(invoice_buf);
    int rc = bsvz_brc43_invoice(
        security_level,
        protocol_id.characters_without_null_termination(), protocol_id.length(),
        key_id.characters_without_null_termination(), key_id.length(),
        invoice_buf, &invoice_len);
    if (rc != 0)
        return Error::from_string_literal("Failed to format BRC-43 invoice");

    u8 derived_pubkey[33] {};

    if (for_self) {
        // For self-derivation: derive child pubkey using own pubkey + own privkey as counterparty
        rc = bsvz_derive_child_pubkey(
            m_identity_pubkey, m_root_privkey,
            invoice_buf, invoice_len,
            derived_pubkey);
    } else {
        // For counterparty derivation: parse counterparty hex pubkey, derive with it
        if (counterparty.length() != 66)
            return Error::from_string_literal("Counterparty must be 66-char hex compressed pubkey");

        u8 counterparty_pubkey[33] {};
        for (size_t i = 0; i < 33; ++i) {
            auto hi = decode_hex_digit(counterparty[i * 2]);
            auto lo = decode_hex_digit(counterparty[i * 2 + 1]);
            if (hi >= 16 || lo >= 16)
                return Error::from_string_literal("Invalid hex in counterparty pubkey");
            counterparty_pubkey[i] = static_cast<u8>((hi << 4) | lo);
        }

        rc = bsvz_derive_child_pubkey(
            counterparty_pubkey, m_root_privkey,
            invoice_buf, invoice_len,
            derived_pubkey);
    }

    if (rc != 0)
        return Error::from_string_literal("Failed to derive child public key");

    // Hex-encode the 33-byte compressed pubkey
    StringBuilder hex;
    for (size_t i = 0; i < sizeof(derived_pubkey); ++i)
        TRY(hex.try_appendff("{:02x}", derived_pubkey[i]));

    return hex.to_string_without_validation();
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

    long long confirmed = 0;
    long long unconfirmed = 0;

    int rc = bsvwallet_get_balance_remote(
        m_root_privkey, 0,
        backend_url.characters_without_null_termination(), backend_url.length(),
        &confirmed, &unconfirmed);

    if (rc != 0) {
        dbgln("WalletManager: bsvwallet_get_balance_remote failed with rc={}", rc);
        return Error::from_string_literal("Failed to fetch balance from backend");
    }

    dbgln("WalletManager: Balance: confirmed={} unconfirmed={}", confirmed, unconfirmed);

    return TRY(String::formatted("{{\"confirmed\":{},\"unconfirmed\":{}}}", confirmed, unconfirmed));
}

ErrorOr<String> WalletManager::fetch_ordinals(StringView backend_url)
{
    if (!m_initialized)
        return Error::from_string_literal("Wallet not initialized");

    bsvwallet_t handle = nullptr;
    int rc = bsvwallet_create_remote(
        m_root_privkey, 0,
        backend_url.characters_without_null_termination(), backend_url.length(),
        &handle);

    if (rc != 0) {
        dbgln("WalletManager: bsvwallet_create_remote failed with rc={}", rc);
        return Error::from_string_literal("Failed to connect to backend");
    }

    constexpr size_t buf_capacity = 65536;
    auto response_buf = TRY(ByteBuffer::create_uninitialized(buf_capacity));
    size_t body_len = buf_capacity;

    static constexpr auto args = "{\"basket\":\"ordinals\",\"limit\":100,\"includeTags\":true}"sv;
    rc = bsvwallet_list_outputs(
        handle,
        args.characters_without_null_termination(), args.length(),
        reinterpret_cast<char*>(response_buf.data()), &body_len);

    bsvwallet_destroy(handle);

    if (rc != 0) {
        dbgln("WalletManager: bsvwallet_list_outputs (ordinals) failed with rc={}", rc);
        return Error::from_string_literal("Failed to list ordinals from backend");
    }

    auto raw_response = TRY(String::from_utf8({ response_buf.data(), body_len }));
    dbgln("WalletManager: Ordinals response ({} bytes): {}", body_len, raw_response);

    // Parse the listOutputs response and transform into our ordinals format
    auto parsed = JsonValue::from_string(raw_response);
    if (parsed.is_error()) {
        // Return the raw response wrapped in our expected format
        return TRY(String::formatted("{{\"ordinals\":[],\"note\":\"Failed to parse backend response\"}}"));
    }

    // listOutputs returns { outputs: [...], totalOutputs: N }
    // Each output has: outpoint, satoshis, tags, customInstructions, etc.
    auto const& root = parsed.value();
    JsonArray ordinals;

    if (root.is_object()) {
        auto outputs = root.as_object().get_array("outputs"sv);
        if (outputs.has_value()) {
            outputs->for_each([&](JsonValue const& output_val) {
                if (!output_val.is_object())
                    return;
                auto const& output = output_val.as_object();

                JsonObject item;
                auto outpoint = output.get_string("outpoint"sv);
                if (outpoint.has_value())
                    item.set("outpoint"sv, *outpoint);

                // Try to extract name from tags
                auto tags = output.get_array("tags"sv);
                if (tags.has_value()) {
                    tags->for_each([&](JsonValue const& tag_val) {
                        if (!tag_val.is_object())
                            return;
                        auto const& tag = tag_val.as_object();
                        auto tag_name = tag.get_string("tag"sv);
                        auto tag_value = tag.get_string("value"sv);
                        if (tag_name.has_value() && *tag_name == "name" && tag_value.has_value())
                            item.set("name"sv, *tag_value);
                        if (tag_name.has_value() && *tag_name == "contentType" && tag_value.has_value())
                            item.set("contentType"sv, *tag_value);
                    });
                }

                ordinals.must_append(move(item));
            });
        }
    }

    JsonObject result;
    result.set("ordinals"sv, move(ordinals));
    if (ordinals.size() == 0)
        result.set("note"sv, "No ordinals in this wallet"sv);

    StringBuilder json;
    result.serialize(json);
    return json.to_string_without_validation();
}

ErrorOr<String> WalletManager::fetch_tokens(StringView backend_url)
{
    if (!m_initialized)
        return Error::from_string_literal("Wallet not initialized");

    bsvwallet_t handle = nullptr;
    int rc = bsvwallet_create_remote(
        m_root_privkey, 0,
        backend_url.characters_without_null_termination(), backend_url.length(),
        &handle);

    if (rc != 0) {
        dbgln("WalletManager: bsvwallet_create_remote failed with rc={}", rc);
        return Error::from_string_literal("Failed to connect to backend");
    }

    constexpr size_t buf_capacity = 65536;
    auto response_buf = TRY(ByteBuffer::create_uninitialized(buf_capacity));
    size_t body_len = buf_capacity;

    static constexpr auto args = "{\"basket\":\"bsv21\",\"limit\":100,\"includeTags\":true}"sv;
    rc = bsvwallet_list_outputs(
        handle,
        args.characters_without_null_termination(), args.length(),
        reinterpret_cast<char*>(response_buf.data()), &body_len);

    bsvwallet_destroy(handle);

    if (rc != 0) {
        dbgln("WalletManager: bsvwallet_list_outputs (tokens) failed with rc={}", rc);
        return Error::from_string_literal("Failed to list tokens from backend");
    }

    auto raw_response = TRY(String::from_utf8({ response_buf.data(), body_len }));
    dbgln("WalletManager: Tokens response ({} bytes): {}", body_len, raw_response);

    // Parse the listOutputs response and transform into our tokens format
    auto parsed = JsonValue::from_string(raw_response);
    if (parsed.is_error()) {
        return TRY(String::formatted("{{\"tokens\":[],\"note\":\"Failed to parse backend response\"}}"));
    }

    auto const& root = parsed.value();
    JsonArray tokens;

    if (root.is_object()) {
        auto outputs = root.as_object().get_array("outputs"sv);
        if (outputs.has_value()) {
            outputs->for_each([&](JsonValue const& output_val) {
                if (!output_val.is_object())
                    return;
                auto const& output = output_val.as_object();

                JsonObject item;
                auto outpoint = output.get_string("outpoint"sv);
                if (outpoint.has_value())
                    item.set("id"sv, *outpoint);

                auto satoshis = output.get_integer<i64>("satoshis"sv);
                if (satoshis.has_value())
                    item.set("balance"sv, *satoshis);

                // Extract token metadata from tags
                auto tags = output.get_array("tags"sv);
                if (tags.has_value()) {
                    tags->for_each([&](JsonValue const& tag_val) {
                        if (!tag_val.is_object())
                            return;
                        auto const& tag = tag_val.as_object();
                        auto tag_name = tag.get_string("tag"sv);
                        auto tag_value = tag.get_string("value"sv);
                        if (tag_name.has_value() && tag_value.has_value()) {
                            if (*tag_name == "symbol" || *tag_name == "sym")
                                item.set("symbol"sv, *tag_value);
                            if (*tag_name == "icon")
                                item.set("icon"sv, *tag_value);
                        }
                    });
                }

                // Default symbol from id if not found in tags
                if (!item.has_string("symbol"sv)) {
                    auto id = item.get_string("id"sv);
                    if (id.has_value())
                        item.set("symbol"sv, id->bytes_as_string_view().substring_view(0, min(static_cast<size_t>(8), id->bytes_as_string_view().length())));
                }

                tokens.must_append(move(item));
            });
        }
    }

    JsonObject result;
    result.set("tokens"sv, move(tokens));
    if (tokens.size() == 0)
        result.set("note"sv, "No tokens in this wallet"sv);

    StringBuilder json;
    result.serialize(json);
    return json.to_string_without_validation();
}

}
