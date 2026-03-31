/*
 * Copyright (c) 2026, the Bottlebird contributors.
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
    void create_wallet();
    void import_wallet(JsonValue const&);
    void import_backup_file(JsonValue const&);
    void confirm_wallet_creation();
    void list_ordinals();
    void list_tokens();

    String m_pending_mnemonic;
};

}
