(function () {
  "use strict";

  const WORLD = { width: 1000, height: 620, groundY: 520 };
  const TASKS = {
    run:    { id: "run",    name: "RUN",    duration: 7.0, par: 620, startX: 230, goalX: 860, terrain: [{ x0: -10000, x1: 30000, y: 520 }] },
    jump:   { id: "jump",   name: "JUMP",   duration: 5.5, par: 170, startX: 250, goalX: null, terrain: [{ x0: -10000, x1: 30000, y: 520 }], heightMarks: [470, 410, 350, 290] },
    climb:  { id: "climb",  name: "CLIMB",  duration: 9.0, par: 680, startX: 230, goalX: 860, terrain: [{ x0: -10000, x1: 420, y: 520 }, { x0: 420, x1: 550, y: 472 }, { x0: 550, x1: 680, y: 424 }, { x0: 680, x1: 810, y: 376 }, { x0: 810, x1: 30000, y: 328 }] },
    custom: { id: "custom", name: "CUSTOM", duration: 8.0, par: 500, startX: 230, goalX: 750, goalY: 480, terrain: [{ x0: -10000, x1: 30000, y: 520 }] }
  };

  const PRESETS = {
    runner: () => ({
      name: "RUNNER",
      joints: [
        { x: 235, y: 382, r: 9.5, mass: 1.55 }, { x: 330, y: 382, r: 9.5, mass: 1.55 },
        { x: 390, y: 360, r: 8.5, mass: 0.80 }, { x: 350, y: 450, r: 7.5, mass: 0.80 },
        { x: 355, y: 512, r: 8.0, mass: 0.70 }, { x: 215, y: 450, r: 7.5, mass: 0.80 },
        { x: 205, y: 512, r: 8.0, mass: 0.70 }, { x: 180, y: 375, r: 7.0, mass: 0.55 }
      ],
      bones: [{ a: 0, b: 1 }, { a: 1, b: 2 }, { a: 0, b: 7 }, { a: 1, b: 3 }, { a: 3, b: 4 }, { a: 0, b: 5 }, { a: 5, b: 6 }],
      muscles: [{ a: 0, b: 3, strength: 1.0 }, { a: 3, b: 4, strength: 1.0 }, { a: 0, b: 5, strength: 1.0 }, { a: 5, b: 6, strength: 1.0 }]
    }),
    hopper: () => ({
      name: "HOPPER",
      joints: [
        { x: 250, y: 338, r: 10, mass: 1.5 }, { x: 326, y: 338, r: 9, mass: 1.1 },
        { x: 370, y: 298, r: 8, mass: 0.7 },  { x: 326, y: 420, r: 7.5, mass: 0.8 },
        { x: 326, y: 512, r: 8, mass: 0.7 },  { x: 250, y: 420, r: 7.5, mass: 0.8 },
        { x: 250, y: 512, r: 8, mass: 0.7 }
      ],
      bones: [{ a: 0, b: 1 }, { a: 1, b: 2 }, { a: 1, b: 3 }, { a: 3, b: 4 }, { a: 0, b: 5 }, { a: 5, b: 6 }],
      muscles: [{ a: 0, b: 2, strength: 1.0 }, { a: 2, b: 3, strength: 1.0 }, { a: 0, b: 4, strength: 1.0 }, { a: 4, b: 5, strength: 1.0 }]
    })
  };

  const $ = id => document.getElementById(id);
  const $$ = sel => Array.from(document.querySelectorAll(sel));
  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  const dist = (ax, ay, bx, by) => Math.hypot(bx - ax, by - ay);
  const pad2 = n => (n < 10 ? "0" : "") + n;

  const state = {
    screen: "build", task: "run", tool: "select", preset: "runner",
    body: PRESETS.runner(), undo: [], selection: null, pendingJoint: null, pendingBone: null,
    drag: null, pointer: { x: 0, y: 0, inside: false },
    duration: 7.0, ghosts: 24, mutPct: 18, speed: 1, paused: false, warping: false,
    focusIndex: 0, camX: 300, viewZoom: 1, viewPanX: 0, viewPanY: 0, isPanning: false, panMoved: false,
    snapshot: null, busy: false, stepAcc: 0, buildView: null
  };

  let toastTimer = 0;
  function showToast(msg) {
    $("toast").textContent = msg;
    $("toast").classList.add("is-visible");
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => $("toast").classList.remove("is-visible"), 2600);
  }

  function surfaceY(task, x) {
    let y = WORLD.groundY, found = false;
    for (const s of task.terrain) if (x >= s.x0 && x <= s.x1 && (!found || s.y < y)) { y = s.y; found = true; }
    return y;
  }

  function musclePts(body, m) {
    const bA = body.bones[m.a], bB = body.bones[m.b];
    if (!bA || !bB) return null;
    const a1 = body.joints[bA.a], a2 = body.joints[bA.b], b1 = body.joints[bB.a], b2 = body.joints[bB.b];
    if (!a1 || !a2 || !b1 || !b2) return null;
    return { ax: (a1.x + a2.x) * 0.5, ay: (a1.y + a2.y) * 0.5, bx: (b1.x + b2.x) * 0.5, by: (b1.y + b2.y) * 0.5 };
  }

  async function apiPost(url, text = "") {
    try {
      const res = await fetch(url, { method: "POST", body: text });
      if (!res.ok) throw new Error("HTTP " + res.status);
      return await res.json();
    } catch (err) {
      showToast("C++ engine not reachable! Run build.bat / ./evowalk and open http://localhost:8080");
      return null;
    }
  }

  function encodeStartPayload() {
    const t = TASKS[state.task], b = state.body;
    const lines = [
      `${state.task} ${state.duration} ${state.ghosts} ${(state.mutPct / 100).toFixed(3)} ${t.goalX ?? 750} ${t.goalY ?? 480}`,
      `${b.joints.length}`,
      ...b.joints.map(j => `${j.x.toFixed(1)} ${j.y.toFixed(1)} ${j.r} ${j.mass}`),
      `${b.bones.length}`,
      ...b.bones.map(bo => `${bo.a} ${bo.b}`),
      `${b.muscles.length}`,
      ...b.muscles.map(m => `${m.a} ${m.b} ${m.strength}`)
    ];
    return lines.join("\n");
  }

  function syncBuildUi() {
    const b = state.body, t = TASKS[state.task];
    $("creatureName").textContent = $("bodyLabel").textContent = $("recordBody").textContent = b.name;
    $("hudJoints").textContent = $("jointCount").textContent = b.joints.length;
    $("hudBones").textContent = $("boneCount").textContent = b.bones.length;
    $("hudMuscles").textContent = $("muscleCount").textContent = b.muscles.length;
    const mass = b.joints.reduce((s, j) => s + j.mass, 0).toFixed(1);
    $("massCount").textContent = mass; $("bodyMass").textContent = "MASS " + mass;
    $("undoBtn").disabled = state.undo.length === 0;
    $("taskPar").textContent = "PAR " + t.par; $("chartPar").textContent = t.par;
    $("evolveMeta").textContent = `${state.ghosts} GHOSTS · ${state.duration.toFixed(1)} S / GEN`;

    const ok = b.joints.length >= 3 && b.bones.length >= 1 && b.muscles.length >= 1;
    $("evolveBtn").disabled = !ok;
    $("evolveReady").textContent = ok ? "READY" : "INCOMPLETE";
    $("evolveReady").style.color = ok ? "var(--teal)" : "var(--red)";

    $$(".task-tab, .task-card").forEach(el => el.classList.toggle("is-active", el.dataset.task === state.task));
    $$(".preset-btn[data-preset]").forEach(el => el.classList.toggle("is-active", el.dataset.preset === state.preset));
    $$(".tool[data-tool]").forEach(el => el.classList.toggle("is-active", el.dataset.tool === state.tool));
    renderSelection();
  }

  function renderSelection() {
    const sel = state.selection, b = state.body, panel = $("selectionPanel");
    if (!sel) {
      $("selectionHint").textContent = "CLICK A PART";
      panel.innerHTML = '<p class="selection-empty">Drag a joint to reshape, or click a part to tune it.</p>';
      return;
    }
    if (sel.type === "joint" && b.joints[sel.index]) {
      const j = b.joints[sel.index];
      $("selectionHint").textContent = "JOINT " + pad2(sel.index + 1);
      panel.innerHTML = `<div class="sel-head"><span class="sel-title">JOINT #${pad2(sel.index + 1)}</span><button class="sel-delete" data-del>DELETE</button></div>`
        + `<label class="slider-row"><span>MASS</span><input type="range" data-prop="mass" min="0.4" max="2" step="0.05" value="${j.mass}"><output>${j.mass.toFixed(2)}</output></label>`
        + `<label class="slider-row"><span>RADIUS</span><input type="range" data-prop="r" min="5" max="14" step="0.5" value="${j.r}"><output>${j.r.toFixed(1)}</output></label>`;
    } else if (sel.type === "bone" && b.bones[sel.index]) {
      const bo = b.bones[sel.index], ja = b.joints[bo.a], jb = b.joints[bo.b];
      $("selectionHint").textContent = "BONE " + pad2(sel.index + 1);
      panel.innerHTML = `<div class="sel-head"><span class="sel-title">BONE #${pad2(sel.index + 1)}</span><button class="sel-delete" data-del>DELETE</button></div>`
        + `<div class="sel-meta"><span>LENGTH</span><span><b>${dist(ja.x, ja.y, jb.x, jb.y).toFixed(1)} u</b></span></div>`;
    } else if (sel.type === "muscle" && b.muscles[sel.index]) {
      const m = b.muscles[sel.index], ep = musclePts(b, m);
      $("selectionHint").textContent = "MUSCLE " + pad2(sel.index + 1);
      panel.innerHTML = `<div class="sel-head"><span class="sel-title">MUSCLE #${pad2(sel.index + 1)}</span><button class="sel-delete" data-del>DELETE</button></div>`
        + `<label class="slider-row"><span>STRENGTH</span><input type="range" data-prop="strength" min="0.4" max="1.8" step="0.05" value="${m.strength}"><output>${m.strength.toFixed(2)}</output></label>`
        + `<div class="sel-meta"><span>REST LENGTH</span><span><b>${ep ? dist(ep.ax, ep.ay, ep.bx, ep.by).toFixed(1) : "—"} u</b></span></div>`;
    }
  }

  function pushUndo() { state.undo.push(JSON.stringify(state.body)); if (state.undo.length > 40) state.undo.shift(); }
  function markCustom() { state.preset = "custom"; state.body.name = "CUSTOM BODY"; syncBuildUi(); }

  function deletePart(sel) {
    if (!sel) return;
    const b = state.body;
    pushUndo();
    if (sel.type === "joint" && b.joints.length > 2) {
      const idx = sel.index, map = {}; let nxt = 0;
      b.joints.forEach((_, i) => { if (i !== idx) map[i] = nxt++; });
      b.joints.splice(idx, 1);
      const bMap = {}; let nb = 0;
      b.bones = b.bones.filter((bo, bi) => { if (bo.a !== idx && bo.b !== idx) { bMap[bi] = nb++; return true; } return false; })
                       .map(bo => ({ a: map[bo.a], b: map[bo.b] }));
      b.muscles = b.muscles.filter(m => bMap[m.a] !== undefined && bMap[m.b] !== undefined)
                           .map(m => ({ a: bMap[m.a], b: bMap[m.b], strength: m.strength }));
    } else if (sel.type === "bone") {
      const idx = sel.index, map = {}; let nxt = 0;
      b.bones.forEach((_, i) => { if (i !== idx) map[i] = nxt++; });
      b.bones.splice(idx, 1);
      b.muscles = b.muscles.filter(m => m.a !== idx && m.b !== idx).map(m => ({ a: map[m.a], b: map[m.b], strength: m.strength }));
    } else if (sel.type === "muscle") {
      b.muscles.splice(sel.index, 1);
    }
    state.selection = state.pendingJoint = state.pendingBone = null;
    markCustom();
  }

  function ptSegDist(px, py, ax, ay, bx, by) {
    const dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
    if (l2 < 1e-4) return dist(px, py, ax, ay);
    const t = clamp(((px - ax) * dx + (py - ay) * dy) / l2, 0, 1);
    return dist(px, py, ax + dx * t, ay + dy * t);
  }

  function hitTest(x, y) {
    if (state.task === "custom") {
      const gx = TASKS.custom.goalX ?? 750, gy = TASKS.custom.goalY ?? 480;
      if (Math.hypot(x - gx, y - gy) < 32 || (Math.abs(x - gx) < 20 && y >= gy - 95 && y <= 525)) {
        return { type: "flag" };
      }
    }
    for (let i = 0; i < state.body.joints.length; i++) {
      const j = state.body.joints[i];
      if (dist(x, y, j.x, j.y) <= Math.max(11, j.r + 6)) return { type: "joint", index: i };
    }
    for (let i = 0; i < state.body.muscles.length; i++) {
      const ep = musclePts(state.body, state.body.muscles[i]);
      if (ep && ptSegDist(x, y, ep.ax, ep.ay, ep.bx, ep.by) < 8) return { type: "muscle", index: i };
    }
    for (let i = 0; i < state.body.bones.length; i++) {
      const bo = state.body.bones[i], a = state.body.joints[bo.a], c = state.body.joints[bo.b];
      if (a && c && ptSegDist(x, y, a.x, a.y, c.x, c.y) < 9) return { type: "bone", index: i };
    }
    return null;
  }

  async function startEvolution() {
    const snap = await apiPost("/api/start", encodeStartPayload());
    if (!snap) return;
    state.snapshot = snap;
    state.screen = "evolve";
    state.paused = false; state.focusIndex = 0; state.stepAcc = 0;
    $("buildScreen").classList.add("is-hidden");
    $("evolveScreen").classList.remove("is-hidden");
    $("taskLabel").textContent = TASKS[state.task].name;
    $("bestPar").textContent = "/ " + TASKS[state.task].par + " PAR";
    $("pauseBanner").classList.remove("is-visible");
    if (!snap.connected) showToast("Warning: BFS detected a disconnected skeleton!");
    updateEvolveHud();
    drawChart();
  }

  let stopWarpRequested = false;

  async function runWarp() {
    if (state.warping) return;
    const totalGens = clamp(parseInt($("warpGensInput").value, 10) || 50, 1, 500);
    state.warping = true;
    stopWarpRequested = false;
    $("warpTitle").textContent = `WARPING ${totalGens} GENERATIONS IN C++...`;
    $("warpStatus").textContent = "RUNNING NATIVE C++ ENGINE...";
    $("warpBar").style.width = "0%";
    $("warpBanner").classList.add("is-visible");
    $("warpBtn").classList.add("is-active");
    $("warpBtn").textContent = "STOP";

    const t0 = performance.now();
    let completed = 0;
    while (completed < totalGens && !stopWarpRequested) {
      const batch = Math.min(5, totalGens - completed);
      const snap = await apiPost(`/api/warp?g=${batch}`);
      if (!snap) break;
      state.snapshot = snap;
      completed += batch;
      const progress = clamp(completed / totalGens, 0, 1);
      $("warpBar").style.width = `${(progress * 100).toFixed(1)}%`;
      $("warpStatus").textContent = `GEN ${pad2(snap.gen)} · BEST: ${Math.round(snap.bestEver)} · (${completed}/${totalGens})`;
      updateEvolveHud();
      drawChart();
      await new Promise(r => setTimeout(r, 0));
    }
    state.warping = false;
    $("warpBanner").classList.remove("is-visible");
    $("warpBtn").classList.remove("is-active");
    $("warpBtn").textContent = "⚡ WARP";
    const sec = ((performance.now() - t0) / 1000).toFixed(2);
    showToast(`C++ warped ${completed} generations in ${sec}s · Best: ${Math.round(state.snapshot?.bestEver || 0)}`);
  }

  function stopWarp() {
    if (state.warping) {
      stopWarpRequested = true;
      $("warpStatus").textContent = "STOPPING...";
    }
  }

  function updateEvolveHud() {
    const s = state.snapshot, t = TASKS[state.task];
    if (!s) return;
    $("genValue").textContent = pad2(s.gen);
    $("timerValue").textContent = `${s.genTime.toFixed(1)} / ${s.duration.toFixed(1)} S`;
    $("progressFill").style.width = `${(clamp(s.genTime / s.duration, 0, 1) * 100).toFixed(1)}%`;

    const ranked = s.creatures.map((c, idx) => ({ idx, fit: c.fit })).sort((a, b) => b.fit - a.fit);
    const liveBest = ranked[0]?.fit || 0;
    $("bestValue").textContent = Math.round(liveBest);
    $("bestParFill").style.width = `${(clamp(liveBest / t.par, 0, 1) * 100).toFixed(1)}%`;
    $("aliveValue").textContent = `${s.creatures.length} RUNNING`;
    $("recordValue").textContent = Math.round(s.bestEver);
    $("recordGenValue").textContent = s.bestEverGen ? `GEN ${pad2(s.bestEverGen)}` : "—";

    const top = Math.max(1, liveBest);
    $("leaderboard").innerHTML = ranked.slice(0, 6).map((item, i) =>
      `<li class="${i === 0 ? "is-first " : ""}${item.idx === state.focusIndex ? "is-tracked" : ""}" data-index="${item.idx}" style="cursor:pointer;">`
      + `<span class="lb-rank">${pad2(i + 1)}</span>`
      + `<span class="lb-bar"><i style="width:${(clamp(item.fit / top, 0, 1) * 100).toFixed(1)}%"></i></span>`
      + `<span class="lb-score">${Math.round(item.fit)}</span></li>`
    ).join("");
  }

  function sizeCanvas(canvas) {
    const r = canvas.getBoundingClientRect(), dpr = Math.min(window.devicePixelRatio || 1, 2);
    const w = Math.max(1, r.width), h = Math.max(1, r.height);
    if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(h * dpr)) {
      canvas.width = Math.round(w * dpr); canvas.height = Math.round(h * dpr);
    }
    return { ctx: canvas.getContext("2d"), w, h, dpr };
  }

  function paintWorld(ctx, task, px, isEvolve) {
    const minX = isEvolve ? -5000 : 0, maxX = isEvolve ? 15000 : WORLD.width;
    ctx.fillStyle = "#14171b";
    ctx.fillRect(minX, -1000, maxX - minX, WORLD.height + 1500);

    ctx.lineWidth = px;
    for (let x = minX; x < maxX; x += 50) {
      ctx.strokeStyle = x % 100 === 0 ? "rgba(232,229,222,0.06)" : "rgba(232,229,222,0.025)";
      ctx.beginPath(); ctx.moveTo(x, -500); ctx.lineTo(x, WORLD.height + 200); ctx.stroke();
    }
    for (let y = 0; y < WORLD.height; y += 50) {
      ctx.strokeStyle = y % 100 === 0 ? "rgba(232,229,222,0.06)" : "rgba(232,229,222,0.025)";
      ctx.beginPath(); ctx.moveTo(minX, y); ctx.lineTo(maxX, y); ctx.stroke();
    }

    for (const seg of task.terrain) {
      const sx0 = Math.max(minX, seg.x0);
      const sx1 = Math.min(maxX, seg.x1);
      if (sx1 <= sx0) continue;
      ctx.fillStyle = "#101317";
      ctx.fillRect(sx0, seg.y, sx1 - sx0, 400);
      ctx.strokeStyle = "rgba(232,229,222,0.6)"; ctx.lineWidth = 1.4 * px;
      ctx.beginPath(); ctx.moveTo(sx0, seg.y); ctx.lineTo(sx1, seg.y); ctx.stroke();
    }

    for (let i = 1; i < task.terrain.length; i++) {
      const prev = task.terrain[i - 1], seg = task.terrain[i];
      if (seg.y < prev.y) {
        ctx.strokeStyle = "rgba(232,229,222,0.34)"; ctx.lineWidth = 1.2 * px;
        ctx.beginPath(); ctx.moveTo(seg.x0, seg.y); ctx.lineTo(seg.x0, prev.y); ctx.stroke();
      }
    }

    ctx.strokeStyle = "rgba(232,229,222,0.45)";
    ctx.lineWidth = 1.5 * px;
    ctx.beginPath();
    ctx.moveTo(task.startX, surfaceY(task, task.startX));
    ctx.lineTo(task.startX, surfaceY(task, task.startX) - 160 * px);
    ctx.stroke();

    if (task.id === "run") {
      ctx.font = `500 ${10 * px}px "Barlow Condensed", sans-serif`;
      ctx.textAlign = "center";
      const maxDist = isEvolve ? 10000 : 700;
      for (let d = 100; d <= maxDist; d += (d >= 2000 ? 500 : (d >= 1000 ? 200 : 100))) {
        const dx = task.startX + d;
        const dy = surfaceY(task, dx);
        ctx.strokeStyle = "rgba(232,229,222,0.22)"; ctx.lineWidth = px;
        ctx.beginPath(); ctx.moveTo(dx, dy + 2 * px); ctx.lineTo(dx, dy + 10 * px); ctx.stroke();
        ctx.fillStyle = "rgba(232,229,222,0.38)";
        ctx.fillText(String(d), dx, dy + 22 * px);
      }
    }

    if (task.goalX) {
      const gx = task.goalX, gy = task.id === "custom" ? (task.goalY ?? 480) : surfaceY(task, gx);
      ctx.strokeStyle = "rgba(230,72,61,0.85)"; ctx.lineWidth = 1.6 * px;
      ctx.beginPath(); ctx.moveTo(gx, surfaceY(task, gx)); ctx.lineTo(gx, gy - 90 * px); ctx.stroke();
      ctx.fillStyle = "#e6483d";
      ctx.beginPath(); ctx.moveTo(gx, gy - 90 * px); ctx.lineTo(gx + 22 * px, gy - 82 * px); ctx.lineTo(gx, gy - 74 * px); ctx.fill();
    }
  }

  function drawBuild() {
    const { ctx, w, h, dpr } = sizeCanvas($("buildCanvas"));
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    const scale = Math.min((w - 30) / WORLD.width, (h - 30) / WORLD.height);
    const view = { scale, ox: (w - WORLD.width * scale) / 2, oy: (h - WORLD.height * scale) / 2 };
    state.buildView = view;
    const px = 1 / scale, b = state.body;

    ctx.save(); ctx.translate(view.ox, view.oy); ctx.scale(scale, scale);
    paintWorld(ctx, TASKS[state.task], px, false);

    ctx.lineCap = "round"; ctx.strokeStyle = "#dedad1"; ctx.lineWidth = 5.4 * px;
    b.bones.forEach((bo, i) => {
      const a = b.joints[bo.a], c = b.joints[bo.b];
      if (state.selection?.type === "bone" && state.selection.index === i) {
        ctx.save(); ctx.strokeStyle = "rgba(53,184,164,0.4)"; ctx.lineWidth = 11 * px;
        ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(c.x, c.y); ctx.stroke(); ctx.restore();
      }
      ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(c.x, c.y); ctx.stroke();
    });

    b.muscles.forEach((m, i) => {
      const ep = musclePts(b, m); if (!ep) return;
      ctx.strokeStyle = (state.selection?.type === "muscle" && state.selection.index === i) ? "#35b8a4" : "rgba(230,72,61,0.92)";
      ctx.lineWidth = 2 * px;
      ctx.beginPath(); ctx.moveTo(ep.ax, ep.ay); ctx.lineTo(ep.bx, ep.by); ctx.stroke();
    });

    b.joints.forEach((j, i) => {
      ctx.beginPath(); ctx.arc(j.x, j.y, j.r, 0, Math.PI * 2);
      ctx.fillStyle = "#15171b"; ctx.fill();
      ctx.strokeStyle = (state.selection?.type === "joint" && state.selection.index === i) ? "#35b8a4" : "#e8e5de";
      ctx.lineWidth = 2 * px; ctx.stroke();
    });
    ctx.restore();
  }

  function drawEvolve() {
    const s = state.snapshot;
    if (!s) return;
    const { ctx, w, h, dpr } = sizeCanvas($("evolveCanvas"));
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    const sideInset = w > 1020 ? 310 : w > 860 ? 268 : 0;
    const baseScale = Math.min((w - sideInset - 24) / WORLD.width, (h - 24) / WORLD.height);
    const scale = baseScale * state.viewZoom, px = 1 / scale;

    const champ = s.creatures[state.focusIndex] || s.creatures[0];
    if (champ && champ.j.length) {
      const cx = champ.j.reduce((sum, pt) => sum + pt[0], 0) / champ.j.length;
      state.camX += (cx - state.camX) * 0.14;
    }
    const ox = (w - sideInset) / 2 - state.camX * scale + state.viewPanX;
    const oy = (h - WORLD.height * scale) / 2 + state.viewPanY;
    state.evolveView = { scale, ox, oy };

    ctx.save(); ctx.translate(ox, oy); ctx.scale(scale, scale);
    paintWorld(ctx, TASKS[state.task], px, true);

    const drawCreature = (cr, alpha, leader) => {
      const pts = cr.j;
      ctx.lineCap = "round";
      ctx.strokeStyle = leader ? "#e8e5de" : `rgba(232,229,222,${alpha})`;
      ctx.lineWidth = (leader ? 6.0 : 3.8) * px;
      state.body.bones.forEach(bo => {
        ctx.beginPath(); ctx.moveTo(pts[bo.a][0], pts[bo.a][1]); ctx.lineTo(pts[bo.b][0], pts[bo.b][1]); ctx.stroke();
      });
      ctx.lineWidth = (leader ? 1.8 : 1.0) * px;
      state.body.muscles.forEach((m, mi) => {
        const bA = state.body.bones[m.a], bB = state.body.bones[m.b];
        const ax = (pts[bA.a][0] + pts[bA.b][0]) * 0.5, ay = (pts[bA.a][1] + pts[bA.b][1]) * 0.5;
        const bx = (pts[bB.a][0] + pts[bB.b][0]) * 0.5, by = (pts[bB.a][1] + pts[bB.b][1]) * 0.5;
        const act = Math.abs(cr.a[mi] || 0);
        ctx.strokeStyle = `rgba(230,72,61,${leader ? (0.45 + 0.55 * act).toFixed(2) : (alpha * 0.5).toFixed(2)})`;
        ctx.beginPath(); ctx.moveTo(ax, ay); ctx.lineTo(bx, by); ctx.stroke();
      });
      pts.forEach(pt => {
        ctx.beginPath(); ctx.arc(pt[0], pt[1], leader ? pt[2] : pt[2] * 0.72, 0, Math.PI * 2);
        ctx.fillStyle = leader ? "#15171a" : `rgba(13,15,18,${alpha})`; ctx.fill();
        ctx.strokeStyle = leader ? "#e8e5de" : `rgba(232,229,222,${alpha})`; ctx.lineWidth = (leader ? 1.6 : 1) * px; ctx.stroke();
        if (leader && pt[3]) {
          ctx.strokeStyle = "rgba(53,184,164,0.85)"; ctx.lineWidth = 2 * px;
          ctx.beginPath(); ctx.moveTo(pt[0] - pt[2], pt[1] + pt[2] + px); ctx.lineTo(pt[0] + pt[2], pt[1] + pt[2] + px); ctx.stroke();
        }
      });
    };

    s.creatures.forEach((cr, idx) => { if (idx !== state.focusIndex) drawCreature(cr, 0.14, false); });
    if (champ) drawCreature(champ, 1.0, true);
    ctx.restore();
  }

  function drawChart() {
    const { ctx, w, h, dpr } = sizeCanvas($("chartCanvas"));
    if (!ctx || w < 10 || h < 10) return;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, w, h);

    const padL = 38, padR = 12, padT = 10, padB = 16;
    const pw = Math.max(10, w - padL - padR), ph = Math.max(10, h - padT - padB);
    const hist = state.snapshot?.history || [];
    const par = TASKS[state.task].par;
    const top = Math.max(par, ...(hist.map(p => p.best) || [0]), 100) * 1.08;

    const xFor = i => hist.length <= 1 ? padL + pw / 2 : padL + (i / (hist.length - 1)) * pw;
    const yFor = v => padT + ph - (clamp(v, 0, top) / top) * ph;

    ctx.strokeStyle = "rgba(232,229,222,0.08)";
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(padL, padT + ph); ctx.lineTo(padL + pw, padT + ph);
    ctx.moveTo(padL, padT); ctx.lineTo(padL, padT + ph);
    ctx.stroke();

    const parY = yFor(par);
    ctx.save();
    ctx.setLineDash([4, 4]);
    ctx.strokeStyle = "rgba(232,229,222,0.18)";
    ctx.beginPath();
    ctx.moveTo(padL, parY); ctx.lineTo(padL + pw, parY);
    ctx.stroke();
    ctx.restore();

    if (!hist.length) {
      ctx.fillStyle = "rgba(232,229,222,0.3)";
      ctx.font = '500 10.5px "Barlow Condensed", sans-serif';
      ctx.textAlign = "center";
      ctx.fillText("AWAITING GENERATION 01 (LEARNING BEGINS)", padL + pw / 2, padT + ph / 2 + 3);
      $("chartSummary").textContent = "AWAITING GENERATION 01";
      return;
    }

    ctx.strokeStyle = "rgba(53,184,164,0.85)";
    ctx.lineWidth = 1.4;
    ctx.beginPath();
    hist.forEach((p, i) => i ? ctx.lineTo(xFor(i), yFor(p.avg)) : ctx.moveTo(xFor(i), yFor(p.avg)));
    ctx.stroke();

    ctx.strokeStyle = "#e6483d";
    ctx.lineWidth = 1.9;
    ctx.beginPath();
    hist.forEach((p, i) => i ? ctx.lineTo(xFor(i), yFor(p.best)) : ctx.moveTo(xFor(i), yFor(p.best)));
    ctx.stroke();

    const last = hist[hist.length - 1];
    const lastX = xFor(hist.length - 1), lastY = yFor(last.best);
    ctx.fillStyle = "#e6483d";
    ctx.beginPath(); ctx.arc(lastX, lastY, 3, 0, Math.PI * 2); ctx.fill();

    $("chartSummary").textContent = `${hist.length} GENS · BEST ${Math.round(last.best)} · AVG ${Math.round(last.avg)}`;
  }

  function bindEvents() {
    $$(".tool[data-tool]").forEach(b => b.addEventListener("click", () => { state.tool = b.dataset.tool; state.pendingJoint = state.pendingBone = null; syncBuildUi(); }));
    $$("[data-task]").forEach(el => el.addEventListener("click", () => {
      state.task = el.dataset.task;
      state.duration = TASKS[state.task].duration;
      $("buildGenTimeInput").value = $("genTimeInput").value = state.duration.toFixed(1);
      syncBuildUi();
      drawChart();
    }));
    $$(".preset-btn[data-preset]").forEach(b => b.addEventListener("click", () => { pushUndo(); state.preset = b.dataset.preset; state.body = PRESETS[state.preset](); state.selection = null; syncBuildUi(); }));
    $("resetBody").addEventListener("click", () => { pushUndo(); state.body = PRESETS[state.preset === "hopper" ? "hopper" : "runner"](); state.selection = null; syncBuildUi(); });
    $("undoBtn").addEventListener("click", () => { if (!state.undo.length) return; state.body = JSON.parse(state.undo.pop()); state.selection = null; syncBuildUi(); });

    $("selectionPanel").addEventListener("input", ev => {
      const prop = ev.target?.dataset?.prop, v = parseFloat(ev.target?.value);
      if (!prop || !state.selection || !isFinite(v)) return;
      if (state.selection.type === "joint") state.body.joints[state.selection.index][prop] = v;
      if (state.selection.type === "muscle") state.body.muscles[state.selection.index][prop] = v;
      ev.target.parentNode.querySelector("output").textContent = v.toFixed(2);
      markCustom();
    });
    $("selectionPanel").addEventListener("click", ev => { if (ev.target.closest("[data-del]")) deletePart(state.selection); });

    const syncParams = () => {
      if (state.screen === "evolve") apiPost("/api/params", `${state.duration} ${state.ghosts} ${(state.mutPct / 100).toFixed(3)}`);
      syncBuildUi();
    };
    [$("buildGenTimeInput"), $("genTimeInput")].forEach(el => el.addEventListener("change", () => { state.duration = clamp(parseFloat(el.value) || 7, 1, 60); $("buildGenTimeInput").value = $("genTimeInput").value = state.duration.toFixed(1); syncParams(); }));
    [$("buildGhostInput"), $("ghostInput")].forEach(el => el.addEventListener("change", () => { state.ghosts = clamp(parseInt(el.value, 10) || 24, 2, 100); $("buildGhostInput").value = $("ghostInput").value = state.ghosts; syncParams(); }));
    [$("buildMutationInput"), $("mutationInput")].forEach(el => el.addEventListener("change", () => { state.mutPct = clamp(parseInt(el.value, 10) || 18, 0, 100); $("buildMutationInput").value = $("mutationInput").value = state.mutPct; syncParams(); }));

    $("speedSlider").addEventListener("input", e => { state.speed = parseFloat(e.target.value) || 1; $("speedValue").textContent = state.speed + "×"; });
    $("speedValue").addEventListener("dblclick", () => { state.speed = 1; $("speedSlider").value = 1; $("speedValue").textContent = "1×"; });
    $("warpBtn").addEventListener("click", () => {
      if (state.warping) stopWarp();
      else runWarp();
    });
    const stopBtn = $("warpStopBtn");
    if (stopBtn) stopBtn.addEventListener("click", stopWarp);

    $("evolveBtn").addEventListener("click", startEvolution);
    $("pauseBtn").addEventListener("click", () => { state.paused = !state.paused; $("pauseBanner").classList.toggle("is-visible", state.paused); $("pauseBtn").textContent = state.paused ? "RESUME" : "PAUSE"; });
    $("exitBtn").addEventListener("click", () => { state.screen = "build"; $("evolveScreen").classList.add("is-hidden"); $("buildScreen").classList.remove("is-hidden"); syncBuildUi(); });

    $("leaderboard").addEventListener("click", ev => { const li = ev.target.closest("li"); if (li) state.focusIndex = parseInt(li.dataset.index, 10) || 0; });
    $("cppBtn").addEventListener("click", () => { $("cppModal").hidden = false; });
    $("helpBtn").addEventListener("click", () => { $("helpModal").hidden = false; });
    $$("[data-close-modal]").forEach(el => el.addEventListener("click", () => { $("helpModal").hidden = true; $("cppModal").hidden = true; }));

    const toWorld = ev => {
      const r = $("buildCanvas").getBoundingClientRect(), v = state.buildView || { scale: 1, ox: 0, oy: 0 };
      return { x: (ev.clientX - r.left - v.ox) / v.scale, y: (ev.clientY - r.top - v.oy) / v.scale };
    };
    $("buildCanvas").addEventListener("pointerdown", ev => {
      const p = toWorld(ev), hit = hitTest(p.x, p.y);
      if (state.tool === "select") {
        state.selection = hit;
        if (hit?.type === "flag" || (!hit && state.task === "custom")) {
          state.drag = { type: "flag" };
          const gx = Math.round(p.x), gy = Math.round(clamp(p.y, 40, 500));
          TASKS.custom.goalX = gx; TASKS.custom.goalY = gy;
          apiPost("/api/goal", `${gx} ${gy}`);
          showToast(`Target flag moved to (${gx}, ${gy})`);
        } else if (hit?.type === "joint") {
          state.drag = { type: "joint", index: hit.index, pushed: false };
        }
      } else if (state.tool === "joint") {
        pushUndo(); state.body.joints.push({ x: Math.round(p.x / 5) * 5, y: clamp(Math.round(p.y / 5) * 5, 24, 500), r: 8, mass: 1 }); markCustom();
      } else if (state.tool === "bone" && hit?.type === "joint") {
        if (state.pendingJoint === null) state.pendingJoint = hit.index;
        else { if (state.pendingJoint !== hit.index) { pushUndo(); state.body.bones.push({ a: state.pendingJoint, b: hit.index }); markCustom(); } state.pendingJoint = null; }
      } else if (state.tool === "muscle" && hit?.type === "bone") {
        if (state.pendingBone === null) state.pendingBone = hit.index;
        else { if (state.pendingBone !== hit.index) { pushUndo(); state.body.muscles.push({ a: state.pendingBone, b: hit.index, strength: 1 }); markCustom(); } state.pendingBone = null; }
      } else if (state.tool === "erase" && hit) { deletePart(hit); }
      renderSelection();
    });
    $("buildCanvas").addEventListener("pointermove", ev => {
      const p = toWorld(ev);
      $("coordText").textContent = `X ${Math.round(p.x)}   Y ${Math.round(p.y)}`;
      if (state.drag) {
        if (state.drag.type === "flag") {
          const gx = Math.round(p.x), gy = Math.round(clamp(p.y, 40, 500));
          TASKS.custom.goalX = gx; TASKS.custom.goalY = gy;
        } else if (state.drag.type === "joint") {
          if (!state.drag.pushed) { pushUndo(); state.drag.pushed = true; }
          const j = state.body.joints[state.drag.index];
          j.x = clamp(Math.round(p.x / 5) * 5, 14, WORLD.width - 14);
          j.y = clamp(Math.round(p.y / 5) * 5, 24, 500);
          markCustom();
        }
      }
    });
    window.addEventListener("pointerup", () => {
      if (state.drag?.type === "flag") {
        apiPost("/api/goal", `${TASKS.custom.goalX} ${TASKS.custom.goalY}`);
      }
      state.drag = null; state.isPanning = false;
    });

    const toEvolveWorld = ev => {
      const r = $("evolveCanvas").getBoundingClientRect(), v = state.evolveView || { scale: 1, ox: 0, oy: 0 };
      return { x: (ev.clientX - r.left - v.ox) / v.scale, y: (ev.clientY - r.top - v.oy) / v.scale };
    };
    let evolveFlagDrag = false;
    $("evolveCanvas").addEventListener("pointerdown", ev => {
      if (state.task === "custom" && ev.button === 0) {
        const p = toEvolveWorld(ev);
        const gx = Math.round(p.x), gy = Math.round(clamp(p.y, 40, 500));
        evolveFlagDrag = true;
        TASKS.custom.goalX = gx; TASKS.custom.goalY = gy;
        apiPost("/api/goal", `${gx} ${gy}`);
        showToast(`Target flag placed at (${gx}, ${gy})`);
      }
    });
    $("evolveCanvas").addEventListener("pointermove", ev => {
      if (evolveFlagDrag && state.task === "custom") {
        const p = toEvolveWorld(ev);
        const gx = Math.round(p.x), gy = Math.round(clamp(p.y, 40, 500));
        TASKS.custom.goalX = gx; TASKS.custom.goalY = gy;
      }
    });
    window.addEventListener("pointerup", () => {
      if (evolveFlagDrag) {
        evolveFlagDrag = false;
        apiPost("/api/goal", `${TASKS.custom.goalX} ${TASKS.custom.goalY}`);
      }
    });

    $("evolveCanvas").addEventListener("wheel", ev => { ev.preventDefault(); state.viewZoom = clamp(state.viewZoom * (1 - ev.deltaY * 0.0015), 0.35, 3.5); }, { passive: false });
    $("evolveCanvas").addEventListener("dblclick", () => { state.viewZoom = 1; state.viewPanX = 0; state.viewPanY = 0; });
    window.addEventListener("keydown", ev => {
      if (ev.key === "Escape") { $("helpModal").hidden = true; $("cppModal").hidden = true; }
      if (state.screen === "evolve" && ev.key === " ") { ev.preventDefault(); $("pauseBtn").click(); }
    });
    window.addEventListener("resize", () => {
      drawBuild();
      drawChart();
    });
  }

  let lastT = 0, lastHistLen = 0;
  function loop(now) {
    const dt = lastT ? Math.min((now - lastT) / 1000, 0.05) : 0.016;
    lastT = now;

    if (state.screen === "build") {
      drawBuild();
    } else {
      drawEvolve();
      if (!state.paused && !state.warping && !state.busy) {
        state.stepAcc += dt * state.speed * 120;
        const steps = Math.min(Math.floor(state.stepAcc), 160);
        if (steps >= 1) {
          state.stepAcc -= steps;
          state.busy = true;
          apiPost(`/api/step?n=${steps}`).then(snap => {
            state.busy = false;
            if (!snap) return;
            state.snapshot = snap;
            updateEvolveHud();
            if (snap.history.length !== lastHistLen) { lastHistLen = snap.history.length; drawChart(); }
          });
        }
      }
    }
    requestAnimationFrame(loop);
  }

  bindEvents();
  syncBuildUi();
  requestAnimationFrame(loop);
})();
