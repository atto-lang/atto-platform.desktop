// Tests exercise synthetic snapshots ONLY; these do not attest live hosted CI.
import test from "node:test";
import assert from "node:assert/strict";
import {assessMergeWaveEvidence, MERGE_TARGETS} from "../scripts/merge_wave_evidence_gate.mjs";

const HEAD="a".repeat(40), BASE="b".repeat(40), OID="c".repeat(40);
const sourceRepo = "ores-truffle-oreslang/";
function fixture(target="oreslang-platform.desktop#14") {
  const [repo,num]=target.split("#"), repository=sourceRepo+repo;
  const number=Number(num), author="the1mills";
  const required=MERGE_TARGETS[target];
  const pr={repository,number,headSha:HEAD,baseSha:BASE,author,state:"open",draft:false};
  const branch={sha:BASE,protected:true,strictRequiredChecks:true,
    codeOwnerReviewRequired:true,dismissStaleReviews:true,enforceAdmins:true,
    forcePushAllowed:false,deleteAllowed:false,requiredApprovingReviewCount:1};
  const reviews=[{id:1,login:"ORESoftware",state:"APPROVED",commitSha:HEAD,permission:"admin"}];
  if (target==="oreslang-platform.desktop#14")
    reviews.push({id:2,login:"security-second",state:"APPROVED",commitSha:HEAD,permission:"write"});
  const checkRuns=[],jobs=[],workflowRuns=[];
  required.forEach((name,i)=>{
    const id=100+i,runId=200+i;
    checkRuns.push({id,name,headSha:HEAD,appId:15368,status:"completed",conclusion:"success"});
    workflowRuns.push({id:runId,repository,headSha:HEAD,attempt:1,event:"pull_request",
      status:"completed",conclusion:"success"});
    jobs.push({checkRunId:id,runId,attempt:1,runnerId:300+i,runnerName:"host-"+i,
      status:"completed",conclusion:"success",
      steps:[{status:"completed",conclusion:"success"}]});
  });
  const checkout={inventoryComplete:true,rawReadStable:true,
    tracked:[{path:"src/trusted.txt",oid:OID,mode:"100644"}],
    physical:[{path:"src/trusted.txt",oid:OID,mode:"100644",kind:"regular",
      nlink:1,bytesStable:true}]};
  return {pinned:{repository,number,headSha:HEAD,baseSha:BASE},
    snapshot:{startPr:{...pr},endPr:{...pr},startBranch:{...branch},
      endBranch:{...branch},reviews,checkRuns,workflowRuns,jobs,checkout}};
}
const audit=({pinned,snapshot})=>assessMergeWaveEvidence(snapshot,pinned);
const denied=(entry,reason)=>{
  const result=audit(entry);
  assert.equal(result.readyForIndependentReview,false);
  assert.equal(result.mergeAuthorized,false);
  assert.equal(result.releaseAuthorized,false);
  assert.ok(result.reasons.includes(reason),result.reasons.join(","));
};
for(const target of Object.keys(MERGE_TARGETS)) {
  test("synthetic green "+target+" never authorizes merge/release",()=>{
    const d=audit(fixture(target));
    assert.equal(d.readyForIndependentReview,true);
    assert.equal(d.mergeAuthorized,false);
    assert.equal(d.releaseAuthorized,false);
    assert.equal(d.kind,"SYNTHETIC_SOURCE_FREE_DIAGNOSTIC");
  });
}
const change=(name,mutate,reason)=>test(name,()=>{
  const e=fixture(); mutate(e.snapshot,e.pinned);denied(e,reason);
});
change("draft PR denied",(s)=>{s.startPr.draft=true;},"STALE_OR_MOVING_PR");
change("head changed during API audit denied",(s)=>{s.endPr.headSha="d".repeat(40);},"STALE_OR_MOVING_PR");
change("base changed mid-audit denied",(s)=>{s.endBranch.sha="d".repeat(40);},"UNPROTECTED_OR_MOVING_BASE");
change("unprotected main denied",(s)=>{s.startBranch.protected=false;},"UNPROTECTED_OR_MOVING_BASE");
change("admin bypass denied",(s)=>{s.startBranch.enforceAdmins=false;},"UNPROTECTED_OR_MOVING_BASE");
change("stale approval denied",(s)=>{s.reviews[0].commitSha=BASE;},"MISSING_CURRENT_INDEPENDENT_APPROVAL");
change("self approval cannot count",(s)=>{s.reviews=[{id:5,login:"the1mills",state:"APPROVED",commitSha:HEAD,permission:"admin"}];},"MISSING_CURRENT_INDEPENDENT_APPROVAL");
change("latest CHANGES_REQUESTED supersedes approval",(s)=>{s.reviews.push({id:3,login:"ORESoftware",state:"CHANGES_REQUESTED",commitSha:HEAD,permission:"admin"});},"MISSING_CURRENT_INDEPENDENT_APPROVAL");
change("review dismissed supersedes approval",(s)=>{s.reviews.push({id:4,login:"ORESoftware",state:"DISMISSED",commitSha:HEAD,permission:"admin"});},"MISSING_CURRENT_INDEPENDENT_APPROVAL");
change("reviewer lacks write permission",(s)=>{s.reviews[0].permission="read";},"MISSING_CURRENT_INDEPENDENT_APPROVAL");
change("missing second sensitive approval",(s)=>{s.reviews.pop();},"MISSING_CURRENT_INDEPENDENT_APPROVAL");
change("duplicate review event id denied",(s)=>{s.reviews.push({...s.reviews[0]});},"MISSING_CURRENT_INDEPENDENT_APPROVAL");
change("CI check spoofed GitHub issuer",(s)=>{s.checkRuns[0].appId=12;},"UNEXECUTED_OR_UNTRUSTED_CI");
change("CI check issuer absent",(s)=>{delete s.checkRuns[0].appId;},"UNEXECUTED_OR_UNTRUSTED_CI");
change("CI check for old head",(s)=>{s.checkRuns[0].headSha=BASE;},"UNEXECUTED_OR_UNTRUSTED_CI");
change("CI check pending",(s)=>{s.checkRuns[0].status="in_progress";},"UNEXECUTED_OR_UNTRUSTED_CI");
change("CI check neutral conclusion",(s)=>{s.checkRuns[0].conclusion="neutral";},"UNEXECUTED_OR_UNTRUSTED_CI");
change("source-free canary cannot replace host matrix",(s)=>{s.checkRuns[0].name="allocation-only";},"UNEXECUTED_OR_UNTRUSTED_CI");
change("forged extra passing check cannot replace missing check",(s)=>{s.checkRuns.shift();s.jobs.shift();s.workflowRuns.shift();s.checkRuns.push({id:999,name:"forged pass",headSha:HEAD,appId:15368,status:"completed",conclusion:"success"});},"UNEXECUTED_OR_UNTRUSTED_CI");
change("zero-step runner failure",(s)=>{s.jobs[0].steps=null;},"UNEXECUTED_OR_UNTRUSTED_CI");
change("runner not assigned",(s)=>{s.jobs[0].runnerId=null;},"UNEXECUTED_OR_UNTRUSTED_CI");
change("untrusted runner metadata",(s)=>{s.jobs[0].runnerName="";},"UNEXECUTED_OR_UNTRUSTED_CI");
change("job skipped",(s)=>{s.jobs[0].conclusion="skipped";},"UNEXECUTED_OR_UNTRUSTED_CI");
change("job step failed",(s)=>{s.jobs[0].steps[0].conclusion="failure";},"UNEXECUTED_OR_UNTRUSTED_CI");
change("job from another workflow run",(s)=>{s.jobs[0].runId=999;},"UNEXECUTED_OR_UNTRUSTED_CI");
change("job stale attempt",(s)=>{s.jobs[0].attempt=2;},"UNEXECUTED_OR_UNTRUSTED_CI");
change("run from wrong repo",(s)=>{s.workflowRuns[0].repository="attacker/fork";},"UNEXECUTED_OR_UNTRUSTED_CI");
change("run for wrong head",(s)=>{s.workflowRuns[0].headSha=BASE;},"UNEXECUTED_OR_UNTRUSTED_CI");
change("missing source owned checkout inventory",(s)=>{s.checkout=null;},"UNVERIFIED_PHYSICAL_CHECKOUT");
change("untracked physical source file denied",(s)=>{s.checkout.physical.push({path:"src/evil.txt",oid:OID,mode:"100644",kind:"regular",nlink:1,bytesStable:true});},"UNVERIFIED_PHYSICAL_CHECKOUT");
change("ignored file cannot be hidden",(s)=>{s.checkout.physical.push({path:"ignored.bin",oid:OID,mode:"100644",kind:"regular",nlink:1,bytesStable:true});},"UNVERIFIED_PHYSICAL_CHECKOUT");
change("case colliding physical alias denied",(s)=>{s.checkout.physical.push({path:"SRC/TRUSTED.TXT",oid:OID,mode:"100644",kind:"regular",nlink:1,bytesStable:true});},"UNVERIFIED_PHYSICAL_CHECKOUT");
change("symlink substituted tracked source denied",(s)=>{s.checkout.physical[0].kind="symlink";},"UNVERIFIED_PHYSICAL_CHECKOUT");
change("fifo substituted tracked source denied",(s)=>{s.checkout.physical[0].kind="fifo";},"UNVERIFIED_PHYSICAL_CHECKOUT");
change("hardlinked tracked source denied",(s)=>{s.checkout.physical[0].nlink=2;},"UNVERIFIED_PHYSICAL_CHECKOUT");
change("worktree bytes changed",(s)=>{s.checkout.physical[0].oid="d".repeat(40);},"UNVERIFIED_PHYSICAL_CHECKOUT");
change("worktree mode changed",(s)=>{s.checkout.physical[0].mode="100755";},"UNVERIFIED_PHYSICAL_CHECKOUT");
change("raw-read stability missing",(s)=>{s.checkout.rawReadStable=false;},"UNVERIFIED_PHYSICAL_CHECKOUT");
change("incomplete inventory cannot be proof",(s)=>{s.checkout.inventoryComplete=false;},"UNVERIFIED_PHYSICAL_CHECKOUT");
change("unsafe git metadata path denied",(s)=>{s.checkout.tracked[0].path=".git/config";s.checkout.physical[0].path=".git/config";},"UNVERIFIED_PHYSICAL_CHECKOUT");
change("traversal source path denied",(s)=>{s.checkout.tracked[0].path="../evil";s.checkout.physical[0].path="../evil";},"UNVERIFIED_PHYSICAL_CHECKOUT");
change("truncated source snapshot denied",(s)=>{s.checkout.physical.length=0;},"UNVERIFIED_PHYSICAL_CHECKOUT");
test("invalid pinned target rejected without throwing",()=>{
  const x=fixture();x.pinned.headSha="short";
  denied(x,"INVALID_TRUSTED_PIN");
});
test("missing snapshot rejected without granting merge/release",()=>{
  const x=fixture();x.snapshot=null;
  denied(x,"MALFORMED_EVIDENCE");
});


