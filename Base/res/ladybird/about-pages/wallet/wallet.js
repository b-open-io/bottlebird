// Views
const onboardingView = document.querySelector("#onboarding-view");
const mnemonicShowView = document.querySelector("#mnemonic-show-view");
const importView = document.querySelector("#import-view");
const importMnemonicView = document.querySelector("#import-mnemonic-view");
const importFileView = document.querySelector("#import-file-view");
const walletView = document.querySelector("#wallet-view");

// Wallet dashboard controls
const walletEnabledToggle = document.querySelector("#wallet-enabled-toggle");
const backendURL = document.querySelector("#backend-url");
const statusIndicator = document.querySelector("#status-indicator");
const statusText = document.querySelector("#status-text");
const balanceConfirmed = document.querySelector("#balance-confirmed");
const balanceUnconfirmed = document.querySelector("#balance-unconfirmed");
const receiveAddress = document.querySelector("#receive-address");
const sendRecipient = document.querySelector("#send-recipient");
const sendAmount = document.querySelector("#send-amount");
const sendButton = document.querySelector("#send-button");
const sendMessage = document.querySelector("#send-message");

// Onboarding
const btnCreateWallet = document.querySelector("#btn-create-wallet");
const btnImportWallet = document.querySelector("#btn-import-wallet");
const btnMnemonicBack = document.querySelector("#btn-mnemonic-back");
const btnMnemonicConfirm = document.querySelector("#btn-mnemonic-confirm");
const mnemonicDisplay = document.querySelector("#mnemonic-display");

// Import method choice
const btnImportBack = document.querySelector("#btn-import-back");
const btnImportMnemonic = document.querySelector("#btn-import-mnemonic");
const btnImportFile = document.querySelector("#btn-import-file");

// Import: mnemonic
const btnMnemonicImportBack = document.querySelector("#btn-mnemonic-import-back");
const btnMnemonicImportConfirm = document.querySelector("#btn-mnemonic-import-confirm");
const importMnemonic = document.querySelector("#import-mnemonic");
const importMnemonicError = document.querySelector("#import-mnemonic-error");

// Import: file
const btnFileImportBack = document.querySelector("#btn-file-import-back");
const btnFileImportConfirm = document.querySelector("#btn-file-import-confirm");
const btnPickFile = document.querySelector("#btn-pick-file");
const fileNameDisplay = document.querySelector("#file-name");
const filePasswordGroup = document.querySelector("#file-password-group");
const filePassword = document.querySelector("#file-password");
const importFileError = document.querySelector("#import-file-error");

// Identity view elements
const identityBapIdView = document.querySelector("#identity-bap-id");
const identityKeyView = document.querySelector("#identity-key");
const identityEmpty = document.querySelector("#identity-empty");
const identityReceiveAddress = document.querySelector("#identity-receive-address");

// Dashboard add account
const btnAddAccount = document.querySelector("#btn-add-account");

// Ordinals view elements
const ordinalsGrid = document.querySelector("#ordinals-grid");
const ordinalsEmpty = document.querySelector("#ordinals-empty");
const ordinalsSkeleton = document.querySelector("#ordinals-skeleton");
const ordinalsError = document.querySelector("#ordinals-error");

// Tokens view elements
const tokensList = document.querySelector("#tokens-list");
const tokensEmpty = document.querySelector("#tokens-empty");
const tokensLoading = document.querySelector("#tokens-loading");
const tokensError = document.querySelector("#tokens-error");

// Market view elements
const marketGrid = document.querySelector("#market-grid");
const marketEmpty = document.querySelector("#market-empty");
const marketSkeleton = document.querySelector("#market-skeleton");
const marketError = document.querySelector("#market-error");
const marketSearch = document.querySelector("#market-search");
const marketSort = document.querySelector("#market-sort");
const marketRefresh = document.querySelector("#market-refresh");

let walletEnabled = false;
let previousView = "onboarding";
let pendingFileData = null;

// ── Data fetch state tracking ─────────────────────────────────────
// Prevents re-fetching data every time a user switches tabs
const fetchState = {
    ordinals: { loaded: false, loading: false },
    tokens: { loaded: false, loading: false },
    market: { loaded: false, loading: false, data: [] },
};

const ORDFS_BASE = "https://ordfs.network";
// All wallet data comes through WalletUI C++ via message passing
// ORDFS_BASE is only used for ordinal thumbnail images

