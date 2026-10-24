// SPDX-License-Identifier: MIT
import {test} from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {officialUbuntuMirrors} from './ci/ubuntu-mirror.mjs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const read=name=>fs.readFileSync(path.join(root,name),'utf8');
const contract=JSON.parse(read('tests/Nexus/contracts.lock.json'));
test('all original raw migration assertions remain byte-identical',()=>{
    assert.equal(Object.keys(contract.probes).length,6);
    for(const [name,expected] of Object.entries(contract.probes))
        assert.equal(crypto.createHash('sha256').update(read('tests/Nexus/'+name)).digest('hex'),expected,name);
});
test('all upstream threading declarations remain present, including the 42 active cases',()=>{
    const names=[...read('tests/System/test_xthreading.cpp').matchAll(/TEST_F\(XThreadingTests,\s*(\w+)\)/g)].map(m=>m[1]);
    assert.equal(contract.upstreamDeclarations.length,45);
    assert.deepEqual(names,contract.upstreamDeclarations);
    assert.doesNotMatch(read('tests/System/test_xthreading.cpp'),/DISABLED_/);
});
test('both ABIs use explicit compiler and IDL inputs without permissive flags',()=>{
    assert.match(read('win32-target.cmake'),/i686-w64-mingw32-g\+\+/);
    assert.match(read('win64-target.cmake'),/x86_64-w64-mingw32-g\+\+/);
    assert.match(read('CMakeLists.txt'),/WIDL_ARCH -m32/);
    assert.match(read('CMakeLists.txt'),/WIDL_ARCH -m64/);
    assert.doesNotMatch(read('CMakeLists.txt')+read('src/CMakeLists.txt'),/-fpermissive|-Wno-error|--enable-stdcall-fixup/);
    assert.match(read('src/xgameruntime-x86.def'),/QueryApiImpl=QueryApiImpl@12/);
});
test('component CI cannot package, publish or imply complete runtime validation',()=>{
    const workflow=read('.github/workflows/test.yml');
    assert.match(workflow,/arch: \[win64, win32\]/);
    assert.match(workflow,/-DNEXUS_MIGRATION_PROBES=ON/);
    assert.match(workflow,/scripts\/test-component\.mjs/);
    assert.doesNotMatch(workflow,/gh release|build-proton|package-candidate|release-validation|secrets\./);
    assert.match(read('scripts/test-component.mjs'),/completeRuntime: false/);
    assert.match(read('scripts/test-component.mjs'),/realGameTest: false/);
    assert.match(read('scripts/test-component.mjs'),/stableApproved: false/);
    assert.match(read('tests/CMakeLists.txt'),/NEXUS_MIGRATION_PROBES[^\n]+ON/);
});
test('mirror configuration retains security origin, suites and signing keys',()=>{
    const source='URIs: http://azure.archive.ubuntu.com/ubuntu\nSuites: noble noble-updates\nSigned-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\nhttp://security.ubuntu.com/ubuntu https://example.org/apt';
    const value=officialUbuntuMirrors(source);
    assert.ok(value.includes('https://archive.ubuntu.com/ubuntu'));
    assert.ok(value.includes('https://security.ubuntu.com/ubuntu'));
    assert.ok(value.includes('Suites: noble noble-updates'));
    assert.ok(value.includes('Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg'));
    assert.ok(value.includes('https://example.org/apt'));
});

test('configuration retains the original initialization ABI fixture',()=>{
    for(const [name,expected] of Object.entries(contract.configurationProbes))
        assert.equal(crypto.createHash('sha256').update(read('tests/Nexus/'+name)).digest('hex'),expected,name);
    for(const mode of ['inline','retry','default']) assert.ok(read('scripts/test-component.mjs').includes("'"+mode+"'"));
});
test('XML parser inputs are pinned and checked without fallback or external entity loading',()=>{
    const lock=JSON.parse(read('cmake/xml2.lock.json'));
    assert.equal(lock.commit,'7fc704ef49c50e9702c9983a5250d5738697bb04');
    assert.ok(Object.keys(lock.files).length>70);
    assert.match(read('cmake/Xml2.cmake'),/file\(SHA256/);
    assert.match(read('cmake/Xml2.cmake'),/FATAL_ERROR "Pinned XML source mismatch/);
    assert.match(read('src/Runtime/Xml.cpp'),/XML_PARSE_NONET/);
    assert.match(read('src/Runtime/Xml.cpp'),/XML_PARSER_LOADDTD, 0/);
    assert.match(read('src/Runtime/Xml.cpp'),/XML_PARSER_SUBST_ENTITIES, 0/);
    assert.doesNotMatch(read('src/Runtime/Xml.cpp'),/XML_PARSE_NOENT|XML_PARSE_DTDLOAD/);
});

test('authentication compiles pinned WinRT declarations and never exports transport injection',()=>{
    const lock=JSON.parse(read('cmake/winrt.lock.json'));
    assert.equal(lock.commit,'7fc704ef49c50e9702c9983a5250d5738697bb04');
    assert.equal(Object.keys(lock.files).length,13);
    assert.match(read('cmake/Winrt.cmake'),/file\(SHA256/);
    assert.match(read('cmake/Winrt.cmake'),/FATAL_ERROR "Pinned WinRT source mismatch/);
    assert.doesNotMatch(read('src/xgameruntime.def')+read('src/xgameruntime-x86.def'),/Transport|Synthetic|TestHook/);
    assert.match(read('scripts/test-authentication.mjs'),/Number\(match\[1\]\)>=19/);
    assert.match(read('scripts/test-authentication.mjs'),/realAccounts:false,realCloud:false,fullRuntime:false/);
    assert.match(read('tests/Nexus/test_user.cpp'),/There is deliberately no network fallback/);
});

test('delayed callbacks use actual monotonic Windows uptime in every SDK',()=>{
    assert.match(read('src/System/Threading/WaitTimer.cpp'),/GetTickCount64\(\)/);
    assert.doesNotMatch(read('src/System/Threading/WaitTimer.cpp'),/std::chrono::steady_clock|gettimeofday\(/);
    assert.match(read('scripts/test-component.mjs'),/test_nexus_timer\.exe/);
    assert.match(read('tests/Nexus/test_timer.cpp'),/EXPECT_GE\( state.fired, due \)/);
});
