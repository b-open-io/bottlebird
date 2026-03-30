const searchInput = document.getElementById("search-input");

searchInput.addEventListener("keydown", event => {
    if (event.key !== "Enter") {
        return;
    }

    const query = searchInput.value.trim();
    if (query.length === 0) {
        return;
    }

    // Check if input looks like a URL: has a scheme or contains a dot
    const hasScheme = /^[a-zA-Z][a-zA-Z0-9+.-]*:\/\//.test(query);
    const looksLikeURL = hasScheme || query.includes(".");

    if (looksLikeURL) {
        // If no scheme, prepend https://
        window.location.href = hasScheme ? query : `https://${query}`;
    } else {
        // Search with DuckDuckGo
        window.location.href = `https://duckduckgo.com/?q=${encodeURIComponent(query)}`;
    }
});
