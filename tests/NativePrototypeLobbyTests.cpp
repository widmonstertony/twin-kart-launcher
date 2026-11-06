#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "../native/NativePrototypeLobby.h"
#include "../native/NativeObjectiveCapture.h"
#include "../native/NativeEventMenu.h"
#include "../native/PrototypeSettlement.h"
#include "../native/PrototypeResultPanel.h"
namespace O=TwinKartObjective;
namespace R=TwinKartRace;
namespace {
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int calls=0,nativeSize=16;
int __fastcall Team(const R::PreparationMode*){++calls;return nativeSize;}
int type=1,outer=2,object=3;unsigned constructed=0;bool badInitializer=false;
void* __fastcall Initialize(O::AuthoredObjectParameters* p,void* cls){*p={};p->type=cls;if(badInitializer)p->name=1;return p;}
void* __fastcall Construct(O::AuthoredObjectParameters* p){
    Need(p->type==&type && p->outer==&outer && p->flags==0xc0 && !p->name && !p->internalFlags &&
         !p->objectTemplate && !p->instanceGraph && !p->externalPackage,"Owned descriptor construction boundary");
    ++constructed;return &object;
}
void TeamChecks(){
    O::AuthoredLobbyDefinition definition;
    Need(O::PrototypeLobbyDefinition(100,definition) && definition.teamSize==4 && definition.teamCount==1 &&
         !wcscmp(definition.gameClass,L"/Script/KartDrift.TobyRushGameMode") && !wcscmp(definition.travel,L"TobyRush"),"Factory native frontend contract differs");
    Need(O::PrototypeLobbyDefinition(109,definition) && definition.teamSize==16 && definition.teamCount==1 &&
         !wcscmp(definition.gameClass,L"/Script/KartDrift.HideNSeekGameMode") && !wcscmp(definition.travel,L"HideNSeek"),"Hide native frontend contract differs");
    Need(!O::PrototypeLobbyDefinition(108,definition) && !O::PrototypeLobbyDefinition(4,definition),"Authored descriptor overrode a retained/ordinary mode");
    R::PreparationMode m{};
    for(int mode=0;mode<256;++mode)for(int flags=0;flags<8;++flags){
        m.mode=static_cast<unsigned char>(mode);calls=0;nativeSize=16;
        bool owner=(flags&1)!=0,chaser=(flags&2)!=0,hide=(flags&4)!=0;
        int expected=owner && ((mode==108 && chaser) || (mode==109 && hide))?1:16;
        Need(O::PrototypeLobbyTeamSize(&m,owner,chaser,hide,Team)==expected && calls==1,"Menu bucket scope or forwarding");
    }
    m.mode=109;wchar_t profile[]=L"other";m.profile={profile,6,6};
    Need(O::PrototypeLobbyTeamSize(&m,true,true,true,Team)==16,"Explicit profile changed");
    wchar_t empty[]=L"";m.profile={empty,1,1};
    Need(O::PrototypeLobbyTeamSize(&m,true,false,true,Team)==1,"Native empty string rejected");
    m.profile={nullptr,1,1};Need(O::PrototypeLobbyTeamSize(&m,true,true,true,Team)==16,"Malformed string admitted");
    m.profile={};nativeSize=4;Need(O::PrototypeLobbyTeamSize(&m,true,true,true,Team)==4,"Nonprototype native size changed");
    Need(O::PrototypeLobbyTeamSize(nullptr,true,true,true,Team)==4,"Null mode forwarding changed");
    Need(O::PrototypeLobbyTeamSize(&m,true,true,true,nullptr)==0,"Missing native lookup admitted");
    std::puts("PASS prototype menu: all enum/gate combinations, original once, profile and capacity guards");
}
void CommandChecks(){
    R::Request r;const char* good="TKHIDE_REPAIRED1 0123456789abcdef0123456789abcdef\n";
    Need(O::ParseHideRepairedStart(good,&r) && r.mode==109 && r.ai==7 && r.laps==0 &&
         !strcmp(r.track,"HideNSeekTestLevel") && !r.profile[0] && !r.custom[0],"Normal exact Hide command");
    wchar_t options[256]{};Need(O::HideCaptureOptions(r,options,256) &&
        !wcscmp(options,L"game=/Script/KartDrift.HideNSeekGameMode?players=8?ais=0"),"Native owned-world options");
    for(auto text:{"TKHIDE_REPAIRED1 0123456789abcdef0123456789abcdef other", "TKHIDE_REPAIRED1 abc",
        "TKHIDE_REPAIRED1 z123456789abcdef0123456789abcdef", "TKHIDE_AI_CAPTURE1 0123456789abcdef0123456789abcdef"})
        Need(!O::ParseHideRepairedStart(text,&r),"Nonexact normal Hide command admitted");
    Need(!O::ParseHideRepairedStart(nullptr,&r) && !O::ParseHideRepairedStart(good,nullptr),"Null command admitted");
    std::puts("PASS normal Hide command: fixed world/population/options, malformed and diagnostic requests rejected");
}
void MenuChecks(){
    size_t profiles=0,localCount=0;R::EventProfiles(profiles);const int* local=R::LocalLobbyModes(localCount);size_t base=profiles+localCount;
    for(unsigned flags=0;flags<8;++flags){
        bool factory=(flags&1)!=0,chaser=(flags&2)!=0,hide=(flags&4)!=0;
        R::EventMenuCopySource menu;Need(menu.Build(&object,factory,chaser,hide),"Menu source build failed");
        Need(menu.source.rows.count==static_cast<int>(base+factory+chaser+hide),"Optional menu row count");
        unsigned seen=0;
        for(int i=0;i<menu.source.rows.count;++i){
            const auto& row=menu.source.rows.data[i];const auto& mode=row.modes.data[0];
            Need(row.index==i+1 && row.modes.count==1,"Native menu row shape");
            const int localIndex=R::EventMenuLocalModeIndex(i,profiles,base);
            if(localIndex>=0) {
                Need(mode.mode==local[localIndex] && mode.profile.count==1 && !mode.profile.data[0],"Missing/wrong native default lobby mode");
                auto id=L"TwinKart_LocalMode_"+std::to_wstring(mode.mode);
                Need(!wcscmp(row.id.data,id.c_str()),"Default mode identity differs from backend schedule");
            }
            if(mode.mode==100)seen|=1;
            if(mode.mode==108)seen|=2;
            if(mode.mode==109){seen|=4;Need(row.noParty==1 && !wcscmp(row.id.data,L"TwinKart_Hide_Repaired") &&
                mode.profile.count==1 && !mode.profile.data[0],"Exact repaired Hide menu metadata");}
            for(int j=0;j<i;++j)Need(wcscmp(row.id.data,menu.source.rows.data[j].id.data)!=0,"Duplicate menu identity");
        }
        Need(seen==flags,"Optional menu indexing changed other modes");
    }
    std::puts("PASS native event rows: all Factory/Chaser/Hide combinations and unique owned metadata");
}
void ResultChecks(){
    O::PrototypeSettlementMailbox results;O::PrototypeSettlement value{0x1234,22,3,2},read;
    auto invalid=value;invalid.winner=5;
    Need(!results.Publish(invalid) && !results.Take(read),"Undecided native result published");
    invalid=value;invalid.serial=0;Need(!results.Publish(invalid),"Unretained result published");
    Need(results.Publish(value) && !results.Publish(value) && results.Take(read) && read.winner==2 && read.SameWorld(value),"Decided original result lost");
    Need(!results.Take(read) && !results.Publish(value),"Result shown twice");
    auto other=value;other.winner=1;
    Need(!results.Restore(other) && results.Restore(value) && !results.Restore(value) && results.Take(read),"Failed UI construction did not restore the exact result once");
    invalid=value;invalid.serial=4;invalid.winner=1;
    Need(results.Publish(invalid) && results.Take(read) && read.serial==4 && read.winner==1,"Recycled address retained an earlier result");
    for(unsigned winner=0;winner<5;++winner)Need(O::PrototypeWinnerText(winner)!=nullptr,"Original team result has no display name");
    Need(!O::PrototypeWinnerText(5),"None displayed as winner");
    results.Reset();Need(results.Publish(value) && results.Take(read),"Next session lost its result");
    std::puts("PASS compatibility settlement: actual decided native enum only, once per retained world, recycled serial and reset");
    O::HideSettlementJournal hide;O::HideMatchSummary summary;
    Need(!hide.Record(0x1234,22,3,2,0,false,false),"Hide cancellation became a completed round");
    Need(hide.Record(0x1234,22,3,2,0,true,false),"Native timed round was lost");
    Need(!hide.Record(0x1234,22,3,2,0,false,true),"Native round was overwritten");
    Need(!hide.Finish(0x1234,22,3,0x80000001u,2,2,summary),"Incomplete Hide result published");
    Need(!hide.Record(0x1234,22,4,2,1,false,true),"Recycled rule/world joined an old match");
    Need(hide.Record(0x1234,22,3,2,1,false,true),"Native no-hiders round was lost");
    Need(!hide.Finish(0x1234,22,3,0x80000002u,2,2,summary) && !hide.Finish(0x1234,22,3,0x80000001u,1,2,summary),"Hide result before native program terminal state");
    Need(hide.Finish(0x1234,22,3,0x80000001u,2,2,summary) && summary.Valid() && summary.outcomes[0]==1 && summary.outcomes[1]==2,"Native per-round conditions changed");
    Need(!hide.Finish(0x1234,22,3,0x80000001u,2,2,summary),"Duplicate Hide match result");
    hide.Reset();Need(hide.Record(0x1234,22,4,1,0,true,true) && hide.Finish(0x1234,22,4,0x80000001u,1,1,summary) && summary.outcomes[0]==2,"Reset or native simultaneous end conditions changed");
    std::puts("PASS Hide settlement: complete original rounds plus native terminal key/counter, no elapsed-time inference or invented aggregate winner");
}
void PresentationLifecycleChecks(){
    using Outcome=O::PrototypeReturnOutcome;
    O::PrototypePresentationState state;O::PrototypePresentationWorld world{0x1234,22,3,108};
    O::PrototypeSettlementMailbox mailbox;O::PrototypeSettlement result{world.world,world.index,world.serial,2},read;
    Need(!state.Qualify(world).generation && state.Observe(world),"Unobserved result received a presentation grant");
    Need(mailbox.Publish(result),"Background result was not queued");
    // No worker display occurs while backgrounded. Native automatic travel
    // reaches a lobby before focus returns, without our explicit exit/reset.
    Need(state.Observe({}) && mailbox.Take(read),"Native auto-lobby observation was lost");
    Need(!state.Qualify({read.world,read.index,read.serial,108}).generation,
        "Delayed foreground displayed a result from the departed world");
    Need(state.Observe(world),"Current match observation did not resume");
    mailbox.Reset();Need(mailbox.Publish(result) && mailbox.Take(read),"Valid result lost");
    auto grant=state.Qualify(world);
    Need(state.Current(grant) && !mailbox.Take(read),"Current decided result did not display exactly once");
    auto recycled=world;++recycled.serial;
    Need(state.Observe(recycled) && !state.Current(grant) && !state.Qualify(world).generation,
        "Recycled world address retained the old presentation serial");
    auto recycledGrant=state.Qualify(recycled);Need(state.Current(recycledGrant),"New world serial could not be presented");
    auto wrongMode=recycled;wrongMode.mode=109;Need(!state.Qualify(wrongMode).generation,"Cross-mode result grant admitted");
    state.Reset();Need(!state.Current(recycledGrant) && !state.Qualify(recycled).generation,"Reset retained an earlier display grant");
    Need(state.Observe(world),"Return fixture world did not become current");grant=state.Qualify(world);
    std::uint64_t first=0,second=0;
    Need(state.BeginReturn(grant,first) && first && state.ReturnOutcome(grant)==Outcome::Pending,
        "Return request was not reserved before queue submission");
    Need(!state.BeginReturn(grant,second),"Repeated Enter queued overlapping return operations");
    Need(!state.CompleteReturn(grant,first+1,Outcome::Submitted) &&
        state.CompleteReturn(grant,first,Outcome::Failed) && state.Current(grant) && state.ReturnOutcome(grant)==Outcome::Failed,
        "Queue/native failure closed a current result instead of preserving retry");
    Need(state.BeginReturn(grant,second) && second>first && !state.CompleteReturn(grant,first,Outcome::Submitted),
        "Retry did not reserve a fresh request or accepted an old acknowledgment");
    // Successful native exit invalidates the display in its cleanup before
    // the queued operation publishes its explicit acknowledgment.
    state.Reset();Need(!state.Current(grant) && state.CompleteReturn(grant,second,Outcome::Submitted) &&
        state.ReturnOutcome(grant)==Outcome::Submitted,"Native cleanup lost the successful return acknowledgment");
    Need(!state.BeginReturn(grant,first),"Stale presentation could request another game action");
    Need(state.Observe(world),"Cancelled-return fixture world did not resume");grant=state.Qualify(world);
    Need(state.BeginReturn(grant,first) && state.Observe({}) && state.CompleteReturn(grant,first,Outcome::Failed) &&
        state.ReturnOutcome(grant)==Outcome::Stale,"Departed world left a failed action retryable");
    std::puts("PASS result lifecycle: delayed foreground/auto-lobby, serial/mode/reset grants, single display, bounded return acknowledgment and retry");
}
void PanelChecks(){
    auto instance=GetModuleHandleW(nullptr);
    auto parent=CreateWindowExW(0,L"STATIC",L"Owned hidden fixture",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,instance,nullptr);
    Need(parent!=nullptr,"Hidden UI fixture parent failed");
    O::PrototypeResultPanel panel;
    Need(!panel.Open(instance,nullptr,L"Result",L"text",false),"Result UI accepted no owned parent");
    Need(panel.Open(instance,parent,L"Result fixture",L"first\nsecond\r\nthird",false) && !IsWindowVisible(panel.Handle()),"Hidden modeless result fixture creation failed");
    auto text=FindWindowExW(panel.Handle(),nullptr,L"EDIT",nullptr);wchar_t buffer[128]{};
    Need(text && GetWindowTextW(text,buffer,128)>0 && !wcscmp(buffer,L"first\r\nsecond\r\nthird") &&
        (GetWindowLongPtrW(text,GWL_STYLE)&ES_READONLY),"Result text newlines/read-only presentation changed");
    Need(PostMessageW(text,WM_KEYDOWN,VK_RETURN,0)!=FALSE,"Enter fixture post failed");panel.Pump();
    Need(panel.IsOpen() && panel.TakeReturn() && !panel.TakeReturn(),"Modeless Enter return action was not single-delivery");
    panel.ShowReturnPending();Need(!IsWindowEnabled(GetDlgItem(panel.Handle(),IDOK)),"Pending return button remained active");
    panel.ShowReturnUnavailable();GetWindowTextW(GetDlgItem(panel.Handle(),IDOK),buffer,128);
    Need(wcslen(buffer)>0 && IsWindowEnabled(GetDlgItem(panel.Handle(),IDOK)),"Queue rejection removed the retry action");
    Need(PostMessageW(text,WM_KEYDOWN,VK_ESCAPE,0)!=FALSE,"Escape fixture post failed");panel.Pump();
    Need(!panel.IsOpen() && !panel.TakeReturn(),"Escape did not close just the result panel");
    Need(panel.Open(instance,parent,L"Next match",L"result",false),"Result UI could not reopen");
    SendMessageW(panel.Handle(),WM_COMMAND,MAKEWPARAM(IDCANCEL,BN_CLICKED),0);
    Need(!panel.IsOpen() && !panel.TakeReturn(),"Close-result button requested lobby travel");
    Need(panel.Open(instance,parent,L"Last match",L"result",false),"Result lifecycle fixture failed");
    Need(PostMessageW(parent,WM_APP+17,0,0)!=FALSE,"Parent isolation fixture post failed");panel.Pump();
    MSG msg{};Need(PeekMessageW(&msg,parent,WM_APP+17,WM_APP+17,PM_REMOVE)!=FALSE,"Result pump consumed the game owner's messages");
    SendMessageW(panel.Handle(),WM_COMMAND,MAKEWPARAM(IDOK,BN_CLICKED),0);
    Need(panel.TakeReturn(),"Return button was not wired");panel.Close();DestroyWindow(parent);
    std::puts("PASS modeless result UI: hidden owned windows, readable newlines, Enter/Return, Escape/Close, retry, reopen and game-message isolation; no game executed");
}
}
int main(){try{
    TeamChecks();CommandChecks();MenuChecks();ResultChecks();PresentationLifecycleChecks();PanelChecks();
    Need(O::CreateAuthoredLobbyObject(&type,&outer,Initialize,Construct)==&object && constructed==1,"Owned object construction failed");
    badInitializer=true;Need(!O::CreateAuthoredLobbyObject(&type,&outer,Initialize,Construct) && constructed==1,"Invalid initializer reached constructor");
    Need(!O::CreateAuthoredLobbyObject(&type,nullptr,Initialize,Construct),"Missing transient outer admitted");
    std::puts("PASS owned descriptor ABI, transient/root flags and rejection before construction");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
