; open rc=0 18.0s
; .text 0x140001000..0x145D95000

===== QUERY 5: vtable sanity =====

  CCamDriver concrete vtbl @0x145E3F290 rva=0x5E3F290 seg=.rdata name='off_145E3F290'
    [0] ->0x1407A16A0 rva=0x7A16A0 sub_1407A16A0
    [1] ->0x1407A6F70 rva=0x7A6F70 sub_1407A6F70
    [2] ->0x1407A6430 rva=0x7A6430 sub_1407A6430
    [3] ->0x1407A6F00 rva=0x7A6F00 sub_1407A6F00
    [4] ->0x1407A9DD0 rva=0x7A9DD0 sub_1407A9DD0
    [5] ->0x1407A3220 rva=0x7A3220 sub_1407A3220
    RTTI col@(vt-8)=0x146899FD0 rva=0x6899FD0 ''
    dref<-0x14079B8EE (rva 0x79B8EE) in sub_14079B8A0

  ForzaMultiCam vtbl A @0x1465D6808 rva=0x65D6808 seg=.rdata name=''
    [0] ->0x144A1D570 rva=0x4A1D570 sub_144A1D570
    [1] ->0x1407A6F70 rva=0x7A6F70 sub_1407A6F70
    [2] ->0x140602220 rva=0x602220 sub_140602220
    [3] ->0x140602BA0 rva=0x602BA0 
    [4] ->0x140609640 rva=0x609640 sub_140609640
    [5] ->0x144A24210 rva=0x4A24210 sub_144A24210
    RTTI col@(vt-8)=0x147317D38 rva=0x7317D38 ''

  ForzaMultiCam vtbl B @0x1465D6BA0 rva=0x65D6BA0 seg=.rdata name=''
    [0] ->0x1405F4360 rva=0x5F4360 sub_1405F4360
    [1] ->0x140605520 rva=0x605520 sub_140605520
    [2] ->0x140605570 rva=0x605570 sub_140605570
    [3] ->0x140608BF0 rva=0x608BF0 sub_140608BF0
    [4] ->0x140608BA0 rva=0x608BA0 sub_140608BA0
    [5] ->0x140608B90 rva=0x608B90 
    RTTI col@(vt-8)=0x147317DD0 rva=0x7317DD0 ''

===== QUERY 4 (FAST): displacement accessors =====
; [scan +0x320] 400 hits in 1.6s

--- +0x320: 400 accesses (249 W / 151 r) ---
  FUNC (no func) W=8 N=10 
     [W] 0x1402DDBDE (rva 0x2DDBDE) db    0
     [W] 0x1403BCEDE (rva 0x3BCEDE) db  89h
     [W] 0x1403E369F (rva 0x3E369F) db 0FFh
     [r] 0x140480CB4 (rva 0x480CB4) db  8Bh
     [W] 0x1405928E8 (rva 0x5928E8) movups  xmmword ptr [rcx+320h], xmm2
     [W] 0x1406DFF7B (rva 0x6DFF7B) db  11h
  FUNC sub_A69B20 @0x140A69B20 W=7 N=8 sub_140A69B20
     [r] 0x140A69BBF (rva 0xA69BBF) mov     eax, [rdi+320h]
     [W] 0x140A69BDA (rva 0xA69BDA) mov     [rdi+320h], eax
     [W] 0x140A69C17 (rva 0xA69C17) cmp     dword ptr [rdi+320h], 6
     [W] 0x140A69C20 (rva 0xA69C20) mov     dword ptr [rdi+320h], 6
     [W] 0x140A69C61 (rva 0xA69C61) cmp     byte ptr [rbx+320h], 0
     [W] 0x140A69DFA (rva 0xA69DFA) cmp     byte ptr [r12+320h], 0
  FUNC sub_D760 @0x14000D760 W=6 N=6 sub_14000D760
     [W] 0x14000D804 (rva 0xD804) mov     [rbp+320h], rax
     [W] 0x14000D8CF (rva 0xD8CF) mov     [rbp+320h], rax
     [W] 0x14000D9AD (rva 0xD9AD) mov     [rbp+320h], rax
     [W] 0x14000DA88 (rva 0xDA88) mov     [rbp+320h], rax
     [W] 0x14000DB41 (rva 0xDB41) mov     [rbp+320h], rax
     [W] 0x14000DBFA (rva 0xDBFA) mov     [rbp+320h], rax
  FUNC sub_A40470 @0x140A40470 W=6 N=8 sub_140A40470
     [r] 0x140A40562 (rva 0xA40562) mov     eax, [rbx+320h]
     [W] 0x140A4057D (rva 0xA4057D) mov     [rbx+320h], eax
     [W] 0x140A405A8 (rva 0xA405A8) cmp     dword ptr [rbx+320h], 1
     [W] 0x140A405B1 (rva 0xA405B1) mov     dword ptr [rbx+320h], 1
     [r] 0x140A405CD (rva 0xA405CD) mov     eax, [rbx+320h]
     [W] 0x140A405E8 (rva 0xA405E8) mov     [rbx+320h], eax
  FUNC sub_5AE3A0 @0x1405AE3A0 W=5 N=6 sub_1405AE3A0
     [W] 0x1405AEEF6 (rva 0x5AEEF6) cmp     byte ptr [rbx+320h], 0
     [r] 0x1405AEF6B (rva 0x5AEF6B) mov     eax, [r14+320h]
     [W] 0x1405AEF8A (rva 0x5AEF8A) mov     [r14+320h], eax
     [W] 0x1405AEFCE (rva 0x5AEFCE) cmp     dword ptr [r14+320h], 7
     [W] 0x1405AEFD8 (rva 0x5AEFD8) mov     dword ptr [r14+320h], 7
     [W] 0x1405AF589 (rva 0x5AF589) cmp     byte ptr [r12+320h], 0
  FUNC sub_A9FC10 @0x140A9FC10 W=5 N=6 sub_140A9FC10
     [W] 0x140A9FD98 (rva 0xA9FD98) cmp     dword ptr [rdi+320h], 2
     [W] 0x140A9FDA1 (rva 0xA9FDA1) mov     dword ptr [rdi+320h], 2
     [r] 0x140A9FDF2 (rva 0xA9FDF2) mov     eax, [rdi+320h]
     [W] 0x140A9FE10 (rva 0xA9FE10) mov     [rdi+320h], eax
     [W] 0x140AA00F8 (rva 0xAA00F8) cmp     dword ptr [rdi+320h], 1
     [W] 0x140AA0101 (rva 0xAA0101) mov     dword ptr [rdi+320h], 1
  FUNC sub_A68F80 @0x140A68F80 W=4 N=6 sub_140A68F80
     [r] 0x140A69496 (rva 0xA69496) mov     eax, [rdi+320h]
     [W] 0x140A69571 (rva 0xA69571) cmp     dword ptr [rdi+320h], 3
     [W] 0x140A6957A (rva 0xA6957A) mov     dword ptr [rdi+320h], 3
     [r] 0x140A6970C (rva 0xA6970C) cmp     r8d, [rdi+320h]
     [W] 0x140A69715 (rva 0xA69715) mov     [rdi+320h], r8d
     [W] 0x140A69B02 (rva 0xA69B02) movaps  xmm6, xmmword ptr [rsp+320h]
  FUNC sub_AA0220 @0x140AA0220 W=4 N=6 sub_140AA0220
     [r] 0x140AA034A (rva 0xAA034A) mov     eax, [rsi+320h]
     [W] 0x140AA0366 (rva 0xAA0366) mov     [rsi+320h], eax
     [W] 0x140AA03A4 (rva 0xAA03A4) cmp     dword ptr [rsi+320h], 2
     [W] 0x140AA03AD (rva 0xAA03AD) mov     dword ptr [rsi+320h], 2
     [r] 0x140AA0460 (rva 0xAA0460) mov     eax, [rsi+320h]
     [W] 0x140AA047B (rva 0xAA047B) mov     [rsi+320h], eax
  FUNC sub_C06F50 @0x140C06F50 W=4 N=5 sub_140C06F50
     [W] 0x140C06FB8 (rva 0xC06FB8) db  83h
     [W] 0x140C06FC1 (rva 0xC06FC1) db 0C7h
     [r] 0x140C0776B (rva 0xC0776B) mov     eax, [rdi+320h]
     [W] 0x140C07786 (rva 0xC07786) mov     [rdi+320h], eax
     [W] 0x140C077A9 (rva 0xC077A9) cmp     byte ptr [rbx+320h], 0
  FUNC sub_CA7130 @0x140CA7130 W=4 N=6 sub_140CA7130
     [W] 0x140CA716D (rva 0xCA716D) db  83h
     [W] 0x140CA7176 (rva 0xCA7176) db 0C7h
     [r] 0x140CA71A5 (rva 0xCA71A5) db  8Bh
     [W] 0x140CA71C1 (rva 0xCA71C1) db  89h
     [r] 0x140CA7321 (rva 0xCA7321) db  8Bh
     [W] 0x140CA73C6 (rva 0xCA73C6) db  80h
  FUNC sub_CB2330 @0x140CB2330 W=4 N=5 sub_140CB2330
     [W] 0x140CB26ED (rva 0xCB26ED) db  83h
     [W] 0x140CB26F7 (rva 0xCB26F7) db 0C7h
     [r] 0x140CB272A (rva 0xCB272A) db  8Bh
     [W] 0x140CB2749 (rva 0xCB2749) db  89h
     [W] 0x140CB2B04 (rva 0xCB2B04) cmp     byte ptr [rbx+320h], 0
  FUNC sub_CCD2C0 @0x140CCD2C0 W=4 N=8 sub_140CCD2C0
     [r] 0x140CCD520 (rva 0xCCD520) mov     ecx, [rbx+320h]
     [W] 0x140CCD553 (rva 0xCCD553) movups  xmmword ptr [rbx+320h], xmm0
     [r] 0x140CCD581 (rva 0xCCD581) mov     ecx, [rbx+320h]
     [W] 0x140CCD5BA (rva 0xCCD5BA) movups  xmmword ptr [rbx+320h], xmm0
     [r] 0x140CCD736 (rva 0xCCD736) mov     ecx, [rbx+320h]
     [W] 0x140CCD769 (rva 0xCCD769) movups  xmmword ptr [rbx+320h], xmm0
