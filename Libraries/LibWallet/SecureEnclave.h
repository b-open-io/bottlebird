/*
 * Copyright (c) 2026, the Bottlebird contributors.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/String.h>
#include <AK/StringView.h>

#ifdef __APPLE__

namespace Wallet {

class SecureEnclave {
public:
    struct CheckResult {
        bool supported;
        String biometry_type;
        bool biometry_available;
    };

    static ErrorOr<CheckResult> check();
    static ErrorOr<void> generate_key(StringView label);
    static ErrorOr<String> encrypt(StringView label, StringView plaintext);
    static ErrorOr<String> decrypt(StringView label, StringView ciphertext);
    static ErrorOr<void> delete_key(StringView label);

    static bool is_available();

private:
    static ErrorOr<String> run_enclave_command(ReadonlySpan<ByteString> args);
    static ByteString find_binary_path();
};

}

#endif
