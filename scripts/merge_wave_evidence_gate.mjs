// Synthetic, source-free evidence classifier for the five-PR platform merge wave.
// Never publish as a merge check or release/runner attestation. The caller must
// obtain authoritative GitHub/API and physical-checkout evidence separately.
const SHA = /^[0-9a-f]{40}$/;
const HOST = "ores-truffle-oreslang/";
const ACTIONS_APP_ID = 15368;
const DESKTOP = ["linux-gcc", "linux-clang", "linux-sanitizers", "windows", "macos"];
export const MERGE_TARGETS = Object.freeze({
  "oreslang-platform.mobile#25": ["host (gcc)", "host (clang)", "sanitizers", "physical-checkout-regressions"],
  "oreslang-platform.asic#26": ["preflight", "physical-checkout-regressions"],
  "oreslang-platform.desktop#14": DESKTOP,
  "oreslang-platform.desktop#22": [...DESKTOP, "member-contract"],
  "oreslang-platform.desktop#35": DESKTOP
});

const object = v => v !== null && typeof v === "object" && !Array.isArray(v);
const sha = v => typeof v === "string" && SHA.test(v);
const positiveInt = v => Number.isSafeInteger(v) && v > 0;
const has = (v, key) => Object.prototype.hasOwnProperty.call(v, key);
const forbidden = /[<>:"\\|?*\x00-\x1f\x7f]/;
function safePath(name) {
  // JavaScript lowercasing is not full Unicode case folding. Refuse to make
  // positive physical-checkout claims for non-ASCII names without an external,
  // authoritative portable-path attestation.
  if (typeof name !== "string" || !name || name.length > 4096 ||
      name.startsWith("/") || /[^\x20-\x7e]/.test(name) ||
      forbidden.test(name) || name.normalize("NFC") !== name) return false;
  return name.split("/").every(s => s && s !== "." && s !== ".." &&
    s.toLowerCase() !== ".git" && !/[. ]$/.test(s) &&
    !/^(conin\$|conout\$|con|prn|aux|nul|com[1-9¹²³]|lpt[1-9¹²³])(?:\.|$)/i.test(s));
}
function checkoutEvidence(c) {
  if (!object(c) || !Array.isArray(c.tracked) || !Array.isArray(c.physical) ||
      c.inventoryComplete !== true || c.rawReadStable !== true) return false;
  if (!c.tracked.length || c.tracked.length !== c.physical.length) return false;
  const tracked = new Map(), physical = new Map();
  for (const x of c.tracked) {
    if (!object(x) || !safePath(x.path) || !sha(x.oid) ||
        !["100644","100755"].includes(x.mode)) return false;
    const key = x.path.toLowerCase();
    if (tracked.has(key)) return false;
    tracked.set(key, x);
  }
  for (const x of c.physical) {
    if (!object(x) || !safePath(x.path) || !sha(x.oid) ||
        !["100644","100755"].includes(x.mode) || x.kind !== "regular" ||
        x.nlink !== 1 || x.bytesStable !== true) return false;
    const key = x.path.toLowerCase();
    if (physical.has(key)) return false;
    physical.set(key, x);
  }
  for (const [key,t] of tracked) {
    const p = physical.get(key);
    if (!p || p.path !== t.path || p.oid !== t.oid || p.mode !== t.mode) return false;
  }
  return true;
}
function snapshotMatch(p, pinned, author) {
  return object(p) && p.repository === pinned.repository &&
    p.number === pinned.number && p.headSha === pinned.headSha &&
    p.baseSha === pinned.baseSha && p.author === author &&
    p.state === "open" && p.draft === false;
}
function branchMatch(b, pinned) {
  return object(b) && b.sha === pinned.baseSha &&
    b.protected === true && b.strictRequiredChecks === true &&
    b.codeOwnerReviewRequired === true && b.dismissStaleReviews === true &&
    b.enforceAdmins === true && b.forcePushAllowed === false &&
    b.deleteAllowed === false && positiveInt(b.requiredApprovingReviewCount) &&
    b.requiredApprovingReviewCount <= 100;
}
function reviewEvidence(reviews, pinned, author, securitySensitive, configuredQuorum) {
  if (!Array.isArray(reviews) || !positiveInt(configuredQuorum) || configuredQuorum > 100) return false;
  const latest = new Map();
  for (const r of reviews) {
    if (!object(r) || typeof r.login !== "string" ||
        !/^[A-Za-z0-9-]{1,39}$/.test(r.login) || !positiveInt(r.id) ||
        !["APPROVED","COMMENTED","CHANGES_REQUESTED","DISMISSED"].includes(r.state)) return false;
    const key = r.login.toLowerCase();
    const previous = latest.get(key);
    if (previous && previous.id >= r.id) return false; // monotonic immutable event order
    latest.set(key, r);
  }
  // An unresolved independent CHANGES_REQUESTED must not be ignored just
  // because two *other* people have already approved.
  if ([...latest.values()].some(r => r.state === "CHANGES_REQUESTED" &&
      r.login.toLowerCase() !== author.toLowerCase())) return false;
  const approved = [...latest.values()].filter(r =>
    r.state === "APPROVED" && r.commitSha === pinned.headSha &&
    r.login.toLowerCase() !== author.toLowerCase() &&
    ["write","maintain","admin"].includes(r.permission));
  const owners = new Set(["oresoftware","the1mills"]);
  if (!approved.some(r => owners.has(r.login.toLowerCase()))) return false;
  return new Set(approved.map(r => r.login.toLowerCase())).size >=
    Math.max(configuredQuorum, securitySensitive ? 2 : 1);
}
function ciEvidence(s, pinned, required) {
  if (!Array.isArray(s.checkRuns) || !Array.isArray(s.workflowRuns) || !Array.isArray(s.jobs)) return false;
  const checkNames = new Set(), checkIds = new Set();
  const runs = new Map(), jobs = new Map();
  for (const r of s.workflowRuns) {
    if (!object(r) || !positiveInt(r.id) || runs.has(r.id) ||
        r.repository !== pinned.repository || r.headSha !== pinned.headSha ||
        !["pull_request","push"].includes(r.event) ||
        !positiveInt(r.attempt) || r.status !== "completed" || r.conclusion !== "success") return false;
    runs.set(r.id, r);
  }
  for (const j of s.jobs) {
    if (!object(j) || !positiveInt(j.checkRunId) || jobs.has(j.checkRunId) ||
        !positiveInt(j.runId) || !positiveInt(j.attempt) ||
        !positiveInt(j.runnerId) || typeof j.runnerName !== "string" || !j.runnerName.trim() ||
        j.status !== "completed" || j.conclusion !== "success" ||
        !Array.isArray(j.steps) || !j.steps.length ||
        j.steps.some(x => !object(x) || x.status !== "completed" || x.conclusion !== "success")) return false;
    const r = runs.get(j.runId);
    if (!r || r.attempt !== j.attempt) return false;
    jobs.set(j.checkRunId, j);
  }
  for (const c of s.checkRuns) {
    if (!object(c) || !positiveInt(c.id) || checkIds.has(c.id) ||
        typeof c.name !== "string" || checkNames.has(c.name) ||
        c.headSha !== pinned.headSha || c.appId !== ACTIONS_APP_ID ||
        c.status !== "completed" || c.conclusion !== "success") return false;
    checkIds.add(c.id); checkNames.add(c.name);
    if (!jobs.has(c.id)) return false;
  }
  return required.every(name => checkNames.has(name));
}
/**
 * A diagnostic model for data read *independently* from trusted GitHub and
 * physical checkout sources. Callers must not trust a PR-supplied JSON object
 * as evidence. Even the green path NEVER authorizes a merge or a release.
 */
export function assessMergeWaveEvidence(snapshot, pinned) {
  const reasons = [];
  const deny = code => { if (!reasons.includes(code)) reasons.push(code); };
  if (!object(pinned) || !sha(pinned.headSha) || !sha(pinned.baseSha) ||
      typeof pinned.repository !== "string" || !pinned.repository.startsWith(HOST) ||
      !positiveInt(pinned.number)) {
    deny("INVALID_TRUSTED_PIN");
  }
  const key = object(pinned) && typeof pinned.repository === "string"
    ? pinned.repository.slice(HOST.length) + "#" + pinned.number : "";
  const required = MERGE_TARGETS[key];
  if (!required) deny("UNKNOWN_MERGE_TARGET");
  if (!object(snapshot)) deny("MALFORMED_EVIDENCE");
  if (reasons.length) return Object.freeze({
    kind:"SYNTHETIC_SOURCE_FREE_DIAGNOSTIC", readyForIndependentReview:false,
    mergeAuthorized:false, releaseAuthorized:false, reasons:Object.freeze(reasons)
  });
  if (!object(snapshot.startPr) || typeof snapshot.startPr.author !== "string" ||
      !/^[A-Za-z0-9-]{1,39}$/.test(snapshot.startPr.author)) deny("INVALID_AUTHOR");
  const author = object(snapshot.startPr) ? snapshot.startPr.author : "";
  if (!snapshotMatch(snapshot.startPr, pinned, author) ||
      !snapshotMatch(snapshot.endPr, pinned, author)) deny("STALE_OR_MOVING_PR");
  if (!branchMatch(snapshot.startBranch, pinned) || !branchMatch(snapshot.endBranch, pinned) ||
      snapshot.startBranch.requiredApprovingReviewCount !==
      snapshot.endBranch.requiredApprovingReviewCount)
    deny("UNPROTECTED_OR_MOVING_BASE");
  if (!reviewEvidence(snapshot.reviews, pinned, author, key === "oreslang-platform.desktop#14",
      object(snapshot.startBranch) ? snapshot.startBranch.requiredApprovingReviewCount : null))
    deny("MISSING_CURRENT_INDEPENDENT_APPROVAL");
  if (!ciEvidence(snapshot, pinned, required)) deny("UNEXECUTED_OR_UNTRUSTED_CI");
  if (!checkoutEvidence(snapshot.checkout)) deny("UNVERIFIED_PHYSICAL_CHECKOUT");
  return Object.freeze({
    kind:"SYNTHETIC_SOURCE_FREE_DIAGNOSTIC",
    readyForIndependentReview:reasons.length===0,
    mergeAuthorized:false,
    releaseAuthorized:false,
    reasons:Object.freeze(reasons)
  });
}
