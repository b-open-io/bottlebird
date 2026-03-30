/*
 * Copyright (c) 2026, the Bottlebird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/Runtime/Realm.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/Bindings/WalletPrototype.h>
#include <LibWeb/Wallet/Wallet.h>
#include <LibWeb/WebIDL/DOMException.h>
#include <LibWeb/WebIDL/Promise.h>

namespace Web::Wallet {

GC_DEFINE_ALLOCATOR(Wallet);

GC::Ref<Wallet> Wallet::create(JS::Realm& realm)
{
    return realm.create<Wallet>(realm);
}

Wallet::Wallet(JS::Realm& realm)
    : PlatformObject(realm)
{
}

Wallet::~Wallet() = default;

void Wallet::initialize(JS::Realm& realm)
{
    WEB_SET_PROTOTYPE_FOR_INTERFACE(Wallet);
    Base::initialize(realm);
}

void Wallet::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
}

GC::Ref<WebIDL::Promise> Wallet::get_public_key(WalletKeyOptions const&)
{
    auto& realm = this->realm();
    auto promise = WebIDL::create_promise(realm);
    WebIDL::reject_promise(realm, *promise, WebIDL::NotSupportedError::create(realm, "Wallet not configured"_string));
    return promise;
}

GC::Ref<WebIDL::Promise> Wallet::create_action(WalletActionOptions const&)
{
    auto& realm = this->realm();
    auto promise = WebIDL::create_promise(realm);
    WebIDL::reject_promise(realm, *promise, WebIDL::NotSupportedError::create(realm, "Wallet not configured"_string));
    return promise;
}

GC::Ref<WebIDL::Promise> Wallet::sign_action(WalletSignOptions const&)
{
    auto& realm = this->realm();
    auto promise = WebIDL::create_promise(realm);
    WebIDL::reject_promise(realm, *promise, WebIDL::NotSupportedError::create(realm, "Wallet not configured"_string));
    return promise;
}

GC::Ref<WebIDL::Promise> Wallet::list_outputs(WalletListOptions const&)
{
    auto& realm = this->realm();
    auto promise = WebIDL::create_promise(realm);
    WebIDL::reject_promise(realm, *promise, WebIDL::NotSupportedError::create(realm, "Wallet not configured"_string));
    return promise;
}

GC::Ref<WebIDL::Promise> Wallet::list_actions(WalletListOptions const&)
{
    auto& realm = this->realm();
    auto promise = WebIDL::create_promise(realm);
    WebIDL::reject_promise(realm, *promise, WebIDL::NotSupportedError::create(realm, "Wallet not configured"_string));
    return promise;
}

}