change("independent outstanding changes request blocks even with two other approvals",
  s=>{s.reviews.push({id:9,login:"security-third",state:"CHANGES_REQUESTED",commitSha:HEAD,permission:"write"});},
  "MISSING_CURRENT_INDEPENDENT_APPROVAL");
test("resolved third-party changes request permits diagnostic only",()=>{
  const e=fixture();
  e.snapshot.reviews.push({id:9,login:"security-third",state:"CHANGES_REQUESTED",commitSha:HEAD,permission:"write"});
  e.snapshot.reviews.push({id:10,login:"security-third",state:"COMMENTED",commitSha:HEAD,permission:"write"});
  const outcome=audit(e);
  assert.equal(outcome.readyForIndependentReview,true);
  assert.equal(outcome.mergeAuthorized,false);
  assert.equal(outcome.releaseAuthorized,false);
});
change("ordinary PR requires branch-configured second approval",
  (s,p)=>{
    p.repository=sourceRepo+"oreslang-platform.desktop";p.number=35;
    s.startPr.repository=p.repository;s.endPr.repository=p.repository;
    s.startPr.number=35;s.endPr.number=35;
    s.startBranch.requiredApprovingReviewCount=2;s.endBranch.requiredApprovingReviewCount=2;
    s.reviews=[s.reviews[0]];
    const required=MERGE_TARGETS["oreslang-platform.desktop#35"];
    s.checkRuns=s.checkRuns.filter(x=>required.includes(x.name));
    const ids=new Set(s.checkRuns.map(x=>x.id));
    s.jobs=s.jobs.filter(x=>ids.has(x.checkRunId));
    const runs=new Set(s.jobs.map(x=>x.runId));
    s.workflowRuns=s.workflowRuns.filter(x=>runs.has(x.id));
  },
  "MISSING_CURRENT_INDEPENDENT_APPROVAL");