; [scan +0x330] 400 hits in 1.3s

--- +0x330: 400 accesses (229 W / 171 r) ---
  FUNC (no func) W=18 N=28 
     [W] 0x1402DDBAE (rva 0x2DDBAE) db    0
     [r] 0x1403B2162 (rva 0x3B2162) db  8Dh
     [W] 0x1403BCEEC (rva 0x3BCEEC) db  89h
     [W] 0x1403E36B7 (rva 0x3E36B7) db 0FFh
     [r] 0x1403EBFC4 (rva 0x3EBFC4) db  8Bh
     [r] 0x140482354 (rva 0x482354) db  8Bh
  FUNC sub_11BC9F0 @0x1411BC9F0 W=8 N=9 sub_1411BC9F0
     [W] 0x1411BD780 (rva 0x11BD780) db 0FFh
     [W] 0x1411BD796 (rva 0x11BD796) db 0FFh
     [W] 0x1411BD7B6 (rva 0x11BD7B6) db 0FFh
     [W] 0x1411BD7DF (rva 0x11BD7DF) db 0FFh
     [W] 0x1411BD7FF (rva 0x11BD7FF) db 0FFh
     [r] 0x1411BDD08 (rva 0x11BDD08) db  8Dh
  FUNC sub_114EAC0 @0x14114EAC0 W=5 N=5 sub_14114EAC0
     [W] 0x14114ED2F (rva 0x114ED2F) db 0FFh
     [W] 0x14114ED5D (rva 0x114ED5D) db 0FFh
     [W] 0x14114ED77 (rva 0x114ED77) db 0FFh
     [W] 0x14114EE85 (rva 0x114EE85) db 0FFh
     [W] 0x14114EE96 (rva 0x114EE96) db 0FFh
  FUNC sub_1166910 @0x141166910 W=5 N=5 sub_141166910
     [W] 0x141166CF7 (rva 0x1166CF7) call    qword ptr [rax+330h]
     [W] 0x141166D79 (rva 0x1166D79) call    qword ptr [rax+330h]
     [W] 0x141166F33 (rva 0x1166F33) call    qword ptr [rax+330h]
     [W] 0x141166FD7 (rva 0x1166FD7) call    qword ptr [rax+330h]
     [W] 0x141166FE8 (rva 0x1166FE8) call    qword ptr [rax+330h]
  FUNC sub_C41A70 @0x140C41A70 W=4 N=4 sub_140C41A70
     [W] 0x140C420E0 (rva 0xC420E0) movaps  xmmword ptr [rbp+330h], xmm1
     [W] 0x140C42115 (rva 0xC42115) movaps  xmm0, xmmword ptr [rbp+330h]
     [W] 0x140C4211F (rva 0xC4211F) movaps  xmmword ptr [rbp+330h], xmm0
     [W] 0x140C43705 (rva 0xC43705) mov     [rcx+330h], eax
  FUNC sub_1106360 @0x141106360 W=4 N=5 sub_141106360
     [W] 0x1411065F7 (rva 0x11065F7) mov     byte ptr [rbp+330h], 0
     [W] 0x14110660F (rva 0x110660F) mov     byte ptr [rbp+330h], 1
     [r] 0x141106B43 (rva 0x1106B43) lea     rax, [rbp+330h]
     [W] 0x141106DFA (rva 0x1106DFA) cmp     byte ptr [rbp+330h], 0
     [W] 0x1411071A8 (rva 0x11071A8) cmp     byte ptr [rbp+330h], 0
  FUNC sub_6548B0 @0x1406548B0 W=3 N=7 sub_1406548B0
     [W] 0x140654903 (rva 0x654903) cmp     qword ptr [rbx+330h], 0
     [r] 0x140654927 (rva 0x654927) mov     rcx, [rbx+330h]
     [r] 0x140654944 (rva 0x654944) mov     rax, [rbx+330h]
     [r] 0x140654984 (rva 0x654984) mov     rax, [rbx+330h]
     [W] 0x14065499D (rva 0x65499D) cmp     qword ptr [rbx+330h], 0
     [r] 0x1406549C1 (rva 0x6549C1) mov     rcx, [rbx+330h]
  FUNC sub_C894A0 @0x140C894A0 W=3 N=3 sub_140C894A0
     [W] 0x140C89533 (rva 0xC89533) db 0FFh
     [W] 0x140C895B8 (rva 0xC895B8) db 0FFh
     [W] 0x140C895F8 (rva 0xC895F8) db 0FFh
  FUNC sub_C98BA0 @0x140C98BA0 W=3 N=4 sub_140C98BA0
     [W] 0x140C98FB6 (rva 0xC98FB6) mov     [rdi+330h], rsi
     [W] 0x140C98FCA (rva 0xC98FCA) mov     [rdi+330h], rsi
     [W] 0x140C9960F (rva 0xC9960F) movups  xmmword ptr [rcx+330h], xmm3
     [r] 0x140C9AFAF (rva 0xC9AFAF) lea     rcx, [rbp+330h]
  FUNC sub_E61BC0 @0x140E61BC0 W=3 N=3 sub_140E61BC0
     [W] 0x140E62304 (rva 0xE62304) mov     [rbp+330h], r14d
     [W] 0x140E62A4C (rva 0xE62A4C) mov     [rbp+330h], r14d
     [W] 0x140E6321E (rva 0xE6321E) mov     [rbp+330h], r14d
  FUNC sub_11691D0 @0x1411691D0 W=3 N=3 sub_1411691D0
     [W] 0x14116933F (rva 0x116933F) db  11h
     [W] 0x141169493 (rva 0x1169493) db  11h
     [W] 0x1411698E2 (rva 0x11698E2) movss   xmm1, dword ptr [rbp+330h]
  FUNC sub_11AB180 @0x1411AB180 W=3 N=4 sub_1411AB180
     [W] 0x1411AB530 (rva 0x11AB530) db 0FFh
     [r] 0x1411ABA8A (rva 0x11ABA8A) lea     rdx, [r10+330h]
     [W] 0x1411AC0A6 (rva 0x11AC0A6) movups  xmm0, xmmword ptr [rax+rcx+330h]
     [W] 0x1411ACB79 (rva 0x11ACB79) call    qword ptr [rax+330h]
