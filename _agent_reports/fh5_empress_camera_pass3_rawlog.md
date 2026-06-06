# pass3

```
=============== PASS3: object-relative filter ===============
object-relative BOTH candidates: 6

  [score 16] FUNC 0x140C98BA0 (rva 0xC98BA0) sub_140C98BA0  same_base={'rcx'}
      OUT +0x320 w [rdi] @0x140C98F9A: mov     [rdi+320h], rsi
      OUT +0x320 w [rdi] @0x140C98FAE: mov     [rdi+320h], rsi
      OUT +0x320 w [rcx] @0x140C99607: movups  xmmword ptr [rcx+320h], xmm2
      OUT +0x330 w [rdi] @0x140C98FB5: mov     [rdi+330h], rsi
      OUT +0x330 w [rdi] @0x140C98FC9: mov     [rdi+330h], rsi
      OUT +0x330 w [rcx] @0x140C9960E: movups  xmmword ptr [rcx+330h], xmm3
      IN  +0x540   [rcx] @0x140C99830: movups  xmmword ptr [rcx+540h], xmm0
      IN  +0x550   [rcx] @0x140C99837: movups  xmmword ptr [rcx+550h], xmm1

  [score 14] FUNC 0x14072CA00 (rva 0x72CA00) sub_14072CA00  same_base={'r11'}
      OUT +0x320 w [r11] @0x14072CD45: movups  xmmword ptr [r11+320h], xmm0
      OUT +0x330 w [r11] @0x14072CD55: movups  xmmword ptr [r11+330h], xmm1
      OUT +0x340 w [r11] @0x14072CD65: movups  xmmword ptr [r11+340h], xmm0
      OUT +0x350 w [r11] @0x14072CD75: movups  xmmword ptr [r11+350h], xmm0
      OUT +0x360 w [r11] @0x14072CD85: movups  xmmword ptr [r11+360h], xmm1
      IN  +0x540   [r10] @0x14072CFA2: movups  xmm1, xmmword ptr [r10+540h]
      IN  +0x540   [r11] @0x14072CFAA: movups  xmmword ptr [r11+540h], xmm1
      IN  +0x550   [r10] @0x14072CFB2: movups  xmm0, xmmword ptr [r10+550h]
      IN  +0x550   [r11] @0x14072CFBA: movups  xmmword ptr [r11+550h], xmm0

  [score 11] FUNC 0x1403D0E50 (rva 0x3D0E50) sub_1403D0E50  same_base={'rdi'}
      OUT +0x320 w [rdi] @0x1403D11B2: mov     [rdi+320h], r13
      OUT +0x340 w [rdi] @0x1403D11D3: mov     [rdi+340h], r15
      OUT +0x360 w [rdi] @0x1403D11F4: mov     [rdi+360h], r15
      IN  +0x540   [rdi] @0x1403D13E1: lea     rcx, [rdi+540h]
      IN  +0x540   [rdi] @0x1403D13F4: mov     [rdi+540h], rax
      IN  +0x558   [rdi] @0x1403D13FB: mov     [rdi+558h], r12

  [score 4] FUNC 0x1407A6300 (rva 0x7A6300) sub_1407A6300  same_base=-
      OUT +0x328 w [rax] @0x1407A635E: call    qword ptr [rax+328h]
      IN  +0x540   [r14] @0x1407A6382: movups  xmm0, xmmword ptr [r14+540h]
      IN  +0x550   [r14] @0x1407A63B2: mov     [r14+550h], rcx
      IN  +0x558   [r14] @0x1407A63BD: mov     [r14+558h], rcx

  [score 2] FUNC 0x141024270 (rva 0x1024270) sub_141024270  same_base=-
      OUT +0x320 w [rcx] @0x141024BBE: cmp     byte ptr [rcx+320h], 0
      IN  +0x558   [rdi] @0x141024331: mov     [rdi+558h], rax

  [score 2] FUNC 0x1407A6460 (rva 0x7A6460) sub_1407A6460  same_base=-
      OUT +0x328 w [rax] @0x1407A65E2: call    qword ptr [rax+328h]
      IN  +0x558   [r15] @0x1407A64A6: mov     [r15+558h], bl

=============== decompile top object-relative ===============

########## 0x140C98BA0 (rva 0xC98BA0) sub_140C98BA0 ##########
  AOB: 48 89 5C 24 10 48 89 4C 24 08 55 56 57 41 54 41 55 41 56 41 57
  CALLERS(0): 
__int64 __fastcall sub_140C98BA0(__int64 a1, __int64 a2)
{
  _QWORD *v4; // r14
  __int64 v5; // rax
  __int64 v6; // rax
  __int64 v7; // rax
  __int64 v8; // rax
  volatile signed __int32 *v9; // rbx
  __int64 v10; // rcx
  volatile signed __int32 *v11; // rsi
  __int64 v12; // rdx
  _DWORD *v13; // r8
  _OWORD *v14; // rcx
  __int64 v15; // r9
  __int128 v16; // xmm1
  __int128 v17; // xmm2
  __int128 v18; // xmm3
  __int128 v19; // xmm1
  __int128 v20; // xmm2
  __int128 v21; // xmm3
  __int128 v22; // xmm1
  __int128 v23; // xmm2
  __int128 v24; // xmm3
  __int128 v25; // xmm1
  __int128 v26; // xmm2
  __int128 v27; // xmm3
  __int128 v28; // xmm1
  __int128 v29; // xmm2
  __int128 v30; // xmm3
  __int128 v31; // xmm1
  __int128 v32; // xmm2
  __int128 v33; // xmm3
  __int128 v34; // xmm1
  __int128 v35; // xmm2
  __int128 v36; // xmm3
  __int128 v37; // xmm1
  __int128 v38; // xmm2
  __int128 v39; // xmm3
  __int128 v40; // xmm1
  __int128 v41; // xmm2
  __int128 v42; // xmm3
  __int128 v43; // xmm1
  __int128 v44; // xmm2
  __int128 v45; // xmm3
  __int128 v46; // xmm1
  __int128 v47; // xmm2
  __int128 v48; // xmm3
  __int128 v49; // xmm1
  __int128 v50; // xmm2
  __int128 v51; // xmm3
  __int128 v52; // xmm1
  __int128 v53; // xmm2
  __int128 v54; // xmm3
  __int128 v55; // xmm1
  __int128 v56; // xmm2
  __int128 v57; // xmm3
  __int128 v58; // xmm1
  __int128 v59; // xmm2
  __int128 v60; // xmm3
  __int128 v61; // xmm1
  __int128 v62; // xmm2
  __int128 v63; // xmm3
  __int128 v64; // xmm1
  __int128 v65; // xmm2
  __int128 v66; // xmm3
  __int128 v67; // xmm1
  __int128 v68; // xmm2
  __int128 v69; // xmm3
  __int128 v70; // xmm1
  __int128 v71; // xmm2
  __int128 v72; // xmm3
  __int128 v73; // xmm1
  __int128 v74; // xmm2
  __int128 v75; // xmm3
  __int128 v76; // xmm1
  __int128 v77; // xmm2
  __int128 v78; // xmm3
  __int128 v79; // xmm1
  __int128 v80; // xmm2
  __int128 v81; // xmm3
  __int128 v82; // xmm1
  __int128 v83; // xmm2
  __int128 v84; // xmm3
  __int128 v85; // xmm1
  __int128 v86; // xmm2
  __int128 v87; // xmm3
  __int128 v88; // xmm1
  __int128 v89; // xmm2
  __int128 v90; // xmm3
  __int128 v91; // xmm1
  __int128 v92; // xmm2
  __int128 v93; // xmm3
  __int128 v94; // xmm1
  __int128 v95; // xmm2
  __int128 v96; // xmm3
  __int128 v97; // xmm1
  __int128 v98; // xmm2
  __int128 v99; // xmm3
  __int128 v100; // xmm1
  __int128 v101; // xmm2
  __int128 v102; // xmm3
  __int128 v103; // xmm1
  __int128 v104; // xmm2
  __int128 v105; // xmm3
  __int128 v106; // xmm1
  __int128 v107; // xmm2
  __int128 v108; // xmm3
  __int128 v109; // xmm1
  __int128 v110; // xmm2
  __int128 v111; // xmm3
  __int64 v112; // rbx
  char *v113; // rax
  __int64 v114; // rdx
  __int64 v115; // rcx
  unsigned __int64 v116; // rdx
  __int64 v117; // rcx
  char *v118; // rax
  __int64 v119; // rdx
  __int64 v120; // rcx
  unsigned __int64 v121; // rdx
  __int64 v122; // rcx
  char *v123; // rax
  __int64 v124; // rdx
  __int64 v125; // rcx
  unsigned __int64 v126; // rdx
  __int64 v127; // rcx
  char *v128; // rax
  unsigned __int64 v129; // rdx
  __int64 v130; // rcx
  unsigned __int64 v131; // rdx
  __int64 v132; // rcx
  unsigned __int64 v133; // rdx
  __int64 v134; // rcx
  unsigned __int64 v135; // rdx
  __int64 v136; // rcx
  unsigned __int64 v137; // rdx
  __int64 v138; // rcx
  unsigned __int64 v139; // rdx
  __int64 v140; // rcx
  unsigned __int64 v141; // rdx
  __int64 v142; // rcx
  unsigned __int64 v143; // rdx
  __int64 v144; // rcx
  unsigned __int64 v145; // rdx
  __int64 v146; // rcx
  unsigned __int64 v147; // rdx
  __int64 v148; // rcx
  unsigned __int64 v149; // rdx
  __int64 v150; // rcx
  unsigned __int64 v151; // rdx
  __int64 v152; // rcx
  unsigned __int64 v153; // rdx
  __int64 v154; // rcx
  unsigned __int64 v155; // rdx
  __int64 v156; // rcx
  unsigned __int64 v157; // rdx
  __int64 v158; // rcx
  unsigned __int64 v159; // rdx
  __int64 v160; // rcx
  unsigned __int64 v161; // rdx
  __int64 v162; // rcx
  unsigned __int64 v163; // rdx
  __int64 v164; // rcx
  __int64 v165; // rbx
  __int64 v166; // rbx
  __int64 v167; // rbx
  __int64 v168; // rbx
  __int64 v169; // rbx
  __int64 v170; // rbx
  __int64 v171; // rbx
  __int64 v172; // rbx
  __int64 v173; // rbx
  __int64 v174; // rbx
  __int64 v175; // rbx
  __int64 v176; // rbx
  __int64 v177; // rbx
  __int64 v178; // rbx
  __int64 v179; // rbx
  __int64 v180; // rbx
  __int64 *v181; // rax
  __int64 v182; // rcx
  __int64 v183; // rdx
  __int64 v184; // rcx
  __int64 v185; // rax
  __int64 v186; // rdx
  __int64 v187; // rcx
  __int64 v188; // r8
  int v189; // ebx
  __int64 v190; // r8
  __int64 v191; // r8
  int v192; // ebx
  __int64 v193; // r8
  __int64 v194; // rax
  __int64 v195; // r8
  __int64 v196; // rdx
  __int64 v197; // rcx
  int v198; // ebx
  __int64 v199; // r8
  __int128 v201; // [rsp+60h] [rbp-A0h] BYREF
  __int128 v202; // [rsp+70h] [rbp-90h]
  _DWORD v203[2]; // [rsp+80h] [rbp-80h] BYREF
  __int64 v204; // [rsp+88h] [rbp-78h]
  int v205; // [rsp+90h] [rbp-70h]
  _DWORD v206[2]; // [rsp+98h] [rbp-68h] BYREF
  __int64 v207; // [rsp+A0h] [rbp-60h]
  int v208; // [rsp+A8h] [rbp-58h]
  _DWORD v209[2]; // [rsp+B0h] [rbp-50h] BYREF
  __int64 v210; // [rsp+B8h] [rbp-48h]
  int v211; // [rsp+C0h] [rbp-40h]
  _BYTE v212[8]; // [rsp+C8h] [rbp-38h] BYREF
  _BYTE v213[4]; // [rsp+D0h] [rbp-30h] BYREF
  unsigned int v214; // [rsp+D4h] [rbp-2Ch]
  _BYTE v215[8]; // [rsp+D8h] [rbp-28h] BYREF
  __int64 (__fastcall **v216)(); // [rsp+E0h] [rbp-20h] BYREF
  __int64 v217; // [rsp+E8h] [rbp-18h] BYREF
  __m128i v218; // [rsp+F8h] [rbp-8h]
  __int64 (__fastcall **v219)(); // [rsp+108h] [rbp+8h] BYREF
  __int64 v220; // [rsp+110h] [rbp+10h] BYREF
  __m128i si128; // [rsp+120h] [rbp+20h]
  __int64 (__fastcall **v222)(); // [rsp+130h] [rbp+30h] BYREF
  __int64 v223; // [rsp+138h] [rbp+38h] BYREF
  __m128i v224; // [rsp+148h] [rbp+48h]
  _QWORD v225[3]; // [rsp+158h] [rbp+58h] BYREF
  unsigned __int64 v226; // [rsp+170h] [rbp+70h]
  _QWORD v227[3]; // [rsp+178h] [rbp+78h] BYREF
  unsigned __int64 v228; // [rsp+190h] [rbp+90h]
  _QWORD v229[3]; // [rsp+198h] [rbp+98h] BYREF
  unsigned __int64 v230; // [rsp+1B0h] [rbp+B0h]
  _QWORD v231[3]; // [rsp+1B8h] [rbp+B8h] BYREF
  unsigned __int64 v232; // [rsp+1D0h] [rbp+D0h]
  _QWORD v233[3]; // [rsp+1D8h] [rbp+D8h] BYREF
  unsigned __int64 v234; // [rsp+1F0h] [rbp+F0h]
  _QWORD v235[3]; // [rsp+1F8h] [rbp+F8h] BYREF
  unsigned __int64 v236; // [rsp+210h] [rbp+110h]
  _QWORD v237[3]; // [rsp+218h] [rbp+118h] BYREF
  unsigned __int64 v238; // [rsp+230h] [rbp+130h]
  _QWORD v239[3]; // [rsp+238h] [rbp+138h] BYREF
  unsigned __int64 v240; // [rsp+250h] [rbp+150h]
  _QWORD v241[3]; // [rsp+258h] [rbp+158h] BYREF
  unsigned __int64 v242; // [rsp+270h] [rbp+170h]
  ...[+1356]

########## 0x14072CA00 (rva 0x72CA00) sub_14072CA00 ##########
  AOB: 48 8B 41 10 4C 8B D2 4C 8B D9 4C 63 48 04 48 8B 42 10 4C 63 40 04
  CALLERS(6): 0x14072B78A(in 0x14072B120), 0x14072B79B(in 0x14072B120), 0x14072B7AB(in 0x14072B120), 0x14072B7BC(in 0x14072B120), 0x14072B7CC(in 0x14072B120), 0x14072B7DD(in 0x14072B120)
__int64 __fastcall sub_14072CA00(__int64 a1, __int64 a2)
{
  __int64 v4; // r9
  __int64 v5; // r8
  __int64 v6; // rdx
  __int64 v7; // rcx
  __int64 v8; // rdx
  __int64 v9; // rcx
  __int64 v10; // rdx
  __int64 v11; // rcx
  __int64 v12; // rdx
  __int64 v13; // rcx
  __int64 result; // rax

  v4 = *(int *)(*(_QWORD *)(a1 + 16) + 4LL);
  v5 = *(int *)(*(_QWORD *)(a2 + 16) + 4LL);
  *(_DWORD *)(v4 + a1 + 24) = *(_DWORD *)(v5 + a2 + 24);
  *(_DWORD *)(v4 + a1 + 28) = *(_DWORD *)(v5 + a2 + 28);
  *(_BYTE *)(v4 + a1 + 32) = *(_BYTE *)(v5 + a2 + 32);
  *(_DWORD *)(a1 + 32) = *(_DWORD *)(a2 + 32);
  *(_DWORD *)(a1 + 36) = *(_DWORD *)(a2 + 36);
  *(_DWORD *)(a1 + 40) = *(_DWORD *)(a2 + 40);
  *(_OWORD *)(a1 + 48) = *(_OWORD *)(a2 + 48);
  *(_OWORD *)(a1 + 64) = *(_OWORD *)(a2 + 64);
  *(_OWORD *)(a1 + 80) = *(_OWORD *)(a2 + 80);
  *(_OWORD *)(a1 + 96) = *(_OWORD *)(a2 + 96);
  *(_OWORD *)(a1 + 112) = *(_OWORD *)(a2 + 112);
  *(_OWORD *)(a1 + 128) = *(_OWORD *)(a2 + 128);
  *(_OWORD *)(a1 + 144) = *(_OWORD *)(a2 + 144);
  *(_OWORD *)(a1 + 160) = *(_OWORD *)(a2 + 160);
  *(_OWORD *)(a1 + 176) = *(_OWORD *)(a2 + 176);
  *(_OWORD *)(a1 + 192) = *(_OWORD *)(a2 + 192);
  *(_OWORD *)(a1 + 208) = *(_OWORD *)(a2 + 208);
  *(_OWORD *)(a1 + 224) = *(_OWORD *)(a2 + 224);
  *(_OWORD *)(a1 + 240) = *(_OWORD *)(a2 + 240);
  *(_OWORD *)(a1 + 256) = *(_OWORD *)(a2 + 256);
  *(_OWORD *)(a1 + 272) = *(_OWORD *)(a2 + 272);
  v6 = *(int *)(*(_QWORD *)(a1 + 336) + 4LL);
  v7 = *(int *)(*(_QWORD *)(a2 + 336) + 4LL);
  *(_DWORD *)(v6 + a1 + 344) = *(_DWORD *)(v7 + a2 + 344);
  *(_DWORD *)(v6 + a1 + 348) = *(_DWORD *)(v7 + a2 + 348);
  *(_BYTE *)(v6 + a1 + 352) = *(_BYTE *)(v7 + a2 + 352);
  *(_DWORD *)(a1 + 352) = *(_DWORD *)(a2 + 352);
  *(_DWORD *)(a1 + 356) = *(_DWORD *)(a2 + 356);
  *(_DWORD *)(a1 + 360) = *(_DWORD *)(a2 + 360);
  *(_OWORD *)(a1 + 368) = *(_OWORD *)(a2 + 368);
  *(_OWORD *)(a1 + 384) = *(_OWORD *)(a2 + 384);
  *(_OWORD *)(a1 + 400) = *(_OWORD *)(a2 + 400);
  *(_OWORD *)(a1 + 416) = *(_OWORD *)(a2 + 416);
  *(_OWORD *)(a1 + 432) = *(_OWORD *)(a2 + 432);
  *(_OWORD *)(a1 + 448) = *(_OWORD *)(a2 + 448);
  *(_OWORD *)(a1 + 464) = *(_OWORD *)(a2 + 464);
  *(_OWORD *)(a1 + 480) = *(_OWORD *)(a2 + 480);
  *(_OWORD *)(a1 + 496) = *(_OWORD *)(a2 + 496);
  *(_OWORD *)(a1 + 512) = *(_OWORD *)(a2 + 512);
  *(_OWORD *)(a1 + 528) = *(_OWORD *)(a2 + 528);
  *(_OWORD *)(a1 + 544) = *(_OWORD *)(a2 + 544);
  *(_OWORD *)(a1 + 560) = *(_OWORD *)(a2 + 560);
  *(_OWORD *)(a1 + 576) = *(_OWORD *)(a2 + 576);
  *(_OWORD *)(a1 + 592) = *(_OWORD *)(a2 + 592);
  v8 = *(int *)(*(_QWORD *)(a1 + 656) + 4LL);
  v9 = *(int *)(*(_QWORD *)(a2 + 656) + 4LL);
  *(_DWORD *)(v8 + a1 + 664) = *(_DWORD *)(v9 + a2 + 664);
  *(_DWORD *)(v8 + a1 + 668) = *(_DWORD *)(v9 + a2 + 668);
  *(_BYTE *)(v8 + a1 + 672) = *(_BYTE *)(v9 + a2 + 672);
  *(_DWORD *)(a1 + 672) = *(_DWORD *)(a2 + 672);
  *(_DWORD *)(a1 + 676) = *(_DWORD *)(a2 + 676);
  *(_DWORD *)(a1 + 680) = *(_DWORD *)(a2 + 680);
  *(_OWORD *)(a1 + 688) = *(_OWORD *)(a2 + 688);
  *(_OWORD *)(a1 + 704) = *(_OWORD *)(a2 + 704);
  *(_OWORD *)(a1 + 720) = *(_OWORD *)(a2 + 720);
  *(_OWORD *)(a1 + 736) = *(_OWORD *)(a2 + 736);
  *(_OWORD *)(a1 + 752) = *(_OWORD *)(a2 + 752);
  *(_OWORD *)(a1 + 768) = *(_OWORD *)(a2 + 768);
  *(_OWORD *)(a1 + 784) = *(_OWORD *)(a2 + 784);
  *(_OWORD *)(a1 + 800) = *(_OWORD *)(a2 + 800);
  *(_OWORD *)(a1 + 816) = *(_OWORD *)(a2 + 816);
  *(_OWORD *)(a1 + 832) = *(_OWORD *)(a2 + 832);
  *(_OWORD *)(a1 + 848) = *(_OWORD *)(a2 + 848);
  *(_OWORD *)(a1 + 864) = *(_OWORD *)(a2 + 864);
  *(_OWORD *)(a1 + 880) = *(_OWORD *)(a2 + 880);
  *(_OWORD *)(a1 + 896) = *(_OWORD *)(a2 + 896);
  *(_OWORD *)(a1 + 912) = *(_OWORD *)(a2 + 912);
  v10 = *(int *)(*(_QWORD *)(a1 + 976) + 4LL);
  v11 = *(int *)(*(_QWORD *)(a2 + 976) + 4LL);
  *(_DWORD *)(v10 + a1 + 984) = *(_DWORD *)(v11 + a2 + 984);
  *(_DWORD *)(v10 + a1 + 988) = *(_DWORD *)(v11 + a2 + 988);
  *(_BYTE *)(v10 + a1 + 992) = *(_BYTE *)(v11 + a2 + 992);
  *(_DWORD *)(a1 + 992) = *(_DWORD *)(a2 + 992);
  *(_DWORD *)(a1 + 996) = *(_DWORD *)(a2 + 996);
  *(_DWORD *)(a1 + 1000) = *(_DWORD *)(a2 + 1000);
  *(_OWORD *)(a1 + 1008) = *(_OWORD *)(a2 + 1008);
  *(_OWORD *)(a1 + 1024) = *(_OWORD *)(a2 + 1024);
  *(_OWORD *)(a1 + 1040) = *(_OWORD *)(a2 + 1040);
  *(_OWORD *)(a1 + 1056) = *(_OWORD *)(a2 + 1056);
  *(_OWORD *)(a1 + 1072) = *(_OWORD *)(a2 + 1072);
  *(_OWORD *)(a1 + 1088) = *(_OWORD *)(a2 + 1088);
  *(_OWORD *)(a1 + 1104) = *(_OWORD *)(a2 + 1104);
  *(_OWORD *)(a1 + 1120) = *(_OWORD *)(a2 + 1120);
  *(_OWORD *)(a1 + 1136) = *(_OWORD *)(a2 + 1136);
  *(_OWORD *)(a1 + 1152) = *(_OWORD *)(a2 + 1152);
  *(_OWORD *)(a1 + 1168) = *(_OWORD *)(a2 + 1168);
  *(_OWORD *)(a1 + 1184) = *(_OWORD *)(a2 + 1184);
  *(_OWORD *)(a1 + 1200) = *(_OWORD *)(a2 + 1200);
  *(_OWORD *)(a1 + 1216) = *(_OWORD *)(a2 + 1216);
  *(_OWORD *)(a1 + 1232) = *(_OWORD *)(a2 + 1232);
  v12 = *(int *)(*(_QWORD *)(a1 + 1296) + 4LL);
  v13 = *(int *)(*(_QWORD *)(a2 + 1296) + 4LL);
  *(_DWORD *)(v12 + a1 + 1304) = *(_DWORD *)(v13 + a2 + 1304);
  *(_DWORD *)(v12 + a1 + 1308) = *(_DWORD *)(v13 + a2 + 1308);
  *(_BYTE *)(v12 + a1 + 1312) = *(_BYTE *)(v13 + a2 + 1312);
  *(_DWORD *)(a1 + 1312) = *(_DWORD *)(a2 + 1312);
  *(_DWORD *)(a1 + 1316) = *(_DWORD *)(a2 + 1316);
  *(_DWORD *)(a1 + 1320) = *(_DWORD *)(a2 + 1320);
  result = a1;
  *(_OWORD *)(a1 + 1328) = *(_OWORD *)(a2 + 1328);
  *(_OWORD *)(a1 + 1344) = *(_OWORD *)(a2 + 1344);
  *(_OWORD *)(a1 + 1360) = *(_OWORD *)(a2 + 1360);
  *(_OWORD *)(a1 + 1376) = *(_OWORD *)(a2 + 1376);
  *(_OWORD *)(a1 + 1392) = *(_OWORD *)(a2 + 1392);
  *(_OWORD *)(a1 + 1408) = *(_OWORD *)(a2 + 1408);
  *(_OWORD *)(a1 + 1424) = *(_OWORD *)(a2 + 1424);
  *(_OWORD *)(a1 + 1440) = *(_OWORD *)(a2 + 1440);
  *(_OWORD *)(a1 + 1456) = *(_OWORD *)(a2 + 1456);
  *(_OWORD *)(a1 + 1472) = *(_OWORD *)(a2 + 1472);
  *(_OWORD *)(a1 + 1488) = *(_OWORD *)(a2 + 1488);
  *(_OWORD *)(a1 + 1504) = *(_OWORD *)(a2 + 1504);
  *(_OWORD *)(a1 + 1520) = *(_OWORD *)(a2 + 1520);
  *(_OWORD *)(a1 + 1536) = *(_OWORD *)(a2 + 1536);
  *(_OWORD *)(a1 + 1552) = *(_OWORD *)(a2 + 1552);
  return result;
}


########## 0x1403D0E50 (rva 0x3D0E50) sub_1403D0E50 ##########
  AOB: 48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 48 89 4C 24 08
  CALLERS(0): 
__int64 __fastcall sub_1403D0E50(__int64 a1)
{
  ((void (__fastcall *)(__int64, int *))unk_1419405F0)(a1, &dword_145DE5710);
  *(_QWORD *)a1 = &off_145DF5428;
  *(_DWORD *)(a1 + 24) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 32, aDrivetypecompa);
  *(_QWORD *)(a1 + 32) = &off_145DF5470;
  *(_BYTE *)(a1 + 56) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 64, aBlacklist);
  *(_QWORD *)(a1 + 64) = &off_145DF54B8;
  *(_QWORD *)(a1 + 88) = 0LL;
  *(_QWORD *)(a1 + 96) = 0LL;
  *(_QWORD *)(a1 + 104) = 0LL;
  *(_QWORD *)(a1 + 112) = 0LL;
  *(_QWORD *)(a1 + 120) = 0LL;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 128, aCylinderid);
  *(_QWORD *)(a1 + 128) = &off_145DF43E8;
  *(_BYTE *)(a1 + 152) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 160, aCylindercompar);
  *(_QWORD *)(a1 + 160) = &off_145DF5470;
  *(_BYTE *)(a1 + 184) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 192, MEMORY[0x145DE5BA0]);
  *(_QWORD *)(a1 + 192) = &off_145DF5500;
  *(_QWORD *)(a1 + 216) = 0LL;
  *(_QWORD *)(a1 + 224) = 0LL;
  *(_QWORD *)(a1 + 232) = 0LL;
  *(_QWORD *)(a1 + 240) = 0LL;
  *(_QWORD *)(a1 + 248) = 0LL;
  *(_QWORD *)(a1 + 256) = 0LL;
  *(_QWORD *)(a1 + 264) = 0LL;
  *(_QWORD *)(a1 + 272) = 0LL;
  *(_QWORD *)(a1 + 280) = 0LL;
  *(_QWORD *)(a1 + 288) = 0LL;
  *(_QWORD *)(a1 + 296) = 0LL;
  *(_QWORD *)(a1 + 304) = 0LL;
  *(_DWORD *)(a1 + 312) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 320, aCarclassid);
  *(_QWORD *)(a1 + 320) = &off_145DF5548;
  *(_BYTE *)(a1 + 344) = -1;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 352, aDrivetypeid);
  *(_QWORD *)(a1 + 352) = &off_145DF43E8;
  *(_BYTE *)(a1 + 376) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 384, aEngineconfigco);
  *(_QWORD *)(a1 + 384) = &off_145DF5470;
  *(_BYTE *)(a1 + 408) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 416, aYearmax);
  *(_QWORD *)(a1 + 416) = &off_145DF1850;
  *(_DWORD *)(a1 + 440) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 448, aEngineconfigid);
  *(_QWORD *)(a1 + 448) = &off_145DF43E8;
  *(_BYTE *)(a1 + 472) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 480, aMinicareerid);
  *(_QWORD *)(a1 + 480) = &off_145DF4E30;
  *(_DWORD *)(a1 + 504) = -1;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 512, MEMORY[0x145DE60F0]);
  *(_QWORD *)(a1 + 512) = &off_145DF55D8;
  *(_BYTE *)(a1 + 536) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 544, aFamilyspeciali);
  *(_QWORD *)(a1 + 544) = &off_145DF43E8;
  *(_BYTE *)(a1 + 568) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 576, aCartypecompara);
  *(_QWORD *)(a1 + 576) = &off_145DF5470;
  *(_BYTE *)(a1 + 600) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 608, MEMORY[0x145DE61B0]);
  *(_QWORD *)(a1 + 608) = &off_145DF16A0;
  *(_BYTE *)(a1 + 632) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 640, aAllowupgrades);
  *(_QWORD *)(a1 + 640) = &off_145DF1C38;
  *(_BYTE *)(a1 + 664) = 1;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 672, aWeightmin);
  *(_QWORD *)(a1 + 672) = &off_145DF1B18;
  *(_DWORD *)(a1 + 696) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 704, aMakecomparator);
  *(_QWORD *)(a1 + 704) = &off_145DF5470;
  *(_BYTE *)(a1 + 728) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 736, aCarbucketid_0);
  *(_QWORD *)(a1 + 736) = &off_145DF43E8;
  *(_BYTE *)(a1 + 760) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 768, aRegioncomparat);
  *(_QWORD *)(a1 + 768) = &off_145DF5470;
  *(_BYTE *)(a1 + 792) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 800, aModelfamilycom);
  *(_QWORD *)(a1 + 800) = &off_145DF5470;
  *(_BYTE *)(a1 + 824) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 832, aAspirationid);
  *(_QWORD *)(a1 + 832) = &off_145DF43E8;
  *(_BYTE *)(a1 + 856) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 864, aEngineplacemen_0);
  *(_QWORD *)(a1 + 864) = &off_145DF43E8;
  *(_BYTE *)(a1 + 888) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 896, aPowermin);
  *(_QWORD *)(a1 + 896) = &off_145DF1B18;
  *(_DWORD *)(a1 + 920) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 928, aBasecostmax);
  *(_QWORD *)(a1 + 928) = &off_145DF1850;
  *(_DWORD *)(a1 + 952) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 960, aModelcomparato);
  *(_QWORD *)(a1 + 960) = &off_145DF5470;
  *(_BYTE *)(a1 + 984) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 992, aBasecostmin);
  *(_QWORD *)(a1 + 992) = &off_145DF1850;
  *(_DWORD *)(a1 + 1016) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 1024, aPimin);
  *(_QWORD *)(a1 + 1024) = &off_145DF4670;
  *(_WORD *)(a1 + 1048) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 1056, aCountryid);
  *(_QWORD *)(a1 + 1056) = &off_145DF43E8;
  *(_BYTE *)(a1 + 1080) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 1088, aRegionid);
  *(_QWORD *)(a1 + 1088) = &off_145DF43E8;
  *(_BYTE *)(a1 + 1112) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 1120, aEngineplacemen);
  *(_QWORD *)(a1 + 1120) = &off_145DF5470;
  *(_BYTE *)(a1 + 1144) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 1152, aPowermax);
  *(_QWORD *)(a1 + 1152) = &off_145DF1B18;
  *(_DWORD *)(a1 + 1176) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 1184, aPimax);
  *(_QWORD *)(a1 + 1184) = &off_145DF4670;
  *(_WORD *)(a1 + 1208) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 1216, aModelfamilyid);
  *(_QWORD *)(a1 + 1216) = &off_145DF4670;
  *(_WORD *)(a1 + 1240) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 1248, aWeightmax);
  *(_QWORD *)(a1 + 1248) = &off_145DF1B18;
  *(_DWORD *)(a1 + 1272) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 1280, aAspirationcomp);
  *(_QWORD *)(a1 + 1280) = &off_145DF5470;
  *(_BYTE *)(a1 + 1304) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 1312, aCountrycompara);
  *(_QWORD *)(a1 + 1312) = &off_145DF5470;
  *(_BYTE *)(a1 + 1336) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 1344, MEMORY[0x145DE6100]);
  *(_QWORD *)(a1 + 1344) = &off_145DF5620;
  *(_QWORD *)(a1 + 1368) = 0LL;
  *(_QWORD *)(a1 + 1376) = 0LL;
  *(_DWORD *)(a1 + 1384) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 1392, aYearmin);
  *(_QWORD *)(a1 + 1392) = &off_145DF1850;
  *(_DWORD *)(a1 + 1416) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 1424, aCartypeid);
  *(_QWORD *)(a1 + 1424) = &off_145DF43E8;
  *(_BYTE *)(a1 + 1448) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 1456, MEMORY[0x145DEA530]);
  *(_QWORD *)(a1 + 1456) = &off_145DF4670;
  *(_WORD *)(a1 + 1480) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 1488, aBodyfamilyid);
  *(_QWORD *)(a1 + 1488) = &off_145DF43E8;
  *(_BYTE *)(a1 + 1512) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 1520, aFamilyspecialc);
  *(_QWORD *)(a1 + 1520) = &off_145DF5470;
  *(_BYTE *)(a1 + 1544) = 0;
  ((void (__fastcall *)(__int64, char *))unk_1419405F0)(a1 + 1552, aBodyfamilycomp);
  *(_QWORD *)(a1 + 1552) = &off_145DF5470;
  *(_BYTE *)(a1 + 1576) = 0;
  return a1;
}


########## 0x1407A6300 (rva 0x7A6300) sub_1407A6300 ##########
  AOB: 48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 50
  CALLERS(1): 0x1407A6439(in 0x1407A6430)
__int64 __fastcall sub_1407A6300(__m128 *a1, __int64 *a2, unsigned __int64 a3, char a4)
{
  unsigned __int64 v8; // rax
  void (__fastcall *v9)(__m128 *, __int64); // rbx
  __int64 v10; // rax
  __int64 v11; // rax
  __int64 *v12; // rax
  __int128 *v13; // rax
  __int64 v14; // r8
  __int128 v15; // xmm0
  unsigned __int64 v16; // rax
  __int64 v17; // rdx
  __int64 result; // rax
  __m128 v19; // [rsp+20h] [rbp-38h] BYREF
  _BYTE v20[40]; // [rsp+30h] [rbp-28h] BYREF

  a1[4].m128_u64[1] = (unsigned __int64)a2;
  a1[5].m128_u64[0] = a3;
  a1[54].m128_i8[7] = 0;
  sub_1407B8A60(&a1[6]);
  a1[96].m128_i8[0] = a4;
  ((void (__fastcall *)(__m128 *, unsigned __int64))sub_1407B8B30)(&a1[92], a3);
  v8 = a1->m128_u64[0];
  a1[96].m128_i8[1] = 1;
  (*(void (__fastcall **)(__m128 *))(v8 + 808))(a1);
  v9 = *(void (__fastcall **)(__m128 *, __int64))(a1->m128_u64[0] + 816);
  v10 = (*(__int64 (__fastcall **)(__int64 *))(*a2 + 144))(a2);
  v9(a1, v10);
  v11 = *a2;
  v19 = _mm_add_ps(a1[84], a1[83]);
  v12 = (__int64 *)(*(__int64 (__fastcall **)(__int64 *, _BYTE *, __m128 *))(v11 + 104))(a2, v20, &v19);
  a1[85].m128_u64[0] = *v12;
  a1[85].m128_u64[1] = v12[1];
  a1[86].m128_u64[0] = v12[2];
  a1[86].m128_u64[1] = v12[3];
  v13 = (__int128 *)(*(__int64 (__fastcall **)(__m128 *, __m128 *))(a1->m128_u64[0] + 824))(a1, &v19);
  LOBYTE(v14) = 1;
  v15 = *v13;
  v16 = a1->m128_u64[0];
  a1[87] = (__m128)v15;
  result = (*(__int64 (__fastcall **)(__m128 *, __int64, __int64, _QWORD))(v16 + 32))(a1, v17, v14, 0LL);
  a1[96].m128_i8[2] = 1;
  return result;
}


########## 0x141024270 (rva 0x1024270) sub_141024270 ##########
  AOB: 48 8B C4 4C 89 48 20 48 89 50 10 55 53 56 57 41 54 41 55 41 56
  CALLERS(1): 0x141029716(in 0x141029630)
__int64 sub_141024270(__int64 a1, _QWORD *a2, _QWORD *a3, __int64 a4, ...)
{
  _QWORD *v6; // rax
  __int64 v7; // rbx
  __int64 v8; // rcx
  __int64 v9; // rax
  __int64 v10; // rax
  __int64 v11; // rdx
  __int64 v12; // rax
  __int64 v13; // rax
  __int64 v14; // rax
  __int64 v15; // rax
  __int64 v16; // rax
  __int64 v17; // rax
  __int64 v18; // rax
  __int64 v19; // rbx
  __int64 v20; // rdx
  __int64 v21; // rcx
  volatile __int64 *v22; // rsi
  volatile __int64 *v23; // rbx
  __int64 v24; // rcx
  __int64 v25; // rcx
  volatile __int64 *v26; // rsi
  volatile __int64 *v27; // rbx
  __int64 v28; // rcx
  __int64 v29; // rcx
  _OWORD *v30; // r15
  _OWORD *v31; // rax
  _OWORD *v32; // rax
  unsigned int v33; // r13d
  _QWORD *v34; // r12
  __m128 v35; // xmm6
  __int64 v36; // rcx
  __int64 *v37; // rbx
  __int64 v38; // rcx
  __int64 v39; // rcx
  volatile __int64 *v40; // rbx
  __int64 v41; // rcx
  volatile __int64 *v42; // r12
  __int64 v43; // rcx
  __int64 v44; // rcx
  __int64 v45; // rcx
  __int64 v46; // rbx
  __int64 v47; // rcx
  volatile signed __int32 *v48; // rbx
  __int64 v49; // rcx
  __int128 v50; // xmm1
  __int64 v51; // rcx
  _OWORD *v52; // rdx
  __int64 v53; // rcx
  __int64 result; // rax
  __int64 v55; // rcx
  volatile signed __int32 *v56; // rbx
  __int64 v57; // [rsp+40h] [rbp-C0h] BYREF
  __int64 v58; // [rsp+48h] [rbp-B8h] BYREF
  volatile __int64 v59; // [rsp+50h] [rbp-B0h] BYREF
  __int64 (__fastcall **v60)(); // [rsp+58h] [rbp-A8h]
  __int64 v61; // [rsp+60h] [rbp-A0h]
  __int64 v62; // [rsp+68h] [rbp-98h]
  int v63; // [rsp+70h] [rbp-90h] BYREF
  int v64; // [rsp+74h] [rbp-8Ch] BYREF
  __int64 v65; // [rsp+78h] [rbp-88h] BYREF
  __int64 v66; // [rsp+80h] [rbp-80h] BYREF
  __int64 v67; // [rsp+88h] [rbp-78h] BYREF
  __int64 v68; // [rsp+90h] [rbp-70h] BYREF
  RTL_SRWLOCK SRWLock; // [rsp+98h] [rbp-68h] BYREF
  __int64 v70; // [rsp+A0h] [rbp-60h]
  __int64 v71; // [rsp+A8h] [rbp-58h] BYREF
  __int64 v72; // [rsp+B0h] [rbp-50h] BYREF
  int v73; // [rsp+B8h] [rbp-48h]
  __int64 v74; // [rsp+C0h] [rbp-40h]
  __int128 v75; // [rsp+C8h] [rbp-38h] BYREF
  _OWORD v76[2]; // [rsp+D8h] [rbp-28h] BYREF
  __int64 v77; // [rsp+F8h] [rbp-8h]
  _BYTE v78[8]; // [rsp+100h] [rbp+0h] BYREF
  __int64 v79; // [rsp+108h] [rbp+8h]
  _BYTE v80[8]; // [rsp+110h] [rbp+10h] BYREF
  __int64 v81; // [rsp+118h] [rbp+18h]
  _BYTE v82[8]; // [rsp+120h] [rbp+20h] BYREF
  __int64 v83; // [rsp+128h] [rbp+28h]
  _BYTE v84[8]; // [rsp+130h] [rbp+30h] BYREF
  __int64 v85; // [rsp+138h] [rbp+38h]
  _BYTE v86[8]; // [rsp+140h] [rbp+40h] BYREF
  __int64 v87; // [rsp+148h] [rbp+48h]
  _BYTE v88[8]; // [rsp+150h] [rbp+50h] BYREF
  __int64 v89; // [rsp+158h] [rbp+58h]
  __int64 v90; // [rsp+160h] [rbp+60h]
  volatile signed __int32 *v91; // [rsp+168h] [rbp+68h]
  _OWORD v92[4]; // [rsp+170h] [rbp+70h] BYREF
  _BYTE v93[40]; // [rsp+1B0h] [rbp+B0h] BYREF
  _BYTE v94[40]; // [rsp+1D8h] [rbp+D8h] BYREF
  int v95; // [rsp+260h] [rbp+160h] BYREF
  _QWORD *v96; // [rsp+268h] [rbp+168h]
  int v97; // [rsp+270h] [rbp+170h] BYREF
  __int64 v98; // [rsp+278h] [rbp+178h]
  __int64 v99; // [rsp+280h] [rbp+180h] BYREF
  va_list va; // [rsp+280h] [rbp+180h]
  __int64 v101; // [rsp+288h] [rbp+188h]
  va_list va1; // [rsp+290h] [rbp+190h] BYREF

  va_start(va1, a4);
  va_start(va, a4);
  v99 = va_arg(va1, _QWORD);
  v101 = va_arg(va1, _QWORD);
  v98 = a4;
  v96 = a2;
  *(_DWORD *)(a1 + 1288) = 34;
  *(_DWORD *)(a1 + 1292) = 37;
  *(_DWORD *)(a1 + 1296) = -1;
  *(_DWORD *)(a1 + 1300) = 14;
  v6 = (_QWORD *)(*(__int64 (__fastcall **)(_QWORD *, __int64 *))(*a3 + 112LL))(a3, &v72);
  v7 = *(_QWORD *)(*(__int64 (__fastcall **)(_QWORD, __int64 *))(*(_QWORD *)*v6 + 408LL))(*v6, &v71);
  if ( v7 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v7 + 16LL))(v7);
  v8 = *(_QWORD *)(a1 + 16);
  *(_QWORD *)(a1 + 16) = v7;
  if ( v8 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v8 + 24LL))(v8);
  if ( v71 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v71 + 24LL))(v71);
  if ( v72 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v72 + 24LL))(v72);
  *(_QWORD *)(a1 + 1368) = (*(__int64 (__fastcall **)(_QWORD *))(*a3 + 64LL))(a3);
  v9 = ((__int64 (__fastcall *)(__int64))unk_144A1BAC0)(72LL);
  v77 = v9;
  if ( v9 )
    v10 = sub_1409BDDD0(v9, a3);
  else
    v10 = 0LL;
  v11 = *(_QWORD *)(a1 + 1280);
  *(_QWORD *)(a1 + 1280) = v10;
  if ( v11 )
    sub_141023540(a1 + 1280);
  v12 = sub_140E5F4E0(*(_QWORD *)(a1 + 16), v78, &unk_148FBB4E8, 0LL, 0);
  sub_14057E370(a1 + 1424, v12);
  if ( v79 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v79 + 24LL))(v79);
  v13 = sub_140E5F4E0(*(_QWORD *)(a1 + 16), v80, &unk_148FBB3B0, 0LL, 0);
  sub_14057E370(a1 + 1440, v13);
  if ( v81 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v81 + 24LL))(v81);
  v14 = sub_140E5F4E0(*(_QWORD *)(a1 + 16), v82, &unk_148FBB368, 0LL, 0);
  sub_14057E370(a1 + 1456, v14);
  if ( v83 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v83 + 24LL))(v83);
  v15 = sub_140E5F4E0(*(_QWORD *)(a1 + 16), v84, &unk_148FBB430, 0LL, 0);
  sub_14057E370(a1 + 1472, v15);
  if ( v85 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v85 + 24LL))(v85);
  v16 = sub_140E5F4E0(*(_QWORD *)(a1 + 16), v86, &unk_148FBB1D8, 0LL, 0);
  sub_14057E370(a1 + 1488, v16);
  if ( v87 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v87 + 24LL))(v87);
  v17 = sub_140E5F4E0(*(_QWORD *)(a1 + 16), v88, &unk_148FBB1A0, 0LL, 0);
  sub_14057E370(a1 + 1504, v17);
  if ( v89 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v89 + 24LL))(v89);
  sub_141029280(a1, a1 + 1424);
  sub_141029280(a1, a1 + 1440);
  sub_141029280(a1, a1 + 1456);
  sub_141029280(a1, a1 + 1472);
  sub_141029280(a1, a1 + 1488);
  sub_141029280(a1, a1 + 1504);
  v18 = sub_140A48900(0LL);
  sub_141029280(a1, v18);
  sub_140E5F4E0(*(_QWORD *)(a1 + 16), &SRWLock, &unk_148FBB0A0, 0LL, 0);
  v60 = &off_145E15D50;
  v61 = 0LL;
  v62 = 0LL;
  AcquireSRWLockExclusive(&SRWLock);
  v19 = v70;
  if ( v70 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v70 + 16LL))(v70);
  ReleaseSRWLockExclusive(&SRWLock);
  v61 = v19;
  if ( v19 )
  {
    LOBYTE(v20) = 1;
    if ( (unsigned __int8)sub_141327A40(v61, v20) )
    {
      v62 = v61 + 296;
    }
    else
    {
      v21 = v61;
      v61 = 0LL;
      (*(void (__fastcall **)(__int64))(*(_QWORD *)v21 + 24LL))(v21);
    }
  }
  sub_140A7BFA0(v62, a1 + 480);
  v22 = (volatile __int64 *)(*(__int64 (__fastcall **)(_QWORD *, __int64 *))(*a3 + 224LL))(a3, &v65);
  v23 = (volatile __int64 *)(a1 + 528);
  if ( (volatile __int64 *)(a1 + 528) != v22 )
  {
    v24 = _InterlockedExchange64(v23, 0LL);
    if ( v24 )
      (*(void (__fastcall **)(__int64))(*(_QWORD *)v24 + 16LL))(v24);
    *v23 = *v22;
    *v22 = 0LL;
  }
  v25 = _InterlockedExchange64(&v65, 0LL);
  if ( v25 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v25 + 16LL))(v25);
  v26 = (volatile __int64 *)(*(__int64 (__fastcall **)(_QWORD *, __int64 *))(*a3 + 232LL))(a3, &v66);
  v27 = (volatile __int64 *)(a1 + 536);
  if ( (volatile __int64 *)(a1 + 536) != v26 )
  {
    v28 = _InterlockedExchange64(v27, 0LL);
    if ( v28 )
      (*(void (__fastcall **)(__int64))(*(_QWORD *)v28 + 16LL))(v28);
    *v27 = *v26;
    *v26 = 0LL;
  }
  v29 = _InterlockedExchange64(&v66, 0LL);
  if ( v29 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v29 + 16LL))(v29);
  v30 = (_OWORD *)v99;
  *(_QWORD *)(a1 + 1272) = *(_QWORD *)(v99 + 24);
  ((void (__fastcall *)(__int64, _QWORD, __int64))unk_144D392D6)(a1 + 80, 0LL, 272LL);
  if ( a1 == -80 || v30 == (_OWORD *)-48LL )
  {
    *errno() = 22;
    invalid_parameter_noinfo();
  }
  else
  {
    *(_OWORD *)(a1 + 80) = v30[3];
    *(_OWORD *)(a1 + 96) = v30[4];
    *(_OWORD *)(a1 + 112) = v30[5];
    *(_OWORD *)(a1 + 128) = v30[6];
  }
  v31 = (_OWORD *)(a1 + 144);
  if ( a1 != -144 )
  {
    if ( v30 != (_OWORD *)-112LL )
    {
      *v31 = v30[7];
      *(_OWORD *)(a1 + 160) = v30[8];
      *(_OWORD *)(a1 + 176) = v30[9];
      *(_OWORD *)(a1 + 192) = v30[10];
  ...[+189]
```
