/* SPDX-License-Identifier: MIT
 * Account-free launch resolution. All executables/files are this fixture's
 * own copies in its fresh private prefix. Never starts a game or signs in.
 */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "xgame.h"
static unsigned checks, failures;
#define CHECK(name,x) do { ++checks; if (!(x)) { ++failures; printf("FAIL %s line=%d\n",name,__LINE__); } } while(0)
static void join(WCHAR *out,const WCHAR *root,const WCHAR *leaf)
{
    wcscpy(out,root);wcscat(out,L"\\");wcscat(out,leaf);
}
static int target(int argc,WCHAR **argv)
{
    WCHAR expected[32768],actual[32768],cwd[32768],file[32768];
    BOOL okay=argc==3&&!wcscmp(argv[2],L"two private words");
    GetModuleFileNameW(NULL,actual,ARRAYSIZE(actual));
    okay=okay&&GetEnvironmentVariableW(L"NEXUS_SEARCH_EXPECTED",expected,ARRAYSIZE(expected))&&!wcsicmp(actual,expected);
    GetCurrentDirectoryW(ARRAYSIZE(cwd),cwd);
    okay=okay&&GetEnvironmentVariableW(L"NEXUS_SEARCH_CWD",expected,ARRAYSIZE(expected))&&!wcsicmp(cwd,expected);
    if(!GetEnvironmentVariableW(L"NEXUS_SEARCH_RESULT",file,ARRAYSIZE(file)))return 90;
    HANDLE h=CreateFileW(file,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);DWORD written=0;char value=okay?'1':'0';
    if(h==INVALID_HANDLE_VALUE)return 91;
    WriteFile(h,&value,1,&written,NULL);CloseHandle(h);return okay&&written==1?0:92;
}
static void launch(const WCHAR *path)
{
    HMODULE dll=LoadLibraryW(L"xgameruntime.dll");
    HRESULT (WINAPI *query)(REFCLSID,REFIID,void **)=dll?(void*)GetProcAddress(dll,"QueryApiImpl"):NULL;
    IXGameImpl2 *game=NULL;char utf8[32768];
    if(!query||FAILED(query(&CLSID_XGameImpl,&IID_IXGameImpl2,(void**)&game))||!game)ExitProcess(93);
    if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,path,-1,utf8,sizeof(utf8),NULL,NULL))ExitProcess(94);
    IXGameImpl2_XLaunchNewGame(game,utf8,"--target \"two private words\"",NULL);
    ExitProcess(77);
}
int wmain(int argc,WCHAR **argv)
{
    static const WCHAR leaf[]=L"sibling \x010d.exe";
    WCHAR self[32768],temp[32768],root[32768],folder[32768],app[32768],cwd[32768],pathdir[32768];
    WCHAR caller[32768],sibling[32768],current[32768],other[32768],result[32768],command[32768],requested[32768],expected[32768];
    DWORD status,wait;
    if(argc>1&&!wcscmp(argv[1],L"--target"))return target(argc,argv);
    if(argc==3&&!wcscmp(argv[1],L"--launch")){launch(argv[2]);return 78;}
    CHECK("self",GetModuleFileNameW(NULL,self,ARRAYSIZE(self))>0);
    CHECK("temporary-root",GetTempPathW(ARRAYSIZE(temp),temp)>0);
    CHECK("unique-root",GetTempFileNameW(temp,L"nxs",0,root)!=0);CHECK("remove-root-file",DeleteFileW(root));
    CHECK("create-private-root",CreateDirectoryW(root,NULL));
    for(unsigned kind=0;kind<8;kind++)
    {
        STARTUPINFOW si={sizeof(si)};PROCESS_INFORMATION pi;
        WCHAR number[12];_snwprintf(number,ARRAYSIZE(number),L"%u",kind);join(folder,root,number);
        CHECK("case-directory",CreateDirectoryW(folder,NULL));
        join(app,folder,L"application");join(cwd,folder,L"working");join(pathdir,folder,L"path-only");
        CHECK("app-directory",CreateDirectoryW(app,NULL));CHECK("working-directory",CreateDirectoryW(cwd,NULL));
        CHECK("path-directory",CreateDirectoryW(pathdir,NULL));
        join(caller,app,L"caller.exe");CHECK("caller-copy",CopyFileW(self,caller,TRUE));
        join(sibling,app,leaf);join(current,cwd,leaf);join(other,pathdir,leaf);join(result,folder,L"result.txt");
        wcscpy(requested,leaf);wcscpy(expected,sibling);
        if(kind!=5)CHECK("sibling-copy",CopyFileW(self,sibling,TRUE));
        if(kind==1){CHECK("current-copy",CopyFileW(self,current,TRUE));wcscpy(expected,current);}
        if(kind==2||kind==3)
        {
            WCHAR sub[32768];join(sub,app,kind==2?L"sub":L"missing");CHECK("app-subdirectory",CreateDirectoryW(sub,NULL));
            join(sibling,sub,leaf);CHECK("app-qualified-copy",CopyFileW(self,sibling,TRUE));
            if(kind==2){join(sub,cwd,L"sub");CHECK("cwd-subdirectory",CreateDirectoryW(sub,NULL));join(current,sub,leaf);CHECK("cwd-qualified-copy",CopyFileW(self,current,TRUE));wcscpy(expected,current);}
            join(requested,kind==2?L"sub":L"missing",leaf);
        }
        if(kind==4||kind==5){CHECK("other-copy",CopyFileW(self,other,TRUE));if(kind==4){wcscpy(requested,other);wcscpy(expected,other);}}
        if(kind==6)
        {
            HANDLE h=CreateFileW(current,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);DWORD written;
            CHECK("invalid-image-created",h!=INVALID_HANDLE_VALUE);if(h!=INVALID_HANDLE_VALUE){CHECK("invalid-image-written",WriteFile(h,"invalid",7,&written,NULL)&&written==7);CloseHandle(h);}
        }
        if(kind==7)CHECK("directory-not-executable",CreateDirectoryW(current,NULL));
        SetEnvironmentVariableW(L"NEXUS_SEARCH_EXPECTED",expected);SetEnvironmentVariableW(L"NEXUS_SEARCH_CWD",cwd);
        SetEnvironmentVariableW(L"NEXUS_SEARCH_RESULT",result);
        /* Only this child sees the synthetic PATH. It must never be searched. */
        WCHAR oldpath[32768];DWORD oldsize=GetEnvironmentVariableW(L"PATH",oldpath,ARRAYSIZE(oldpath));
        if(kind==5)SetEnvironmentVariableW(L"PATH",pathdir);
        wcscpy(command,L"\"");wcscat(command,caller);wcscat(command,L"\" --launch \"");
        wcscat(command,requested);wcscat(command,L"\"");
        BOOL started=CreateProcessW(caller,command,NULL,NULL,FALSE,0,NULL,cwd,&si,&pi);
        if(kind==5)SetEnvironmentVariableW(L"PATH",oldsize&&oldsize<ARRAYSIZE(oldpath)?oldpath:NULL);
        CHECK("caller-started",started);if(!started)continue;
        wait=WaitForSingleObject(pi.hProcess,20000);CHECK("caller-finished",wait==WAIT_OBJECT_0);
        if(wait!=WAIT_OBJECT_0){TerminateProcess(pi.hProcess,99);WaitForSingleObject(pi.hProcess,2000);}
        status=999;CHECK("caller-status-readable",GetExitCodeProcess(pi.hProcess,&status));CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
        BOOL success=kind==0||kind==1||kind==2||kind==4;
        CHECK("caller-never-returns",status!=77&&status!=78&&status!=93&&status!=94);
        CHECK("correct-launch-result",status==(success?0:1));
        if(success)
        {
            HANDLE h=INVALID_HANDLE_VALUE;DWORD read=0;char value='0';
            for(unsigned attempt=0;attempt<200;attempt++){h=CreateFileW(result,GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);if(h!=INVALID_HANDLE_VALUE)break;Sleep(25);}
            CHECK("target-reported",h!=INVALID_HANDLE_VALUE);if(h!=INVALID_HANDLE_VALUE){ReadFile(h,&value,1,&read,NULL);CloseHandle(h);}
            CHECK("exact-target-cwd-and-arguments",read==1&&value=='1');
        }
        else CHECK("no-unintended-target",GetFileAttributesW(result)==INVALID_FILE_ATTRIBUTES);
    }
    printf("CHECKS %u FAILURES %u\n",checks,failures);return failures?1:0;
}