; [scan +0x340] 400 hits in 0.4s

--- +0x340: 400 accesses (346 W / 54 r) ---
  FUNC (no func) W=3 N=8 
     [r] 0x140317F85 (rva 0x317F85) db  8Dh
     [W] 0x1403BCEFA (rva 0x3BCEFA) db  89h
     [W] 0x1403E36CF (rva 0x3E36CF) db 0FFh
     [r] 0x1404B6841 (rva 0x4B6841) db  8Bh
     [r] 0x1404B6A61 (rva 0x4B6A61) db  8Bh
     [r] 0x1404B7201 (rva 0x4B7201) db  8Bh
  FUNC sub_4ACD20 @0x1404ACD20 W=2 N=2 sub_1404ACD20
     [W] 0x1404ACF3A (rva 0x4ACF3A) db  89h
     [W] 0x1404AD07A (rva 0x4AD07A) mov     [rsi+340h], r14
  FUNC sub_515360 @0x140515360 W=2 N=2 sub_140515360
     [W] 0x140516157 (rva 0x516157) mov     [rdi+340h], rax
     [W] 0x1405163C0 (rva 0x5163C0) movdqu  xmmword ptr [rbp+340h], xmm0
  FUNC sub_58FA20 @0x14058FA20 W=2 N=2 sub_14058FA20
     [W] 0x1405910DD (rva 0x5910DD) mov     [rbp+340h], r15
     [W] 0x140591120 (rva 0x591120) mov     [rbp+340h], rbx
  FUNC sub_5CECA0 @0x1405CECA0 W=2 N=4 sub_1405CECA0
     [W] 0x1405CF53D (rva 0x5CF53D) mov     [rbp+340h], rax
     [r] 0x1405CF544 (rva 0x5CF544) lea     rax, [rbp+340h]
     [r] 0x1405CF5BB (rva 0x5CF5BB) lea     rax, [rbp+340h]
     [W] 0x1405CF7BE (rva 0x5CF7BE) cmp     qword ptr [rax+340h], 0
  FUNC sub_10980 @0x140010980 W=1 N=1 sub_140010980
     [W] 0x140010992 (rva 0x10992) mov     rax, gs:58h
  FUNC sub_1B430 @0x14001B430 W=1 N=1 sub_14001B430
     [W] 0x14001B442 (rva 0x1B442) mov     rax, gs:58h
  FUNC sub_1B4F0 @0x14001B4F0 W=1 N=1 sub_14001B4F0
     [W] 0x14001B502 (rva 0x1B502) mov     rax, gs:58h
  FUNC sub_1B5B0 @0x14001B5B0 W=1 N=1 sub_14001B5B0
     [W] 0x14001B5C2 (rva 0x1B5C2) mov     rax, gs:58h
  FUNC sub_245A0 @0x1400245A0 W=1 N=1 sub_1400245A0
     [W] 0x1400245B2 (rva 0x245B2) mov     rax, gs:58h
  FUNC sub_255A0 @0x1400255A0 W=1 N=1 sub_1400255A0
     [W] 0x1400255B2 (rva 0x255B2) mov     rax, gs:58h
  FUNC sub_2A3D0 @0x14002A3D0 W=1 N=1 sub_14002A3D0
     [W] 0x14002A3E2 (rva 0x2A3E2) mov     rax, gs:58h
; [scan +0x350] 400 hits in 1.3s

