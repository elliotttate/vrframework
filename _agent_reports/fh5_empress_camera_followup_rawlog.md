# followup

```
=============== Q1: unique ENTRY AOB for sub_1407A6300 ===============
  + 5B (thru 0x1407A6300 mov     [rsp+8], rbx): count=143907
  +10B (thru 0x1407A6305 mov     [rsp+10h], rsi): count=44461
  +15B (thru 0x1407A630A mov     [rsp+18h], rdi): count=6157
  +17B (thru 0x1407A630F push    r14): count=837
  +21B (thru 0x1407A6311 sub     rsp, 50h): count=104
  +24B (thru 0x1407A6315 mov     r14, rcx): count=30
  +28B (thru 0x1407A6318 mov     [rcx+48h], rdx): count=1
  +32B (thru 0x1407A631C mov     [rcx+50h], r8): count=1
  +36B (thru 0x1407A6320 movzx   ebx, r9b): count=1

  >>> UNIQUE entry AOB (28 bytes, count==1):
      48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 50 4C 8B F1 48 89 51 48
      covers 0x1407A6300..0x1407A6318 (last insn: mov     [rcx+48h], rdx)

=============== Q2: callers / xrefs of sub_1407A6300 ===============
  total xrefs: 4
    <- 0x1407A6439 [call] in func 0x1407A6430 sub_1407A6430
    <- 0x1407A6DC9 [] in func 0x1407A6DC0 sub_1407A6DC0
    <- 0x1407A6DF9 [] in func 0x1407A6DF0 sub_1407A6DF0
    <- 0x1407A6E39 [] in func 0x1407A6E30 sub_1407A6E30
  vtable slots holding sub_1407A6300: ['0x145e3e210', '0x145e3e560', '0x145e3ec00']

  -- decompile direct callers (loop over cameras?) --

  ### caller 0x1407A6430 sub_1407A6430 ###
    __int64 __fastcall sub_1407A6430(__int64 a1)
    {
      __int64 v2; // rdx
    
      sub_1407A6300();
      return (*(__int64 (__fastcall **)(__int64, __int64, _QWORD, _QWORD))(*(_QWORD *)a1 + 32LL))(a1, v2, 0LL, 0LL);
    }
    callers-of-caller: 

=============== Q3: writers of [reg+0x540] (object-relative, non-stack) ===============
  object-relative [reg+0x540] op0 sites: 39
    @0x1403D13F4 mov [rdi+540h] in fn 0x1403D0E50 sub_1403D0E50  | mov     [rdi+540h], rax
    @0x1403D240B mov [rdi+540h] in fn 0x1403D1F00 sub_1403D1F00  | mov     byte ptr [rdi+540h], 0
    @0x1403DB667 mov [rdi+540h] in fn 0x1403DAD30 sub_1403DAD30  | mov     [rdi+540h], al
    @0x140581F8B movups [rsi+540h] in fn 0x140581F40 sub_140581F40  | movups  xmmword ptr [rsi+540h], xmm0
    @0x1405929A5 movups [rcx+540h] in fn 0x0 ?  | movups  xmmword ptr [rcx+540h], xmm0
    @0x1405EF7E1 mov [r15+540h] in fn 0x1405EF780 sub_1405EF780  | mov     [r15+540h], rsi
    @0x14071BE31 mov [r14+540h] in fn 0x14071BAF0 sub_14071BAF0  | mov     [r14+540h], rax
    @0x14072CFAA movups [r11+540h] in fn 0x14072CA00 sub_14072CA00 (COPY sub_14072CA00)  | movups  xmmword ptr [r11+540h], xmm1
    @0x14079B7E8 mov [rsi+540h] in fn 0x14079B750 sub_14079B750  | mov     [rsi+540h], ebp
    @0x14079BCF0 movups [rdi+540h] in fn 0x14079BBC0 sub_14079BBC0  | movups  xmmword ptr [rdi+540h], xmm1
    @0x1407A98AC movups [rbx+540h] in fn 0x1407A9880 sub_1407A9880  | movups  xmmword ptr [rbx+540h], xmm5
    @0x1407A98F0 movups [rbx+540h] in fn 0x1407A9880 sub_1407A9880  | movups  xmmword ptr [rbx+540h], xmm3
    @0x1407A9962 movups [rbx+540h] in fn 0x1407A9880 sub_1407A9880  | movups  xmmword ptr [rbx+540h], xmm5
    @0x1407A99A6 movups [rbx+540h] in fn 0x1407A9880 sub_1407A9880  | movups  xmmword ptr [rbx+540h], xmm3
    @0x1407A9B85 movups [rcx+540h] in fn 0x1407A9B60 sub_1407A9B60  | movups  xmmword ptr [rcx+540h], xmm0
    @0x1407A9BA7 mov [rcx+540h] in fn 0x1407A9B60 sub_1407A9B60  | mov     [rcx+540h], eax
    @0x1407AC240 movups [rsi+540h] in fn 0x1407ABF60 sub_1407ABF60  | movups  xmmword ptr [rsi+540h], xmm1
    @0x1407B2462 mov [rbx+540h] in fn 0x1407B2430 sub_1407B2430  | mov     [rbx+540h], rdi
    @0x140BD2448 mov [rbx+540h] in fn 0x140BD1F70 sub_140BD1F70  | mov     [rbx+540h], eax
    @0x140BEF977 mov [r14+540h] in fn 0x140BEF760 sub_140BEF760  | mov     [r14+540h], rdi
    @0x140BF3213 mov [rdi+540h] in fn 0x140BF2CF0 sub_140BF2CF0  | mov     [rdi+540h], r14
    @0x140BF3AF7 mov [rdi+540h] in fn 0x140BF39E0 sub_140BF39E0  | mov     qword ptr [rdi+540h], 0Fh
    @0x140C5532A mov [rsi+540h] in fn 0x140C4FE80 sub_140C4FE80  | mov     [rsi+540h], ecx
    @0x140C96830 movups [r12+540h] in fn 0x140C96630 sub_140C96630  | movups  xmmword ptr [r12+540h], xmm1
    @0x140C99830 movups [rcx+540h] in fn 0x140C98BA0 sub_140C98BA0 (INIT sub_140C98BA0)  | movups  xmmword ptr [rcx+540h], xmm0
    @0x140DBCD1A mov [r14+540h] in fn 0x140DBC9C0 sub_140DBC9C0  | mov     [r14+540h], rdi
    @0x140DBD9C3 mov [r14+540h] in fn 0x140DBC9C0 sub_140DBC9C0  | mov     [r14+540h], rcx
    @0x140F80078 mov [r14+540h] in fn 0x140F7FE30 sub_140F7FE30  | mov     [r14+540h], rbp
    @0x141032D7A mov [rcx+540h] in fn 0x0 ?  | mov     dword ptr [rcx+540h], 0FFFFFFFFh
    @0x141033163 movups [rcx+540h] in fn 0x0 ?  | movups  xmmword ptr [rcx+540h], xmm0
    @0x14103E13F movups [rcx+540h] in fn 0x14103E120 sub_14103E120  | movups  xmmword ptr [rcx+540h], xmm0
    @0x141055584 movsd [rbx+540h] in fn 0x141055480 sub_141055480  | movsd   qword ptr [rbx+540h], xmm0
    @0x1410602AD movups [rcx+540h] in fn 0x0 ?  | movups  xmmword ptr [rcx+540h], xmm1
    @0x14109DFC0 movss [rbx+540h] in fn 0x14109DE00 sub_14109DE00  | movss   dword ptr [rbx+540h], xmm0
    @0x1410C7B0C mov [rdi+540h] in fn 0x1410C79C0 sub_1410C79C0  | mov     [rdi+540h], rbp
    @0x1410CB752 mov [rsi+540h] in fn 0x1410CB6D0 sub_1410CB6D0  | mov     [rsi+540h], rax
    @0x1410DC112 mov [rcx+540h] in fn 0x0 ?  | mov     dword ptr [rcx+540h], 41200000h
    @0x141143C0A movsd [rbx+540h] in fn 0x141143220 sub_141143220  | movsd   qword ptr [rbx+540h], xmm1
    @0x14AF509B1 mov [rbx+540h] in fn 0x0 ?  | mov     byte ptr [rbx+540h], 0

=============== Q4: width/type at +0x540 + camera-space confirm ===============
  fold reads (sub_1407A6300):
    0x1407A6382: movups  xmm0, xmmword ptr [r14+540h]
    0x1407A638F: addps   xmm0, xmmword ptr [r14+530h]
  -> movups (128-bit / 4x f32) load of [+0x540], addps to [+0x530] (also 128-bit camera-space).
  derived write rows after transform:
    0x1407A63B2: mov     [r14+550h], rcx
    0x1407A63BD: mov     [r14+558h], rcx
    0x1407A63C8: mov     [r14+560h], rcx
    0x1407A63D6: mov     [r14+568h], rax
    0x1407A63F8: movups  xmmword ptr [r14+570h], xmm0
  init default written to +0x540 by sub_140C98BA0 (const xmmword_145DD9C40 rows):
    C40 row0 = [1.0, 0.0, 0.0, 0.0]
    C60 row0 = [0.0, 1.0, 0.0, 0.0]
    C70 row0 = [0.0, 0.0, 1.0, 0.0]
    C90 row0 = [0.0, 0.0, 0.0, 1.0]
```
