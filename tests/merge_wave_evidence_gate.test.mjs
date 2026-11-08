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
    forcePushAllowed:false,deleteAllowed:false};
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