function formatSatoshis(sats) {
    return `${Number(sats).toLocaleString()} sat`;
}

function formatSatsCompact(sats) {
    if (sats >= 1_000_000) return `${(sats / 1_000_000).toFixed(2)}M`;
    if (sats >= 1_000) return `${(sats / 1_000).toFixed(0)}K`;
    return sats.toLocaleString();
}

function truncateOutpoint(outpoint) {
    if (!outpoint || outpoint.length <= 14) return outpoint || "";
    return `${outpoint.slice(0, 8)}...${outpoint.slice(-4)}`;
}

const ALL_VIEWS = [onboardingView, mnemonicShowView, importView, importMnemonicView, importFileView, walletView];

function showView(name) {
    const map = {
        "onboarding": onboardingView,
        "mnemonic-show": mnemonicShowView,
        "import": importView,
        "import-mnemonic": importMnemonicView,
        "import-file": importFileView,
        "wallet": walletView,
    };
    for (const v of ALL_VIEWS) {
        v.classList.add("hidden");
    }
    if (map[name]) map[name].classList.remove("hidden");
}

// ── Hash-based sub-view routing ────────────────────────────────────

const SUB_VIEWS = ["dashboard", "ordinals", "tokens", "market", "identity", "send", "receive", "chat", "publish"];

function navigateToView(hash) {
    const view = hash.replace("#", "") || "dashboard";
    const resolvedView = SUB_VIEWS.includes(view) ? view : "dashboard";

    // Hide all sub-views
    for (const v of SUB_VIEWS) {
        const el = document.querySelector(`#view-${v}`);
        if (el) el.classList.add("hidden");
    }

    // Show active sub-view
    const activeEl = document.querySelector(`#view-${resolvedView}`);
    if (activeEl) {
        activeEl.classList.remove("hidden");
    } else {
        const dashboard = document.querySelector("#view-dashboard");
        if (dashboard) dashboard.classList.remove("hidden");
    }

    // Update nav active state
    document.querySelectorAll(".nav-tab").forEach(tab => {
        const isActive = tab.dataset.view === resolvedView;
        tab.classList.toggle("active", isActive);
    });

    // Trigger data fetching for content views
    if (resolvedView === "ordinals" && !fetchState.ordinals.loaded && !fetchState.ordinals.loading) {
        fetchOrdinals();
    }
    if (resolvedView === "tokens" && !fetchState.tokens.loaded && !fetchState.tokens.loading) {
        fetchTokens();
    }
    if (resolvedView === "market" && !fetchState.market.loaded && !fetchState.market.loading) {
        fetchMarketListings();
    }
}

window.addEventListener("hashchange", () => navigateToView(location.hash));

// ── Ordinals fetching ─────────────────────────────────────────────

function fetchOrdinals() {
    fetchState.ordinals.loading = true;
    ordinalsEmpty.classList.add("hidden");
    ordinalsError.classList.add("hidden");
    ordinalsGrid.innerHTML = "";
    ordinalsSkeleton.classList.remove("hidden");

    // Request ordinals via WalletUI C++ (BRC-100 listOutputs on background thread)
    ladybird.sendMessage("listOrdinals");
    return; // Response handled via WebUIMessage "ordinalsList"
}

// Legacy fetch removed — ordinals come from wallet's listOutputs via C++
function handleOrdinalsResponse(data) {
    ordinalsSkeleton.classList.add("hidden");
    fetchState.ordinals.loading = false;

    if (data.error) {
        ordinalsError.textContent = data.error;
        ordinalsError.classList.remove("hidden");
        return;
    }

    const items = data.ordinals || [];
    if (items.length === 0) {
        ordinalsEmpty.classList.remove("hidden");
        fetchState.ordinals.loaded = true;
        return;
    }

    for (const item of items) {
        ordinalsGrid.appendChild(createOrdinalCard(item));
    }
    fetchState.ordinals.loaded = true;
}