--- +0x350: 400 accesses (171 W / 229 r) ---
  FUNC (no func) W=15 N=22 
     [W] 0x1403BCF08 (rva 0x3BCF08) db  89h
     [W] 0x1403E36E7 (rva 0x3E36E7) db 0FFh
     [r] 0x1403EAF77 (rva 0x3EAF77) db  0Fh
     [r] 0x1403EC497 (rva 0x3EC497) db  8Bh
     [r] 0x1404B6F71 (rva 0x4B6F71) db  8Bh
     [W] 0x140592870 (rva 0x592870) movups  xmmword ptr [rcx+350h], xmm1
  FUNC sub_12B9040 @0x1412B9040 W=5 N=8 sub_1412B9040
     [W] 0x1412B90E6 (rva 0x12B90E6) db  89h
     [W] 0x1412B9106 (rva 0x12B9106) db  83h
     [W] 0x1412B912F (rva 0x12B912F) db  89h
     [W] 0x1412B9165 (rva 0x12B9165) db  89h
     [r] 0x1412B9185 (rva 0x12B9185) db  8Bh
     [W] 0x1412B91AD (rva 0x12B91AD) db  89h
  FUNC sub_11EDD30 @0x1411EDD30 W=4 N=6 sub_1411EDD30
     [W] 0x1411EF8A1 (rva 0x11EF8A1) db  11h
     [W] 0x1411EFE44 (rva 0x11EFE44) db  11h
     [W] 0x1411F06DE (rva 0x11F06DE) db  11h
     [W] 0x1411F0732 (rva 0x11F0732) db  0Fh
     [r] 0x1411F079D (rva 0x11F079D) db  8Dh
     [r] 0x1411F07F2 (rva 0x11F07F2) db  8Dh
  FUNC sub_583E30 @0x140583E30 W=3 N=3 sub_140583E30
     [W] 0x140584267 (rva 0x584267) call    sub_140B7FE20
     [W] 0x14058457E (rva 0x58457E) call    qword ptr [rax+8]
     [W] 0x1405846DA (rva 0x5846DA) mov     [rbp+350h], r13
  FUNC sub_40B000 @0x14040B000 W=2 N=2 sub_14040B000
     [W] 0x14040B22A (rva 0x40B22A) db  38h ; 8
     [W] 0x14040B233 (rva 0x40B233) db  88h
  FUNC sub_4ACD20 @0x1404ACD20 W=2 N=2 sub_1404ACD20
     [W] 0x1404ACF48 (rva 0x4ACF48) db  89h
     [W] 0x1404AD001 (rva 0x4AD001) mov     [rsi+350h], rcx
  FUNC sub_515360 @0x140515360 W=2 N=2 sub_140515360
     [W] 0x1405161ED (rva 0x5161ED) mov     [rdi+350h], rax
     [W] 0x140516619 (rva 0x516619) movdqu  xmmword ptr [rbp+350h], xmm0
  FUNC sub_58FA20 @0x14058FA20 W=2 N=2 sub_14058FA20
     [W] 0x140591127 (rva 0x591127) mov     [rbp+350h], r15
     [W] 0x14059116A (rva 0x59116A) mov     [rbp+350h], rbx
  FUNC sub_6E83E0 @0x1406E83E0 W=2 N=2 sub_1406E83E0
     [W] 0x1406E84FF (rva 0x6E84FF) db  29h ; )
     [W] 0x1406E9532 (rva 0x6E9532) movaps  xmm14, xmmword ptr [rsp+350h]
  FUNC sub_72CA00 @0x14072CA00 W=2 N=2 sub_14072CA00
     [W] 0x14072CD6F (rva 0x72CD6F) movups  xmm0, xmmword ptr [r10+350h]
     [W] 0x14072CD77 (rva 0x72CD77) movups  xmmword ptr [r11+350h], xmm0
  FUNC sub_98BB60 @0x14098BB60 W=2 N=4 sub_14098BB60
     [W] 0x14098BC0A (rva 0x98BC0A) mov     [rbx+350h], r14
     [r] 0x14098BEE8 (rva 0x98BEE8) mov     r14, [rbx+350h]
     [W] 0x14098BEEF (rva 0x98BEEF) mov     [rbx+350h], rsi
     [r] 0x14098C592 (rva 0x98C592) mov     rsi, [rbp+3A0h+var_50]
  FUNC sub_9E3B80 @0x1409E3B80 W=2 N=2 sub_1409E3B80
     [W] 0x1409E3EB0 (rva 0x9E3EB0) mov     [rsi+350h], r15
     [W] 0x1409E5DB3 (rva 0x9E5DB3) mov     [rsi+350h], rax
; [scan +0x360] 400 hits in 1.0s

--- +0x360: 400 accesses (154 W / 246 r) ---
  FUNC (no func) W=21 N=29 
     [W] 0x1403BCF16 (rva 0x3BCF16) db  89h
     [W] 0x1403E36FF (rva 0x3E36FF) db 0FFh
     [r] 0x14046C3C5 (rva 0x46C3C5) db  3Bh ; ;
     [W] 0x1404B8C61 (rva 0x4B8C61) db  83h
     [r] 0x1404BB541 (rva 0x4BB541) mov     r8, [rcx+360h]
     [r] 0x14058CDB0 (rva 0x58CDB0) db  8Bh
  FUNC sub_11A98A0 @0x1411A98A0 W=3 N=3 sub_1411A98A0
     [W] 0x1411AA0F2 (rva 0x11AA0F2) movups  xmmword ptr [r14+360h], xmm4
     [W] 0x1411AA1D7 (rva 0x11AA1D7) movups  xmm11, xmmword ptr [r14+360h]
     [W] 0x1411AA396 (rva 0x11AA396) movups  xmmword ptr [r14+360h], xmm0
  FUNC sub_58FA20 @0x14058FA20 W=2 N=2 sub_14058FA20
     [W] 0x140591171 (rva 0x591171) mov     [rbp+360h], r15
     [W] 0x1405911AB (rva 0x5911AB) mov     [rbp+360h], rbx
  FUNC sub_60A380 @0x14060A380 W=2 N=2 sub_14060A380
     [W] 0x14060A4A7 (rva 0x60A4A7) movss   xmm1, dword ptr [rcx+360h]
     [W] 0x14060A4B6 (rva 0x60A4B6) movss   xmm0, dword ptr [rax+360h]
  FUNC sub_687290 @0x140687290 W=2 N=3 sub_140687290
     [W] 0x140689826 (rva 0x689826) movdqa  xmmword ptr [rbp+360h], xmm0
     [W] 0x14068984C (rva 0x68984C) movdqa  xmmword ptr [rbp+360h], xmm0
     [r] 0x140689870 (rva 0x689870) lea     r8, [rbp+360h]
  FUNC sub_72CA00 @0x14072CA00 W=2 N=2 sub_14072CA00
     [W] 0x14072CD7F (rva 0x72CD7F) movups  xmm1, xmmword ptr [r10+360h]
     [W] 0x14072CD87 (rva 0x72CD87) movups  xmmword ptr [r11+360h], xmm1
  FUNC sub_7A1AC0 @0x1407A1AC0 W=2 N=3 sub_1407A1AC0
     [W] 0x1407A1B4C (rva 0x7A1B4C) db  11h
     [r] 0x1407A1DC9 (rva 0x7A1DC9) db  0Fh
     [W] 0x1407A1DD2 (rva 0x7A1DD2) db  11h
  FUNC sub_98BB60 @0x14098BB60 W=2 N=3 sub_14098BB60
     [W] 0x14098BC18 (rva 0x98BC18) mov     [rbx+360h], r14
     [r] 0x14098C40C (rva 0x98C40C) mov     rsi, [rbx+360h]
     [W] 0x14098C413 (rva 0x98C413) mov     [rbx+360h], rdx
  FUNC sub_B51AD0 @0x140B51AD0 W=2 N=4 sub_140B51AD0
     [W] 0x140B52669 (rva 0xB52669) mov     [rbp+360h], rax
     [W] 0x140B5268A (rva 0xB5268A) mov     [rbp+360h], rax
     [r] 0x140B52697 (rva 0xB52697) lea     rdx, [rbp+360h]
     [r] 0x140B526AB (rva 0xB526AB) lea     rcx, [rbp+360h]
  FUNC sub_B56C30 @0x140B56C30 W=2 N=4 sub_140B56C30
     [W] 0x140B57777 (rva 0xB57777) mov     [rbp+0D50h+var_9F0], rax
     [r] 0x140B57785 (rva 0xB57785) lea     rdx, [rbp+0D50h+var_9F0]
     [r] 0x140B57832 (rva 0xB57832) mov     rcx, [rbp+0D50h+var_9F0]
     [W] 0x140B57879 (rva 0xB57879) mov     byte ptr [rbp+0D50h+var_9F0], cl
  FUNC sub_C4FE80 @0x140C4FE80 W=2 N=2 sub_140C4FE80
     [W] 0x140C525C7 (rva 0xC525C7) movss   xmm0, dword ptr [rsi+360h]
     [W] 0x140C525F1 (rva 0xC525F1) mov     [rsi+360h], ecx
  FUNC sub_CF6200 @0x140CF6200 W=2 N=5 sub_140CF6200
     [r] 0x140CF66E4 (rva 0xCF66E4) db  8Bh
     [W] 0x140CF67FF (rva 0xCF67FF) db  89h
     [r] 0x140CF6806 (rva 0xCF6806) lea     rdx, [rbp+360h]
     [r] 0x140CF681C (rva 0xCF681C) xchg    rax, [rbp+360h]
     [W] 0x140CF6823 (rva 0xCF6823) mov     qword ptr [rbp+360h], 0