test("ordinary PR with two branch-required independent approvals is diagnostic-only",()=>{
  const e=fixture("oreslang-platform.desktop#35");
  e.snapshot.startBranch.requiredApprovingReviewCount=2;
  e.snapshot.endBranch.requiredApprovingReviewCount=2;
  e.snapshot.reviews.push({id:3,login:"security-second",state:"APPROVED",commitSha:HEAD,permission:"write"});
  const actual=audit(e);
  assert.equal(actual.readyForIndependentReview,true,actual.reasons.join(","));
  assert.equal(actual.mergeAuthorized,false);
});
change("sensitive PR branch-required third approval cannot be replaced by only two",
  s=>{s.startBranch.requiredApprovingReviewCount=3;s.endBranch.requiredApprovingReviewCount=3;},
  "MISSING_CURRENT_INDEPENDENT_APPROVAL");
test("sensitive PR with three required approvals remains diagnostic only",()=>{
  const e=fixture();
  e.snapshot.startBranch.requiredApprovingReviewCount=3;
  e.snapshot.endBranch.requiredApprovingReviewCount=3;
  e.snapshot.reviews.push({id:3,login:"security-third",state:"APPROVED",commitSha:HEAD,permission:"maintain"});
  const x=audit(e);
  assert.equal(x.readyForIndependentReview,true,x.reasons.join(","));
  assert.equal(x.mergeAuthorized,false);
});
change("branch approval count modified during audit denied",
  s=>{s.endBranch.requiredApprovingReviewCount=3;},
  "UNPROTECTED_OR_MOVING_BASE");
