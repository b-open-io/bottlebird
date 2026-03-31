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

// Profile elements
const profileName = document.querySelector("#profile-name");
const profileDescription = document.querySelector("#profile-description");
const profileAvatar = document.querySelector("#profile-avatar");
const btnSaveProfile = document.querySelector("#btn-save-profile");
const profileMessage = document.querySelector("#profile-message");

// Chat / Publish elements
const btnOpenBitchat = document.querySelector("#btn-open-bitchat");
const btnInscribeFile = document.querySelector("#btn-inscribe-file");
const inscribeFileName = document.querySelector("#inscribe-file-name");
const inscribeTypeDisplay = document.querySelector("#inscribe-type-display");
const inscribeContentType = document.querySelector("#inscribe-content-type");
const inscribePreview = document.querySelector("#inscribe-preview");
const inscribePreviewArea = document.querySelector("#inscribe-preview-area");
const btnInscribe = document.querySelector("#btn-inscribe");
const inscribeMessage = document.querySelector("#inscribe-message");

// Receive copy button
const btnCopyAddress = document.querySelector("#btn-copy-address");
// Apps view elements
const appsGrid = document.querySelector("#apps-grid");
const appsLoading = document.querySelector("#apps-loading");
const appsEmpty = document.querySelector("#apps-empty");
const appsError = document.querySelector("#apps-error");
const appsErrorText = document.querySelector("#apps-error-text");
const appsRetry = document.querySelector("#apps-retry");
const appsSearch = document.querySelector("#apps-search");
const appsCategories = document.querySelector("#apps-categories");
const appsRefresh = document.querySelector("#apps-refresh");

let walletEnabled = false;
let previousView = "onboarding";
let pendingFileData = null;

// ── Data fetch state tracking ─────────────────────────────────────
// Prevents re-fetching data every time a user switches tabs
const fetchState = {
    ordinals: { loaded: false, loading: false },
    tokens: { loaded: false, loading: false },
    market: { loaded: false, loading: false, data: [] },
    apps: { loaded: false, loading: false, data: [], activeCategory: "All" },
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

const SUB_VIEWS = ["dashboard", "ordinals", "tokens", "market", "apps", "identity", "send", "receive", "chat", "publish"];

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
    if (resolvedView === "apps" && !fetchState.apps.loaded && !fetchState.apps.loading) {
        fetchApps();
    }
}

window.addEventListener("hashchange", () => navigateToView(location.hash));

// ── Ordinals fetching ─────────────────────────────────────────────

function fetchOrdinals() {
    fetchState.ordinals.loading = true;
    ordinalsEmpty.classList.add("hidden");
    ordinalsError.classList.add("hidden");
    ordinalsGrid.innerHTML = "";
    if (ordinalsSkeleton) ordinalsSkeleton.classList.remove("hidden");

    // Request ordinals via WalletUI C++ (BRC-100 listOutputs on background thread)
    ladybird.sendMessage("listOrdinals");
    return; // Response handled via WebUIMessage "ordinalsList"
}

