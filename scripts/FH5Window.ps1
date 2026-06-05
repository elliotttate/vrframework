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
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] static extern bool AttachThreadInput(uint a, uint b, bool attach);
  [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr h,int n);
  [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
  delegate bool EnumProc(IntPtr h, IntPtr p);
  static IntPtr _found; static uint _pid; static string _want;
  static bool Cb(IntPtr h, IntPtr p){
    uint wp; GetWindowThreadProcessId(h, out wp);
    if(wp==_pid && IsWindowVisible(h)){ var s=new StringBuilder(256); GetWindowText(h,s,256); if(s.ToString()==_want) _found=h; }
    return true;
  }
  public static IntPtr Find(uint pid, string title){ _found=IntPtr.Zero; _pid=pid; _want=title; EnumWindows(Cb, IntPtr.Zero); return _found; }
  public static string Title(IntPtr h){ var s=new StringBuilder(256); GetWindowText(h,s,256); return s.ToString(); }
  public static bool Focus(IntPtr h){
    uint cur=GetCurrentThreadId(); uint fg=GetWindowThreadProcessId(GetForegroundWindow(), out _);
    AttachThreadInput(cur,fg,true); ShowWindow(h,9); BringWindowToTop(h); SetForegroundWindow(h); AttachThreadInput(cur,fg,false);
    System.Threading.Thread.Sleep(350); return GetForegroundWindow()==h;
  }
  public static bool Key(IntPtr h, byte vk){ if(!Focus(h)) return false; keybd_event(vk,0x01,0,IntPtr.Zero); System.Threading.Thread.Sleep(110); keybd_event(vk,0x01,2,IntPtr.Zero); return true; }
}
'@

function Get-FH5GameWindow {
    $p = Get-Process ForzaHorizon5 -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $p) { return [IntPtr]::Zero }
    return [FH5Win]::Find([uint32]$p.Id, "Forza Horizon 5")
}

# Send a virtual-key (default Enter 0x0D) to the GAME window only. Returns $true if the game was
# actually frontmost when the key was sent.
function Send-FH5GameKey {
    param([byte]$Vk = 0x0D)
    $h = Get-FH5GameWindow
    if ($h -eq [IntPtr]::Zero) { return $false }
    return [FH5Win]::Key($h, $Vk)
}
