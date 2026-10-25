/* SPDX-License-Identifier: MIT
 * Selected installation snapshots over private synthetic files only.
 * No account, game, network, cloud or cached availability. */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xpackage.h"
#include "xtaskqueue.h"
#include "xasyncprovider.h"

static LONG checks, failures;
static IXPackageImpl4 *package;
static WCHAR root[MAX_PATH];
static const char *identifier="12345678123456789abcdef012345678";
static const WCHAR *layout_name=L"layout_12345678-1234-5678-9abc-def012345678.xml";
static const char *config="<Game><Identity Name=\"Nexus.Selection\" Publisher=\"CN=Probe\" Version=\"1.2.3.4\"/><StoreId>B00000000001</StoreId></Game>";
static const char *layout="<Package><Features><Feature Id=\"Mapped\" Tags=\"base\"/></Features>"
    "<Chunk Id=\"Registration\"><FileGroup SourcePath=\".\" Include=\"MicrosoftGame.config\"/></Chunk>"
    "<Chunk Id=\"3\"><FileGroup SourcePath=\".\" Include=\"nx_common_probe.bin\"/></Chunk>"
    "<Chunk Id=\"7\" Languages=\"en-US\" Tags=\"base\" Features=\"Direct\"><FileGroup SourcePath=\".\" Include=\"nx_selected_probe.bin\"/></Chunk>"
    "<Chunk Id=\"9\" Languages=\"fr-FR\" Tags=\"other\" Features=\"Other\"><FileGroup SourcePath=\".\" Include=\"nx_unused_probe.bin\"/></Chunk>"
    "<Chunk Id=\"11\" Languages=\"en-US\" Tags=\"voice\" Features=\"Voice\"><FileGroup SourcePath=\".\" Include=\"nx_voice_probe.bin\"/></Chunk>"
    "<Chunk Id=\"13\" Languages=\"en-US\" Tags=\"base\" Features=\"Direct\"><FileGroup SourcePath=\".\" Include=\"NX_SELECTED_PROBE.bin\"/></Chunk>"
    "<Chunk Id=\"15\" Languages=\"en-GB\" Tags=\"optional\" Features=\"Optional\"><FileGroup SourcePath=\".\" Include=\"nx_missing_probe.bin\"/></Chunk></Package>";
