# pass4

```
=============== PASS4: matrix/lane regions + AOB uniqueness ===============

  --- disasm sub_140C98BA0 matrix-write around 0x140C99607 (rva 0xC99607) ---
     0x140C995D6: lea     eax, [rdx+0Bh]
     0x140C995D9: mov     [r8+30h], eax
     0x140C995DD: movaps  xmm0, cs:xmmword_145DD9C40
     0x140C995E4: movaps  xmm1, cs:xmmword_145DD9C60
     0x140C995EB: movaps  xmm2, cs:xmmword_145DD9C70
     0x140C995F2: movaps  xmm3, cs:xmmword_145DD9C90
     0x140C995F9: movups  xmmword ptr [rcx+300h], xmm0
     0x140C99600: movups  xmmword ptr [rcx+310h], xmm1
     0x140C99607: movups  xmmword ptr [rcx+320h], xmm2 <==
     0x140C9960E: movups  xmmword ptr [rcx+330h], xmm3
     0x140C99615: lea     eax, [rdx+0Ch]
     0x140C99618: mov     [r8+34h], eax
     0x140C9961C: movaps  xmm0, cs:xmmword_145DD9C40
     0x140C99623: movaps  xmm1, cs:xmmword_145DD9C60
     0x140C9962A: movaps  xmm2, cs:xmmword_145DD9C70
     0x140C99631: movaps  xmm3, cs:xmmword_145DD9C90
     0x140C99638: movups  xmmword ptr [rcx+340h], xmm0
     0x140C9963F: movups  xmmword ptr [rcx+350h], xmm1
     0x140C99646: movups  xmmword ptr [rcx+360h], xmm2

  --- disasm sub_140C98BA0 lane-write +0x540/+0x550 around 0x140C99830 (rva 0xC99830) ---
     0x140C997EA: movaps  xmm3, cs:xmmword_145DD9C90
     0x140C997F1: movups  xmmword ptr [rcx+500h], xmm0
     0x140C997F8: movups  xmmword ptr [rcx+510h], xmm1
     0x140C997FF: movups  xmmword ptr [rcx+520h], xmm2
     0x140C99806: movups  xmmword ptr [rcx+530h], xmm3
     0x140C9980D: lea     eax, [rdx+14h]
     0x140C99810: mov     [r8+54h], eax
     0x140C99814: movaps  xmm0, cs:xmmword_145DD9C40
     0x140C9981B: movaps  xmm1, cs:xmmword_145DD9C60
     0x140C99822: movaps  xmm2, cs:xmmword_145DD9C70
     0x140C99829: movaps  xmm3, cs:xmmword_145DD9C90
     0x140C99830: movups  xmmword ptr [rcx+540h], xmm0 <==
     0x140C99837: movups  xmmword ptr [rcx+550h], xmm1
     0x140C9983E: movups  xmmword ptr [rcx+560h], xmm2
     0x140C99845: movups  xmmword ptr [rcx+570h], xmm3
     0x140C9984C: lea     eax, [rdx+15h]
     0x140C9984F: mov     [r8+58h], eax
     0x140C99853: movaps  xmm0, cs:xmmword_145DD9C40
     0x140C9985A: movaps  xmm1, cs:xmmword_145DD9C60

  --- disasm sub_1407A6300 +0x540 read region around 0x1407A6382 (rva 0x7A6382) ---
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
     0x1407A6382: movups  xmm0, xmmword ptr [r14+540h] <==
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

  --- disasm sub_14072CA00 +0x320 copy around 0x14072CD45 (rva 0x72CD45) ---
     0x14072CD25: movups  xmmword ptr [r11+300h], xmm0
     0x14072CD2D: movups  xmm1, xmmword ptr [r10+310h]
     0x14072CD35: movups  xmmword ptr [r11+310h], xmm1
     0x14072CD3D: movups  xmm0, xmmword ptr [r10+320h]
     0x14072CD45: movups  xmmword ptr [r11+320h], xmm0 <==
     0x14072CD4D: movups  xmm1, xmmword ptr [r10+330h]
     0x14072CD55: movups  xmmword ptr [r11+330h], xmm1
     0x14072CD5D: movups  xmm0, xmmword ptr [r10+340h]
     0x14072CD65: movups  xmmword ptr [r11+340h], xmm0
     0x14072CD6D: movups  xmm0, xmmword ptr [r10+350h]
     0x14072CD75: movups  xmmword ptr [r11+350h], xmm0
     0x14072CD7D: movups  xmm1, xmmword ptr [r10+360h]
     0x14072CD85: movups  xmmword ptr [r11+360h], xmm1
     0x14072CD8D: movups  xmm0, xmmword ptr [r10+370h]
     0x14072CD95: movups  xmmword ptr [r11+370h], xmm0
     0x14072CD9D: movups  xmm1, xmmword ptr [r10+380h]

  --- disasm sub_14072CA00 +0x540/+0x550 copy around 0x14072CFA2 (rva 0x72CFA2) ---
     0x14072CF92: movups  xmm0, xmmword ptr [r10+530h]
     0x14072CF9A: movups  xmmword ptr [r11+530h], xmm0
     0x14072CFA2: movups  xmm1, xmmword ptr [r10+540h] <==
     0x14072CFAA: movups  xmmword ptr [r11+540h], xmm1
     0x14072CFB2: movups  xmm0, xmmword ptr [r10+550h]
     0x14072CFBA: movups  xmmword ptr [r11+550h], xmm0
     0x14072CFC2: movups  xmm1, xmmword ptr [r10+560h]
     0x14072CFCA: movups  xmmword ptr [r11+560h], xmm1
     0x14072CFD2: movups  xmm0, xmmword ptr [r10+570h]
     0x14072CFDA: movups  xmmword ptr [r11+570h], xmm0

=============== AOB uniqueness ===============
  sub_14072CA00 @0x14072CA00 (rva 0x72CA00): count=1
    AOB: 48 8B 41 10 4C 8B D2 4C 8B D9 4C 63 48 04 48 8B 42 10 4C 63 40 04
  sub_140C98BA0 @0x140C98BA0 (rva 0xC98BA0): count=199
    AOB: 48 89 5C 24 10 48 89 4C 24 08 55 56 57 41 54 41 55 41 56 41 57
  sub_1407A6300 @0x1407A6300 (rva 0x7A6300): count=30
    AOB: 48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 50 4C 8B F1
  sub_1407A6460 @0x1407A6460 (rva 0x7A6460): count=1
    AOB: 40 53 55 57 41 57 48 81 EC 98 00 00 00 4C 8B F9 48 89 51 48 4C 89 41 50
  sub_14072B120 (per-frame caller of 72CA00) @0x14072B120 (rva 0x72B120): count=1
    AOB: 48 89 4C 24 08 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 48 F4 FF FF

=============== caller sub_14072B120 ===============
  caller<- 0x140715F21 in func 0x140715B00
void __fastcall sub_14072B120(__int64 a1, __int64 a2)
{
  __int64 v3; // rsi
  __int64 v4; // rcx
  __int64 v5; // rcx
  __int64 v6; // rcx
  __int64 v7; // rdi
  __int64 v8; // rcx
  __int64 v9; // rcx
  __int64 v10; // rcx
  __int64 v11; // rbx
  __int64 v12; // rcx
  __int64 v13; // rcx
  __int64 v14; // rcx
  _BYTE v15[320]; // [rsp+30h] [rbp-D0h] BYREF
  _BYTE v16[320]; // [rsp+170h] [rbp+70h] BYREF
  _BYTE v17[320]; // [rsp+2B0h] [rbp+1B0h] BYREF
  _BYTE v18[320]; // [rsp+3F0h] [rbp+2F0h] BYREF
  _BYTE v19[320]; // [rsp+530h] [rbp+430h] BYREF
  _BYTE v20[320]; // [rsp+670h] [rbp+570h] BYREF
  _BYTE v21[320]; // [rsp+7B0h] [rbp+6B0h] BYREF
  _BYTE v22[320]; // [rsp+8F0h] [rbp+7F0h] BYREF
  _BYTE v23[320]; // [rsp+A30h] [rbp+930h] BYREF
  _BYTE v24[384]; // [rsp+B70h] [rbp+A70h] BYREF
  char v25; // [rsp+D08h] [rbp+C08h] BYREF
  LARGE_INTEGER PerformanceCount; // [rsp+D10h] [rbp+C10h] BYREF
  LARGE_INTEGER v27; // [rsp+D18h] [rbp+C18h] BYREF

  PerformanceCount.LowPart = 0;
  ((void (__fastcall *)(__int64, __int64, __int64))unk_14072B940)(a1, a2, 1LL);
  *(_QWORD *)(a1 + 72) = 7LL;
  *(_BYTE *)(a1 + 80) = 0;
  *(_DWORD *)(a1 + 88) = 1145569280;
  *(_DWORD *)(a1 + 92) = 1167867904;
  *(_DWORD *)(a1 + 96) = 1145569280;
  *(_DWORD *)(a1 + 100) = 1167867904;
  *(_QWORD *)(a1 + 64) = &off_145E28210;
  *(_QWORD *)(a1 + 104) = 3LL;
  *(_DWORD *)(a1 + 112) = 3;
  *(_DWORD *)(a1 + 116) = 1165623296;
  *(_DWORD *)(a1 + 120) = 1167867904;
  *(_QWORD *)(a1 + 136) = 0LL;
  *(_BYTE *)(a1 + 144) = 0;
  *(_QWORD *)(a1 + 128) = &off_145E282B0;
  *(_QWORD *)(a1 + 152) = 1157234688LL;
  sub_14072ACB0(a1 + 160);
  sub_14072ACB0(a1 + 208);
  *(_DWORD *)(a1 + 136) = 11;
  v3 = a1 + 256;
  v27.QuadPart = a1 + 256;
  *(_QWORD *)(a1 + 272) = &unk_145E28508;
  *(_QWORD *)(a1 + 3552) = &unk_145E28518;
  *(_QWORD *)(a1 + 3536) = 0LL;
  *(_BYTE *)(a1 + 3544) = 0;
  *(_QWORD *)(a1 + 3528) = &off_145E28308;
  v4 = *(int *)(*(_QWORD *)(a1 + 3552) + 4LL);
  *(_DWORD *)(v4 + v3 + 3292) = v4 - 24;
  *(_DWORD *)(a1 + 3560) = 0;
  *(_DWORD *)(*(int *)(*(_QWORD *)(a1 + 3552) + 4LL) + v3 + 3304) = 12;
  *(_QWORD *)(*(int *)(*(_QWORD *)(a1 + 3552) + 4LL) + v3 + 3296) = &off_145E28358;
  v5 = *(int *)(*(_QWORD *)(a1 + 3552) + 4LL);
  *(_DWORD *)(v5 + v3 + 3292) = v5 - 32;
  *(_QWORD *)(a1 + 3568) = 0LL;
  *(_DWORD *)(*(int *)(*(_QWORD *)(a1 + 3552) + 4LL) + v3 + 3304) = 13;
  PerformanceCount.LowPart = 3;
  sub_14072AE70(a1 + 256, 0LL);
  *(_QWORD *)(a1 + 256) = &off_145E284A8;
  *(_QWORD *)(*(int *)(*(_QWORD *)(a1 + 272) + 4LL) + a1 + 256 + 16) = &off_145E284C0;
  v6 = *(int *)(*(_QWORD *)(a1 + 272) + 4LL);
  *(_DWORD *)(v6 + v3 + 12) = v6 - 3256;
  *(_DWORD *)(*(int *)(*(_QWORD *)(a1 + 272) + 4LL) + a1 + 256 + 24) = 20;
  v7 = a1 + 3584;
  v27.QuadPart = a1 + 3584;
  *(_QWORD *)(a1 + 3600) = &unk_145E28588;
  *(_QWORD *)(a1 + 6880) = &unk_145E28598;
  *(_QWORD *)(a1 + 6864) = 0LL;
  *(_BYTE *)(a1 + 6872) = 0;
  *(_QWORD *)(a1 + 6856) = &off_145E28308;
  v8 = *(int *)(*(_QWORD *)(a1 + 6880) + 4LL);
  *(_DWORD *)(v8 + v7 + 3292) = v8 - 24;
  *(_DWORD *)(a1 + 6888) = 0;
  *(_DWORD *)(*(int *)(*(_QWORD *)(a1 + 6880) + 4LL) + v7 + 3304) = 12;
  *(_QWORD *)(*(int *)(*(_QWORD *)(a1 + 6880) + 4LL) + v7 + 3296) = &off_145E28358;
  v9 = *(int *)(*(_QWORD *)(a1 + 6880) + 4LL);
  *(_DWORD *)(v9 + v7 + 3292) = v9 - 32;
  *(_QWORD *)(a1 + 6896) = 0LL;
  *(_DWORD *)(*(int *)(*(_QWORD *)(a1 + 6880) + 4LL) + v7 + 3304) = 13;
  PerformanceCount.LowPart = 51;
  sub_14072AE70(a1 + 3584, 0LL);
  *(_QWORD *)(a1 + 3584) = &off_145E28528;
  *(_QWORD *)(*(int *)(*(_QWORD *)(a1 + 3600) + 4LL) + a1 + 3584 + 16) = &off_145E28540;
  v10 = *(int *)(*(_QWORD *)(a1 + 3600) + 4LL);
  *(_DWORD *)(v10 + v7 + 12) = v10 - 3256;
  *(_DWORD *)(*(int *)(*(_QWORD *)(a1 + 3600) + 4LL) + a1 + 3584 + 24) = 21;
  v11 = a1 + 6912;
  v27.QuadPart = a1 + 6912;
  *(_QWORD *)(a1 + 6928) = &unk_145E28608;
  *(_QWORD *)(a1 + 10208) = &unk_145E28618;
  *(_QWORD *)(a1 + 10192) = 0LL;
  *(_BYTE *)(a1 + 10200) = 0;
  *(_QWORD *)(a1 + 10184) = &off_145E28308;
  v12 = *(int *)(*(_QWORD *)(a1 + 10208) + 4LL);
  *(_DWORD *)(v12 + v11 + 3292) = v12 - 24;
  *(_DWORD *)(a1 + 10216) = 0;
  *(_DWORD *)(*(int *)(*(_QWORD *)(a1 + 10208) + 4LL) + v11 + 3304) = 12;
  *(_QWORD *)(*(int *)(*(_QWORD *)(a1 + 10208) + 4LL) + v11 + 3296) = &off_145E28358;
  v13 = *(int *)(*(_QWORD *)(a1 + 10208) + 4LL);
  *(_DWORD *)(v13 + v11 + 3292) = v13 - 32;
  *(_QWORD *)(a1 + 10224) = 0LL;
  *(_DWORD *)(*(int *)(*(_QWORD *)(a1 + 10208) + 4LL) + v11 + 3304) = 13;
  PerformanceCount.LowPart = 819;
  sub_14072AE70(a1 + 6912, 0LL);
  *(_QWORD *)(a1 + 6912) = &off_145E285A8;
  *(_QWORD *)(*(int *)(*(_QWORD *)(a1 + 6928) + 4LL) + a1 + 6912 + 16) = &off_145E285C0;
  v14 = *(int *)(*(_QWORD *)(a1 + 6928) + 4LL);
  *(_DWORD *)(v14 + v11 + 12) = v14 - 3256;
  *(_DWORD *)(*(int *)(*(_QWORD *)(a1 + 6928) + 4LL) + v11 + 24) = 22;
  sub_144D39930(a1 + 10240, 48LL, 4LL, sub_14072AD50, sub_14072C430);
  sub_144D39930(a1 + 10432, 48LL, 4LL, sub_14072AD50, sub_14072C430);
  sub_144D39930(a1 + 10624, 48LL, 4LL, sub_14072AD50, sub_14072C430);
  ...[+64]

=============== CCamDriver size sanity (dtor frees 1616=0x650) ===============
  CCamDriver vtable[0]=sub_1407A16A0 -> operator delete(this,1616) => sizeof=0x650
  => +0x540 and +0x550 are valid fields (0x540<0x650, 0x550<0x650). +0x320 matrix likewise.
```
