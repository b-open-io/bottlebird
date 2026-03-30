/*
 * Copyright (c) 2026, the Bottlebird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/HashMap.h>
#include <LibJS/Forward.h>
#include <LibWeb/Bindings/PlatformObject.h>
#include <LibWeb/WebIDL/Promise.h>

namespace Web::Wallet {

struct WalletKeyOptions {
    String protocol_id {};
    String key_id {};
    bool for_self { true };
};

struct WalletActionOptions {
    String description {};
};

struct WalletSignOptions {
    String reference {};
};

struct WalletListOptions {
    String basket { "default"_string };
    u32 limit { 25 };
    u32 offset { 0 };
};

class BSVWallet final : public Bindings::PlatformObject {
    WEB_PLATFORM_OBJECT(BSVWallet, Bindings::PlatformObject);
    GC_DECLARE_ALLOCATOR(BSVWallet);

public:
    [[nodiscard]] static GC::Ref<BSVWallet> create(JS::Realm&);

    virtual ~BSVWallet() override;

    GC::Ref<WebIDL::Promise> get_public_key(WalletKeyOptions const& options);
    GC::Ref<WebIDL::Promise> create_action(WalletActionOptions const& options);
    GC::Ref<WebIDL::Promise> sign_action(WalletSignOptions const& options);
    GC::Ref<WebIDL::Promise> list_outputs(WalletListOptions const& options);
    GC::Ref<WebIDL::Promise> list_actions(WalletListOptions const& options);

private:
    explicit BSVWallet(JS::Realm&);
    virtual void initialize(JS::Realm&) override;
    virtual void visit_edges(Cell::Visitor&) override;

    GC::Ref<WebIDL::Promise> send_wallet_operation(String const& operation, String const& params);
};

}