#define CHECK(name,expr) do { InterlockedIncrement(&checks); if(!(expr)){InterlockedIncrement(&failures);printf("FAIL %s line=%d\n",name,__LINE__);} } while(0)
static void full_path(const WCHAR *name,WCHAR *out){swprintf(out,MAX_PATH,L"%ls\\%ls",root,name);}
static void write_file(const WCHAR *name,const char *text)
{
    WCHAR path[MAX_PATH];DWORD written;HANDLE file;full_path(name,path);
    file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    CHECK("fixture-open",file!=INVALID_HANDLE_VALUE);if(file==INVALID_HANDLE_VALUE)return;
    CHECK("fixture-write",WriteFile(file,text,strlen(text),&written,NULL)&&written==strlen(text));CloseHandle(file);
}
static XPackageInstallationMonitorHandle create(UINT count,XPackageChunkSelector *selectors)
{
    XPackageInstallationMonitorHandle result=NULL;
    CHECK("create",IXPackageImpl4_XPackageCreateInstallationMonitor(package,identifier,count,selectors,0,NULL,&result)==S_OK&&result);
    return result;
}
static void close_monitor(XPackageInstallationMonitorHandle m){IXPackageImpl4_XPackageCloseInstallationMonitorHandle(package,m);}
static void progress(XPackageInstallationMonitorHandle m,BOOL ready,UINT64 bytes)
{
    XPackageInstallationProgress p;memset(&p,0xcc,sizeof(p));IXPackageImpl4_XPackageGetInstallationProgress(package,m,&p);
    CHECK("completed",p.completed==ready);CHECK("launchable",p.launchable==ready);
    CHECK("total",p.totalBytes==(ready?bytes:~(UINT64)0));
    CHECK("installed",p.installedBytes==(ready?bytes:0));CHECK("launch-bytes",p.launchBytes==(ready?bytes:0));
}
static void selection(XPackageChunkSelectorType type,const char *value,UINT id,BOOL ready,UINT64 bytes)
{
    XPackageChunkSelector s={0};XPackageInstallationMonitorHandle m;s.type=type;
    if(type==XPackageChunkSelectorType_Chunk)s.chunkId=id;else s.language=value;
    m=create(1,&s);progress(m,ready,bytes);close_monitor(m);
}
struct worker_state{XPackageInstallationMonitorHandle monitor;UINT64 bytes;};
static DWORD WINAPI worker(void *arg)
{
    struct worker_state *s=arg;
    for(unsigned i=0;i<12;i++){
        CHECK("concurrent-refresh",IXPackageImpl4_XPackageUpdateInstallationMonitor(package,s->monitor));
        progress(s->monitor,TRUE,s->bytes);
    }
    return 0;
}
static void invalid_unselected(const char *bad)
{
    XPackageChunkSelector s={0};XPackageInstallationMonitorHandle m=(void *)1;
    s.type=XPackageChunkSelectorType_Chunk;s.chunkId=7;write_file(layout_name,bad);
    CHECK("invalid-unselected-metadata-rejected",FAILED(IXPackageImpl4_XPackageCreateInstallationMonitor(package,identifier,1,&s,0,NULL,&m))&&!m);
    write_file(layout_name,layout);
}
static void contracts(void)
{
    UINT64 reg=strlen(config);XPackageInstallationMonitorHandle m;XPackageChunkSelector s[2]={{0}};
    WCHAR path[MAX_PATH];HANDLE locked;
    selection(XPackageChunkSelectorType_Chunk,NULL,7,TRUE,reg+9);
    selection(XPackageChunkSelectorType_Chunk,NULL,3,TRUE,reg+5);
    selection(XPackageChunkSelectorType_Language,"EN-us",0,TRUE,reg+5+9+5);
    selection(XPackageChunkSelectorType_Tag,"base",0,TRUE,reg+5+9);
    selection(XPackageChunkSelectorType_Feature,"Mapped",0,TRUE,reg+5+9);
    selection(XPackageChunkSelectorType_Feature,"Direct",0,TRUE,reg+5+9);
    selection(XPackageChunkSelectorType_Chunk,NULL,999,FALSE,0);
    selection(XPackageChunkSelectorType_Language,"xx-XX",0,FALSE,0);
    selection(XPackageChunkSelectorType_Feature,"Unknown",0,FALSE,0);
    selection(XPackageChunkSelectorType_Language,"en-GB",0,FALSE,0);
    s[0].type=s[1].type=XPackageChunkSelectorType_Chunk;s[0].chunkId=7;s[1].chunkId=9;
    m=create(2,s);progress(m,TRUE,reg+9+6);close_monitor(m);
    m=create(0,NULL);progress(m,FALSE,0);close_monitor(m);
    /* An unrelated unreadable file cannot alter the selected result. */
    full_path(L"nx_unused_probe.bin",path);locked=CreateFileW(path,GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
    CHECK("unselected-lock",locked!=INVALID_HANDLE_VALUE);selection(XPackageChunkSelectorType_Chunk,NULL,7,TRUE,reg+9);
    if(locked!=INVALID_HANDLE_VALUE)CloseHandle(locked);
    m=create(1,s);progress(m,TRUE,reg+9);
    full_path(L"nx_selected_probe.bin",path);locked=CreateFileW(path,GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
    CHECK("selected-lock",locked!=INVALID_HANDLE_VALUE);
    CHECK("selected-locked-refresh",IXPackageImpl4_XPackageUpdateInstallationMonitor(package,m));progress(m,FALSE,0);
    if(locked!=INVALID_HANDLE_VALUE)CloseHandle(locked);
    CHECK("selected-unlocked-refresh",IXPackageImpl4_XPackageUpdateInstallationMonitor(package,m));progress(m,TRUE,reg+9);
    CHECK("delete-selected",DeleteFileW(path));CHECK("deleted-refresh",IXPackageImpl4_XPackageUpdateInstallationMonitor(package,m));progress(m,FALSE,0);
    write_file(L"nx_selected_probe.bin","replacement");CHECK("replace-refresh",IXPackageImpl4_XPackageUpdateInstallationMonitor(package,m));progress(m,TRUE,reg+11);
    write_file(L"nx_selected_probe.bin","synthetic");CHECK("resize-refresh",IXPackageImpl4_XPackageUpdateInstallationMonitor(package,m));progress(m,TRUE,reg+9);
    write_file(layout_name,"not metadata");CHECK("invalid-refresh",!IXPackageImpl4_XPackageUpdateInstallationMonitor(package,m));progress(m,FALSE,0);
    write_file(layout_name,layout);CHECK("restore-refresh",IXPackageImpl4_XPackageUpdateInstallationMonitor(package,m));progress(m,TRUE,reg+9);
    {
        HANDLE threads[8];struct worker_state state={m,reg+9};unsigned created=0;
        for(unsigned i=0;i<8;i++){threads[i]=CreateThread(NULL,0,worker,&state,0,NULL);CHECK("worker-start",threads[i]!=NULL);if(!threads[i])break;created++;}
        CHECK("all-workers-started",created==8);
        CHECK("workers-complete",created&&WaitForMultipleObjects(created,threads,TRUE,15000)==WAIT_OBJECT_0);
        for(unsigned i=0;i<created;i++)CloseHandle(threads[i]);
    }
    close_monitor(m);
    invalid_unselected("<Package><Chunk Id=\"7\"/><Chunk Id=\"9\"><FileGroup SourcePath=\".\" Include=\"../outside\"/></Chunk></Package>");
    invalid_unselected("<Package><Chunk Id=\"7\"/><Chunk Id=\"9\"><FileGroup SourcePath=\".\" Include=\"file\" Exclude=\"other\"/></Chunk></Package>");
    invalid_unselected("<Package><Chunk Id=\"7\"/><Chunk Id=\"9\" Languages=\"en-US;\"/></Package>");
    invalid_unselected("<Package><Chunk Id=\"7\"/><Chunk Id=\"9\"/><Chunk Id=\"9\"/></Package>");
    invalid_unselected("<Package><Features><Feature Id=\"invalid;feature\" Tags=\"other\"/></Features><Chunk Id=\"7\"/></Package>");
}
static void run_child(const char *mode)
{
    HMODULE dll=LoadLibraryW(L"xgameruntime.dll");HRESULT(WINAPI *query)(const GUID*,REFIID,void**)=dll?(void*)GetProcAddress(dll,"QueryApiImpl"):NULL;
    CHECK("query",query!=NULL);if(!query)return;
    CHECK("package",query(&CLSID_XPackageImpl,&IID_IXPackageImpl4,(void**)&package)==S_OK&&package);if(!package)return;
    GetModuleFileNameW(NULL,root,MAX_PATH);*wcsrchr(root,'\\')=0;
    if(!strcmp(mode,"--trace-select"))selection(XPackageChunkSelectorType_Chunk,NULL,7,TRUE,strlen(config)+9);
    else if(!strcmp(mode,"--trace-all")){XPackageInstallationMonitorHandle m=create(0,NULL);progress(m,FALSE,0);close_monitor(m);}
    else contracts();
    IXPackageImpl4_Release(package);FreeLibrary(dll);
}
int main(int argc,char **argv)
{
    if(argc==2&&strcmp(argv[1],"--prepare"))run_child(argv[1]);
    else{
        WCHAR temp[MAX_PATH],self[MAX_PATH],exe[MAX_PATH],command[MAX_PATH+30];STARTUPINFOW start={0};PROCESS_INFORMATION child={0};DWORD code=99;start.cb=sizeof(start);
        CHECK("temp",GetTempPathW(MAX_PATH,temp));swprintf(root,MAX_PATH,L"%lsnexus-selection-%lu-%lu",temp,GetCurrentProcessId(),GetTickCount());CHECK("private-root",CreateDirectoryW(root,NULL));
        write_file(L"MicrosoftGame.config",config);write_file(L"appxmanifest.xml","<Package><Identity Name=\"Nexus.Selection\" Publisher=\"CN=Probe\" Version=\"1.2.3.4\"/><Applications/></Package>");
        write_file(layout_name,layout);write_file(L"nx_common_probe.bin","share");write_file(L"nx_selected_probe.bin","synthetic");write_file(L"nx_unused_probe.bin","unused");write_file(L"nx_voice_probe.bin","voice");
        CHECK("self",GetModuleFileNameW(NULL,self,MAX_PATH));full_path(L"probe.exe",exe);CHECK("copy",CopyFileW(self,exe,TRUE));
        if(argc==2){printf("SELECTION FIXTURE %ls\n",exe);}
        else{swprintf(command,MAX_PATH+30,L"\"%ls\" --child",exe);CHECK("child",CreateProcessW(exe,command,NULL,NULL,FALSE,0,NULL,NULL,&start,&child));
            if(child.hProcess){DWORD wait=WaitForSingleObject(child.hProcess,30000);CHECK("child-finished",wait==WAIT_OBJECT_0);if(wait!=WAIT_OBJECT_0){TerminateProcess(child.hProcess,77);WaitForSingleObject(child.hProcess,1000);}GetExitCodeProcess(child.hProcess,&code);CHECK("child-result",code==0);CloseHandle(child.hProcess);CloseHandle(child.hThread);}}
    }
    printf("CHECKS %ld FAILURES %ld\n",checks,failures);return failures?1:0;
}