// Legacy fetch removed — ordinals come from wallet's listOutputs via C++
function handleOrdinalsResponse(data) {
    if (ordinalsSkeleton) ordinalsSkeleton.classList.add("hidden");
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

    ordinalsGrid.classList.remove("hidden");
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
        const fallback = document.createElementNS("http://www.w3.org/2000/svg", "svg");
        fallback.setAttribute("width", "24");
        fallback.setAttribute("height", "24");
        fallback.setAttribute("viewBox", "0 0 24 24");
        fallback.setAttribute("fill", "none");
        fallback.setAttribute("stroke", "currentColor");
        fallback.setAttribute("stroke-width", "2");
        fallback.setAttribute("stroke-linecap", "round");
        fallback.setAttribute("stroke-linejoin", "round");
        fallback.classList.add("thumb-error");
        const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
        path.setAttribute("d", "M6 3h12l4 6-10 13L2 9z");
        fallback.appendChild(path);
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
    if (marketSkeleton) marketSkeleton.classList.add("hidden");
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
    marketGrid.classList.remove("hidden");
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
        const fallback = document.createElementNS("http://www.w3.org/2000/svg", "svg");
        fallback.setAttribute("width", "24");
        fallback.setAttribute("height", "24");
        fallback.setAttribute("viewBox", "0 0 24 24");
        fallback.setAttribute("fill", "none");
        fallback.setAttribute("stroke", "currentColor");
        fallback.setAttribute("stroke-width", "2");
        fallback.setAttribute("stroke-linecap", "round");
        fallback.setAttribute("stroke-linejoin", "round");
        fallback.classList.add("thumb-error");
        const p1 = document.createElementNS("http://www.w3.org/2000/svg", "path");
        p1.setAttribute("d", "M6 2 3 6v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2V6l-3-4Z");
        const p2 = document.createElementNS("http://www.w3.org/2000/svg", "path");
        p2.setAttribute("d", "M3 6h18");
        const p3 = document.createElementNS("http://www.w3.org/2000/svg", "path");
        p3.setAttribute("d", "M16 10a4 4 0 0 1-8 0");
        fallback.appendChild(p1);
        fallback.appendChild(p2);
        fallback.appendChild(p3);
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

// ── Apps fetching (overlay network) ──────────────────────────────

// SLAP tracker hosts for mainnet overlay discovery
const OVERLAY_HOSTS = [
    "https://overlay-us-1.bsvb.tech",
    "https://overlay-eu-1.bsvb.tech",
    "https://overlay-ap-1.bsvb.tech",
];

async function fetchApps(force = false) {
    if (fetchState.apps.loading) return;
    fetchState.apps.loading = true;

    appsLoading.classList.remove("hidden");
    appsGrid.classList.add("hidden");
    appsEmpty.classList.add("hidden");
    appsError.classList.add("hidden");
    appsCategories.innerHTML = "";

    if (force) {
        fetchState.apps.loaded = false;
        fetchState.apps.data = [];
        fetchState.apps.activeCategory = "All";
    }

    // Try each overlay host until one succeeds
    let data = null;
    let lastErr = null;
    for (const host of OVERLAY_HOSTS) {
        try {
            const controller = new AbortController();
            const timer = setTimeout(() => controller.abort(), 8000);
            const resp = await fetch(`${host}/lookup`, {
                method: "POST",
                headers: {
                    "Content-Type": "application/json",
                    "X-Aggregation": "yes",
                },
                body: JSON.stringify({
                    service: "ls_apps",
                    query: { includeBeef: false },
                }),
                signal: controller.signal,
            });
            clearTimeout(timer);

            if (!resp.ok) {
                lastErr = new Error(`HTTP ${resp.status} from ${host}`);
                continue;
            }

            data = await resp.json();
            break;
        } catch (err) {
            lastErr = err;
        }
    }

    appsLoading.classList.add("hidden");
    fetchState.apps.loading = false;

    if (!data) {
        appsErrorText.textContent = lastErr?.message || "Failed to reach overlay network";
        appsError.classList.remove("hidden");
        return;
    }

    // Parse the overlay response: { type: "output-list", outputs: [...] }
    const apps = parseOverlayApps(data);
    fetchState.apps.data = apps;
    fetchState.apps.loaded = true;

    renderApps();
}

function parseOverlayApps(answer) {
    if (!answer || answer.type !== "output-list" || !Array.isArray(answer.outputs)) return [];
    const apps = [];

    // The overlay returns BEEF binary with PushDrop tokens.
    // The app metadata JSON is embedded literally in the BEEF bytes.
    // Extract it by searching for the JSON pattern.
    const VERSION_MARKER = [123, 34, 118, 101, 114, 115, 105, 111, 110, 34, 58]; // {"version":

    for (const output of answer.outputs) {
        try {
            if (!output.beef || !Array.isArray(output.beef)) continue;
            const beef = output.beef;

            // Find {"version": in the byte array
            let jsonStart = -1;
            for (let i = 0; i < beef.length - VERSION_MARKER.length; i++) {
                let match = true;
                for (let j = 0; j < VERSION_MARKER.length; j++) {
                    if (beef[i + j] !== VERSION_MARKER[j]) { match = false; break; }
                }
                if (match) { jsonStart = i; break; }
            }
            if (jsonStart === -1) continue;

            // Find the end of the JSON object by counting braces
            let depth = 0;
            let jsonEnd = jsonStart;
            for (let i = jsonStart; i < beef.length; i++) {
                if (beef[i] === 123) depth++; // {
                else if (beef[i] === 125) depth--; // }
                if (depth === 0) { jsonEnd = i + 1; break; }
            }

            const jsonBytes = beef.slice(jsonStart, jsonEnd);
            const jsonStr = new TextDecoder().decode(new Uint8Array(jsonBytes));
            const metadata = JSON.parse(jsonStr);
            apps.push({ metadata, outputIndex: output.outputIndex });
        } catch {
            // Skip malformed records
        }
    }

    return apps;
}

function renderApps() {
    const apps = fetchState.apps.data;
    const activeCategory = fetchState.apps.activeCategory;
    const query = (appsSearch?.value || "").toLowerCase().trim();

    // Build category list from actual data
    const cats = new Set();
    for (const app of apps) {
        if (app.metadata?.category) cats.add(app.metadata.category);
    }
    const categories = ["All", ...Array.from(cats).sort()];

    // Render category pills
    appsCategories.innerHTML = "";
    for (const cat of categories) {
        const pill = document.createElement("button");
        pill.className = `category-pill${cat === activeCategory ? " active" : ""}`;
        pill.textContent = cat;
        pill.addEventListener("click", () => {
            fetchState.apps.activeCategory = cat;
            renderApps();
        });
        appsCategories.appendChild(pill);
    }

    // Filter
    const filtered = apps.filter(app => {
        const m = app.metadata;
        if (!m) return false;
        const matchesCat = activeCategory === "All" || m.category === activeCategory;
        if (!matchesCat) return false;
        if (!query) return true;
        return (
            (m.name || "").toLowerCase().includes(query) ||
            (m.description || "").toLowerCase().includes(query) ||
            (m.domain || "").toLowerCase().includes(query) ||
            (m.tags || []).some(t => t.toLowerCase().includes(query))
        );
    });

    appsGrid.innerHTML = "";
    appsEmpty.classList.add("hidden");

    if (filtered.length === 0) {
        appsGrid.classList.add("hidden");
        if (apps.length === 0) {
            appsEmpty.classList.remove("hidden");
        } else {
            // Search/filter yielded no results
            appsEmpty.classList.remove("hidden");
            appsEmpty.querySelector("h3").textContent = "No apps match your search";
            appsEmpty.querySelector("p").textContent = "Try a different search term or category.";
        }
        return;
    }

    appsGrid.classList.remove("hidden");
    for (const app of filtered) {
        appsGrid.appendChild(createAppCard(app));
    }
}

function createAppCard(app) {
    const m = app.metadata;
    const url = m.httpURL || (m.domain ? `https://${m.domain}` : null);

    const card = document.createElement("div");
    card.className = "app-card";
    if (url) {
        card.addEventListener("click", () => {
            window.open(url, "_blank");
        });
    }

    // Icon
    const iconWrap = document.createElement("div");
    iconWrap.className = "app-icon";
    if (m.icon) {
        const img = document.createElement("img");
        img.src = m.icon;
        img.alt = m.name || "";
        img.loading = "lazy";
        img.addEventListener("error", () => {
            img.remove();
            iconWrap.innerHTML = '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><line x1="2" y1="12" x2="22" y2="12"/><path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z"/></svg>';
        });
        iconWrap.appendChild(img);
    } else {
        iconWrap.innerHTML = '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><line x1="2" y1="12" x2="22" y2="12"/><path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z"/></svg>';
    }
    card.appendChild(iconWrap);

    // Name
    const nameEl = document.createElement("div");
    nameEl.className = "app-name";
    nameEl.textContent = m.name || "Unnamed";
    nameEl.title = m.name || "";
    card.appendChild(nameEl);

    // Description
    if (m.description) {
        const descEl = document.createElement("div");
        descEl.className = "app-desc";
        descEl.textContent = m.description;
        card.appendChild(descEl);
    }

    // Category badge
    if (m.category) {
        const badge = document.createElement("span");
        badge.className = "app-badge";
        badge.textContent = m.category;
        card.appendChild(badge);
    }

    return card;
}

// Apps event handlers
if (appsSearch) {
    appsSearch.addEventListener("input", () => {
        if (fetchState.apps.loaded) renderApps();
    });
}

if (appsRefresh) {
    appsRefresh.addEventListener("click", () => fetchApps(true));
}

if (appsRetry) {
    appsRetry.addEventListener("click", () => fetchApps(true));
}

// ── Routing ────────────────────────────────────────────────────────

function refreshWallet() {
    ladybird.sendMessage("loadWalletStatus");
    ladybird.sendMessage("getReceiveAddress");
    ladybird.sendMessage("loadProfile");
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
    } else if (name === "profileLoaded") {
        if (profileName && data.name) profileName.value = data.name;
        if (profileDescription && data.description) profileDescription.value = data.description;
        if (profileAvatar && data.avatar) profileAvatar.value = data.avatar;
    } else if (name === "profileSaved") {
        if (profileMessage) {
            if (data.error) {
                profileMessage.textContent = data.error;
                profileMessage.className = "message error";
            } else {
                profileMessage.textContent = "Profile saved.";
                profileMessage.className = "message success";
                setTimeout(() => { profileMessage.className = "hidden"; }, 3000);
            }
        }
    }
});

