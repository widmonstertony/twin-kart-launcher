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
        MSG tap{};tap.message=WM_KEYDOWN;tap.wParam=VK_F6;
        Need(CameraKey(tap),"First F6 message lost");
        tap.lParam=1LL<<30;Need(!CameraKey(tap),"Held F6 repeats camera toggle");
        tap.lParam=0;tap.message=WM_KEYUP;Need(!CameraKey(tap),"F6 release toggles twice");
        tap.message=WM_KEYDOWN;tap.wParam=VK_ESCAPE;Need(!CameraKey(tap),"Camera consumes Escape");
        enabled=0;auto keyRevision=TwinKartCameraHistoryRevision();ToggleCamera();
        Need(enabled==1 && TwinKartCameraHistoryRevision()>keyRevision,"Message toggle did not reset temporal history");
        ToggleCamera();Need(enabled==0,"Second press cannot restore third person");
        TwinKartCamera::Settings value{};
        TwinKartCamera::Settings defaults{1,-30,140,-10,90,1,12,8};
        auto tall=TwinKartCamera::VehicleSettings(defaults,true);
        Need(tall.eyeForward==-40 && tall.eyeHeight==40 && tall.pitch==-5 && tall.fov==100,"Tall front still occludes default cockpit");
        auto ordinary=TwinKartCamera::VehicleSettings(defaults,false);
        Need(!memcmp(&ordinary,&defaults,sizeof(defaults)),"Vehicle preset leaked into ordinary kart");
        defaults.eyeForward=20;ordinary=TwinKartCamera::VehicleSettings(defaults,true);
        Need(!memcmp(&ordinary,&defaults,sizeof(defaults)),"Vehicle preset overwrote user's custom view");
        defaults.headAnchor=0;ordinary=TwinKartCamera::VehicleSettings(defaults,true);
        Need(!memcmp(&ordinary,&defaults,sizeof(defaults)),"Vehicle preset changed fixed-seat mode");
        TwinKartCamera::CockpitGate gate;
        TwinKartCamera::CockpitSample chase{-520,0,180,90,0};
        Need(!gate.Observe(chase,false) && !gate.baselineValid,"Start presentation hid the head before native BeginRace");
        Need(!gate.Observe(chase,true),"Cockpit entered before chase view was stable");
        Need(!gate.Observe(chase,true),"Cockpit entered after only two chase frames");
        Need(!gate.Observe(chase,true),"Cockpit entered after only three chase frames");
        Need(gate.Observe(chase,true),"Stable driving view did not enter cockpit");
        for(const auto& drift: {TwinKartCamera::CockpitSample{-500,-100,180,92,15},
            TwinKartCamera::CockpitSample{-450,-250,180,94,35},
            TwinKartCamera::CockpitSample{-350,-450,180,96,60},
            TwinKartCamera::CockpitSample{-250,-650,180,98,85}})
            Need(gate.Observe(drift,true) && gate.active,"Continuous drift camera fell back to third person");
        TwinKartCamera::CockpitSample itemHit{120,650,300,65,92};
        Need(!gate.Observe(itemHit,true) && !gate.active,"Item-hit cut kept the head suppressed");
        Need(!gate.Observe(itemHit,true) && !gate.active,"Stable item cinematic became a cockpit baseline");
        for(int frame=0;frame<8;++frame)Need(!gate.Observe(itemHit,true) && !gate.active,"Long item cinematic re-entered cockpit");
        TwinKartCamera::CockpitSample finish{-1700,900,700,55,-70};
        Need(!gate.Observe(finish,true) && !gate.active,"Finish panorama kept the head suppressed");
        Need(!gate.Observe(chase,true) && !gate.active,"Cockpit resumed on first post-cinematic frame");
        Need(!gate.Observe(chase,true) && !gate.active,"Cockpit resumed on second post-cinematic frame");
        Need(!gate.Observe(chase,true) && !gate.active,"Cockpit resumed on third post-cinematic frame");
        Need(gate.Observe(chase,true) && gate.active,"Stable driving view did not resume cockpit");
        Need(!gate.Observe(chase,false) && !gate.active,"F6 disable left cockpit active");
        Need(!gate.Observe(chase,true),"F6 re-enable bypassed stability delay");
        gate.Suspend();Need(!gate.active && gate.baselineValid,"Camera switch discarded the learned drive baseline");
        Need(!gate.Observe(itemHit,true),"Alternate camera switch relearned a cinematic");
        gate.Reset();Need(!gate.baselineValid && !gate.active,"Lifecycle reset retained a stale camera baseline");
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
        puts("PASS cockpit controls: managed-compatible settings, cinematic head restoration gate, delayed stable cockpit re-entry, F6/lifecycle reset, atomic reapplication and invalid-file rollback. No game started.");
        return 0;
    } catch(const std::exception& error) { fprintf(stderr,"%s\n",error.what()); return 1; }
}
