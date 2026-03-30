/*
 * Copyright (c) 2026, the Bottlebird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

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
    Vector<GC::Root<JS::Object>> outputs;
    Vector<GC::Root<JS::Object>> inputs;
};

struct WalletSignOptions {
    String reference {};
    Vector<GC::Root<JS::Object>> spends;
};

struct WalletListOptions {
    String basket { "default"_string };
    u32 limit { 25 };
    u32 offset { 0 };
};

class Wallet final : public Bindings::PlatformObject {
    WEB_PLATFORM_OBJECT(Wallet, Bindings::PlatformObject);
    GC_DECLARE_ALLOCATOR(Wallet);

public:
    [[nodiscard]] static GC::Ref<Wallet> create(JS::Realm&);

    virtual ~Wallet() override;

    GC::Ref<WebIDL::Promise> get_public_key(WalletKeyOptions const& options);
    GC::Ref<WebIDL::Promise> create_action(WalletActionOptions const& options);
    GC::Ref<WebIDL::Promise> sign_action(WalletSignOptions const& options);
    GC::Ref<WebIDL::Promise> list_outputs(WalletListOptions const& options);
    GC::Ref<WebIDL::Promise> list_actions(WalletListOptions const& options);

private:
    explicit Wallet(JS::Realm&);
    virtual void initialize(JS::Realm&) override;
    virtual void visit_edges(Cell::Visitor&) override;
};

}