; [scan +0x540] 400 hits in 1.6s

--- +0x540: 400 accesses (219 W / 181 r) ---
  FUNC (no func) W=28 N=34 
     [r] 0x1403EB337 (rva 0x3EB337) db  0Fh
     [W] 0x1405929A6 (rva 0x5929A6) movups  xmmword ptr [rcx+540h], xmm0
     [r] 0x1405FBA31 (rva 0x5FBA31) db  8Bh
     [r] 0x1405FBA71 (rva 0x5FBA71) db  8Bh
     [W] 0x1407A4401 (rva 0x7A4401) db  10h
     [W] 0x1407A9834 (rva 0x7A9834) db  89h
  FUNC sub_2D4B180 @0x142D4B180 W=8 N=12 sub_142D4B180
     [W] 0x142D4B1A7 (rva 0x2D4B1A7) db  89h
     [W] 0x142D4B417 (rva 0x2D4B417) db 0C7h
     [r] 0x142D4B436 (rva 0x2D4B436) db  8Bh
     [W] 0x142D4B440 (rva 0x2D4B440) db  83h
     [W] 0x142D4B59D (rva 0x2D4B59D) db  83h
     [r] 0x142D4B5B6 (rva 0x2D4B5B6) db  8Bh
  FUNC sub_7A9880 @0x1407A9880 W=4 N=4 sub_1407A9880
     [W] 0x1407A98AD (rva 0x7A98AD) movups  xmmword ptr [rbx+540h], xmm5
     [W] 0x1407A98F1 (rva 0x7A98F1) movups  xmmword ptr [rbx+540h], xmm3
     [W] 0x1407A9963 (rva 0x7A9963) movups  xmmword ptr [rbx+540h], xmm5
     [W] 0x1407A99A7 (rva 0x7A99A7) movups  xmmword ptr [rbx+540h], xmm3
  FUNC sub_58FA20 @0x14058FA20 W=3 N=3 sub_14058FA20
     [W] 0x14058FBDD (rva 0x58FBDD) db  80h
     [W] 0x14058FD5E (rva 0x58FD5E) db  10h
     [W] 0x140591715 (rva 0x591715) cmp     [r14+540h], dil
  FUNC sub_DBC9C0 @0x140DBC9C0 W=3 N=4 sub_140DBC9C0
     [W] 0x140DBCD1B (rva 0xDBCD1B) mov     [r14+540h], rdi
     [W] 0x140DBD9C4 (rva 0xDBD9C4) mov     [r14+540h], rcx
     [r] 0x140DBDA67 (rva 0xDBDA67) mov     rcx, [r14+540h]
     [W] 0x140DBE9FF (rva 0xDBE9FF) mov     [rbp+0BB0h+var_670], 1
  FUNC sub_FCCAF0 @0x140FCCAF0 W=3 N=8 sub_140FCCAF0
     [r] 0x140FCCE3D (rva 0xFCCE3D) mov     rax, [rbp+540h]
     [W] 0x140FCD30F (rva 0xFCD30F) mov     byte ptr [rbp+540h], 80h
     [r] 0x140FCD3C8 (rva 0xFCD3C8) movzx   ecx, byte ptr [rbp+540h]
     [W] 0x140FCD3D3 (rva 0xFCD3D3) mov     byte ptr [rbp+540h], 0
     [W] 0x140FCE046 (rva 0xFCE046) mov     [rbp+540h], edi
     [r] 0x140FCE074 (rva 0xFCE074) lea     rdx, [rbp+540h]
  FUNC sub_1B356C0 @0x141B356C0 W=3 N=3 sub_141B356C0
     [W] 0x141B3790E (rva 0x1B3790E) db  10h
     [W] 0x141B37916 (rva 0x1B37916) db  10h
     [W] 0x141B3792A (rva 0x1B3792A) db  11h
  FUNC sub_3DAD30 @0x1403DAD30 W=2 N=3 sub_1403DAD30
     [r] 0x1403DB651 (rva 0x3DB651) movzx   eax, byte ptr [rbx+540h]
     [W] 0x1403DB658 (rva 0x3DB658) cmp     [rdi+540h], al
     [W] 0x1403DB667 (rva 0x3DB667) mov     [rdi+540h], al
  FUNC sub_40B520 @0x14040B520 W=2 N=2 sub_14040B520
     [W] 0x14040BA12 (rva 0x40BA12) db  38h ; 8
     [W] 0x14040BA22 (rva 0x40BA22) db  88h
  FUNC sub_581F40 @0x140581F40 W=2 N=2 sub_140581F40
     [W] 0x140581F8C (rva 0x581F8C) movups  xmmword ptr [rsi+540h], xmm0
     [W] 0x140581FE0 (rva 0x581FE0) and     [rsi+540h], cl
  FUNC sub_5EF780 @0x1405EF780 W=2 N=2 sub_1405EF780
     [W] 0x1405EF7E2 (rva 0x5EF7E2) mov     [r15+540h], rsi
     [W] 0x1405F08E3 (rva 0x5F08E3) db  89h
  FUNC sub_68E910 @0x14068E910 W=2 N=2 sub_14068E910
     [W] 0x140690AA8 (rva 0x690AA8) movups  xmmword ptr [rbp+540h], xmm0
     [W] 0x1406910C3 (rva 0x6910C3) mov     [rbp+540h], r15d
