/*
 * Copyright (c) 2026, the Bottlebird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/Runtime/Realm.h>
#include <LibWeb/Bindings/BSVWalletPrototype.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/Wallet/BSVWallet.h>
#include <LibWeb/WebIDL/DOMException.h>
#include <LibWeb/WebIDL/Promise.h>

namespace Web::Wallet {

GC_DEFINE_ALLOCATOR(BSVWallet);

GC::Ref<BSVWallet> BSVWallet::create(JS::Realm& realm)
{
    return realm.create<BSVWallet>(realm);
}

BSVWallet::BSVWallet(JS::Realm& realm)
    : PlatformObject(realm)
{
}

BSVWallet::~BSVWallet() = default;

void BSVWallet::initialize(JS::Realm& realm)
{
    WEB_SET_PROTOTYPE_FOR_INTERFACE(BSVWallet);
    Base::initialize(realm);
}

void BSVWallet::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
}

GC::Ref<WebIDL::Promise> BSVWallet::get_public_key(WalletKeyOptions const&)
{
    auto& realm = this->realm();
    auto promise = WebIDL::create_promise(realm);
    WebIDL::reject_promise(realm, *promise, WebIDL::NotSupportedError::create(realm, "Wallet not configured"_utf16));
    return promise;
}

GC::Ref<WebIDL::Promise> BSVWallet::create_action(WalletActionOptions const&)
{
    auto& realm = this->realm();
    auto promise = WebIDL::create_promise(realm);
    WebIDL::reject_promise(realm, *promise, WebIDL::NotSupportedError::create(realm, "Wallet not configured"_utf16));
    return promise;
}

GC::Ref<WebIDL::Promise> BSVWallet::sign_action(WalletSignOptions const&)
{
    auto& realm = this->realm();
    auto promise = WebIDL::create_promise(realm);
    WebIDL::reject_promise(realm, *promise, WebIDL::NotSupportedError::create(realm, "Wallet not configured"_utf16));
    return promise;
}

GC::Ref<WebIDL::Promise> BSVWallet::list_outputs(WalletListOptions const&)
{
    auto& realm = this->realm();
    auto promise = WebIDL::create_promise(realm);
    WebIDL::reject_promise(realm, *promise, WebIDL::NotSupportedError::create(realm, "Wallet not configured"_utf16));
    return promise;
}

GC::Ref<WebIDL::Promise> BSVWallet::list_actions(WalletListOptions const&)
{
    auto& realm = this->realm();
    auto promise = WebIDL::create_promise(realm);
    WebIDL::reject_promise(realm, *promise, WebIDL::NotSupportedError::create(realm, "Wallet not configured"_utf16));
    return promise;
}

}
