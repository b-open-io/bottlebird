/*
 * Copyright (c) 2025, the Ladybird contributors.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWebView/WebUI.h>

namespace WebView {

class WalletUI : public WebUI {
    WEB_UI(WalletUI);

private:
    virtual void register_interfaces() override;

    void load_wallet_status();
    void get_balance();
    void get_receive_address();
    void send_payment(JsonValue const&);
    void set_wallet_enabled(JsonValue const&);
    void set_wallet_backend_url(JsonValue const&);
};

}