; [scan +0x550] 400 hits in 1.3s

--- +0x550: 400 accesses (184 W / 216 r) ---
  FUNC (no func) W=19 N=29 
     [W] 0x140592A2B (rva 0x592A2B) movups  xmmword ptr [rcx+550h], xmm1
     [W] 0x1407A39B2 (rva 0x7A39B2) db  10h
     [W] 0x1407A3B82 (rva 0x7A3B82) db  10h
     [W] 0x1407A435D (rva 0x7A435D) db  10h
     [r] 0x1407A4411 (rva 0x7A4411) db  8Bh
     [W] 0x14092C77E (rva 0x92C77E) mov     qword ptr [rcx+550h], 2FCAD433h
  FUNC sub_B418D0 @0x140B418D0 W=4 N=5 sub_140B418D0
     [W] 0x140B41913 (rva 0xB41913) db  89h
     [r] 0x140B4288B (rva 0xB4288B) db  8Bh
     [W] 0x140B429AD (rva 0xB429AD) db  89h
     [W] 0x140B42A79 (rva 0xB42A79) db  89h
     [W] 0x140B42B7C (rva 0xB42B7C) db  89h
  FUNC sub_108C230 @0x14108C230 W=4 N=8 sub_14108C230
     [W] 0x14108C2B3 (rva 0x108C2B3) mov     [rbp+550h], r12d
     [W] 0x14108C340 (rva 0x108C340) mov     [rbp+550h], r14d
     [r] 0x14108CC30 (rva 0x108CC30) mov     r14d, [rbp+550h]
     [W] 0x14108CFEE (rva 0x108CFEE) movss   dword ptr [rbp+550h], xmm1
     [r] 0x14108CFFC (rva 0x108CFFC) lea     rcx, [rbp+550h]
     [r] 0x14108D3D5 (rva 0x108D3D5) lea     rdx, [rbp+550h]
  FUNC sub_581F40 @0x140581F40 W=3 N=3 sub_140581F40
     [W] 0x140581F9A (rva 0x581F9A) movups  xmmword ptr [rsi+550h], xmm1
     [W] 0x140581FCE (rva 0x581FCE) mov     [rsi+550h], ecx
     [W] 0x14058202D (rva 0x58202D) db  89h
  FUNC sub_5EF780 @0x1405EF780 W=3 N=4 sub_1405EF780
     [r] 0x1405EF7F0 (rva 0x5EF7F0) lea     rbx, [r15+550h]
     [W] 0x1405F0730 (rva 0x5F0730) mov     byte ptr [rbp+448h], 1
     [W] 0x1405F0900 (rva 0x5F0900) db  0Fh
     [W] 0x1405F091B (rva 0x5F091B) db  89h
  FUNC sub_B56C30 @0x140B56C30 W=3 N=4 sub_140B56C30
     [W] 0x140B57630 (rva 0xB57630) mov     [rbp+0D50h+var_800], r14
     [W] 0x140B57651 (rva 0xB57651) mov     [rbp+0D50h+var_800], rax
     [r] 0x140B5765E (rva 0xB5765E) lea     rdx, [rbp+0D50h+var_800]
     [W] 0x140B57672 (rva 0xB57672) mov     [rbp+0D50h+var_800], r14
  FUNC sub_1B356C0 @0x141B356C0 W=3 N=3 sub_141B356C0
     [W] 0x141B378EF (rva 0x1B378EF) db  10h
     [W] 0x141B378F6 (rva 0x1B378F6) db  10h
     [W] 0x141B37906 (rva 0x1B37906) db  11h
  FUNC sub_36CEAC0 @0x1436CEAC0 W=3 N=5 sub_1436CEAC0
     [r] 0x1436CEE30 (rva 0x36CEE30) db  8Bh
     [W] 0x1436CEE47 (rva 0x36CEE47) db  89h
     [W] 0x1436CEE51 (rva 0x36CEE51) db  89h
     [r] 0x1436CEE7F (rva 0x36CEE7F) db  8Bh
     [W] 0x1436CEE96 (rva 0x36CEE96) db  89h
  FUNC sub_58FA20 @0x14058FA20 W=2 N=2 sub_14058FA20
     [W] 0x14058FD6D (rva 0x58FD6D) db  10h
     [W] 0x14059179C (rva 0x59179C) cmp     dword ptr [r14+550h], 1
  FUNC sub_72CA00 @0x14072CA00 W=2 N=2 sub_14072CA00
     [W] 0x14072CFB4 (rva 0x72CFB4) movups  xmm0, xmmword ptr [r10+550h]
     [W] 0x14072CFBC (rva 0x72CFBC) movups  xmmword ptr [r11+550h], xmm0
  FUNC sub_BD1F70 @0x140BD1F70 W=2 N=2 sub_140BD1F70
     [W] 0x140BD2467 (rva 0xBD2467) movups  xmm0, xmmword ptr [rdi+550h]
     [W] 0x140BD246E (rva 0xBD246E) movups  xmmword ptr [rbx+550h], xmm0
  FUNC sub_C4FE80 @0x140C4FE80 W=2 N=2 sub_140C4FE80
     [W] 0x140C51E3D (rva 0xC51E3D) movss   xmm0, dword ptr [rsi+550h]
     [W] 0x140C51E67 (rva 0xC51E67) mov     [rsi+550h], ecx
; [scan +0x5C8] 400 hits in 1.1s

