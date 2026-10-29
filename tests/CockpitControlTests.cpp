#include "../native/DriftCamera.cpp"
#include <stdexcept>
#include <string>
void Need(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
void WriteControl(const wchar_t* path,const char* text) {
    std::wstring temporary=std::wstring(path)+L".next";
    HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,0,nullptr);
    Need(file!=INVALID_HANDLE_VALUE,"Fixture create failed"); DWORD written=0;
    bool ok=WriteFile(file,text,static_cast<DWORD>(strlen(text)),&written,nullptr)!=FALSE; CloseHandle(file);
    Need(ok && written==strlen(text),"Fixture write failed");
    Need(MoveFileExW(temporary.c_str(),path,MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)!=FALSE,"Fixture replacement failed");
}
int wmain(int argc,wchar_t** argv) {
    try {
        TwinKartCamera::Settings value{};
        Need(TwinKartCamera::ParseSettings("1 -30 140 -10 90",&value) && !value.headAnchor,"Legacy seat format broken");
        Need(TwinKartCamera::ParseSettings("1 -30 140 -10 90 1 12 8\n",&value) && value.headAnchor==1 && value.eyeForward==12,"Head-anchor format rejected");
        for(const char* bad:{"1 -30 140 -10 90 1", "1 -30 140 -10 90 1 12 8 extra", "1 -30 140 -10 90 1 100 8", "2 -30 140 -10 90", "1 -30 140 -10 150 1 12 8"})
            Need(!TwinKartCamera::ParseSettings(bad,&value),"Malformed or out-of-range control accepted");
        wchar_t path[32768];
        if(argc==2) { Need(wcscpy_s(path,argv[1])==0,"Fixture path too long"); }
        else { swprintf_s(path,L"CockpitControlTests-%lu.control",GetCurrentProcessId()); WriteControl(path,"1 -30 140 -10 90 1 12 8\n"); }
        wcscpy_s(controlPath,path); ReadSettings();
        Need(enabled==1 && settings.headAnchor==1,"Managed/native initial control interoperability failed");
        auto revision=TwinKartCameraHistoryRevision();
        enabled=0; ReadSettings(); Need(enabled==0,"Unchanged file undid an F6 toggle");
        Need(TwinKartCameraHistoryRevision()==revision,"Unchanged file reset temporal history every poll");
        WriteControl(path,"1 -30 140 -10 90 1 12 8\n"); ReadSettings();
        Need(enabled==1,"Reapplying identical GUI settings did not override an F6 toggle");
        Need(TwinKartCameraHistoryRevision()>revision,"GUI camera mode change did not publish reset");
        revision=TwinKartCameraHistoryRevision();
        WriteControl(path,"1 -30 140 -10 90 1 12 8\n"); ReadSettings();
        Need(TwinKartCameraHistoryRevision()==revision,"Identical effective settings reset history unnecessarily");
        WriteControl(path,"0 -30 140 -10 90 1 12 8\n"); ReadSettings(); Need(enabled==0,"Explicit third-person control failed");
        WriteControl(path,"1 -30 140 -10 150 1 12 8\n"); ReadSettings(); Need(enabled==0 && settings.fov==90,"Invalid control partially applied");
        DeleteFileW(path);
        puts("PASS cockpit controls: managed-compatible head/seat settings, bounds, atomic identical reapplication after F6, persistent manual toggle, explicit third person and invalid-file rollback. No game started.");
        return 0;
    } catch(const std::exception& error) { fprintf(stderr,"%s\n",error.what()); return 1; }
}
