const API = window.GRAPHENEDB_EVIDENCE_LAB_API.replace(/\/$/, "");
const state = { sessionId: null, samples: [], selection: null, run: null };

const $ = (id) => document.getElementById(id);
const escapeHtml = (value) => String(value ?? "").replace(/[&<>'"]/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;","'":"&#39;",'"':"&quot;"}[c]));

async function api(path, options = {}) {
  const headers = new Headers(options.headers || {});
  if (state.sessionId) headers.set("X-Session-ID", state.sessionId);
  const response = await fetch(`${API}${path}`, { ...options, headers });
  if (!response.ok) {
    let detail = `${response.status} ${response.statusText}`;
    try { const body = await response.json(); detail = body.detail || body.error || detail; } catch (_) {}
    throw new Error(detail);
  }
  return response;
}

async function boot() {
  bindTabs();
  bindActions();
  try {
    const health = await (await api("/v1/public/health")).json();
    const badge = $("backendBadge");
    badge.textContent = health.live_backend_configured ? "LIVE BACKEND CONFIGURED" : "RECORDED PREVIEW MODE";
    badge.className = `badge ${health.live_backend_configured ? "live" : "recorded"}`;
    const session = await (await api("/v1/public/sessions", { method: "POST" })).json();
    state.sessionId = session.session_id;
    await loadSamples();
  } catch (error) {
    $("backendBadge").textContent = `BACKEND ERROR: ${error.message}`;
    $("backendBadge").className = "badge recorded";
  }
}

function bindTabs() {
  document.querySelectorAll(".tab").forEach(button => button.addEventListener("click", () => {
    document.querySelectorAll(".tab").forEach(item => { item.classList.remove("active"); item.setAttribute("aria-selected", "false"); });
    document.querySelectorAll(".tab-panel").forEach(item => item.classList.remove("active"));
    button.classList.add("active");
    button.setAttribute("aria-selected", "true");
    $(`${button.dataset.tab}Panel`).classList.add("active");
  }));
}

function bindActions() {
  $("uploadButton").addEventListener("click", uploadFiles);
  $("runButton").addEventListener("click", runSelected);
  $("downloadBundle").addEventListener("click", downloadBundle);
  $("deleteSession").addEventListener("click", deleteSession);
}

async function loadSamples() {
  const data = await (await api("/v1/public/samples")).json();
  state.samples = data.samples.filter(sample => !sample.invalid);
  const grid = $("sampleGrid");
  grid.innerHTML = state.samples.map(sample => `
    <article class="sample-card">
      <div>
        <p class="eyebrow">VERIFIED SAMPLE</p>
        <h3>${escapeHtml(sample.title)}</h3>
        <p>${escapeHtml(sample.description)}</p>
      </div>
      <div class="sample-meta">
        <span>${sample.node_count} nodes</span><span>${sample.edge_count} edges</span><span>${sample.evidence_family_count} families</span>
      </div>
      <button class="button primary" data-sample="${escapeHtml(sample.sample_id)}" type="button">Inspect sample</button>
    </article>`).join("");
  grid.querySelectorAll("[data-sample]").forEach(button => button.addEventListener("click", () => inspectSample(button.dataset.sample)));
}

async function inspectSample(sampleId) {
  const data = await (await api(`/v1/public/samples/${encodeURIComponent(sampleId)}`)).json();
  state.selection = { type: "sample", id: sampleId, dataset: data.dataset, summary: data };
  renderDataset(data.title, data.dataset_hash, data.dataset, data.warnings || []);
}

async function uploadFiles() {
  const files = Array.from($("fileInput").files || []);
  if (!files.length) return setStatus("uploadStatus", "Select one or more files first.", true);
  const form = new FormData();
  files.forEach(file => form.append("files", file));
  setStatus("uploadStatus", "Uploading and validating…");
  try {
    const validation = await (await api("/v1/public/uploads", { method: "POST", body: form })).json();
    const data = await (await api(`/v1/public/datasets/${encodeURIComponent(validation.dataset_id)}`)).json();
    state.selection = { type: "upload", id: validation.dataset_id, dataset: data.dataset, summary: validation };
    renderDataset(data.dataset.manifest.title, validation.dataset_hash, data.dataset, validation.warnings || []);
    setStatus("uploadStatus", JSON.stringify(validation, null, 2));
  } catch (error) {
    setStatus("uploadStatus", `Upload rejected: ${error.message}`, true);
  }
}

function renderDataset(title, hash, dataset, warnings) {
  $("datasetInspector").classList.remove("hidden");
  $("datasetTitle").textContent = title;
  $("datasetHash").textContent = hash;
  $("datasetMetrics").innerHTML = [
    ["Nodes", dataset.nodes.length], ["Edges", dataset.edges.length], ["Queries", dataset.queries.length],
    ["Evidence families", new Set(dataset.edges.map(edge => edge.evidence_family_id)).size],
    ["Derivations", new Set(dataset.edges.map(edge => edge.derivation_id)).size]
  ].map(([name, value]) => metricCard(name, value)).join("");
  $("datasetWarnings").innerHTML = warnings.length
    ? `<div class="warning">${warnings.map(escapeHtml).join("<br>")}</div>`
    : `<div class="badge live">SCHEMA VALID</div>`;
  $("querySelect").innerHTML = dataset.queries.map(query => `<option value="${escapeHtml(query.query_id)}">${escapeHtml(query.question)}</option>`).join("");
  $("datasetInspector").scrollIntoView({ behavior: "smooth", block: "center" });
}

async function runSelected() {
  if (!state.selection) return;
  const body = {
    query_id: $("querySelect").value,
    policy: $("policySelect").value,
    ...(state.selection.type === "sample" ? { sample_id: state.selection.id } : { dataset_id: state.selection.id })
  };
  $("runButton").disabled = true;
  $("runButton").textContent = "Running…";
  try {
    const run = await (await api("/v1/public/runs", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body)
    })).json();
    state.run = run;
    renderRun(run);
  } catch (error) {
    alert(`Run failed: ${error.message}`);
  } finally {
    $("runButton").disabled = false;
    $("runButton").textContent = "Run GrapheneDB";
  }
}

