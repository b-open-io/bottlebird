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

// Identity
const identityCard = document.querySelector("#identity-card");
const bapIdDisplay = document.querySelector("#bap-id");
const identityPubkey = document.querySelector("#identity-pubkey");

// Dashboard add account
const btnAddAccount = document.querySelector("#btn-add-account");

let walletEnabled = false;
let previousView = "onboarding";
let pendingFileData = null;

function formatSatoshis(sats) {
    return `${Number(sats).toLocaleString()} sat`;
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

function refreshWallet() {
    ladybird.sendMessage("loadWalletStatus");
    ladybird.sendMessage("getReceiveAddress");
}

async function fetchBalanceFromBackend(address) {
    if (!address) return;
    const url = backendURL.value || "https://ordinals.gorillapool.io";
    try {
        const resp = await fetch(`${url}/api/bsv/address/${address}/balance`);
        if (resp.ok) {
            const data = await resp.json();
            updateBalance({ confirmed: data.confirmed || 0, unconfirmed: data.unconfirmed || 0 });
        } else {
            updateBalance({ confirmed: 0, unconfirmed: 0 });
        }
    } catch (e) {
        console.log("Balance fetch failed:", e);
        updateBalance({ confirmed: 0, unconfirmed: 0 });
    }
}

// ── Status updates ──────────────────────────────────────────────────

function updateStatus(status) {
    walletEnabled = status.enabled;

    if (!status.enabled) {
        showView("onboarding");
        return;
    }

    showView("wallet");
    walletEnabledToggle.checked = status.enabled;
    if (status.backendURL) backendURL.value = status.backendURL;

    if (status.connected) {
        statusIndicator.className = "status-indicator connected";
        statusText.textContent = "Connected";
    } else {
        statusIndicator.className = "status-indicator disconnected";
        statusText.textContent = "Not Connected";
    }

    if (status.bapId || status.identityPubkey) {
        identityCard.classList.remove("hidden");
        if (status.bapId) bapIdDisplay.textContent = status.bapId;
        if (status.identityPubkey) identityPubkey.textContent = status.identityPubkey;
    } else {
        identityCard.classList.add("hidden");
        bapIdDisplay.textContent = "";
        identityPubkey.textContent = "";
    }
}

function updateBalance(balance) {
    balanceConfirmed.textContent = formatSatoshis(balance.confirmed);
    balanceUnconfirmed.textContent = `${formatSatoshis(balance.unconfirmed)} unconfirmed`;
}

function updateReceiveAddress(data) {
    if (data.address) {
        receiveAddress.className = "address-display";
        receiveAddress.textContent = data.address;
        fetchBalanceFromBackend(data.address);
    } else {
        receiveAddress.className = "address-placeholder";
        receiveAddress.textContent = "Enable wallet to see your address";
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

bapIdDisplay.addEventListener("click", () => {
    if (bapIdDisplay.textContent) {
        navigator.clipboard.writeText(bapIdDisplay.textContent);
        const original = bapIdDisplay.textContent;
        bapIdDisplay.textContent = "Copied!";
        setTimeout(() => { bapIdDisplay.textContent = original; }, 1500);
    }
});

identityPubkey.addEventListener("click", () => {
    if (identityPubkey.textContent) {
        navigator.clipboard.writeText(identityPubkey.textContent);
        const original = identityPubkey.textContent;
        identityPubkey.textContent = "Copied!";
        setTimeout(() => { identityPubkey.textContent = original; }, 1500);
    }
});

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
        if (data.identityPubkey) {
            identityCard.classList.remove("hidden");
            identityPubkey.textContent = data.identityPubkey;
        }
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
