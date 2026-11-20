// Host integration manifest only. It does not assert GUI, compiler or runtime parity.
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const raw=()=>readFileSync(new URL('../.oreslang/integration.json',import.meta.url),'utf8');
const record=()=>JSON.parse(raw());
const keys=['schemaVersion','protocol','repository','role','upstreams','evidence','note'];

test('desktop member is exactly the expected non-attesting integration identity',()=>{
 const x=record();
 assert.deepEqual(Object.keys(x).sort(),[...keys].sort());
 assert.equal(x.schemaVersion,1);
 assert.equal(x.protocol,'oreslang-ecosystem-integration');
 assert.equal(x.repository,'ores-truffle-oreslang/oreslang-platform.desktop');
 assert.equal(x.role,'host');
 assert.equal(x.evidence,'not-attested');
 assert.match(x.note,/no CI or runtime parity is asserted/);
});

test('desktop upstream declarations are exact, sorted and never execution receipts',()=>{
 const x=record();
 assert.deepEqual(x.upstreams,['oreslang-source.llvm','oreslang-target-profiles']);
 assert.deepEqual(x.upstreams,[...new Set(x.upstreams)].sort());
 for(const upstream of x.upstreams){
  assert.match(upstream,/^oreslang-(?:source\.llvm|target-profiles)$/);
 }
 assert.equal(Object.hasOwn(x,'workflow_run_id'),false);
 assert.equal(Object.hasOwn(x,'guestExecuted'),false);
 assert.equal(Object.hasOwn(x,'binaryAttested'),false);
});

test('parsing exact bytes cannot elevate a source-only member to guest certification',()=>{
 const x=record();
 const parsed=JSON.parse(raw());
 assert.equal(parsed.evidence,'not-attested');
 assert.notEqual(parsed.role,'compiler');
 assert.notEqual(parsed.role,'guest-runtime');
 assert.equal(Object.getPrototypeOf(parsed),Object.prototype);
 assert.equal(x.note.includes('Declared integration touchpoints only'),true);
});
