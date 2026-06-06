# FH5 Empress camera pass2 (raw log)

```

=============== PART A: lane-name descriptor tables ===============
  'CameraSpaceYPROffset' str=0x145E43038 ptr-slots=['0x145e41df0']
  'CameraSpaceYPRImpulse' str=0x145E43050 ptr-slots=['0x145e41e00']
  'CameraSpaceXYZOffset' str=0x145E43068 ptr-slots=['0x145e41e10']
  'CameraSpaceXYZImpulse' str=0x145E43080 ptr-slots=['0x145e41e20']
  'CameraSpaceYPRMultiplier' str=0x145E43190 ptr-slots=['0x145e41f50']
  'CameraSpaceXYZMultiplier' str=0x145E431B0 ptr-slots=['0x145e41f60']
  'CameraSpaceYPRSet' str=0x145E431D0 ptr-slots=['0x145e41f70']
  'CameraSpaceXYZSet' str=0x145E431E8 ptr-slots=['0x145e41f80']
  'HeadHomeOffsetX' str=0x145E42B08 ptr-slots=['0x145e42350']
  'HeadHomeOffsetY' str=0x145E42B18 ptr-slots=['0x145e42360']
  'HeadHomeOffsetZ' str=0x145E42B28 ptr-slots=['0x145e42370']
  'HeadTrackingX' str=0x145E42B38 ptr-slots=['0x145e42380']
  'HeadTrackingY' str=0x145E42B48 ptr-slots=['0x145e42390']
  'HeadTrackingZ' str=0x145E42B58 ptr-slots=['0x145e423a0']
  'HeadTrackingYaw' str=0x145E42B68 ptr-slots=['0x145e423b0']
  'HeadTrackingPitch' str=0x145E42B78 ptr-slots=['0x145e423c0']

  --- descriptor entry hex dumps (ptr-slot -0x10 .. +0x30) ---

  [CameraSpaceYPROffset] slot @0x145E41DF0 (.rdata):
     +0x00 @0x145E41DE0: q=0x0000000145E43020 (lo=1172582432 hi=1) -> "CarSpaceXYZImpulse"
     +0x08 @0x145E41DE8: q=0x0000000000000004 (lo=4 hi=0)
     +0x10 @0x145E41DF0: q=0x0000000145E43038 (lo=1172582456 hi=1) -> "CameraSpaceYPROffset"
     +0x18 @0x145E41DF8: q=0x0000000000000005 (lo=5 hi=0)
     +0x20 @0x145E41E00: q=0x0000000145E43050 (lo=1172582480 hi=1) -> "CameraSpaceYPRImpulse"
     +0x28 @0x145E41E08: q=0x0000000000000006 (lo=6 hi=0)
     +0x30 @0x145E41E10: q=0x0000000145E43068 (lo=1172582504 hi=1) -> "CameraSpaceXYZOffset"
     +0x38 @0x145E41E18: q=0x0000000000000007 (lo=7 hi=0)

  [CameraSpaceXYZOffset] slot @0x145E41E10 (.rdata):
     +0x00 @0x145E41E00: q=0x0000000145E43050 (lo=1172582480 hi=1) -> "CameraSpaceYPRImpulse"
     +0x08 @0x145E41E08: q=0x0000000000000006 (lo=6 hi=0)
     +0x10 @0x145E41E10: q=0x0000000145E43068 (lo=1172582504 hi=1) -> "CameraSpaceXYZOffset"
     +0x18 @0x145E41E18: q=0x0000000000000007 (lo=7 hi=0)
     +0x20 @0x145E41E20: q=0x0000000145E43080 (lo=1172582528 hi=1) -> "CameraSpaceXYZImpulse"
     +0x28 @0x145E41E28: q=0x0000000000000008 (lo=8 hi=0)
     +0x30 @0x145E41E30: q=0x0000000145E3DAB0 (lo=1172560560 hi=1) -> "FOV"
     +0x38 @0x145E41E38: q=0x0000000000000009 (lo=9 hi=0)

  [CameraSpaceYPRSet] slot @0x145E41F70 (.rdata):
     +0x00 @0x145E41F60: q=0x0000000145E431B0 (lo=1172582832 hi=1) -> "CameraSpaceXYZMultiplier"
     +0x08 @0x145E41F68: q=0x000000000000001C (lo=28 hi=0)
     +0x10 @0x145E41F70: q=0x0000000145E431D0 (lo=1172582864 hi=1) -> "CameraSpaceYPRSet"
     +0x18 @0x145E41F78: q=0x000000000000001D (lo=29 hi=0)
     +0x20 @0x145E41F80: q=0x0000000145E431E8 (lo=1172582888 hi=1) -> "CameraSpaceXYZSet"
     +0x28 @0x145E41F88: q=0x000000000000001E (lo=30 hi=0)
     +0x30 @0x145E41F90: q=0x0000000145E43200 (lo=1172582912 hi=1) -> "WorldSpace"
     +0x38 @0x145E41F98: q=0x0000000000000000 (lo=0 hi=0)

  [CameraSpaceXYZSet] slot @0x145E41F80 (.rdata):
     +0x00 @0x145E41F70: q=0x0000000145E431D0 (lo=1172582864 hi=1) -> "CameraSpaceYPRSet"
     +0x08 @0x145E41F78: q=0x000000000000001D (lo=29 hi=0)
     +0x10 @0x145E41F80: q=0x0000000145E431E8 (lo=1172582888 hi=1) -> "CameraSpaceXYZSet"
     +0x18 @0x145E41F88: q=0x000000000000001E (lo=30 hi=0)
     +0x20 @0x145E41F90: q=0x0000000145E43200 (lo=1172582912 hi=1) -> "WorldSpace"
     +0x28 @0x145E41F98: q=0x0000000000000000 (lo=0 hi=0)
     +0x30 @0x145E41FA0: q=0x0000000145E43210 (lo=1172582928 hi=1) -> "CarSpace"
     +0x38 @0x145E41FA8: q=0x0000000000000001 (lo=1 hi=0)

  [HeadTrackingX] slot @0x145E42380 (.rdata):
     +0x00 @0x145E42370: q=0x0000000145E42B28 (lo=1172581160 hi=1) -> "HeadHomeOffsetZ"
     +0x08 @0x145E42378: q=0x0000000000000021 (lo=33 hi=0)
     +0x10 @0x145E42380: q=0x0000000145E42B38 (lo=1172581176 hi=1) -> "HeadTrackingX"
     +0x18 @0x145E42388: q=0x0000000000000022 (lo=34 hi=0)
     +0x20 @0x145E42390: q=0x0000000145E42B48 (lo=1172581192 hi=1) -> "HeadTrackingY"
     +0x28 @0x145E42398: q=0x0000000000000023 (lo=35 hi=0)
     +0x30 @0x145E423A0: q=0x0000000145E42B58 (lo=1172581208 hi=1) -> "HeadTrackingZ"
     +0x38 @0x145E423A8: q=0x0000000000000024 (lo=36 hi=0)

  [HeadTrackingYaw] slot @0x145E423B0 (.rdata):
     +0x00 @0x145E423A0: q=0x0000000145E42B58 (lo=1172581208 hi=1) -> "HeadTrackingZ"
     +0x08 @0x145E423A8: q=0x0000000000000024 (lo=36 hi=0)
     +0x10 @0x145E423B0: q=0x0000000145E42B68 (lo=1172581224 hi=1) -> "HeadTrackingYaw"
     +0x18 @0x145E423B8: q=0x0000000000000025 (lo=37 hi=0)
     +0x20 @0x145E423C0: q=0x0000000145E42B78 (lo=1172581240 hi=1) -> "HeadTrackingPitch"
     +0x28 @0x145E423C8: q=0x0000000000000026 (lo=38 hi=0)
     +0x30 @0x145E423D0: q=0x0000000145E42B90 (lo=1172581264 hi=1) -> "OneWhenStationary"
     +0x38 @0x145E423D8: q=0x0000000000000027 (lo=39 hi=0)

  [HeadTrackingPitch] slot @0x145E423C0 (.rdata):
     +0x00 @0x145E423B0: q=0x0000000145E42B68 (lo=1172581224 hi=1) -> "HeadTrackingYaw"
     +0x08 @0x145E423B8: q=0x0000000000000025 (lo=37 hi=0)
     +0x10 @0x145E423C0: q=0x0000000145E42B78 (lo=1172581240 hi=1) -> "HeadTrackingPitch"
     +0x18 @0x145E423C8: q=0x0000000000000026 (lo=38 hi=0)
     +0x20 @0x145E423D0: q=0x0000000145E42B90 (lo=1172581264 hi=1) -> "OneWhenStationary"
     +0x28 @0x145E423D8: q=0x0000000000000027 (lo=39 hi=0)
     +0x30 @0x145E423E0: q=0x0000000145E42BA8 (lo=1172581288 hi=1) -> "PitchAngleDegrees"
     +0x38 @0x145E423E8: q=0x0000000000000028 (lo=40 hi=0)

  [HeadHomeOffsetX] slot @0x145E42350 (.rdata):
     +0x00 @0x145E42340: q=0x0000000145E42AF8 (lo=1172581112 hi=1) -> "ShoulderRoll"
     +0x08 @0x145E42348: q=0x000000000000001E (lo=30 hi=0)
     +0x10 @0x145E42350: q=0x0000000145E42B08 (lo=1172581128 hi=1) -> "HeadHomeOffsetX"
     +0x18 @0x145E42358: q=0x000000000000001F (lo=31 hi=0)
     +0x20 @0x145E42360: q=0x0000000145E42B18 (lo=1172581144 hi=1) -> "HeadHomeOffsetY"
     +0x28 @0x145E42368: q=0x0000000000000020 (lo=32 hi=0)
     +0x30 @0x145E42370: q=0x0000000145E42B28 (lo=1172581160 hi=1) -> "HeadHomeOffsetZ"
     +0x38 @0x145E42378: q=0x0000000000000021 (lo=33 hi=0)

  --- xrefs into the descriptor table region (locate registrar) ---
  table span ~0x145E41DF0..0x145E423C0
    (no direct code xrefs to any slot -> pure data table)

=============== PART B: decompile object-relative candidates ===============

########## 0x1407A6300 (rva 0x7A6300) sub_1407A6300 ##########
  AOB: 48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 50 4C 8B F1
     0x1407A6300: mov     [rsp+8], rbx
     0x1407A6305: mov     [rsp+10h], rsi
     0x1407A630A: mov     [rsp+18h], rdi
     0x1407A630F: push    r14
     0x1407A6311: sub     rsp, 50h
     0x1407A6315: mov     r14, rcx
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


########## 0x1407A6460 (rva 0x7A6460) sub_1407A6460 ##########
  AOB: 40 53 55 57 41 57 48 81 EC 98 00 00 00 4C 8B F9 48 89 51 48 4C 89 41 50
     0x1407A6460: push    rbx
     0x1407A6462: push    rbp
     0x1407A6463: push    rdi
     0x1407A6464: push    r15
     0x1407A6466: sub     rsp, 98h
     0x1407A646D: mov     r15, rcx
     0x1407A6470: mov     [rcx+48h], rdx
     0x1407A6474: mov     [rcx+50h], r8
  CALLERS(0): 
__int64 __fastcall sub_1407A6460(__int64 *a1, __int64 a2, __int64 a3, char a4)
{
  int *v7; // rdx
  __int64 v8; // r8
  __int64 v9; // rcx
  __int64 *v10; // rax
  __m128 *v11; // rax
  __int64 v12; // rsi
  void (__fastcall *v13)(__int64, _BYTE *, __int64); // rdi
  float v14; // xmm0_4
  __int64 v15; // rax
  __int64 v16; // rcx
  void (__fastcall *v17)(__int64 *, __int64); // rbx
  __int64 v18; // rax
  unsigned __int8 (__fastcall *v19)(__int64 *, __int64); // rbx
  __int64 v20; // rax
  __int64 v21; // rax
  __int64 v22; // r14
  double (__fastcall *v23)(__int64, __int64); // rsi
  __int64 v24; // rdi
  __int64 (__fastcall *v25)(__int64, _BYTE *, __int64); // rbx
  double v26; // xmm0_8
  __int64 v27; // rax
  float v28; // xmm2_4
  float *v29; // rcx
  float v30; // xmm6_4
  __int64 v31; // rax
  float v32; // xmm6_4
  float v33; // xmm0_4
  float v34; // xmm1_4
  float *v35; // rax
  float v36; // xmm0_4
  float v37; // xmm2_4
  float v38; // xmm0_4
  __int64 v39; // rax
  __int64 result; // rax
  _BYTE v41[4]; // [rsp+20h] [rbp-98h] BYREF
  float v42; // [rsp+24h] [rbp-94h]
  float v43; // [rsp+34h] [rbp-84h]
  _BYTE v44[32]; // [rsp+40h] [rbp-78h] BYREF
  int v45; // [rsp+C0h] [rbp+8h] BYREF
  float v46; // [rsp+C8h] [rbp+10h] BYREF
  int v47; // [rsp+D0h] [rbp+18h] BYREF
  float v48; // [rsp+D8h] [rbp+20h] BYREF

  a1[9] = a2;
  a1[10] = a3;
  *((_BYTE *)a1 + 871) = 0;
  sub_1407B8A60(a1 + 12);
  *((_BYTE *)a1 + 871) = 0;
  *((_BYTE *)a1 + 1368) = a4;
  sub_1407B8B30(a1 + 190, a3);
  sub_1407B8B30(a1 + 198, a3);
  v9 = a1[9];
  if ( v9 )
  {
    v10 = (__int64 *)(*(__int64 (__fastcall **)(__int64, _BYTE *))(*(_QWORD *)v9 + 16LL))(v9, v44);
    a1[172] = *v10;
    a1[173] = v10[1];
    a1[174] = v10[2];
    a1[175] = v10[3];
    v11 = (__m128 *)(*(__int64 (__fastcall **)(__int64, _BYTE *, __int64))(*(_QWORD *)a1[9] + 40LL))(a1[9], v41, 2LL);
    v12 = a1[9];
    *((__m128 *)a1 + 88) = _mm_sub_ps((__m128)0LL, *v11);
    v13 = *(void (__fastcall **)(__int64, _BYTE *, __int64))(*(_QWORD *)v12 + 40LL);
    v13(v12, v41, 2LL);
    v13(v12, v44, 2LL);
    v14 = ((float (*)(void))unk_144D3932A)();
    v15 = *a1;
    v16 = a1[9];
    *((float *)a1 + 356) = v14;
    *(__int64 *)((char *)a1 + 1428) = 0LL;
    v17 = *(void (__fastcall **)(__int64 *, __int64))(v15 + 800);
    *((_DWORD *)a1 + 359) = 0;
    *((_DWORD *)a1 + 366) = 0;
    v18 = (*(__int64 (__fastcall **)(__int64))(*(_QWORD *)v16 + 144LL))(v16);
    v17(a1, v18);
    v19 = *(unsigned __int8 (__fastcall **)(__int64 *, __int64))(*a1 + 816);
    v20 = (*(__int64 (__fastcall **)(__int64))(*(_QWORD *)a1[9] + 144LL))(a1[9]);
    if ( !v19(a1, v20) )
    {
      (*(void (__fastcall **)(__int64 *))(*a1 + 808))(a1);
      if ( (*(__int64 (__fastcall **)(__int64))(*(_QWORD *)a1[9] + 120LL))(a1[9]) )
      {
        v21 = (*(__int64 (__fastcall **)(__int64))(*(_QWORD *)a1[9] + 120LL))(a1[9]);
        (*(void (__fastcall **)(__int64, _BYTE *))(*(_QWORD *)v21 + 648LL))(v21, v41);
        v22 = (*(__int64 (__fastcall **)(__int64))(*(_QWORD *)a1[9] + 120LL))(a1[9]);
        v23 = *(double (__fastcall **)(__int64, __int64))(*(_QWORD *)v22 + 1680LL);
        v24 = (*(__int64 (__fastcall **)(__int64))(*(_QWORD *)a1[9] + 120LL))(a1[9]);
        v25 = *(__int64 (__fastcall **)(__int64, _BYTE *, __int64))(*(_QWORD *)v24 + 2520LL);
        v26 = v23(v22, 2LL);
        v27 = v25(v24, v44, 2LL);
        v28 = *((float *)a1 + 371);
        v29 = &v48;
        v7 = &v47;
        v45 = 0;
        v30 = *(float *)&v26 - *(float *)(v27 + 4);
        v31 = a1[166];
        v47 = 0;
        v32 = (float)((float)((float)(v30 + v42) + v43) + *((float *)a1 + 370)) + v28;
        v33 = *(float *)(v31 + 12) - v32;
        v34 = (float)(*(float *)(v31 + 16) + *((float *)a1 + 375)) - v32;
        v46 = v33;
        v48 = v34;
        if ( v34 >= 0.0 )
          v29 = (float *)&v45;
        v35 = &v46;
        if ( v33 <= 0.0 )
          v35 = (float *)&v47;
        v36 = *v29 + *v35;
        v37 = v28 + v36;
        v38 = v36 + *((float *)a1 + 374);
        *((float *)a1 + 371) = v37;
        *((float *)a1 + 374) = v38;
      }
    }
  }
  v39 = *a1;
  LOBYTE(v8) = 1;
  *(__int64 *)((char *)a1 + 1508) = 0LL;
  *((_DWORD *)a1 + 379) = 0;
  result = (*(__int64 (__fastcall **)(__int64 *, int *, __int64, _QWORD))(v39 + 32))(a1, v7, v8, 0LL);
  *((_BYTE *)a1 + 1648) = 1;
  return result;
}


########## 0x141033D80 (rva 0x1033D80) sub_141033D80 ##########
  AOB: 40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 18 F1 FF FF 48 81 EC E8 0F 00 00
     0x141033D80: push    rbp
     0x141033D82: push    rbx
     0x141033D83: push    rsi
     0x141033D84: push    rdi
     0x141033D85: push    r12
     0x141033D87: push    r13
     0x141033D89: push    r14
     0x141033D8B: push    r15
  CALLERS(1): 0x141046E0B(in 0x1410469C0)
__int64 __fastcall sub_141033D80(__int64 a1)
{
  __int64 *v2; // rax
  __int64 v3; // rbx
  __int64 v4; // rcx
  __int64 *v5; // rax
  __int64 v6; // rcx
  __int64 v7; // rcx
  int v8; // ebx
  __int64 *v9; // rax
  __int64 v10; // rsi
  __int64 v11; // rcx
  __int64 v12; // r15
  __int64 v13; // rdx
  unsigned __int64 v14; // r14
  __int64 v15; // rax
  __int64 v16; // r14
  __int64 v17; // rdx
  __int64 *v18; // rax
  __int64 v19; // rbx
  __int64 v20; // rcx
  __int64 v21; // r14
  __int64 v22; // rdx
  unsigned __int64 v23; // rsi
  __int64 v24; // rax
  __int64 v25; // rsi
  __int64 v26; // rdx
  __int64 *v27; // rax
  __int64 v28; // rbx
  __int64 v29; // rcx
  __int64 v30; // r14
  __int64 v31; // rdx
  unsigned __int64 v32; // rsi
  __int64 v33; // rax
  __int64 v34; // rsi
  __int64 v35; // rdx
  __int64 *v36; // rax
  __int64 v37; // rbx
  __int64 v38; // rcx
  __int64 *v39; // rax
  __int64 v40; // rbx
  __int64 v41; // rcx
  __int64 *v42; // rax
  __int64 v43; // rbx
  __int64 v44; // rcx
  _QWORD *v45; // rcx
  __int64 v46; // rcx
  __int64 *v47; // rax
  __int64 v48; // rcx
  int v49; // ebx
  __int64 *v50; // rax
  __int64 v51; // rsi
  __int64 v52; // rcx
  __int64 *v53; // rax
  __int64 v54; // rbx
  __int64 v55; // rcx
  __int64 *v56; // rax
  __int64 v57; // rbx
  __int64 v58; // rcx
  __int64 *v59; // rax
  __int64 v60; // rbx
  __int64 v61; // rcx
  __int64 *v62; // rax
  __int64 v63; // rbx
  __int64 v64; // rcx
  _QWORD *v65; // rcx
  __int64 v66; // rcx
  __int64 v67; // rcx
  __int64 v68; // rax
  __int64 v69; // rcx
  int v70; // ebx
  __int64 v71; // rcx
  __int64 v72; // rcx
  __int64 v73; // rcx
  __int64 v74; // rcx
  __int64 v75; // rcx
  __int64 v76; // rcx
  __int64 v77; // rcx
  __int64 v78; // rcx
  __int64 v79; // rcx
  __int64 v80; // rcx
  __int64 v81; // rcx
  __int64 v82; // rcx
  __int64 v83; // rcx
  _QWORD *v84; // rcx
  int v85; // ebx
  _QWORD *v86; // rcx
  __int64 v87; // rcx
  int v88; // ebx
  _QWORD *v89; // rcx
  __int64 v90; // rcx
  int v91; // ebx
  _QWORD *v92; // rcx
  __int64 v93; // rcx
  int v94; // ebx
  _QWORD *v95; // rcx
  int v96; // ebx
  _QWORD *v97; // rcx
  __int64 v98; // rcx
  int v99; // ebx
  _QWORD *v100; // rcx
  int v101; // ebx
  _QWORD *v102; // rcx
  __int64 v103; // rcx
  int v104; // ebx
  _QWORD *v105; // rcx
  int v106; // ebx
  _QWORD *v107; // rcx
  __int64 v108; // rcx
  __int64 v110; // [rsp+30h] [rbp-D0h]
  __int64 v111; // [rsp+38h] [rbp-C8h]
  __int64 v112; // [rsp+40h] [rbp-C0h]
  __int64 v113; // [rsp+48h] [rbp-B8h]
  __int64 v114; // [rsp+50h] [rbp-B0h]
  __int64 v115; // [rsp+58h] [rbp-A8h] BYREF
  __int64 v116; // [rsp+60h] [rbp-A0h] BYREF
  __int64 v117; // [rsp+68h] [rbp-98h] BYREF
  int v118; // [rsp+70h] [rbp-90h]
  int v119; // [rsp+74h] [rbp-8Ch]
  __int64 v120; // [rsp+80h] [rbp-80h] BYREF
  unsigned int v121; // [rsp+88h] [rbp-78h]
  unsigned int v122; // [rsp+8Ch] [rbp-74h]
  __int64 v123; // [rsp+90h] [rbp-70h]
  unsigned int v124; // [rsp+98h] [rbp-68h]
  unsigned int v125; // [rsp+9Ch] [rbp-64h]
  __int64 v126; // [rsp+A0h] [rbp-60h]
  __int64 v127; // [rsp+A8h] [rbp-58h]
  __int64 v128; // [rsp+B0h] [rbp-50h]
  __int64 v129; // [rsp+B8h] [rbp-48h]
  __int64 v130; // [rsp+C0h] [rbp-40h]
  __int64 v131; // [rsp+C8h] [rbp-38h]
  __int64 v132; // [rsp+D0h] [rbp-30h]
  __int64 v133; // [rsp+D8h] [rbp-28h]
  __int64 v134; // [rsp+E0h] [rbp-20h] BYREF
  __int64 v135; // [rsp+E8h] [rbp-18h] BYREF
  __int64 v136; // [rsp+F0h] [rbp-10h] BYREF
  __int64 v137; // [rsp+F8h] [rbp-8h] BYREF
  __int64 v138; // [rsp+100h] [rbp+0h] BYREF
  __int64 v139; // [rsp+108h] [rbp+8h] BYREF
  char v140[8]; // [rsp+110h] [rbp+10h] BYREF
  __int64 v141; // [rsp+118h] [rbp+18h] BYREF
  __int64 v142; // [rsp+120h] [rbp+20h] BYREF
  __int64 v143; // [rsp+128h] [rbp+28h] BYREF
  __int64 v144; // [rsp+130h] [rbp+30h] BYREF
  char v145[8]; // [rsp+138h] [rbp+38h] BYREF
  __int64 v146; // [rsp+140h] [rbp+40h] BYREF
  __int64 v147; // [rsp+148h] [rbp+48h] BYREF
  __int64 v148; // [rsp+150h] [rbp+50h] BYREF
  __int64 v149; // [rsp+158h] [rbp+58h] BYREF
  __int64 v150; // [rsp+160h] [rbp+60h] BYREF
  __int64 v151; // [rsp+168h] [rbp+68h] BYREF
  char v152[8]; // [rsp+170h] [rbp+70h] BYREF
  __int64 v153; // [rsp+178h] [rbp+78h] BYREF
  __int64 v154; // [rsp+180h] [rbp+80h] BYREF
  __int64 v155; // [rsp+188h] [rbp+88h] BYREF
  __int64 v156; // [rsp+190h] [rbp+90h] BYREF
  __int64 v157; // [rsp+198h] [rbp+98h] BYREF
  char v158[8]; // [rsp+1A0h] [rbp+A0h] BYREF
  __int64 v159; // [rsp+1A8h] [rbp+A8h] BYREF
  __int64 v160; // [rsp+1B0h] [rbp+B0h] BYREF
  __int64 v161; // [rsp+1B8h] [rbp+B8h] BYREF
  __int64 v162; // [rsp+1C0h] [rbp+C0h] BYREF
  __int64 v163; // [rsp+1C8h] [rbp+C8h] BYREF
  __int64 v164; // [rsp+1D0h] [rbp+D0h] BYREF
  __int64 v165; // [rsp+1D8h] [rbp+D8h] BYREF
  __int64 v166; // [rsp+1E0h] [rbp+E0h] BYREF
  char v167[8]; // [rsp+1E8h] [rbp+E8h] BYREF
  __int64 v168; // [rsp+1F0h] [rbp+F0h] BYREF
  __int64 v169; // [rsp+1F8h] [rbp+F8h] BYREF
  char v170[8]; // [rsp+200h] [rbp+100h] BYREF
  __int64 v171; // [rsp+208h] [rbp+108h] BYREF
  __int64 v172; // [rsp+210h] [rbp+110h] BYREF
  __int64 v173; // [rsp+218h] [rbp+118h] BYREF
  __int64 v174; // [rsp+220h] [rbp+120h] BYREF
  __int64 v175; // [rsp+228h] [rbp+128h] BYREF
  char v176[8]; // [rsp+230h] [rbp+130h] BYREF
  __int64 v177; // [rsp+238h] [rbp+138h] BYREF
  __int64 v178; // [rsp+240h] [rbp+140h] BYREF
  __int64 v179; // [rsp+248h] [rbp+148h] BYREF
  __int64 v180; // [rsp+250h] [rbp+150h] BYREF
  char v181[8]; // [rsp+258h] [rbp+158h] BYREF
  __int64 v182; // [rsp+260h] [rbp+160h] BYREF
  __int64 v183; // [rsp+268h] [rbp+168h] BYREF
  __int64 v184; // [rsp+270h] [rbp+170h] BYREF
  __int64 v185; // [rsp+278h] [rbp+178h] BYREF
  char v186[8]; // [rsp+280h] [rbp+180h] BYREF
  __int64 v187; // [rsp+288h] [rbp+188h] BYREF
  __int64 v188; // [rsp+290h] [rbp+190h] BYREF
  __int64 v189; // [rsp+298h] [rbp+198h] BYREF
  __int64 v190; // [rsp+2A0h] [rbp+1A0h] BYREF
  __int64 v191; // [rsp+2A8h] [rbp+1A8h] BYREF
  __int64 v192; // [rsp+2B0h] [rbp+1B0h] BYREF
  __int64 v193; // [rsp+2B8h] [rbp+1B8h] BYREF
  __int64 v194; // [rsp+2C0h] [rbp+1C0h] BYREF
  __int64 v195; // [rsp+2C8h] [rbp+1C8h] BYREF
  __int64 v196; // [rsp+2D0h] [rbp+1D0h] BYREF
  __int64 v197; // [rsp+2D8h] [rbp+1D8h] BYREF
  __int64 v198; // [rsp+2E0h] [rbp+1E0h] BYREF
  __int64 v199; // [rsp+2E8h] [rbp+1E8h] BYREF
  __int64 v200; // [rsp+2F0h] [rbp+1F0h] BYREF
  __int64 v201; // [rsp+2F8h] [rbp+1F8h] BYREF
  __int64 v202; // [rsp+300h] [rbp+200h] BYREF
  __int64 v203; // [rsp+308h] [rbp+208h] BYREF
  __int64 v204; // [rsp+310h] [rbp+210h] BYREF
  __int64 v205; // [rsp+318h] [rbp+218h] BYREF
  __int64 v206; // [rsp+320h] [rbp+220h] BYREF
  __int64 v207; // [rsp+328h] [rbp+228h] BYREF
  __int64 v208; // [rsp+330h] [rbp+230h] BYREF
  __int64 v209; // [rsp+338h] [rbp+238h] BYREF
  char v210[8]; // [rsp+340h] [rbp+240h] BYREF
  int v211; // [rsp+348h] [rbp+248h]
  int v212; // [rsp+34Ch] [rbp+24Ch]
  _BYTE v213[16]; // [rsp+358h] [rbp+258h] BYREF
  char v214[16]; // [rsp+368h] [rbp+268h] BYREF
  __int64 v215; // [rsp+378h] [rbp+278h] BYREF
  int v216; // [rsp+380h] [rbp+280h]
  __int64 v217; // [rsp+384h] [rbp+284h]
  int v218; // [rsp+38Ch] [rbp+28Ch]
  __int64 v219; // [rsp+390h] [rbp+290h] BYREF
  int v220; // [rsp+398h] [rbp+298h]
  __int64 v221; // [rsp+39Ch] [rbp+29Ch]
  int v222; // [rsp+3A4h] [rbp+2A4h]
  __int64 v223; // [rsp+3A8h] [rbp+2A8h] BYREF
  int v224; // [rsp+3B0h] [rbp+2B0h]
  __int64 v225; // [rsp+3B4h] [rbp+2B4h]
  __int64 v226; // [rsp+3C0h] [rbp+2C0h] BYREF
  int v227; // [rsp+3C8h] [rbp+2C8h]
  __int64 v228; // [rsp+3CCh] [rbp+2CCh]
  __int64 v229; // [rsp+3D8h] [rbp+2D8h] BYREF
  int v230; // [rsp+3E0h] [rbp+2E0h]
  __int64 v231; // [rsp+3E4h] [rbp+2E4h]
  __int64 v232; // [rsp+3F0h] [rbp+2F0h] BYREF
  int v233; // [rsp+3F8h] [rbp+2F8h]
  __int64 v234; // [rsp+3FCh] [rbp+2FCh]
  __int64 v235; // [rsp+408h] [rbp+308h] BYREF
  int v236; // [rsp+410h] [rbp+310h]
  __int64 v237; // [rsp+414h] [rbp+314h]
  __int64 v238; // [rsp+420h] [rbp+320h] BYREF
  int v239; // [rsp+428h] [rbp+328h]
  __int64 v240; // [rsp+42Ch] [rbp+32Ch]
  __int64 v241; // [rsp+438h] [rbp+338h] BYREF
  int v242; // [rsp+440h] [rbp+340h]
  __int64 v243; // [rsp+444h] [rbp+344h]
  __int64 v244; // [rsp+450h] [rbp+350h] BYREF
  int v245; // [rsp+458h] [rbp+358h]
  __int64 v246; // [rsp+45Ch] [rbp+35Ch]
  __int64 v247; // [rsp+468h] [rbp+368h] BYREF
  int v248; // [rsp+470h] [rbp+370h]
  __int64 v249; // [rsp+474h] [rbp+374h]
  __int64 v250; // [rsp+480h] [rbp+380h] BYREF
  int v251; // [rsp+488h] [rbp+388h]
  __int64 v252; // [rsp+48Ch] [rbp+38Ch]
  __int64 v253; // [rsp+498h] [rbp+398h] BYREF
  int v254; // [rsp+4A0h] [rbp+3A0h]
  __int64 v255; // [rsp+4A4h] [rbp+3A4h]
  __int64 v256; // [rsp+4B0h] [rbp+3B0h] BYREF
  int v257; // [rsp+4B8h] [rbp+3B8h]
  __int64 v258; // [rsp+4BCh] [rbp+3BCh]
  __int64 v259; // [rsp+4C8h] [rbp+3C8h] BYREF
  int v260; // [rsp+4D0h] [rbp+3D0h]
  ...[+1625 lines]

########## 0x141024270 (rva 0x1024270) sub_141024270 ##########
  AOB: 48 8B C4 4C 89 48 20 48 89 50 10 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 A8 FE FF FF
     0x141024270: mov     rax, rsp
     0x141024273: mov     [rax+20h], r9
     0x141024277: mov     [rax+10h], rdx
     0x14102427B: push    rbp
     0x14102427C: push    rbx
     0x14102427D: push    rsi
     0x14102427E: push    rdi
     0x14102427F: push    r12
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
      goto LABEL_53;
    }
    *v31 = 0LL;
    *(_OWORD *)(a1 + 160) = 0LL;
    *(_OWORD *)(a1 + 176) = 0LL;
    *(_OWORD *)(a1 + 192) = 0LL;
  }
  *errno() = 22;
  invalid_parameter_noinfo();
LABEL_53:
  v32 = (_OWORD *)(a1 + 208);
  if ( a1 != -208 )
  {
    if ( v30 != (_OWORD *)-304LL )
    {
      *v32 = v30[19];
      *(_OWORD *)(a1 + 224) = v30[20];
      *(_OWORD *)(a1 + 240) = v30[21];
      *(_OWORD *)(a1 + 256) = v30[22];
      goto LABEL_58;
  ...[+169 lines]

########## 0x141B4A090 (rva 0x1B4A090) sub_141B4A090 ##########
  AOB: 48 89 5C 24 18 48 89 74 24 20 57 48 83 EC 60 80 3D D2 47 58 07 00 41 0F B6 F0
     0x141B4A090: mov     [rsp+18h], rbx
     0x141B4A095: mov     [rsp+20h], rsi
     0x141B4A096: mov     [rsp+20h], rsi
     0x141B4A09A: push    rdi
     0x141B4A09B: sub     rsp, 60h
     0x141B4A09C: sub     rsp, 60h
     0x141B4A09F: cmp     cs:byte_1490CE878, 0
     0x141B4A0A0: cmp     cs:byte_1490CE878, 0
  CALLERS(0): 
void sub_141B4A090()
{
  JUMPOUT(0x141B4A0AALL);
}


########## 0x1407A16A0 (rva 0x7A16A0) sub_1407A16A0 ##########
  AOB: 48 89 5C 24 08 57 48 83 EC 20 8B DA 48 8B F9 E8 0C E8 FF FF F6 C3 01 74 0D
     0x1407A16A0: mov     [rsp+8], rbx
     0x1407A16A5: push    rdi
     0x1407A16A6: sub     rsp, 20h
     0x1407A16AA: mov     ebx, edx
     0x1407A16AC: mov     rdi, rcx
     0x1407A16AF: call    sub_14079FEC0
     0x1407A16B4: test    bl, 1
     0x1407A16B7: jz      short loc_1407A16C6
  CALLERS(0): 
__int64 __fastcall sub_1407A16A0(__int64 a1, char a2)
{
  sub_14079FEC0();
  if ( (a2 & 1) != 0 )
    ((void (__fastcall *)(__int64, __int64))unk_144D396D8)(a1, 1616LL);
  return a1;
}

```
