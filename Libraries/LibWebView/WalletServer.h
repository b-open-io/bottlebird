/*
 * Copyright (c) 2026, Bottlebird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/NonnullRefPtr.h>
#include <LibCore/Forward.h>
#include <LibCore/Socket.h>

namespace WebView {

constexpr inline u16 default_wallet_server_port = 3321;

class WalletServer {
public:
    static ErrorOr<NonnullOwnPtr<WalletServer>> create(u16 port);
    ~WalletServer();

private:
    explicit WalletServer(NonnullRefPtr<Core::TCPServer>);

    ErrorOr<void> on_new_client();
    ErrorOr<void> handle_http_request(Core::BufferedTCPSocket&);

    // Route POST /{method} to wallet operations
    ErrorOr<String> handle_wallet_method(StringView method, StringView body);

    NonnullRefPtr<Core::TCPServer> m_server;
};

}
