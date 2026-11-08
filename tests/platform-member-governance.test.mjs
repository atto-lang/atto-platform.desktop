// Source-free per-member guard. Not a compiler, guest, CI or hardware attestation.
import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync, lstatSync, fstatSync, openSync, closeSync, constants, mkdtempSync, mkdirSync, writeFileSync, symlinkSync, linkSync, rmSync} from "node:fs";
import {dirname,join} from "node:path";
import {tmpdir} from "node:os";
import {fileURLToPath} from "node:url";

const expected = {
  "schemaVersion": 1,
  "protocol": "oreslang-ecosystem-integration",
  "repository": "ores-truffle-oreslang/oreslang-platform.desktop",
  "role": "host",
  "upstreams": [
    "oreslang-source.llvm",
    "oreslang-target-profiles"
  ],
  "evidence": "not-attested",
  "note": "Declared integration touchpoints only; no CI or runtime parity is asserted."
};
const manifest = new URL("../.oreslang/integration.json", import.meta.url);

function admit(bytes) {
  assert.ok(Buffer.isBuffer(bytes), "input must be bytes");
  assert.ok(bytes.length > 0 && bytes.length <= 65536, "bounded manifest size");
  const raw = new TextDecoder("utf-8", {fatal:true, ignoreBOM:true}).decode(bytes);
  assert.ok(!raw.startsWith("\uFEFF"), "BOM disallowed");
  const data = JSON.parse(raw);
  // Canonical byte equality rejects duplicate authority keys, including
  // Unicode-escaped aliases that JSON.parse would otherwise last-win.
  assert.equal(raw, JSON.stringify(data, null, 2) + "\n", "noncanonical or duplicate keys");
  // Pin expected authority key order and bytes, not just JSON.parse equivalence.
  // A self-serialized reordered object is otherwise incorrectly accepted.
  assert.equal(raw, JSON.stringify(expected, null, 2) + "\n", "canonical authority bytes/order");
  assert.deepEqual(data, expected, "identity, typed upstreams and anti-attestation must match");
  return {verified:false, parity:false, execution:false, evidence:"metadata-only"};
}

test("committed member manifest is a regular, single-linked canonical file", () => {
  assert.deepEqual(readManifestFromDisk(fileURLToPath(manifest)), {
    verified:false, parity:false, execution:false, evidence:"metadata-only"
  });
});

test("reject missing, substituted and duplicate authority fields", () => {
  const altered = structuredClone(expected);
  delete altered.repository;
  assert.throws(() => admit(Buffer.from(JSON.stringify(altered,null,2)+"\n")));
  altered.repository = "attacker/substituted-repo";
  assert.throws(() => admit(Buffer.from(JSON.stringify(altered,null,2)+"\n")));
  assert.throws(() => admit(Buffer.from(JSON.stringify(expected,null,2).replace('"role": "host",','"role": "host",\n  "role": "compiler",')+"\n")));
  assert.throws(() => admit(Buffer.from(JSON.stringify(expected,null,2).replace('"role": "host",','"\\u0072ole": "compiler",\n  "role": "host",')+"\n")));
  assert.throws(() => admit(Buffer.from(JSON.stringify({...expected, attested:true},null,2)+"\n")));
});

test("reject upstream edge substitution, missing edges and duplicate edges", () => {
  for (const candidate of [[],["oreslang-source.js"],expected.upstreams.concat(expected.upstreams[0]),[...expected.upstreams].reverse()]) {
    if (JSON.stringify(candidate) === JSON.stringify(expected.upstreams)) continue;
    assert.throws(() => admit(Buffer.from(JSON.stringify({...expected,upstreams:candidate},null,2)+"\n")));
  }
});

test("reject false release and hardware/guest execution promotion", () => {
  for (const evidence of ["verified","passing","compiler-guest","physical-hardware",true,null]) {
    assert.throws(() => admit(Buffer.from(JSON.stringify({...expected,evidence},null,2)+"\n")));
  }
  assert.throws(() => admit(Buffer.from(JSON.stringify({...expected,role:"compiler"},null,2)+"\n")));
  assert.throws(() => admit(Buffer.from(JSON.stringify({...expected,note:"all tests passed"},null,2)+"\n")));
});

