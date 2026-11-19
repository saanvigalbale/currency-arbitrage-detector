
const form = document.getElementById("analysisForm");
const fileInput = document.getElementById("csvFile");

fileInput.addEventListener("change", () => {
  document.getElementById("fileName").textContent =
    fileInput.files[0]?.name || "No file selected";
});

form.addEventListener("submit", async (event) => {
  event.preventDefault();

  const file = fileInput.files[0];
  const capital = document.getElementById("capital").value;

  if (!file || !file.name.toLowerCase().endsWith(".csv")) {
    showError("Please select a valid CSV file.");
    return;
  }

  if (!capital || Number(capital) <= 0 || !Number.isFinite(Number(capital))) {
    showError("Starting capital must be greater than zero.");
    return;
  }

  const button = document.getElementById("analyzeBtn");
  button.disabled = true;
  document.getElementById("error").hidden = true;
  document.getElementById("results").hidden = true;
  document.getElementById("loading").hidden = false;

  try {
    const data = new FormData();
    data.append("file", file);
    data.append("capital", capital);

    const response = await fetch("/api/analyze", {
      method: "POST",
      body: data
    });

    const result = await response.json();

    if (result.status === "error") {
      throw new Error(
        result.error?.line
          ? `CSV error on line ${result.error.line}: ${result.error.message}`
          : result.error?.message || "Analysis failed."
      );
    }

    renderResult(result);
  } catch (error) {
    showError(error.message || "Unable to connect to the server.");
  } finally {
    button.disabled = false;
    document.getElementById("loading").hidden = true;
  }
});

function showError(message) {
  const el = document.getElementById("error");
  el.textContent = message;
  el.hidden = false;
}

function money(value, currency) {
  return new Intl.NumberFormat("en-US", {
    style: "currency",
    currency: currency === "USD" ? "USD" : "USD",
    maximumFractionDigits: 2
  }).format(value);
}

function renderResult(result) {
  document.getElementById("results").hidden = false;

  const arbitrage = result.status === "arbitrage";
  const title = document.getElementById("resultTitle");
  const badge = document.getElementById("resultBadge");

  title.textContent = arbitrage ? "Arbitrage detected" : "No arbitrage found";
  badge.textContent = arbitrage ? "PROFITABLE CYCLE" : "NO PROFITABLE CYCLE";
  badge.className = arbitrage ? "badge" : "badge error-badge";

  document.getElementById("cycleSection").hidden = !arbitrage;
  document.getElementById("metrics").hidden = !arbitrage;
  document.getElementById("noArbitrage").hidden = arbitrage;

  if (!arbitrage) return;

  const a = result.analysis;
  const currency = a.currency || "USD";

  document.getElementById("cycle").textContent = a.cycle.join(" → ");

  document.getElementById("steps").innerHTML = a.steps.map(step => `
    <div class="step">
      <strong>${escapeHTML(step.from)} → ${escapeHTML(step.to)}</strong>
      <span>Rate: ${Number(step.rate).toPrecision(6)}</span>
      <span>${money(step.amount_before, currency)} → ${money(step.amount_after, currency)}</span>
    </div>
  `).join("");

  document.getElementById("profit").textContent = money(a.profit, currency);
  document.getElementById("profitPercent").textContent =
    `${Number(a.profit_percent).toFixed(4)}%`;
  document.getElementById("finalCapital").textContent =
    money(a.final_capital, currency);
  document.getElementById("multiplier").textContent =
    Number(a.multiplier).toFixed(8);
}

function escapeHTML(value) {
  return String(value).replace(/[&<>"']/g, char => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;",
    '"': "&quot;", "'": "&#39;"
  })[char]);
}