change("branch approval quorum cannot be false",
  s=>{s.startBranch.requiredApprovingReviewCount=false;},
  "UNPROTECTED_OR_MOVING_BASE");
change("branch approval quorum cannot be zero",
  s=>{s.startBranch.requiredApprovingReviewCount=0;},
  "UNPROTECTED_OR_MOVING_BASE");
for(const unsafe of ["CONIN$","CONOUT$","COM¹.txt","LPT².txt","lpt³","CON.txt","AUX.log"]) {
  change("reject Windows reserved checkout path "+unsafe,
    s=>{s.checkout.tracked[0].path="src/"+unsafe;s.checkout.physical[0].path="src/"+unsafe;},
    "UNVERIFIED_PHYSICAL_CHECKOUT");
}
change("ambiguous Unicode case-folded checkout path cannot attest",
  s=>{s.checkout.tracked[0].path="src/straße.txt";s.checkout.physical[0].path="src/STRASSE.txt";},
  "UNVERIFIED_PHYSICAL_CHECKOUT");
change("even matching Unicode paths require an independent casefold attestation",
  s=>{s.checkout.tracked[0].path="src/café.txt";s.checkout.physical[0].path="src/café.txt";},
  "UNVERIFIED_PHYSICAL_CHECKOUT");
test("safe Windows non-device COM10 checkout remains diagnostic-only",()=>{
  const e=fixture();
  e.snapshot.checkout.tracked[0].path="src/COM10.txt";
  e.snapshot.checkout.physical[0].path="src/COM10.txt";
  const x=audit(e);
  assert.equal(x.readyForIndependentReview,true,x.reasons.join(","));
  assert.equal(x.mergeAuthorized,false);
});