function createOrdinalCard(item) {
    const card = document.createElement("div");
    card.className = "ordinal-card";
    card.addEventListener("click", () => {
        window.location.href = `1sat://${item.outpoint}`;
    });

    const thumb = document.createElement("div");
    thumb.className = "thumb";
    const img = document.createElement("img");
    img.src = `${ORDFS_BASE}/${item.outpoint}`;
    img.alt = item.name;
    img.loading = "lazy";
    img.addEventListener("error", () => {
        img.remove();
        const fallback = document.createElement("span");
        fallback.className = "thumb-error";
        fallback.textContent = "\u25C6"; // diamond
        thumb.appendChild(fallback);
    });
    thumb.appendChild(img);

    const meta = document.createElement("div");
    meta.className = "meta";
    const nameEl = document.createElement("div");
    nameEl.className = "name";
    nameEl.textContent = item.name || "Unnamed";
    const outpointEl = document.createElement("div");
    outpointEl.className = "outpoint";
    outpointEl.textContent = truncateOutpoint(item.outpoint);
    meta.appendChild(nameEl);
    meta.appendChild(outpointEl);

    card.appendChild(thumb);
    card.appendChild(meta);
    return card;
}

// ── Tokens fetching ───────────────────────────────────────────────

function fetchTokens() {
    fetchState.tokens.loading = true;
    tokensEmpty.classList.add("hidden");
    tokensError.classList.add("hidden");
    tokensList.innerHTML = "";
    tokensLoading.classList.remove("hidden");

    // Request tokens via WalletUI C++ (BRC-100 listOutputs on background thread)
    ladybird.sendMessage("listTokens");
    return; // Response handled via WebUIMessage "tokensList"
}

function handleTokensResponse(data) {
    tokensLoading.classList.add("hidden");
    fetchState.tokens.loading = false;

    if (data.error) {
        tokensError.textContent = data.error;
        tokensError.classList.remove("hidden");
        return;
    }

    const items = data.tokens || [];
    if (items.length === 0) {
        tokensEmpty.classList.remove("hidden");
        if (data.note) tokensEmpty.querySelector("h3").textContent = data.note;
        fetchState.tokens.loaded = true;
        return;
    }

    for (const token of items) {
        tokensList.appendChild(createTokenRow(token));
    }
    fetchState.tokens.loaded = true;
}

function createTokenRow(token) {
    const sym = token.symbol || token.sym || "?";
    const row = document.createElement("div");
    row.className = "token-row";

    const icon = document.createElement("div");
    icon.className = "token-icon";
    if (token.icon) {
        const img = document.createElement("img");
        img.src = `${ORDFS_BASE}/${token.icon}`;
        img.alt = sym;
        img.loading = "lazy";
        img.addEventListener("error", () => {
            img.remove();
            icon.textContent = sym.charAt(0).toUpperCase();
        });
        icon.appendChild(img);
    } else {
        icon.textContent = sym.charAt(0).toUpperCase();
    }

    const info = document.createElement("div");
    info.className = "token-info";
    const symEl = document.createElement("div");
    symEl.className = "token-symbol";
    symEl.textContent = sym;
    const idEl = document.createElement("div");
    idEl.className = "token-id";
    idEl.textContent = truncateOutpoint(token.id);
    idEl.title = token.id;
    info.appendChild(symEl);
    info.appendChild(idEl);

    const balanceEl = document.createElement("div");
    balanceEl.className = "token-balance";
    balanceEl.textContent = Number(token.balance || 0).toLocaleString();

    row.appendChild(icon);
    row.appendChild(info);
    row.appendChild(balanceEl);
    return row;
}

// ── Market fetching ───────────────────────────────────────────────

function fetchMarketListings() {
    // Market listings require a local 1sat-stack — not available via the wallet API
    fetchState.market.loading = false;
    fetchState.market.loaded = true;
    marketSkeleton.classList.add("hidden");
    marketGrid.innerHTML = "";
    marketEmpty.classList.remove("hidden");
    const heading = marketEmpty.querySelector("h3");
    if (heading) heading.textContent = "Market listings require a 1sat-stack connection.";
    const desc = marketEmpty.querySelector("p");
    if (desc) desc.textContent = "Set your backend URL to a running 1sat-stack instance.";
}

