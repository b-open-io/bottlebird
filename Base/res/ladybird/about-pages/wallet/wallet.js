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

function formatSatoshis(sats) {
    return `${Number(sats).toLocaleString()} sat`;
}

function updateStatus(status) {
    walletEnabledToggle.checked = status.enabled;
    backendURL.value = status.backendURL || "";

    if (status.connected) {
        statusIndicator.className = "status-indicator connected";
        statusText.textContent = "Connected";
    } else {
        statusIndicator.className = "status-indicator disconnected";
        statusText.textContent = status.enabled ? "Not Connected" : "Disabled";
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
        receiveAddress.textContent = "No address available";
    }
}

function showSendMessage(text, isError) {
    sendMessage.textContent = text;
    sendMessage.className = `message ${isError ? "error" : "success"}`;
}

walletEnabledToggle.addEventListener("change", () => {
    ladybird.sendMessage("setWalletEnabled", walletEnabledToggle.checked);
});

backendURL.addEventListener("change", () => {
    if (backendURL.value && backendURL.checkValidity()) {
        ladybird.sendMessage("setWalletBackendURL", backendURL.value);
    }
});

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

document.addEventListener("WebUILoaded", () => {
    ladybird.sendMessage("loadWalletStatus");
    ladybird.sendMessage("getBalance");
    ladybird.sendMessage("getReceiveAddress");
});

document.addEventListener("WebUIMessage", event => {
    if (event.detail.name === "walletStatus") {
        updateStatus(event.detail.data);
    } else if (event.detail.name === "walletBalance") {
        updateBalance(event.detail.data);
    } else if (event.detail.name === "receiveAddress") {
        updateReceiveAddress(event.detail.data);
    } else if (event.detail.name === "paymentResult") {
        const result = event.detail.data;
        if (result.error) {
            showSendMessage(result.error, true);
        } else {
            showSendMessage(`Payment sent. TX: ${result.txid}`, false);
            sendRecipient.value = "";
            sendAmount.value = "";
        }
    }
});
