#pragma once
#include <windows.h>
#include <algorithm>
#include <initializer_list>
#include <string>
namespace TwinKartObjective {
// Owned, modeless compatibility UI for prototypes whose cooked result widget
// is missing. Its thread keeps pumping the bridge; no game/renderer work runs
// in this window procedure. All game actions are taken by the owner adapter.
class PrototypeResultPanel {
    HWND window=nullptr,body=nullptr,returnButton=nullptr,closeButton=nullptr;
    HFONT font=nullptr;DWORD thread=0;bool returnRequested=false;
    static constexpr wchar_t ClassName[]=L"TwinKartPrototypeResultPanel";
    int Unit(int value)const {
        UINT dpi=window?GetDpiForWindow(window):96;return MulDiv(value,dpi?dpi:96,96);
    }
    void UpdateFont(UINT dpi) {
        auto next=CreateFontW(-MulDiv(11,dpi?dpi:96,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        auto selected=next?next:static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        for(HWND child:{body,returnButton,closeButton})if(child)SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(selected),TRUE);
        if(font)DeleteObject(font);font=next;
    }
    void Layout() {
        if(!window)return;RECT area{};GetClientRect(window,&area);
        int margin=Unit(18),buttonHeight=Unit(32),buttonWidth=Unit(124),gap=Unit(12);
        int width=area.right,height=area.bottom;
        MoveWindow(body,margin,margin,(width-2*margin)>0?width-2*margin:1,
            (height-3*margin-buttonHeight)>0?height-3*margin-buttonHeight:1,TRUE);
        MoveWindow(returnButton,width-margin-2*buttonWidth-gap,height-margin-buttonHeight,buttonWidth,buttonHeight,TRUE);
        MoveWindow(closeButton,width-margin-buttonWidth,height-margin-buttonHeight,buttonWidth,buttonHeight,TRUE);
    }
    static LRESULT CALLBACK Procedure(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
        auto self=reinterpret_cast<PrototypeResultPanel*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE) {
            auto create=reinterpret_cast<CREATESTRUCTW*>(lparam);
            self=static_cast<PrototypeResultPanel*>(create->lpCreateParams);if(!self)return FALSE;
            self->window=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        if(!self)return DefWindowProcW(hwnd,message,wparam,lparam);
        switch(message) {
        case WM_COMMAND:
            if(HIWORD(wparam)==BN_CLICKED) {
                if(LOWORD(wparam)==IDOK){self->returnRequested=true;return 0;}
                if(LOWORD(wparam)==IDCANCEL){self->Close();return 0;}
            }
            break;
        case WM_CLOSE:self->Close();return 0;
        case WM_SIZE:self->Layout();return 0;
        case WM_GETMINMAXINFO: {
            auto limits=reinterpret_cast<MINMAXINFO*>(lparam);
            limits->ptMinTrackSize={self->Unit(380),self->Unit(240)};return 0;
        }
        case WM_DPICHANGED: {
            auto rect=reinterpret_cast<const RECT*>(lparam);
            if(self->body)self->UpdateFont(HIWORD(wparam));
            SetWindowPos(hwnd,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);
            self->Layout();return 0;
        }
        case WM_NCDESTROY:
            SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);
            self->window=self->body=self->returnButton=self->closeButton=nullptr;
            if(self->font){DeleteObject(self->font);self->font=nullptr;}
            break;
        }
        return DefWindowProcW(hwnd,message,wparam,lparam);
    }
public:
    PrototypeResultPanel()=default;
    PrototypeResultPanel(const PrototypeResultPanel&)=delete;
    PrototypeResultPanel& operator=(const PrototypeResultPanel&)=delete;
    bool IsOpen()const{return window!=nullptr;}
    HWND Handle()const{return window;}
    void Close() {
        if(window && GetCurrentThreadId()==thread)DestroyWindow(window);
        returnRequested=false;
    }
    bool Open(HINSTANCE instance,HWND owner,const wchar_t* title,const wchar_t* text,bool visible=true) {
        if(!instance || !owner || !IsWindow(owner) || !title || !text ||
           (thread && thread!=GetCurrentThreadId()))return false;
        DWORD process=0;GetWindowThreadProcessId(owner,&process);if(process!=GetCurrentProcessId())return false;
        Close();thread=GetCurrentThreadId();
        std::wstring lines;size_t length=wcsnlen_s(text,8193);if(length>8192)return false;
        for(size_t i=0;i<length;++i){if(text[i]==L'\n' && (!i || text[i-1]!=L'\r'))lines+=L'\r';lines+=text[i];}
        WNDCLASSEXW existing{};existing.cbSize=sizeof(existing);
        if(GetClassInfoExW(instance,ClassName,&existing)) {
            if(existing.lpfnWndProc!=Procedure)return false;
        } else {
            WNDCLASSEXW cls{};cls.cbSize=sizeof(cls);cls.hInstance=instance;cls.lpfnWndProc=Procedure;
            cls.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);cls.lpszClassName=ClassName;
            if(!RegisterClassExW(&cls))return false;
        }
        RECT parent{};if(!GetWindowRect(owner,&parent))return false;
        UINT dpi=GetDpiForWindow(owner);if(!dpi)dpi=96;
        int width=MulDiv(560,dpi,96),height=MulDiv(380,dpi,96);
        int x=parent.left+((parent.right-parent.left)-width)/2,y=parent.top+((parent.bottom-parent.top)-height)/2;
        if(!CreateWindowExW(WS_EX_CONTROLPARENT,ClassName,title,WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME,
            x,y,width,height,owner,nullptr,instance,this))return false;
        body=CreateWindowExW(0,L"EDIT",lines.c_str(),WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,
            0,0,1,1,window,nullptr,instance,nullptr);
        returnButton=CreateWindowExW(0,L"BUTTON",L"\u8fd4\u56de\u5927\u5385",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,
            0,0,1,1,window,reinterpret_cast<HMENU>(IDOK),instance,nullptr);
        closeButton=CreateWindowExW(0,L"BUTTON",L"\u5173\u95ed\u7ed3\u7b97",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
            0,0,1,1,window,reinterpret_cast<HMENU>(IDCANCEL),instance,nullptr);
        if(!body || !returnButton || !closeButton){Close();return false;}
        UpdateFont(dpi);
        Layout();if(visible){ShowWindow(window,SW_SHOWNORMAL);SetFocus(returnButton);}return true;
    }
    void Pump() {
        if(!window || GetCurrentThreadId()!=thread)return;
        MSG msg{};
        while(window && PeekMessageW(&msg,window,0,0,PM_REMOVE)) {
            if(msg.message==WM_KEYDOWN && msg.wParam==VK_ESCAPE){Close();break;}
            if(msg.message==WM_KEYDOWN && msg.wParam==VK_RETURN){returnRequested=true;continue;}
            if(!IsDialogMessageW(window,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        }
    }
    bool TakeReturn(){bool value=returnRequested;returnRequested=false;return value;}
    void ShowReturnPending(){if(window){EnableWindow(returnButton,FALSE);SetWindowTextW(returnButton,L"\u6b63\u5728\u8fd4\u56de\u2026");}}
    void ShowReturnUnavailable(){if(window){EnableWindow(returnButton,TRUE);SetWindowTextW(returnButton,L"\u70b9\u51fb\u91cd\u8bd5\u8fd4\u56de");}}
};
// A passive status card for an owned local match. It never owns keyboard
// focus and never consumes clicks over the game window.
class PrototypeHintPanel {
    HWND window=nullptr;
    HFONT font=nullptr;
    std::wstring text;
    static constexpr wchar_t Name[]=L"TwinKartLocalRoleHint";
    static LRESULT CALLBACK Procedure(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
        auto self=reinterpret_cast<PrototypeHintPanel*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(msg==WM_NCCREATE) {
            self=static_cast<PrototypeHintPanel*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        if(msg==WM_NCHITTEST)return HTTRANSPARENT;
        if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
        if(msg==WM_PAINT && self) {
            PAINTSTRUCT paint{};HDC dc=BeginPaint(hwnd,&paint);RECT area{};GetClientRect(hwnd,&area);
            auto brush=CreateSolidBrush(RGB(23,31,43));
            if(brush){FillRect(dc,&area,brush);DeleteObject(brush);}
            auto old=SelectObject(dc,self->font?self->font:GetStockObject(DEFAULT_GUI_FONT));
            SetTextColor(dc,RGB(245,248,252));SetBkMode(dc,TRANSPARENT);
            InflateRect(&area,-12,-7);
            DrawTextW(dc,self->text.c_str(),-1,&area,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
            if(old)SelectObject(dc,old);EndPaint(hwnd,&paint);return 0;
        }
        return DefWindowProcW(hwnd,msg,wp,lp);
    }
public:
    void Close() {
        if(window){DestroyWindow(window);window=nullptr;}
        if(font){DeleteObject(font);font=nullptr;}
        text.clear();
    }
    void Show(HINSTANCE instance,HWND game,const wchar_t* message) {
        RECT rect{};POINT corner{};
        if(!message || !*message || !game || GetForegroundWindow()!=game || IsIconic(game) ||
           !GetClientRect(game,&rect) || !ClientToScreen(game,&corner) || rect.right<320) {Close();return;}
        const auto dpi=GetDpiForWindow(game);
        const int scale=dpi?static_cast<int>(dpi):96;
        const int margin=MulDiv(18,scale,96),width=(std::min)(static_cast<int>(rect.right)-2*margin,MulDiv(540,scale,96));
        unsigned lines=1;
        for(auto p=message;*p;++p)if(*p==L'\n')++lines;
        const int height=MulDiv(lines>2?106:74,scale,96);
        if(!window) {
            WNDCLASSW wc{};wc.hInstance=instance;wc.lpszClassName=Name;wc.lpfnWndProc=Procedure;
            if(!RegisterClassW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return;
            font=CreateFontW(-MulDiv(13,scale,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
            window=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_TRANSPARENT,Name,L"",WS_POPUP,
                0,0,width,height,game,nullptr,instance,this);
            if(!window || !SetLayeredWindowAttributes(window,0,255,LWA_ALPHA)){Close();return;}
        }
        if(text!=message){text=message;InvalidateRect(window,nullptr,FALSE);}
        SetWindowPos(window,HWND_TOP,corner.x+margin,corner.y+margin,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);
        MSG msg{};
        while(PeekMessageW(&msg,window,0,0,PM_REMOVE))DispatchMessageW(&msg);
    }
};
}