test("reject invalid encodings, size and noncanonical bytes", () => {
  const valid = Buffer.from(JSON.stringify(expected,null,2)+"\n");
  assert.throws(() => admit(Buffer.concat([Buffer.from([0xEF,0xBB,0xBF]),valid])));
  assert.throws(() => admit(Buffer.from([0xC3,0x28])));
  assert.throws(() => admit(Buffer.alloc(65537,0x20)));
  assert.throws(() => admit(Buffer.concat([valid,Buffer.from(" ")])));
  assert.throws(() => admit(Buffer.from(JSON.stringify(expected))));
});


test("reject reordered or relabeled authority keys despite identical object semantics", () => {
  const reordered = {
    protocol:expected.protocol, schemaVersion:expected.schemaVersion,
    repository:expected.repository, role:expected.role, upstreams:expected.upstreams,
    evidence:expected.evidence, note:expected.note
  };
  assert.deepEqual(reordered, expected, "attack preserves semantic fields");
  assert.throws(() => admit(Buffer.from(JSON.stringify(reordered, null, 2) + "\n")),
                /canonical/i);
  const swapped = Object.fromEntries(Object.entries(expected).reverse());
  assert.throws(() => admit(Buffer.from(JSON.stringify(swapped, null, 2) + "\n")),
                /canonical/i);
});

test("reject forged reviewer, CI, hardware and release attestations", () => {
  const forgeries = [
    ["approved",true], ["reviewers",["ORESoftware"]], ["review_sha","0".repeat(40)],
    ["ci",{success:true,steps:1}], ["runner",{id:123,executed:true}],
    ["release",true], ["guest_execution",true], ["aot_passed",true],
    ["hardware_proof",true], ["parity",{java:true,llvm:true}],
    ["artifact_sha256","0".repeat(64)], ["certified",true],
    ["__proto__",{verified:true}], ["constructor",{prototype:{verified:true}}]
  ];
  for (const [key, value] of forgeries) {
    const fake = Object.fromEntries([...Object.entries(expected), [key,value]]);
    assert.throws(() => admit(Buffer.from(JSON.stringify(fake, null, 2) + "\n")),
                  "forged field " + key + " must not be accepted");
  }
});

test("reject type confusion and nested upstream edge substitutions", () => {
  for (const candidate of [
    null, [], 1, "oreslang-source.llvm", true,
    {0:"oreslang-source.llvm",1:"oreslang-target-profiles"},
    ["oreslang-source.llvm",null],
    ["oreslang-source.llvm","oreslang-source.llvm"],
    ["oreslang-source.llvm",{"name":"oreslang-target-profiles"}],
    ["oreslang-source.llvm","oreslang-target-profiles","../other-repo"]
  ]) {
    const fake = {...expected,upstreams:candidate};
    assert.throws(() => admit(Buffer.from(JSON.stringify(fake,null,2)+"\n")));
  }
});

test("reject unicode escapes, noncanonical whitespace and truncated UTF-8", () => {
  const canonical = JSON.stringify(expected, null, 2) + "\n";
  for (const changed of [
    canonical.replace('"host"', '"\u0068ost"'.replace("h","\\u0068")),
    canonical.replace('"not-attested"', '"not\\u002dattested"'),
    canonical.replace('  "protocol":', '\t"protocol":'),
    canonical.replace(/\n/g, "\r\n"),
    canonical + "\n",
    canonical.slice(0, -1),
    canonical + " ",
    canonical.replace('"schemaVersion": 1', '"schemaVersion": 1.0'),
    canonical.replace('"role": "host"', '"role": "host",\n  "role": "host"')
  ]) {
    assert.throws(() => admit(Buffer.from(changed)), "reject noncanonical bytes");
  }
  for (const bytes of [
    Buffer.from([0xC0,0xAF]), Buffer.from([0xE0,0x80,0x80]),
    Buffer.from([0xED,0xA0,0x80]), Buffer.from([0xF0,0x80,0x80,0x80]),
    Buffer.from([0xEF,0xBB,0xBF,...Buffer.from(canonical)]),
    Buffer.concat([Buffer.from(canonical),Buffer.from([0x00])])
  ]) {
    assert.throws(() => admit(bytes), "reject malformed UTF-8 or hidden bytes");
  }
});

test("valid canonical manifest remains metadata-only, never evidence", () => {
  const valid = Buffer.from(JSON.stringify(expected,null,2)+"\n");
  const result = admit(valid);
  assert.deepEqual(result, {verified:false,parity:false,execution:false,evidence:"metadata-only"});
  assert.equal(Object.hasOwn(result,"approved"),false);
  assert.equal(Object.hasOwn(result,"certified"),false);
});


