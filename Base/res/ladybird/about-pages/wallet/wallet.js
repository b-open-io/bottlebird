// Views
const onboardingView = document.querySelector("#onboarding-view");
const mnemonicShowView = document.querySelector("#mnemonic-show-view");
const importView = document.querySelector("#import-view");
const walletView = document.querySelector("#wallet-view");

// Controls
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

// Onboarding buttons
const btnCreateWallet = document.querySelector("#btn-create-wallet");
const btnImportWallet = document.querySelector("#btn-import-wallet");
const btnMnemonicBack = document.querySelector("#btn-mnemonic-back");
const btnMnemonicConfirm = document.querySelector("#btn-mnemonic-confirm");
const btnImportBack = document.querySelector("#btn-import-back");
const btnImportConfirm = document.querySelector("#btn-import-confirm");
const mnemonicDisplay = document.querySelector("#mnemonic-display");
const importMnemonic = document.querySelector("#import-mnemonic");
const importError = document.querySelector("#import-error");

function formatSatoshis(sats) {
    return `${Number(sats).toLocaleString()} sat`;
}

function showView(name) {
    onboardingView.classList.toggle("hidden", name !== "onboarding");
    mnemonicShowView.classList.toggle("hidden", name !== "mnemonic-show");
    importView.classList.toggle("hidden", name !== "import");
    walletView.classList.toggle("hidden", name !== "wallet");
}

function updateStatus(status) {
    if (!status.enabled) {
        showView("onboarding");
        return;
    }

    showView("wallet");
    walletEnabledToggle.checked = status.enabled;

    if (status.backendURL) {
        backendURL.value = status.backendURL;
    }

    if (status.connected) {
        statusIndicator.className = "status-indicator connected";
        statusText.textContent = "Connected";
    } else {
        statusIndicator.className = "status-indicator disconnected";
        statusText.textContent = "Not Connected";
    }

    // If we have a mnemonic display pending, show it
    if (status.mnemonic) {
        mnemonicDisplay.textContent = status.mnemonic;
        showView("mnemonic-show");
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
    } else {
        receiveAddress.className = "address-placeholder";
        receiveAddress.textContent = "Enable wallet to see your address";
    }
}

function showSendMessage(text, isError) {
    sendMessage.textContent = text;
    sendMessage.className = `message ${isError ? "error" : "success"}`;
}

// ── Onboarding: Create ──────────────────────────────────────────────

btnCreateWallet.addEventListener("click", () => {
    ladybird.sendMessage("createWallet");
});

btnMnemonicBack.addEventListener("click", () => {
    showView("onboarding");
});

btnMnemonicConfirm.addEventListener("click", () => {
    ladybird.sendMessage("confirmWalletCreation");
    ladybird.sendMessage("loadWalletStatus");
    ladybird.sendMessage("getBalance");
    ladybird.sendMessage("getReceiveAddress");
});

// ── Onboarding: Import ──────────────────────────────────────────────

btnImportWallet.addEventListener("click", () => {
    importMnemonic.value = "";
    importError.className = "hidden";
    showView("import");
});

btnImportBack.addEventListener("click", () => {
    showView("onboarding");
});

btnImportConfirm.addEventListener("click", () => {
    const mnemonic = importMnemonic.value.trim().toLowerCase();
    const words = mnemonic.split(/\s+/);

    if (words.length !== 12 && words.length !== 24) {
        importError.textContent = `Expected 12 or 24 words, got ${words.length}.`;
        importError.className = "message error";
        return;
    }

    importError.className = "hidden";
    ladybird.sendMessage("importWallet", mnemonic);
});

// ── Settings controls ───────────────────────────────────────────────

walletEnabledToggle.addEventListener("change", () => {
    ladybird.sendMessage("setWalletEnabled", walletEnabledToggle.checked);
});

backendURL.addEventListener("change", () => {
    if (backendURL.value && backendURL.checkValidity()) {
        ladybird.sendMessage("setWalletBackendURL", backendURL.value);
    }
});

// Copy address on click
receiveAddress.addEventListener("click", () => {
    if (receiveAddress.classList.contains("address-display")) {
        navigator.clipboard.writeText(receiveAddress.textContent);
        const original = receiveAddress.textContent;
        receiveAddress.textContent = "Copied!";
        setTimeout(() => { receiveAddress.textContent = original; }, 1500);
    }
});

// Send
sendButton.addEventListener("click", () => {
    const recipient = sendRecipient.value.trim();
    const amount = parseInt(sendAmount.value, 10);

    if (!recipient) {
        showSendMessage("Please enter a recipient address.", true);
        return;
    }

    if (!amount || amount <= 0) {
        showSendMessage("Please enter a valid amount.", true);
        return;
    }

    sendMessage.className = "hidden";
    ladybird.sendMessage("sendPayment", { recipient, amount });
});

// ── WebUI lifecycle ─────────────────────────────────────────────────

document.addEventListener("WebUILoaded", () => {
    ladybird.sendMessage("loadWalletStatus");
    ladybird.sendMessage("getBalance");
    ladybird.sendMessage("getReceiveAddress");
});

document.addEventListener("WebUIMessage", event => {
    const { name, data } = event.detail;

    if (name === "walletStatus") {
        updateStatus(data);
    } else if (name === "walletBalance") {
        updateBalance(data);
    } else if (name === "receiveAddress") {
        updateReceiveAddress(data);
    } else if (name === "walletCreated") {
        // Show the mnemonic for the user to write down
        if (data.mnemonic) {
            mnemonicDisplay.textContent = data.mnemonic;
            showView("mnemonic-show");
        }
    } else if (name === "walletImported") {
        if (data.error) {
            importError.textContent = data.error;
            importError.className = "message error";
        } else {
            // Import succeeded — refresh
            ladybird.sendMessage("loadWalletStatus");
            ladybird.sendMessage("getBalance");
            ladybird.sendMessage("getReceiveAddress");
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