function renderMarketGrid(items) {
    marketGrid.innerHTML = "";

    // Apply search filter
    const query = (marketSearch?.value || "").toLowerCase().trim();
    let filtered = query
        ? items.filter(l => l.name.toLowerCase().includes(query))
        : items;

    // Apply sort
    const sortMode = marketSort?.value || "recent";
    if (sortMode === "price-asc") {
        filtered = [...filtered].sort((a, b) => a.priceSats - b.priceSats);
    } else if (sortMode === "price-desc") {
        filtered = [...filtered].sort((a, b) => b.priceSats - a.priceSats);
    }

    if (filtered.length === 0) {
        marketEmpty.classList.remove("hidden");
        if (query) {
            marketEmpty.querySelector("h3").textContent = `No listings match "${query}"`;
        } else {
            marketEmpty.querySelector("h3").textContent = "No listings available";
        }
        return;
    }

    marketEmpty.classList.add("hidden");
    for (const item of filtered) {
        marketGrid.appendChild(createListingCard(item));
    }
}

function createListingCard(item) {
    const card = document.createElement("div");
    card.className = "listing-card";
    card.addEventListener("click", () => {
        window.location.href = `1sat://${item.outpoint}`;
    });

    const thumb = document.createElement("div");
    thumb.className = "thumb";
    const img = document.createElement("img");
    img.src = `${ORDFS_BASE}/${item.outpoint}`;
    img.alt = item.name;
    img.loading = "lazy";
    img.addEventListener("error", () => {
        img.remove();
        const fallback = document.createElement("span");
        fallback.className = "thumb-error";
        fallback.textContent = "\u2605"; // star
        thumb.appendChild(fallback);
    });
    thumb.appendChild(img);

    const meta = document.createElement("div");
    meta.className = "meta";
    const nameEl = document.createElement("div");
    nameEl.className = "name";
    nameEl.textContent = item.name;
    const priceEl = document.createElement("div");
    priceEl.className = "price";
    priceEl.textContent = `${formatSatsCompact(item.priceSats)} sats`;
    meta.appendChild(nameEl);
    meta.appendChild(priceEl);

    card.appendChild(thumb);
    card.appendChild(meta);
    return card;
}

// Market search/sort event handlers
if (marketSearch) {
    marketSearch.addEventListener("input", () => {
        if (fetchState.market.loaded) {
            renderMarketGrid(fetchState.market.data);
        }
    });
}

if (marketSort) {
    marketSort.addEventListener("change", () => {
        if (fetchState.market.loaded) {
            renderMarketGrid(fetchState.market.data);
        }
    });
}

if (marketRefresh) {
    marketRefresh.addEventListener("click", () => {
        if (!fetchState.market.loading) {
            fetchMarketListings();
        }
    });
}

// ── Routing ────────────────────────────────────────────────────────

function refreshWallet() {
    ladybird.sendMessage("loadWalletStatus");
    ladybird.sendMessage("getReceiveAddress");
}

let currentAddress = null;

function requestBalance() {
    statusIndicator.className = "status-indicator disconnected";
    statusText.textContent = "Connecting...";
    ladybird.sendMessage("getBalance");
}

// ── Status updates ──────────────────────────────────────────────────

function updateStatus(status) {
    walletEnabled = status.enabled;

    // Show onboarding only if no wallet has been created/imported
    if (!status.initialized) {
        showView("onboarding");
        return;
    }

    showView("wallet");
    // Apply hash-based routing once wallet view is visible
    navigateToView(location.hash);

    walletEnabledToggle.checked = status.enabled;
    if (status.backendURL) backendURL.value = status.backendURL;

    if (status.enabled) {
        requestBalance();
    } else {
        statusIndicator.className = "status-indicator disconnected";
        statusText.textContent = "Offline";
    }

    // Populate identity view (dedicated tab)
    if (status.bapId || status.identityPubkey) {
        if (identityBapIdView) identityBapIdView.textContent = status.bapId || "";
        if (identityKeyView) identityKeyView.textContent = status.identityPubkey || "";
        if (identityEmpty) identityEmpty.classList.add("hidden");
    } else {
        if (identityBapIdView) identityBapIdView.textContent = "No BAP ID available";
        if (identityKeyView) identityKeyView.textContent = "No identity key available";
        if (identityEmpty) identityEmpty.classList.remove("hidden");
    }
}

function updateBalance(balance) {
    balanceConfirmed.textContent = formatSatoshis(balance.confirmed);
    balanceUnconfirmed.textContent = `${formatSatoshis(balance.unconfirmed)} unconfirmed`;

    // Update connection status based on backend response
    if (balance.connected === true) {
        statusIndicator.className = "status-indicator connected";
        statusText.textContent = "Connected";
    } else if (balance.connected === false) {
        statusIndicator.className = "status-indicator disconnected";
        statusText.textContent = "Unreachable";
    }
}

