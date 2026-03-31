/*
 * Copyright (c) 2026, the Bottlebird contributors.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifdef __APPLE__

#include <AK/ByteBuffer.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>
#include <LibCore/File.h>
#include <LibCore/Process.h>
#include <LibCore/System.h>
#include <LibWallet/SecureEnclave.h>

#include <fcntl.h>
#include <unistd.h>

namespace Wallet {

ByteString SecureEnclave::find_binary_path()
{
    // 1. Next to the Ladybird executable (production)
    // On macOS the app bundle structure is: Ladybird.app/Contents/MacOS/Ladybird
    // We'd place the enclave binary alongside it.
    // Check /proc/self/exe equivalent on macOS is _NSGetExecutablePath, but
    // we keep it simple: just check a well-known dev path.

    // 2. Dev fallback: the 1sat-sdk source tree
    static constexpr auto dev_path = "/Users/satchmo/code/1sat-sdk/packages/wallet-mac/swift/enclave"sv;

    if (access(dev_path.characters_without_null_termination(), X_OK) == 0)
        return dev_path;

    return {};
}

ErrorOr<String> SecureEnclave::run_enclave_command(ReadonlySpan<ByteString> args)
{
    auto binary_path = find_binary_path();
    if (binary_path.is_empty())
        return Error::from_string_literal("Secure Enclave binary not found");

    // Create a pipe to capture stdout from the child process
    auto stdout_pipe = TRY(Core::System::pipe2(O_CLOEXEC));
    TRY(Core::System::set_close_on_exec(stdout_pipe[1], false));

    // Build spawn options with stdout redirected to our pipe
    Vector<ByteString> arguments;
    for (auto const& arg : args)
        arguments.append(arg);

    Core::ProcessSpawnOptions spawn_options {
        .name = "enclave"sv,
        .executable = binary_path,
        .arguments = arguments,
        .file_actions = {
            Core::FileAction::DupFd { .write_fd = stdout_pipe[1], .fd = STDOUT_FILENO },
            Core::FileAction::CloseFile { .fd = stdout_pipe[1] },
        },
    };

    auto process = TRY(Core::Process::spawn(spawn_options));

    // Close the write end in the parent
    TRY(Core::System::close(stdout_pipe[1]));

    // Read all stdout from the child
    auto stdout_file = TRY(Core::File::adopt_fd(stdout_pipe[0], Core::File::OpenMode::Read));
    auto output = TRY(stdout_file->read_until_eof());

    (void)TRY(process.wait_for_termination());

    auto output_view = StringView { output };
    output_view = output_view.trim_whitespace();

    if (output_view.is_empty())
        return Error::from_string_literal("Secure Enclave binary returned no output");

    // Parse JSON response
    auto parsed = JsonValue::from_string(output_view);
    if (parsed.is_error())
        return Error::from_string_literal("Failed to parse Secure Enclave response");

    auto const& root = parsed.value();
    if (!root.is_object())
        return Error::from_string_literal("Secure Enclave response is not a JSON object");

    auto success = root.as_object().get_bool("success"sv);
    if (!success.has_value() || !success.value()) {
        auto error = root.as_object().get_string("error"sv);
        if (error.has_value()) {
            auto msg = TRY(String::formatted("Secure Enclave error: {}", *error));
            dbgln("{}", msg);
        }
        return Error::from_string_literal("Secure Enclave command failed");
    }

    // Return the full JSON response for callers to extract what they need
    return TRY(String::from_utf8(output_view));
}

bool SecureEnclave::is_available()
{
    auto result = check();
    if (result.is_error())
        return false;
    return result.value().supported;
}

ErrorOr<SecureEnclave::CheckResult> SecureEnclave::check()
{
    Vector<ByteString> args;
    args.append("check"sv);

    auto json_str = TRY(run_enclave_command(args.span()));
    auto parsed = TRY(JsonValue::from_string(json_str));
    auto const& root = parsed.as_object();

    auto meta = root.get_object("meta"sv);
    if (!meta.has_value())
        return Error::from_string_literal("Secure Enclave check: missing meta");

    auto se_str = meta->get_string("secureEnclave"sv);
    auto bio_type = meta->get_string("biometryType"sv);
    auto bio_avail = meta->get_string("biometryAvailable"sv);

    return CheckResult {
        .supported = se_str.has_value() && *se_str == "true"sv,
        .biometry_type = bio_type.has_value() ? TRY(String::from_utf8(bio_type->bytes_as_string_view())) : TRY(String::from_utf8("None"sv)),
        .biometry_available = bio_avail.has_value() && *bio_avail == "true"sv,
    };
}

ErrorOr<void> SecureEnclave::generate_key(StringView label)
{
    Vector<ByteString> args;
    args.append("generate"sv);
    args.append(ByteString { label });

    TRY(run_enclave_command(args.span()));
    return {};
}

ErrorOr<String> SecureEnclave::encrypt(StringView label, StringView plaintext)
{
    Vector<ByteString> args;
    args.append("encrypt"sv);
    args.append(ByteString { label });
    args.append(ByteString { plaintext });

    auto json_str = TRY(run_enclave_command(args.span()));
    auto parsed = TRY(JsonValue::from_string(json_str));
    auto const& root = parsed.as_object();

    auto data = root.get_string("data"sv);
    if (!data.has_value())
        return Error::from_string_literal("Secure Enclave encrypt: missing data");

    return TRY(String::from_utf8(data->bytes_as_string_view()));
}

ErrorOr<String> SecureEnclave::decrypt(StringView label, StringView ciphertext)
{
    Vector<ByteString> args;
    args.append("decrypt"sv);
    args.append(ByteString { label });
    args.append(ByteString { ciphertext });
    args.append("Bottlebird"sv);

    auto json_str = TRY(run_enclave_command(args.span()));
    auto parsed = TRY(JsonValue::from_string(json_str));
    auto const& root = parsed.as_object();

    auto data = root.get_string("data"sv);
    if (!data.has_value())
        return Error::from_string_literal("Secure Enclave decrypt: missing data");

    return TRY(String::from_utf8(data->bytes_as_string_view()));
}

ErrorOr<void> SecureEnclave::delete_key(StringView label)
{
    Vector<ByteString> args;
    args.append("delete"sv);
    args.append(ByteString { label });

    TRY(run_enclave_command(args.span()));
    return {};
}

}

#endif
