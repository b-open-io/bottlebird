// Views
const onboardingView = document.querySelector("#onboarding-view");
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
const btnCreateWallet = document.querySelector("#btn-create-wallet");
const btnImportWallet = document.querySelector("#btn-import-wallet");

let walletConfigured = false;

function formatSatoshis(sats) {
    return `${Number(sats).toLocaleString()} sat`;
}

function showView(view) {
    onboardingView.classList.toggle("hidden", view !== "onboarding");
    walletView.classList.toggle("hidden", view !== "wallet");
}

function updateStatus(status) {
    walletConfigured = status.enabled;

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

// Onboarding actions
btnCreateWallet.addEventListener("click", () => {
    ladybird.sendMessage("setWalletEnabled", true);
    ladybird.sendMessage("loadWalletStatus");
    ladybird.sendMessage("getBalance");
    ladybird.sendMessage("getReceiveAddress");
});

btnImportWallet.addEventListener("click", () => {
    // For now, same as create — will add backup import later
    ladybird.sendMessage("setWalletEnabled", true);
    ladybird.sendMessage("loadWalletStatus");
    ladybird.sendMessage("getBalance");
    ladybird.sendMessage("getReceiveAddress");
});

// Settings controls
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

// WebUI lifecycle
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