function updateReceiveAddress(data) {
    if (data.address) {
        currentAddress = data.address;
        receiveAddress.className = "address-display";
        receiveAddress.textContent = data.address;

        // Also show in identity view
        if (identityReceiveAddress) {
            identityReceiveAddress.textContent = data.address;
        }

        if (walletEnabled) requestBalance();
    } else {
        currentAddress = null;
        receiveAddress.className = "address-placeholder";
        receiveAddress.textContent = "Create or import a wallet to see your address";

        if (identityReceiveAddress) {
            identityReceiveAddress.textContent = "No address available";
        }

        updateBalance({ confirmed: 0, unconfirmed: 0 });
    }
}

function showSendMessage(text, isError) {
    sendMessage.textContent = text;
    sendMessage.className = `message ${isError ? "error" : "success"}`;
}

// ── Create wallet ───────────────────────────────────────────────────

btnCreateWallet.addEventListener("click", () => {
    ladybird.sendMessage("createWallet");
});

btnMnemonicBack.addEventListener("click", () => showView("onboarding"));

btnMnemonicConfirm.addEventListener("click", () => {
    ladybird.sendMessage("confirmWalletCreation");
    refreshWallet();
});

// ── Import: method choice ───────────────────────────────────────────

function openImport(from) {
    previousView = from;
    showView("import");
}

btnImportWallet.addEventListener("click", () => openImport("onboarding"));
btnAddAccount.addEventListener("click", (e) => { e.preventDefault(); openImport("wallet"); });

btnImportBack.addEventListener("click", () => {
    showView(previousView === "wallet" && walletEnabled ? "wallet" : "onboarding");
});

// ── Import: mnemonic ────────────────────────────────────────────────

btnImportMnemonic.addEventListener("click", () => {
    importMnemonic.value = "";
    importMnemonicError.className = "hidden";
    showView("import-mnemonic");
});

btnMnemonicImportBack.addEventListener("click", () => showView("import"));

btnMnemonicImportConfirm.addEventListener("click", () => {
    const mnemonic = importMnemonic.value.trim().toLowerCase();
    const words = mnemonic.split(/\s+/).filter(w => w.length > 0);

    if (words.length !== 12 && words.length !== 24) {
        importMnemonicError.textContent = `Expected 12 or 24 words, got ${words.length}.`;
        importMnemonicError.className = "message error";
        return;
    }

    importMnemonicError.className = "hidden";
    ladybird.sendMessage("importWallet", words.join(" "));
});

// ── Import: backup file ─────────────────────────────────────────────

btnImportFile.addEventListener("click", () => {
    pendingFileData = null;
    fileNameDisplay.textContent = "No file selected";
    filePasswordGroup.classList.add("hidden");
    btnFileImportConfirm.classList.add("hidden");
    importFileError.className = "hidden";
    if (filePassword) filePassword.value = "";
    showView("import-file");
});

btnFileImportBack.addEventListener("click", () => showView("import"));

btnPickFile.addEventListener("click", () => {
    // Use the browser's file input since we don't have native file picker wired yet
    const input = document.createElement("input");
    input.type = "file";
    input.accept = ".bep,.json,.txt";
    input.addEventListener("change", () => {
        const file = input.files[0];
        if (!file) return;

        fileNameDisplay.textContent = file.name;
        const reader = new FileReader();
        reader.onload = () => {
            pendingFileData = reader.result;

            // Check if it's unencrypted JSON
            try {
                JSON.parse(pendingFileData);
                // Unencrypted — import directly
                ladybird.sendMessage("importBackupFile", { data: pendingFileData, password: "" });
            } catch {
                // Encrypted — need password
                filePasswordGroup.classList.remove("hidden");
                btnFileImportConfirm.classList.remove("hidden");
                filePassword.focus();
            }
        };
        reader.readAsText(file);
    });
    input.click();
});

btnFileImportConfirm.addEventListener("click", () => {
    if (!pendingFileData) {
        importFileError.textContent = "No file selected.";
        importFileError.className = "message error";
        return;
    }

    const pw = filePassword ? filePassword.value : "";
    if (!pw) {
        importFileError.textContent = "Please enter the backup password.";
        importFileError.className = "message error";
        return;
    }

    importFileError.className = "hidden";
    ladybird.sendMessage("importBackupFile", { data: pendingFileData, password: pw });
});