function renderRun(run) {
  $("result").classList.remove("hidden");
  $("runHeadline").textContent = `${run.status.replaceAll("_", " ")} — ${run.policy.replaceAll("_", " ")}`;
  const badge = $("runModeBadge");
  badge.textContent = run.live ? "LIVE GRAPHENEDB EXECUTION" : "RECORDED REFERENCE — BACKEND NOT USED";
  badge.className = `badge ${run.live ? "live" : "recorded"}`;
  $("runId").textContent = `Run ${run.run_id}`;
  $("engineVersion").textContent = `${run.graphenedb_version} @ ${run.graphenedb_commit}`;
  $("runDatasetHash").textContent = `Dataset ${run.dataset_hash}`;
  $("runMetrics").innerHTML = [
    ["Status", run.status], ["Confidence", run.confidence ?? "—"],
    ...run.metrics.map(metric => [metric.name.replaceAll("_", " "), metric.value ?? "—"])
  ].map(([name, value]) => metricCard(name, value)).join("");
  $("receipt").textContent = JSON.stringify(run.receipt, null, 2);
  $("events").innerHTML = run.execution_events.map(event => `<li><code>${escapeHtml(event.stage)}</code> — ${escapeHtml(event.detail)}</li>`).join("");
  renderGraph(run.graph);
  $("result").scrollIntoView({ behavior: "smooth", block: "start" });
}

function renderGraph(graph) {
  const svg = $("graph");
  svg.innerHTML = "";
  const height = 500;
  const nodes = graph.nodes || [];
  const roots = nodes.filter(n => n.role === "root");
  const symptoms = nodes.filter(n => ["symptom", "impact"].includes(n.role));
  const middle = nodes.filter(n => !roots.includes(n) && !symptoms.includes(n));
  const place = (list, x) => list.forEach((node, index) => { node.x = x; node.y = ((index + 1) * height) / (list.length + 1); });
  place(roots, 140); place(middle, 480); place(symptoms, 820);
  const lookup = new Map(nodes.map(node => [node.id, node]));
  const ns = "http://www.w3.org/2000/svg";
  (graph.edges || []).forEach(edge => {
    const from = lookup.get(edge.from), to = lookup.get(edge.to); if (!from || !to) return;
    const line = document.createElementNS(ns, "line");
    line.setAttribute("x1", from.x); line.setAttribute("y1", from.y); line.setAttribute("x2", to.x); line.setAttribute("y2", to.y);
    line.setAttribute("class", "graph-edge");
    line.setAttribute("stroke", edge.role === "contradicts" ? "#ef7d7d" : edge.role === "predictive" ? "#6f7a75" : "#8fd3a9");
    if (edge.critical) line.setAttribute("stroke-width", "4");
    line.addEventListener("click", () => $("graphInspector").textContent = JSON.stringify(edge, null, 2));
    svg.appendChild(line);
  });
  nodes.forEach(node => {
    const group = document.createElementNS(ns, "g"); group.setAttribute("class", "graph-node");
    const circle = document.createElementNS(ns, "circle");
    circle.setAttribute("cx", node.x); circle.setAttribute("cy", node.y); circle.setAttribute("r", "22");
    circle.setAttribute("fill", node.role === "root" ? "#183d2a" : node.role === "symptom" ? "#5b2b2b" : "#28332e");
    circle.setAttribute("stroke", "#8fd3a9");
    const text = document.createElementNS(ns, "text");
    text.setAttribute("x", node.x + 30); text.setAttribute("y", node.y + 4); text.setAttribute("class", "graph-label");
    text.textContent = node.label.length > 46 ? `${node.label.slice(0, 43)}…` : node.label;
    group.append(circle, text);
    group.addEventListener("click", () => $("graphInspector").textContent = JSON.stringify(node, null, 2));
    svg.appendChild(group);
  });
}

async function downloadBundle() {
  if (!state.run) return;
  const response = await api(`/v1/public/runs/${encodeURIComponent(state.run.run_id)}/bundle`);
  const blob = await response.blob();
  const link = document.createElement("a");
  link.href = URL.createObjectURL(blob); link.download = `graphenedb-${state.run.run_id}.zip`; link.click();
  URL.revokeObjectURL(link.href);
}

async function deleteSession() {
  if (!state.sessionId) return;
  if (!confirm("Delete all uploaded datasets and run artifacts in this anonymous session?")) return;
  try { await api(`/v1/public/sessions/${encodeURIComponent(state.sessionId)}`, { method: "DELETE" }); } catch (_) {}
  state.sessionId = null; state.selection = null; state.run = null;
  $("datasetInspector").classList.add("hidden"); $("result").classList.add("hidden");
  const next = await (await api("/v1/public/sessions", { method: "POST" })).json();
  state.sessionId = next.session_id;
  alert("Session data deleted. A new anonymous session has been created.");
}

function metricCard(name, value) { return `<div class="metric"><span>${escapeHtml(name)}</span><strong>${escapeHtml(value)}</strong></div>`; }
function setStatus(id, text, error = false) { const el = $(id); el.textContent = text; el.style.borderColor = error ? "#ef7d7d" : "#28332e"; }

document.addEventListener("DOMContentLoaded", boot);
