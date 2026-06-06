# pass5

```
=============== PASS5: additive-fold fn sub_1407A6300 ===============

=== full disasm sub_1407A6300 (additive fold: [+0x540]+[+0x530] -> +0x550) 0x1407A6300 (rva 0x7A6300) size=0x121 ===
  0x1407A6300: mov     [rsp+8], rbx
  0x1407A6305: mov     [rsp+10h], rsi
  0x1407A630A: mov     [rsp+18h], rdi
  0x1407A630F: push    r14
  0x1407A6311: sub     rsp, 50h
  0x1407A6315: mov     r14, rcx
  0x1407A6318: mov     [rcx+48h], rdx
  0x1407A631C: mov     [rcx+50h], r8
  0x1407A6320: movzx   ebx, r9b
  0x1407A6324: mov     byte ptr [rcx+367h], 0
  0x1407A632B: mov     rdi, r8
  0x1407A632E: add     rcx, 60h ; '`'
  0x1407A6332: mov     rsi, rdx
  0x1407A6335: call    sub_1407B8A60
  0x1407A633A: lea     rcx, [r14+5C0h]
  0x1407A6341: mov     [r14+600h], bl
  0x1407A6348: mov     rdx, rdi
  0x1407A634B: call    loc_1407B8B30
  0x1407A6350: mov     rax, [r14]
  0x1407A6353: mov     rcx, r14
  0x1407A6356: mov     byte ptr [r14+601h], 1
  0x1407A635E: call    qword ptr [rax+328h]
  0x1407A6364: mov     rax, [r14]
  0x1407A6367: mov     rcx, rsi
  0x1407A636A: mov     rbx, [rax+330h]
  0x1407A6371: mov     rax, [rsi]
  0x1407A6374: call    qword ptr [rax+90h]
  0x1407A637A: mov     rdx, rax
  0x1407A637D: mov     rcx, r14
  0x1407A6380: call    rbx
  0x1407A6382: movups  xmm0, xmmword ptr [r14+540h]
  0x1407A638A: lea     r8, [rsp+20h]
  0x1407A638F: addps   xmm0, xmmword ptr [r14+530h]
  0x1407A6397: mov     rax, [rsi]
  0x1407A639A: lea     rdx, [rsp+30h]
  0x1407A639F: mov     rcx, rsi
  0x1407A63A2: movaps  xmmword ptr [rsp+20h], xmm0
  0x1407A63A7: call    qword ptr [rax+68h]
  0x1407A63AA: lea     rdx, [rsp+20h]
  0x1407A63AF: mov     rcx, [rax]
  0x1407A63B2: mov     [r14+550h], rcx
  0x1407A63B9: mov     rcx, [rax+8]
  0x1407A63BD: mov     [r14+558h], rcx
  0x1407A63C4: mov     rcx, [rax+10h]
  0x1407A63C8: mov     [r14+560h], rcx
  0x1407A63CF: mov     rcx, r14
  0x1407A63D2: mov     rax, [rax+18h]
  0x1407A63D6: mov     [r14+568h], rax
  0x1407A63DD: mov     rax, [r14]
  0x1407A63E0: call    qword ptr [rax+338h]
  0x1407A63E6: xor     r9d, r9d
  0x1407A63E9: mov     r8b, 1
  0x1407A63EC: xorps   xmm1, xmm1
  0x1407A63EF: mov     rcx, r14
  0x1407A63F2: movups  xmm0, xmmword ptr [rax]
  0x1407A63F5: mov     rax, [r14]
  0x1407A63F8: movups  xmmword ptr [r14+570h], xmm0
  0x1407A6400: call    qword ptr [rax+20h]
  0x1407A6403: mov     rbx, [rsp+60h]
  0x1407A6408: mov     rsi, [rsp+68h]
  0x1407A640D: mov     rdi, [rsp+70h]
  0x1407A6412: mov     byte ptr [r14+602h], 1
  0x1407A641A: add     rsp, 50h
  0x1407A641E: pop     r14
  0x1407A6420: retn

=============== init constants used by sub_140C98BA0 ===============

  xmmword_145DD9C40 @0x145DD9C40:
    +0x0: [1.0, 0.0, 0.0, 0.0]  raw=0000803f000000000000000000000000
    +0x10: [16305.1172, 0.0, 2.7638, 0.0]  raw=78c47e460100000060e1304001000000
    +0x20: [0.0, 1.0, 0.0, 0.0]  raw=000000000000803f0000000000000000
    +0x30: [0.0, 0.0, 1.0, 0.0]  raw=00000000000000000000803f00000000

  xmmword_145DD9C60 @0x145DD9C60:
    +0x0: [0.0, 1.0, 0.0, 0.0]  raw=000000000000803f0000000000000000
    +0x10: [0.0, 0.0, 1.0, 0.0]  raw=00000000000000000000803f00000000
    +0x20: [16305.4766, 0.0, 2.7635, 0.0]  raw=e8c57e4601000000e0dd304001000000
    +0x30: [0.0, 0.0, 0.0, 1.0]  raw=0000000000000000000000000000803f

  xmmword_145DD9C70 @0x145DD9C70:
    +0x0: [0.0, 0.0, 1.0, 0.0]  raw=00000000000000000000803f00000000
    +0x10: [16305.4766, 0.0, 2.7635, 0.0]  raw=e8c57e4601000000e0dd304001000000
    +0x20: [0.0, 0.0, 0.0, 1.0]  raw=0000000000000000000000000000803f
    +0x30: [0.0, 0.0, 0.0, -1.0]  raw=000000000000000000000000000080bf

  xmmword_145DD9C90 @0x145DD9C90:
    +0x0: [0.0, 0.0, 0.0, 1.0]  raw=0000000000000000000000000000803f
    +0x10: [0.0, 0.0, 0.0, -1.0]  raw=000000000000000000000000000080bf
    +0x20: [-0.0, -0.0, -0.0, -0.0]  raw=00000080000000800000008000000080
    +0x30: [nan, nan, 0.0, 0.0]  raw=ffffffffffffffff0000000000000000

=============== caller sub_1407A6430 (calls sub_1407A6300) ===============
__int64 __fastcall sub_1407A6430(__int64 a1)
{
  __int64 v2; // rdx

  sub_1407A6300();
  return (*(__int64 (__fastcall **)(__int64, __int64, _QWORD, _QWORD))(*(_QWORD *)a1 + 32LL))(a1, v2, 0LL, 0LL);
}

=============== which vtables reference these CCamDriver methods ===============
  fn 0x1407A6300 appears in .rdata slots: ['0x145e3e210', '0x145e3e560', '0x145e3ec00']
  fn 0x1407A6430 appears in .rdata slots: ['0x145e3f2a0']
  fn 0x1407A6460 appears in .rdata slots: ['0x145e3fc88', '0x145e3ffd0', '0x145e40318', '0x145e40660']
  fn 0x1407A16A0 appears in .rdata slots: ['0x145e3f290']
```