// ── Profile ────────────────────────────────────────────────────────

if (btnSaveProfile) {
    btnSaveProfile.addEventListener("click", () => {
        const name = profileName ? profileName.value.trim() : "";
        const description = profileDescription ? profileDescription.value.trim() : "";
        const avatar = profileAvatar ? profileAvatar.value.trim() : "";
        ladybird.sendMessage("saveProfile", { name, description, avatar });
    });
}

// ── Copy address button ────────────────────────────────────────────

if (btnCopyAddress) {
    btnCopyAddress.addEventListener("click", () => {
        if (currentAddress) {
            navigator.clipboard.writeText(currentAddress);
            btnCopyAddress.textContent = "Copied!";
            setTimeout(() => {
                btnCopyAddress.innerHTML = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect width="14" height="14" x="8" y="8" rx="2" ry="2"/><path d="M4 16c-1.1 0-2-.9-2-2V4c0-1.1.9-2 2-2h10c1.1 0 2 .9 2 2"/></svg> Copy Address';
            }, 2000);
        }
    });
}

// ── BitChat ────────────────────────────────────────────────────────

if (btnOpenBitchat) {
    btnOpenBitchat.addEventListener("click", () => {
        window.open("https://bitchat.dev", "_blank");
    });
}

// ── Inscription file picker ────────────────────────────────────────

if (btnInscribeFile) {
    btnInscribeFile.addEventListener("click", () => {
        const input = document.createElement("input");
        input.type = "file";
        input.addEventListener("change", () => {
            const file = input.files[0];
            if (!file) return;

            if (inscribeFileName) inscribeFileName.textContent = file.name;
            if (inscribeContentType) inscribeContentType.textContent = file.type || "application/octet-stream";
            if (inscribeTypeDisplay) inscribeTypeDisplay.classList.remove("hidden");

            // Show preview for images
            if (inscribePreview && inscribePreviewArea && file.type.startsWith("image/")) {
                inscribePreviewArea.innerHTML = "";
                const img = document.createElement("img");
                img.src = URL.createObjectURL(file);
                img.style.width = "100%";
                img.style.display = "block";
                inscribePreviewArea.appendChild(img);
                inscribePreview.classList.remove("hidden");
            } else if (inscribePreview) {
                inscribePreview.classList.add("hidden");
            }
        });
        input.click();
    });
}
