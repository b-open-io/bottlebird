set(SOURCES
    WalletManager.cpp
)

ladybird_lib(LibWallet wallet)

target_include_directories(LibWallet PRIVATE /Users/satchmo/code/bsvz/include)
target_link_libraries(LibWallet PRIVATE /Users/satchmo/code/bsvz/zig-out/lib/libbsvz_c.a)