--- +0x5C8: 400 accesses (165 W / 235 r) ---
  FUNC (no func) W=18 N=25 
     [r] 0x1403EB5E4 (rva 0x3EB5E4) db  0Fh
     [W] 0x1407A43CD (rva 0x7A43CD) db  10h
     [r] 0x1407A4486 (rva 0x7A4486) db  8Bh
     [W] 0x1407A4BBD (rva 0x7A4BBD) db  10h
     [W] 0x1407A945B (rva 0x7A945B) db  11h
     [W] 0x1407A948A (rva 0x7A948A) db  89h
  FUNC sub_7D0DD0 @0x1407D0DD0 W=4 N=6 sub_1407D0DD0
     [W] 0x1407D0E04 (rva 0x7D0E04) mov     [rbp+5C8h], r12d
     [W] 0x1407D1053 (rva 0x7D1053) mov     [rbp+5C8h], r12d
     [r] 0x1407D1363 (rva 0x7D1363) mov     esi, [rbp+5C8h]
     [W] 0x1407D164C (rva 0x7D164C) mov     [rbp+5C8h], esi
     [W] 0x1407D1CD4 (rva 0x7D1CD4) or      dword ptr [rbp+5C8h], 1F000h
     [r] 0x1407D202B (rva 0x7D202B) mov     esi, [rbp+5C8h]
  FUNC sub_470A620 @0x14470A620 W=4 N=4 sub_14470A620
     [W] 0x14470A669 (rva 0x470A669) db  10h
     [W] 0x14470A748 (rva 0x470A748) db  10h
     [W] 0x14470A837 (rva 0x470A837) db  10h
     [W] 0x14470A8FE (rva 0x470A8FE) db  11h
  FUNC sub_10F62F0 @0x1410F62F0 W=3 N=3 sub_1410F62F0
     [W] 0x1410F63C4 (rva 0x10F63C4) movss   xmm4, dword ptr [rbx+5C8h]
     [W] 0x1410F6563 (rva 0x10F6563) movss   xmm0, dword ptr [rbx+5C8h]
     [W] 0x1410F66FC (rva 0x10F66FC) movss   xmm0, dword ptr [rbx+5C8h]
  FUNC sub_11BC9F0 @0x1411BC9F0 W=3 N=3 sub_1411BC9F0
     [W] 0x1411BD82F (rva 0x11BD82F) db 0C6h
     [W] 0x1411BE9DE (rva 0x11BE9DE) db  80h
     [W] 0x1411BEA5D (rva 0x11BEA5D) db  88h
  FUNC sub_1A25C40 @0x141A25C40 W=3 N=3 sub_141A25C40
     [W] 0x141A25CE4 (rva 0x1A25CE4) db  10h
     [W] 0x141A25CEB (rva 0x1A25CEB) db  11h
     [W] 0x141A25D12 (rva 0x1A25D12) db  88h
  FUNC sub_3BAB30 @0x1403BAB30 W=2 N=3 sub_1403BAB30
     [r] 0x1403BC21A (rva 0x3BC21A) db  0Fh
     [W] 0x1403BC222 (rva 0x3BC222) db  39h ; 9
     [W] 0x1403BC22B (rva 0x3BC22B) db  89h
  FUNC sub_3D6B50 @0x1403D6B50 W=2 N=3 sub_1403D6B50
     [r] 0x1403D8281 (rva 0x3D8281) movzx   eax, word ptr [rbx+5C8h]
     [W] 0x1403D8289 (rva 0x3D8289) cmp     [rdi+5C8h], ax
     [W] 0x1403D8299 (rva 0x3D8299) mov     [rdi+5C8h], ax
  FUNC sub_5FE310 @0x1405FE310 W=2 N=5 sub_1405FE310
     [W] 0x1405FE355 (rva 0x5FE355) db 0C6h
     [r] 0x1405FF28C (rva 0x5FF28C) movzx   esi, byte ptr [rbp+5C8h]
     [r] 0x1405FF4F1 (rva 0x5FF4F1) movzx   esi, byte ptr [rbp+5C8h]
     [W] 0x14060006D (rva 0x60006D) mov     byte ptr [rbp+5C8h], 0
     [r] 0x14060027E (rva 0x60027E) movzx   eax, byte ptr [rbp+5C8h]
  FUNC sub_7A3ED0 @0x1407A3ED0 W=2 N=2 sub_1407A3ED0
     [W] 0x1407A3F71 (rva 0x7A3F71) movsd   xmm5, qword ptr [rdi+5C8h]
     [W] 0x1407A4025 (rva 0x7A4025) movsd   xmm3, qword ptr [rdi+5C8h]
  FUNC sub_7A6AC0 @0x1407A6AC0 W=2 N=2 sub_1407A6AC0
     [W] 0x1407A6B52 (rva 0x7A6B52) db  10h
     [W] 0x1407A6CCD (rva 0x7A6CCD) db  11h
  FUNC sub_7A9210 @0x1407A9210 W=2 N=2 sub_1407A9210
     [W] 0x1407A9223 (rva 0x7A9223) movsd   xmm2, qword ptr [rcx+5C8h]
     [W] 0x1407A92B4 (rva 0x7A92B4) movsd   qword ptr [rbx+5C8h], xmm2
; [scan +0x660] 400 hits in 1.0s

--- +0x660: 400 accesses (181 W / 219 r) ---
  FUNC (no func) W=16 N=30 
     [W] 0x140592AD3 (rva 0x592AD3) movups  xmmword ptr [rcx+660h], xmm2
     [W] 0x1405FB9AB (rva 0x5FB9AB) db  10h
     [W] 0x1407A24DF (rva 0x7A24DF) db  89h
     [W] 0x14092C9B1 (rva 0x92C9B1) mov     dword ptr [rcx+660h], 0Ch
     [r] 0x140E9339F (rva 0xE9339F) db  87h
     [W] 0x140E933A6 (rva 0xE933A6) db  89h
  FUNC sub_377FF00 @0x14377FF00 W=5 N=5 sub_14377FF00
     [W] 0x14377FF75 (rva 0x377FF75) db  83h
     [W] 0x14377FFA7 (rva 0x377FFA7) db 0C7h
     [W] 0x14377FFBD (rva 0x377FFBD) db  83h
     [W] 0x14377FFD6 (rva 0x377FFD6) db 0C7h
     [W] 0x14377FFF6 (rva 0x377FFF6) db  83h
  FUNC sub_5EF780 @0x1405EF780 W=3 N=3 sub_1405EF780
     [W] 0x1405EF900 (rva 0x5EF900) movups  xmmword ptr [r15+660h], xmm1
     [W] 0x1405EFDA7 (rva 0x5EFDA7) mov     byte ptr [rbp+8], 0
     [W] 0x1405EFE46 (rva 0x5EFE46) mov     byte ptr [rbp+48h], 0
  FUNC sub_577930 @0x140577930 W=2 N=2 sub_140577930
     [W] 0x140577DC5 (rva 0x577DC5) db  10h
     [W] 0x140577DCC (rva 0x577DCC) db  11h
  FUNC sub_63D0D0 @0x14063D0D0 W=2 N=2 sub_14063D0D0
     [W] 0x14063D70A (rva 0x63D70A) movups  xmm0, xmmword ptr [rdi+660h]
     [W] 0x14063D711 (rva 0x63D711) movups  xmmword ptr [rbx+660h], xmm0
  FUNC sub_BD1F70 @0x140BD1F70 W=2 N=2 sub_140BD1F70
     [W] 0x140BD2531 (rva 0xBD2531) movups  xmm0, xmmword ptr [rdi+660h]
     [W] 0x140BD2538 (rva 0xBD2538) movups  xmmword ptr [rbx+660h], xmm0
  FUNC sub_D400E0 @0x140D400E0 W=2 N=2 sub_140D400E0
     [W] 0x140D4040C (rva 0xD4040C) movups  xmm0, xmmword ptr [rbx+660h]
     [W] 0x140D40459 (rva 0xD40459) movups  xmmword ptr [rdi+660h], xmm0
  FUNC sub_1033D80 @0x141033D80 W=2 N=2 sub_141033D80
     [W] 0x141035D53 (rva 0x1035D53) mov     [rbp+660h], ecx
     [W] 0x141035D5C (rva 0x1035D5C) mov     [rbp+660h], r13d
  FUNC sub_1143220 @0x141143220 W=2 N=2 sub_141143220
     [W] 0x141143D47 (rva 0x1143D47) movups  xmm1, xmmword ptr [rdi+660h]
     [W] 0x141143D4E (rva 0x1143D4E) movups  xmmword ptr [rbx+660h], xmm1
  FUNC sub_1B310E0 @0x141B310E0 W=2 N=2 sub_141B310E0
     [W] 0x141B31BA3 (rva 0x1B31BA3) db  10h
     [W] 0x141B31BAA (rva 0x1B31BAA) db  11h
  FUNC sub_3457230 @0x143457230 W=2 N=5 sub_143457230
     [W] 0x143457A14 (rva 0x3457A14) db  89h
     [r] 0x143457B22 (rva 0x3457B22) db  8Bh
     [W] 0x143457B29 (rva 0x3457B29) db  89h
     [r] 0x143457BB3 (rva 0x3457BB3) db  8Bh
     [r] 0x143457BC3 (rva 0x3457BC3) db  8Bh
  FUNC sub_3B446C0 @0x143B446C0 W=2 N=5 sub_143B446C0
     [W] 0x143B448B4 (rva 0x3B448B4) db  83h
     [r] 0x143B44912 (rva 0x3B44912) db  8Dh
     [W] 0x143B44990 (rva 0x3B44990) db  83h
     [r] 0x143B449E2 (rva 0x3B449E2) db  8Dh
     [r] 0x143B44A24 (rva 0x3B44A24) db  8Bh

