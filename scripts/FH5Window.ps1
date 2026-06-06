# FH5Window.ps1 — reusable helpers to target the ACTUAL "Forza Horizon 5" game window.
#
# The FH5 process owns TWO visible windows once the VR mod + SimXR are up: the game ("Forza Horizon 5")
# and the in-process SimXR preview ("O"). Process.MainWindowHandle flips between them, so keystrokes land
# on the wrong window. These helpers always resolve the window whose title is exactly "Forza Horizon 5"
# and foreground it via the AttachThreadInput trick (bypasses Win11's foreground-steal lock).

Add-Type @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;using System.Text;
public class FH5Win {
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] static extern bool AttachThreadInput(uint a, uint b, bool attach);
  [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr h,int n);
  [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
  [DllImport("user32.dll")] static extern uint MapVirtualKey(uint code, uint mapType);
  [DllImport("user32.dll")] static extern bool PostMessage(IntPtr h, uint msg, IntPtr wParam, IntPtr lParam);
  delegate bool EnumProc(IntPtr h, IntPtr p);
  static IntPtr _found; static uint _pid; static string _want;
  static bool Cb(IntPtr h, IntPtr p){
    uint wp; GetWindowThreadProcessId(h, out wp);
    if(wp==_pid && IsWindowVisible(h)){ var s=new StringBuilder(256); GetWindowText(h,s,256); if(s.ToString()==_want) _found=h; }
    return true;
  }
  public static IntPtr Find(uint pid, string title){ _found=IntPtr.Zero; _pid=pid; _want=title; EnumWindows(Cb, IntPtr.Zero); return _found; }
  static bool CbGame(IntPtr h, IntPtr p){
    uint wp; GetWindowThreadProcessId(h, out wp);
    if(wp!=_pid || !IsWindowVisible(h)) return true;
    var s=new StringBuilder(256); GetWindowText(h,s,256); var t=s.ToString();
    var cs=new StringBuilder(256); GetClassName(h,cs,256); var c=cs.ToString();
    // The FH5 process owns several windows; never target the in-process SimXR preview ("O", class
    // "OpenXR Simulator") or the fullscreen shade ("ForzaFullscreenShadeWindow").
    if(c=="OpenXR Simulator" || c=="ForzaFullscreenShadeWindow") return true;
    if(t=="Forza Horizon 5"){ _found=h; return false; }   // best: exact title
    if(c=="App"){ _found=h; return false; }               // game main window class — robust when the title is
                                                          // not yet set (early loading screens => old "no-window")
    if(t.IndexOf("Forza Horizon", StringComparison.OrdinalIgnoreCase)>=0){ _found=h; }
    else if(_found==IntPtr.Zero && t.Length>0 && t!="O"){ _found=h; }
    return true;
  }
  public static IntPtr FindGame(uint pid){ _found=IntPtr.Zero; _pid=pid; EnumWindows(CbGame, IntPtr.Zero); return _found; }
  public static string Title(IntPtr h){ var s=new StringBuilder(256); GetWindowText(h,s,256); return s.ToString(); }
  public static bool Focus(IntPtr h){
    uint dummy; uint cur=GetCurrentThreadId(); uint fg=GetWindowThreadProcessId(GetForegroundWindow(), out dummy);
    AttachThreadInput(cur,fg,true); ShowWindow(h,9); BringWindowToTop(h); SetForegroundWindow(h); AttachThreadInput(cur,fg,false);
    System.Threading.Thread.Sleep(350); return GetForegroundWindow()==h;
  }
  static IntPtr KeyLParam(uint scan, bool up){
    uint v=1 | (scan << 16);
    if(up) v |= (1u << 30) | (1u << 31);
    return new IntPtr(unchecked((int)v));
  }
  public static string KeyStatus(IntPtr h, byte vk){
    uint scan=MapVirtualKey(vk,0);
    if(scan==0) scan=1;
    if(Focus(h)){
      keybd_event(vk,(byte)scan,0,IntPtr.Zero);
      System.Threading.Thread.Sleep(110);
      keybd_event(vk,(byte)scan,2,IntPtr.Zero);
      return "focused";
    }
    bool down=PostMessage(h,0x0100,new IntPtr(vk),KeyLParam(scan,false));
    System.Threading.Thread.Sleep(110);
    bool up=PostMessage(h,0x0101,new IntPtr(vk),KeyLParam(scan,true));
    return (down && up) ? "posted" : "failed";
  }
}
'@

function Get-FH5GameWindow {
    $p = Get-Process ForzaHorizon5 -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $p) { return [IntPtr]::Zero }
    return [FH5Win]::FindGame([uint32]$p.Id)
}

# Send a virtual-key (default Enter 0x0D) to the GAME window only. Returns $true if the game was
# actually frontmost when the key was sent.
function Send-FH5GameKey {
    param([byte]$Vk = 0x0D)
    $h = Get-FH5GameWindow
    if ($h -eq [IntPtr]::Zero) { return "no-window" }
    return [FH5Win]::KeyStatus($h, $Vk)
}