function readManifestFromDisk(path) {
  const parent = lstatSync(dirname(path));
  assert.ok(parent.isDirectory() && !parent.isSymbolicLink(), "unsafe manifest parent");
  const entry = lstatSync(path);
  assert.ok(entry.isFile() && !entry.isSymbolicLink() && entry.nlink === 1,
            "unsafe manifest file type or hard link");
  // O_NOFOLLOW applies at the final path; separately check the parent.
  const fd = openSync(path, constants.O_RDONLY | (constants.O_NOFOLLOW ?? 0));
  try {
    const before = fstatSync(fd);
    assert.ok(before.isFile() && before.nlink === 1 && before.size > 0 &&
              before.size <= 65536, "invalid opened manifest");
    const bytes = readFileSync(fd);
    const after = fstatSync(fd);
    assert.equal(bytes.length, before.size, "manifest size changed while reading");
    assert.equal(after.dev, before.dev, "manifest device changed");
    assert.equal(after.ino, before.ino, "manifest inode changed");
    assert.equal(after.size, before.size, "manifest length changed");
    assert.equal(after.mtimeMs, before.mtimeMs, "manifest modification time changed");
    assert.equal(after.ctimeMs, before.ctimeMs, "manifest change time changed");
    return admit(bytes);
  } finally {
    closeSync(fd);
  }
}

function fixture(callback) {
  const root = mkdtempSync(join(tmpdir(), "desktop-member-governance-"));
  try {
    const trusted = join(root, "trusted");
    mkdirSync(trusted);
    const canonical = join(trusted, "integration.json");
    writeFileSync(canonical, Buffer.from(JSON.stringify(expected,null,2)+"\n"));
    callback({root,trusted,canonical});
  } finally {
    rmSync(root, {recursive:true, force:true});
  }
}

test("disk admission rejects a symbolic-link parent despite valid target bytes", () => {
  fixture(({root,trusted,canonical}) => {
    assert.deepEqual(readManifestFromDisk(canonical),
                     {verified:false,parity:false,execution:false,evidence:"metadata-only"});
    const alias = join(root, "aliased-dir");
    symlinkSync(trusted, alias, "dir");
    assert.throws(() => readManifestFromDisk(join(alias,"integration.json")),
                  /unsafe manifest parent/);
  });
});

test("disk admission refuses a symlink leaf, including a correct canonical target", () => {
  fixture(({root,canonical}) => {
    const alias = join(root, "linked.json");
    symlinkSync(canonical, alias);
    assert.throws(() => readManifestFromDisk(alias),
                  /unsafe manifest file/);
  });
});

test("disk admission refuses a hardlinked manifest with identical trusted bytes", () => {
  fixture(({root,canonical}) => {
    const alias = join(root, "hardlink.json");
    linkSync(canonical, alias);
    assert.throws(() => readManifestFromDisk(alias),
                  /unsafe manifest file/);
    assert.throws(() => readManifestFromDisk(canonical),
                  /unsafe manifest file/);
  });
});

test("disk admission rejects directories, empty files, oversized files and altered bytes", () => {
  fixture(({root,canonical}) => {
    assert.throws(() => readManifestFromDisk(root), /unsafe manifest file/);
    writeFileSync(canonical, Buffer.alloc(0));
    assert.throws(() => readManifestFromDisk(canonical), /invalid opened manifest/);
    writeFileSync(canonical, Buffer.alloc(65537, 0x20));
    assert.throws(() => readManifestFromDisk(canonical), /invalid opened manifest/);
    writeFileSync(canonical, Buffer.from(JSON.stringify({...expected, evidence:"verified"},null,2)+"\n"));
    assert.throws(() => readManifestFromDisk(canonical));
  });
});

test("disk-admitted canonical bytes never authorize an executed CI or guest claim", () => {
  fixture(({canonical}) => {
    const result = readManifestFromDisk(canonical);
    assert.deepEqual(result, {
      verified:false,parity:false,execution:false,evidence:"metadata-only"
    });
    for (const name of ["approved","ci_success","runner_id","guest","hardware"]) {
      assert.ok(!Object.hasOwn(result,name), "spurious field " + name);
    }
  });
});