===== QUERY 4b: writer prologue AOBs (+0x320 / +0x350 writers) =====
  sub_99E0 @0x1400099E0 sub_1400099E0
    prologue AOB(48): 48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 41 54 41 55 41 56 41 57 48 8D AC 24 60 ED FF FF B8 A0 13 00 00 E8 F6 00 D3 04 48 2B E0 45 33 ED
  sub_D760 @0x14000D760 sub_14000D760
    prologue AOB(48): 48 89 5C 24 10 55 48 8D AC 24 F0 FC FF FF 48 81 EC 10 04 00 00 48 C7 45 78 0F 00 00 00 48 C7 45 70 09 00 00 00 F2 0F 10 05 5B 08 DE 05 F2 0F 11
  sub_2BF810 @0x1402BF810 sub_1402BF810
    prologue AOB(48): 48 89 5C 24 08 55 48 8D AC 24 C0 F8 FF FF 48 81 EC 40 08 00 00 33 DB 48 89 5C 24 30 48 C7 44 24 38 0F 00 00 00 88 5C 24 20 8D 4B 30 E8 7F C2 75
  sub_2D64F0 @0x1402D64F0 sub_1402D64F0
    prologue AOB(48): 40 55 48 8D AC 24 E0 F7 FF FF 48 81 EC 20 09 00 00 48 8D 0D 30 0E 1D 06 E8 03 2C D2 00 89 44 24 20 48 8D 05 20 63 2F 06 48 89 44 24 28 48 8D 44
  sub_3D0830 @0x1403D0830 sub_1403D0830
    prologue AOB(48): 48 89 5C 24 18 48 89 6C 24 20 48 89 4C 24 08 56 57 41 54 41 56 41 57 48 83 EC 30 48 8B F9 48 8D 15 B7 FC A0 05 E8 96 FD 56 01 48 8D 05 F7 44 A2
  sub_3D0E50 @0x1403D0E50 sub_1403D0E50
    prologue AOB(48): 48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 48 89 4C 24 08 57 41 54 41 55 41 56 41 57 48 83 EC 20 48 8B F9 48 8D 15 95 48 A1 05 E8 70 F7 56 01
  sub_3D1780 @0x1403D1780 sub_1403D1780
    prologue AOB(48): 48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 48 89 4C 24 08 57 41 54 41 55 41 56 41 57 48 83 EC 30 48 8B F9 48 8D 15 D5 B1 A0 05 E8 40 EE 56 01
  sub_3D1F00 @0x1403D1F00 sub_1403D1F00
    prologue AOB(48): 48 89 5C 24 18 48 89 4C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 30 48 8B F9 48 8D 15 7D 62 A1 05 E8 C8 E6 56 01 48 8D 05 C9 20 A2 05 48
  sub_408AE0 @0x140408AE0 sub_140408AE0
    prologue AOB(48): 40 53 48 83 EC 20 48 83 7A 18 10 4C 8B CA 48 8B 59 10 4C 8B C2 4C 8B 5A 10 72 03 4C 8B 02 49 81 FB B4 00 00 00 0F 87 94 00 00 00 4C 8D 93 54 03
  sub_40B000 @0x14040B000 sub_14040B000
    prologue AOB(48): 48 89 5C 24 08 57 48 83 EC 20 48 8B D9 BA FF FF FF FF 48 8B 89 A0 00 00 00 E8 62 CC FF FF 48 8B 83 A0 00 00 00 80 78 38 01 48 8D 48 20 48 8B D0
  sub_415C30 @0x140415C30 sub_140415C30
    prologue AOB(48): 48 89 5C 24 08 57 48 83 EC 20 48 8B DA 48 8B F9 48 8B CB BA 0A 00 00 00 E8 33 0E 4F 01 BA 08 00 00 00 89 07 48 8B CB E8 74 0F 4F 01 BA 08 00 00
  sub_437AA0 @0x140437AA0 sub_140437AA0
    prologue AOB(48): 48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 48 89 4C 24 08 57 41 54 41 55 41 56 41 57 48 83 EC 30 48 8B F9 48 8D 15 65 F3 9B 05 E8 20 8B 50 01
  sub_479F40 @0x140479F40 sub_140479F40
    prologue AOB(48): 48 89 5C 24 10 48 89 6C 24 18 48 89 4C 24 08 56 57 41 56 48 83 EC 20 48 8B F9 48 8D 15 D7 2B 98 05 E8 8A 66 4C 01 48 8D 05 DB 61 98 05 48 89 07
  sub_488AD0 @0x140488AD0 sub_140488AD0
    prologue AOB(48): 48 89 5C 24 08 57 48 83 EC 20 48 8B D9 48 8B 49 10 80 79 18 00 74 0D C6 41 18 00 E8 F0 E0 4B 01 48 8B 4B 10 48 83 C1 20 33 FF 48 89 79 18 E8 DD
  sub_4ACD20 @0x1404ACD20 sub_1404ACD20
    prologue AOB(48): 48 89 5C 24 10 48 89 6C 24 18 48 89 4C 24 08 56 57 41 56 48 83 EC 30 49 8B E9 49 8B F8 48 8B DA 48 8B F1 48 8D 05 EE 6C 95 05 48 89 01 48 8D 05
  sub_515360 @0x140515360 sub_140515360
    prologue AOB(48): 48 89 5C 24 18 48 89 4C 24 08 55 56 57 48 8D AC 24 A0 FC FF FF 48 81 EC 60 04 00 00 48 8B F9 B9 50 00 00 00 E8 37 67 50 04 48 8B C8 48 89 85 88

; DONE 29.6s -> E:/Github/vrframework/_agent_reports/fh5_empress_writers_vtbl_FAST.md
