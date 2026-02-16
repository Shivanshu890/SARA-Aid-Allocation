/* ---------------- API helpers ---------------- */
const API = {
  async get(url) {
    const r = await fetch(url);
    return r.json();
  },
  async post(url, body = null) {
    const r = await fetch(url, {
      method: "POST",
      headers: body ? { "Content-Type": "application/json" } : undefined,
      body: body ? JSON.stringify(body) : null
    });
    return r.json();
  }
};

/* ---------------- Tabs ---------------- */
document.querySelectorAll(".tab").forEach((btn) => {
  btn.onclick = () => {
    document.querySelectorAll(".tab").forEach((b) => b.classList.remove("active"));
    document.querySelectorAll(".tabpane").forEach((p) => p.classList.remove("active"));
    btn.classList.add("active");
    document.getElementById("tab-" + btn.dataset.tab).classList.add("active");

    if (btn.dataset.tab === "overview" && window.allocations) {
      drawNetwork(window.allocations);
    }
    if (btn.dataset.tab === "routes") drawRoutesPreview();
  };
});

/* ---------------- State ---------------- */
let allocations = [];
let summary = null;
let chart = null;

/* ---------------- Overview: KPIs + Chart ---------------- */
function setKpis(s) {
  document.getElementById("kpi-req").textContent = s?.totalRequested ?? 0;
  document.getElementById("kpi-alloc").textContent = s?.totalAllocated ?? 0;
  document.getElementById("kpi-cov").textContent = (s?.coveragePct ?? 0) + "%";
}

function buildBarChart(s) {
  const ctx = document.getElementById("barChart");
  if (chart) chart.destroy();
  chart = new Chart(ctx, {
    type: "bar",
    data: {
      labels: Object.keys(s.byResourceRequested || {}),
      datasets: [
        { label: "Requested", data: Object.values(s.byResourceRequested || {}) },
        { label: "Allocated", data: Object.values(s.byResourceAllocated || {}) }
      ]
    },
    options: {
      responsive: true,
      plugins: { legend: { position: "bottom" } },
      scales: { y: { beginAtZero: true } }
    }
  });
}

/* =========================================================
   🔷 NEW VISUALIZATION ENGINE (REPLACES OLD ONE)
   ========================================================= */

function drawNetwork(rows) {
  const wrap = document.querySelector("#tab-overview .map-wrap").getBoundingClientRect();
  const canvas = document.getElementById("routeCanvas");
  const dpr = window.devicePixelRatio || 1;

  canvas.width = wrap.width * dpr;
  canvas.height = wrap.height * dpr;
  canvas.style.width = wrap.width + "px";
  canvas.style.height = wrap.height + "px";

  const ctx = canvas.getContext("2d");
  ctx.setTransform(1, 0, 0, 1, 0, 0);
  ctx.scale(dpr, dpr);
  ctx.clearRect(0, 0, wrap.width, wrap.height);

  /* ---------- Extract nodes ---------- */
  const warehouses = new Set();
  const reliefs = new Set();
  const cities = new Set();

  rows.forEach(a => {
    if (a.sourceCity) warehouses.add(a.sourceCity);
    if (a.destCity) reliefs.add(a.destCity);
    (a.path || []).forEach(c => cities.add(c));
  });

  /* ---------- Layout ---------- */
  const pos = {};
  const leftX = wrap.width * 0.15;
  const rightX = wrap.width * 0.85;
  const centerX = wrap.width * 0.5;

  [...warehouses].forEach((c, i) => {
    pos[c] = [leftX, 120 + i * 80];
  });

  [...reliefs].forEach((c, i) => {
    pos[c] = [rightX, 120 + i * 80];
  });

  let mid = 0;
  [...cities].forEach(c => {
    if (!pos[c]) {
      pos[c] = [centerX, 100 + mid * 70];
      mid++;
    }
  });

  /* ---------- Helpers ---------- */
  function drawLabel(x, y, t) {
    ctx.fillStyle = "#0f172a";
    ctx.font = "12px system-ui";
    ctx.fillText(t, x + 10, y - 6);
  }

  function drawWarehouse(x, y, t) {
    ctx.fillStyle = "#2563eb";
    ctx.fillRect(x - 7, y - 7, 14, 14);
    drawLabel(x, y, t);
  }

  function drawRelief(x, y, t) {
    ctx.fillStyle = "#f97316";
    ctx.beginPath();
    ctx.moveTo(x, y - 8);
    ctx.lineTo(x - 7, y + 6);
    ctx.lineTo(x + 7, y + 6);
    ctx.closePath();
    ctx.fill();
    drawLabel(x, y, t);
  }

  function drawCity(x, y, t) {
    ctx.fillStyle = "#475569";
    ctx.beginPath();
    ctx.arc(x, y, 4, 0, Math.PI * 2);
    ctx.fill();
    drawLabel(x, y, t);
  }

  function drawArrow(a, b) {
    const ang = Math.atan2(b[1] - a[1], b[0] - a[0]);
    const l = 8;
    ctx.beginPath();
    ctx.moveTo(b[0], b[1]);
    ctx.lineTo(b[0] - l * Math.cos(ang - Math.PI / 6), b[1] - l * Math.sin(ang - Math.PI / 6));
    ctx.lineTo(b[0] - l * Math.cos(ang + Math.PI / 6), b[1] - l * Math.sin(ang + Math.PI / 6));
    ctx.closePath();
    ctx.fill();
  }

  /* ---------- Draw routes ---------- */
  rows.forEach(a => {
    const p = a.path || [];
    if (p.length < 2) return;

    ctx.strokeStyle =
      a.status === "Met" ? "#22c55e" :
      a.status === "Partial" ? "#f59e0b" : "#ef4444";

    ctx.lineWidth = Math.max(2, (a.allocated || 0) / 50);
    ctx.beginPath();
    ctx.moveTo(...pos[p[0]]);
    for (let i = 1; i < p.length; i++) ctx.lineTo(...pos[p[i]]);
    ctx.stroke();

    ctx.fillStyle = ctx.strokeStyle;
    drawArrow(pos[p[p.length - 2]], pos[p[p.length - 1]]);

    const mid = pos[p[Math.floor(p.length / 2)]];
    if (a.distanceKm)
      ctx.fillText(`${a.distanceKm.toFixed(0)} km`, mid[0] + 6, mid[1] - 6);
  });

  /* ---------- Draw nodes ---------- */
  cities.forEach(c => {
    if (warehouses.has(c)) drawWarehouse(...pos[c], c);
    else if (reliefs.has(c)) drawRelief(...pos[c], c);
    else drawCity(...pos[c], c);
  });
}

/* ---------------- Run Allocation ---------------- */
document.getElementById("runBtn").onclick = async () => {
  document.getElementById("msg").textContent = "Running allocation...";
  const data = await API.post("http://127.0.0.1:5000/admin/allocate?pretty=1");
  if (data.error) return alert(data.error);

  allocations = data.allocations || [];
  summary = data.summary || {};

  setKpis(summary);
  buildBarChart(summary);
  drawNetwork(allocations);

  document.getElementById("msg").textContent = "Done.";
};

/* ---------------- Init ---------------- */
(async function init() {
  const data = await API.get("http://127.0.0.1:5000/public/allocations");
  allocations = data.allocations || [];
  summary = data.summary || {};

  setKpis(summary);
  if (summary) buildBarChart(summary);
  drawNetwork(allocations);

  window.addEventListener("resize", () => {
    if (document.getElementById("tab-overview").classList.contains("active")) {
      drawNetwork(allocations);
    }
  });
})();