// ── Settings ────────────────────────────────────────────────────────

walletEnabledToggle.addEventListener("change", () => {
    ladybird.sendMessage("setWalletEnabled", walletEnabledToggle.checked);
});

backendURL.addEventListener("change", () => {
    if (backendURL.value && backendURL.checkValidity()) {
        ladybird.sendMessage("setWalletBackendURL", backendURL.value);
    }
});

// Click-to-copy for identity view (dedicated tab)
if (identityBapIdView) {
    identityBapIdView.addEventListener("click", () => {
        if (identityBapIdView.textContent && identityBapIdView.textContent !== "No BAP ID available") {
            navigator.clipboard.writeText(identityBapIdView.textContent);
            const original = identityBapIdView.textContent;
            identityBapIdView.textContent = "Copied!";
            setTimeout(() => { identityBapIdView.textContent = original; }, 1500);
        }
    });
}

if (identityKeyView) {
    identityKeyView.addEventListener("click", () => {
        if (identityKeyView.textContent && identityKeyView.textContent !== "No identity key available") {
            navigator.clipboard.writeText(identityKeyView.textContent);
            const original = identityKeyView.textContent;
            identityKeyView.textContent = "Copied!";
            setTimeout(() => { identityKeyView.textContent = original; }, 1500);
        }
    });
}

if (identityReceiveAddress) {
    identityReceiveAddress.addEventListener("click", () => {
        const text = identityReceiveAddress.textContent;
        if (text && text !== "Loading..." && text !== "No address available") {
            navigator.clipboard.writeText(text);
            const original = text;
            identityReceiveAddress.textContent = "Copied!";
            setTimeout(() => { identityReceiveAddress.textContent = original; }, 1500);
        }
    });
}

receiveAddress.addEventListener("click", () => {
    if (receiveAddress.classList.contains("address-display")) {
        navigator.clipboard.writeText(receiveAddress.textContent);
        const original = receiveAddress.textContent;
        receiveAddress.textContent = "Copied!";
        setTimeout(() => { receiveAddress.textContent = original; }, 1500);
    }
});

sendButton.addEventListener("click", () => {
    const recipient = sendRecipient.value.trim();
    const amount = parseInt(sendAmount.value, 10);

    if (!recipient) { showSendMessage("Please enter a recipient address.", true); return; }
    if (!amount || amount <= 0) { showSendMessage("Please enter a valid amount.", true); return; }

    sendMessage.className = "hidden";
    ladybird.sendMessage("sendPayment", { recipient, amount });
});

// ── WebUI lifecycle ─────────────────────────────────────────────────

document.addEventListener("WebUILoaded", () => refreshWallet());

document.addEventListener("WebUIMessage", event => {
    const { name, data } = event.detail;

    if (name === "walletStatus") {
        updateStatus(data);
    } else if (name === "walletBalance") {
        updateBalance(data);
    } else if (name === "receiveAddress") {
        updateReceiveAddress(data);
    } else if (name === "walletCreated") {
        if (data.mnemonic) {
            mnemonicDisplay.textContent = data.mnemonic;
            showView("mnemonic-show");
        }
    } else if (name === "walletImported") {
        if (data.error) {
            // Show error on whichever import view is active
            if (!importMnemonicView.classList.contains("hidden")) {
                importMnemonicError.textContent = data.error;
                importMnemonicError.className = "message error";
            } else if (!importFileView.classList.contains("hidden")) {
                importFileError.textContent = data.error;
                importFileError.className = "message error";
            }
        } else {
            refreshWallet();
        }
    } else if (name === "walletIdentity") {
        if (data.identityPubkey && identityKeyView) {
            identityKeyView.textContent = data.identityPubkey;
        }
        if (data.bapId && identityBapIdView) {
            identityBapIdView.textContent = data.bapId;
        }
    } else if (name === "ordinalsList") {
        handleOrdinalsResponse(data);
    } else if (name === "tokensList") {
        handleTokensResponse(data);
    } else if (name === "paymentResult") {
        if (data.error) {
            showSendMessage(data.error, true);
        } else {
            showSendMessage(`Payment sent. TX: ${data.txid}`, false);
            sendRecipient.value = "";
            sendAmount.value = "";
        }
    }
});
