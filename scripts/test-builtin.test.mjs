// SPDX-License-Identifier: MIT
import {test} from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
test('builtin guard accepts exactly the 32-byte Wine DOS marker',()=>{
    const directory=fs.mkdtempSync(path.join(os.tmpdir(),'nexus-builtin-marker-'));
    try {
        const module=path.join(directory,'fixture.dll');
        const run=bytes=>{
            fs.writeFileSync(module,bytes);
            return spawnSync('cmake',['-DMODULE='+module,'-P',path.join(root,'cmake/CheckBuiltin.cmake')],{encoding:'utf8'});
        };
        const bytes=Buffer.alloc(128);
        bytes.write('Wine builtin DLL',64);
        assert.equal(run(bytes).status,0);
        const native=Buffer.alloc(128);
        assert.notEqual(run(native).status,0);
        const altered=Buffer.from(bytes);altered[95]=1;
        assert.notEqual(run(altered).status,0);
        assert.notEqual(run(bytes.subarray(0,95)).status,0);
        const shifted=Buffer.alloc(128);shifted.write('Wine builtin DLL',65);
        assert.notEqual(run(shifted).status,0);
    } finally {
        // Only this test's newly created synthetic fixture directory.
        fs.rmSync(directory,{recursive:true});
    }
});
test('builtin mode refuses implicit or missing winebuild tools',()=>{
    const directory=fs.mkdtempSync(path.join(os.tmpdir(),'nexus-builtin-config-'));
    try {
        const source='cmake_minimum_required(VERSION 3.20)\nproject(builtin_guard LANGUAGES NONE)\ninclude("'+path.join(root,'cmake/Builtin.cmake')+'")\n';
        fs.writeFileSync(path.join(directory,'CMakeLists.txt'),source);
        for(const [index,tool]of ['', 'winebuild', path.join(directory,'missing-winebuild')].entries()){
            const result=spawnSync('cmake',['-S',directory,'-B',path.join(directory,'build'+index),'-DNEXUS_WINE_BUILTIN=ON','-DNEXUS_WINEBUILD='+tool],{encoding:'utf8'});
            assert.notEqual(result.status,0);
            assert.match(result.stdout+result.stderr,/requires an explicit matching NEXUS_WINEBUILD/);
        }
        const off=spawnSync('cmake',['-S',directory,'-B',path.join(directory,'native')],{encoding:'utf8'});
        assert.equal(off.status,0,off.stdout+off.stderr);
    } finally {
        fs.rmSync(directory,{recursive:true});
    }
});
