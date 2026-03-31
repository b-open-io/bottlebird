# HTML App Publishing Pipeline — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan.

**Goal:** Enable publishing HTML web applications as ORDFS directory inscriptions on BSV, matching the reference wallet-desktop's publish flow.

**Architecture:** The publishing pipeline has 4 layers:

1. **UI Layer** (wallet.html/JS) — multi-step wizard for file selection, configuration, review
2. **WalletUI Layer** (C++) — receives files + metadata, orchestrates inscription
3. **LibWallet Layer** (C++) — calls bsvz/zig-wallet-toolbox C ABI to build inscription transactions
4. **Zig Layer** (bsvz + zig-wallet-toolbox) — builds ORDFS directory structure, creates transaction outputs, signs, broadcasts

**How ORDFS directory inscription works** (from reference `packages/actions/src/registry/package-tx.ts`):
- Each file becomes a 1-sat inscription output with P2PKH prefix
- Subdirectories get their own `ord-fs/json` manifest inscriptions
- Root manifest references files as `_N` (output index N) and subdirs as `_M` (manifest output index)
- MAP metadata (app name, type, version, OpNS) is appended to the root manifest
- AIP signature authenticates the publisher
- All outputs are in a single transaction via `createAction`

**What exists today:**
- Single-file inscription works via `inscribe` action (base64 content → script → createAction)
- The Publish wizard UI (Wave 2) has content type picker, configure, and publish steps
- bsvz has all crypto primitives (signing, script building, P2PKH)
- zig-wallet-toolbox has `createAction` wired to remote storage

**What's missing:**

### Layer 1: UI (about:wallet)
- Directory selection via `<input type="file" webkitdirectory>` (Ladybird may support this)
- Build output display (file list with sizes and types)
- If webkitdirectory not supported: individual file multi-select as fallback

### Layer 2: WalletUI (C++)
- `inscribe_files` handler that accepts multiple file blobs + metadata
- Orchestrates the Zig layer call on a background thread

### Layer 3: LibWallet (C++)
- New method: `inscribe_package(files, metadata, backend_url)`
- Creates remote wallet handle, calls zig-wallet-toolbox's publish function

### Layer 4: Zig (zig-wallet-toolbox + bsvz)
- New `publish_package` function in zig-wallet-toolbox that:
  1. Takes array of {path, content_bytes, content_type}
  2. Builds ORDFS directory manifest (matching reference `buildPackageOutputs`)
  3. Creates inscription scripts for each file
  4. Builds root manifest with `ord-fs/json` content type
  5. Adds MAP metadata + AIP signature
  6. Calls `wallet.createAction` with all outputs
  7. Returns txid + outpoint

### Layer 4 (bsvz additions needed):
- `Inscription.create(content, contentType, scriptPrefix)` — builds inscription envelope script
- `MAP.encode(metadata)` — builds MAP protocol script suffix
- `AIP.sign(script, privateKey)` — builds AIP signature suffix
- These are script template operations that belong in bsvz (go-sdk parity: `@1sat/templates`)

---

## Implementation order

### Phase 1: bsvz script templates (go-sdk parity)
Add inscription envelope, MAP, and AIP script templates to bsvz.
These are fundamental BSV script operations.

### Phase 2: zig-wallet-toolbox publish action
Add `publishPackage` to the wallet that builds ORDFS directory transactions.
Add C ABI export `bsvwallet_publish_package`.

### Phase 3: LibWallet + WalletUI C++ wiring
Wire the C ABI through LibWallet to WalletUI handlers.

### Phase 4: UI file selection + wizard completion
Wire the Publish wizard's HTML App flow to use the backend.

---

## Dependency chain

```
bsvz: Inscription + MAP + AIP templates
    ↓
zig-wallet-toolbox: publishPackage action
    ↓
LibWallet C++: inscribe_package method
    ↓
WalletUI C++: inscribe_files handler
    ↓
wallet.js: HTML App wizard flow
```

Each phase produces independently testable, committable work.
