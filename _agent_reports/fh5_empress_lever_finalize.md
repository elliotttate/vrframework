; open rc=0 15.7s
===== locate pose-writer function (contains 0x592870 movups [rcx+350h]) =====
  0x140592870 -> func NONE
  0x1405928E8 -> func NONE
  0x140592A2B -> func NONE
  0x140592AD3 -> func NONE
  0x1405929A6 -> func NONE

===== DECOMPILE POSE-WRITER (camera-object init +0x320/+0x350/+0x540/+0x550/+0x660) ea=0x140592800 rva=0x592800  =====
  start=0x140592800 end=0x140594395 size=0x1B95
  prologue AOB(48): 81 C0 03 00 00 0F 11 81 00 04 00 00 0F 11 81 40 04 00 00 0F 11 81 80 04 00 00 0F 11 49 10 0F 11 49 50 0F 11 89 90 00 00 00 0F 11 89 D0 00 00 00
// local variable allocation has failed, the output may be wrong!
__int64 __fastcall sub_140592800(__int64 a1, double a2, double a3, double a4)
{
  int v4; // eax
  __int128 v5; // xmm0
  bool v6; // cf
  __int64 result; // rax

  v6 = __CFADD__(v4, 251658243);
  result = (unsigned int)(v4 + 251658243);
  *(_DWORD *)(a1 + 1024) += result + v6;
  *(_OWORD *)(a1 + 1088) = v5;
  *(_OWORD *)(a1 + 1152) = v5;
  *(_OWORD *)(a1 + 16) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 80) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 144) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 208) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 272) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 336) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 400) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 464) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 528) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 592) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 656) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 720) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 784) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 848) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 912) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 976) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 1040) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 1104) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 1168) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 32) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 96) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 160) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 224) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 288) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 352) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 416) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 480) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 544) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 608) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 672) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 736) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 800) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 864) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 928) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 992) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 1056) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 1120) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 1184) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 48) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 112) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 176) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 240) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 304) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 368) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 432) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 496) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 560) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 624) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 688) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 752) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 816) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 880) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 944) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 1008) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 1072) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 1136) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 1200) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 1216) = v5;
  *(_OWORD *)(a1 + 1280) = v5;
  *(_OWORD *)(a1 + 1344) = v5;
  *(_OWORD *)(a1 + 1408) = v5;
  *(_OWORD *)(a1 + 1472) = v5;
  *(_OWORD *)(a1 + 1536) = v5;
  *(_OWORD *)(a1 + 1600) = v5;
  *(_OWORD *)(a1 + 1664) = v5;
  *(_OWORD *)(a1 + 1728) = v5;
  *(_OWORD *)(a1 + 1792) = v5;
  *(_OWORD *)(a1 + 1856) = v5;
  *(_OWORD *)(a1 + 1920) = v5;
  *(_OWORD *)(a1 + 1984) = v5;
  *(_OWORD *)(a1 + 2048) = v5;
  *(_OWORD *)(a1 + 2112) = v5;
  *(_OWORD *)(a1 + 2176) = v5;
  *(_OWORD *)(a1 + 2240) = v5;
  *(_OWORD *)(a1 + 2304) = v5;
  *(_OWORD *)(a1 + 2368) = v5;
  *(_OWORD *)(a1 + 2432) = v5;
  *(_OWORD *)(a1 + 1296) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 1360) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 1424) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 1488) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 1552) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 1616) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 1680) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 1744) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 1808) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 1872) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 1936) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 2000) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 2064) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 2128) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 2192) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 2256) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 2320) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 2384) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 2448) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 1232) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 1312) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 1376) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 1440) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 1504) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 1568) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 1632) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 1696) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 1760) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 1824) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 1888) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 1952) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 2016) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 2080) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 2144) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 2208) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 2272) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 2336) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 2400) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 2464) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 1248) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 2496) = v5;
  *(_OWORD *)(a1 + 1328) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 1392) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 1456) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 1520) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 1584) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 1648) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 1712) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 1776) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 1840) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 1904) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 1968) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 2032) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 2096) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 2160) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 2224) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 2288) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 2352) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 2416) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 2480) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 1264) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 2512) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 2560) = v5;
  *(_OWORD *)(a1 + 2624) = v5;
  *(_OWORD *)(a1 + 2688) = v5;
  *(_OWORD *)(a1 + 2752) = v5;
  *(_OWORD *)(a1 + 2816) = v5;
  *(_OWORD *)(a1 + 2880) = v5;
  *(_OWORD *)(a1 + 2944) = v5;
  *(_OWORD *)(a1 + 3008) = v5;
  *(_OWORD *)(a1 + 3072) = v5;
  *(_OWORD *)(a1 + 3136) = v5;
  *(_OWORD *)(a1 + 3200) = v5;
  *(_OWORD *)(a1 + 3264) = v5;
  *(_OWORD *)(a1 + 3328) = v5;
  *(_OWORD *)(a1 + 3392) = v5;
  *(_OWORD *)(a1 + 3456) = v5;
  *(_OWORD *)(a1 + 3520) = v5;
  *(_OWORD *)(a1 + 3584) = v5;
  *(_OWORD *)(a1 + 3648) = v5;
  *(_OWORD *)(a1 + 3712) = v5;
  *(_OWORD *)(a1 + 2576) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 2640) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 2704) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 2768) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 2832) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 2896) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 2960) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 3024) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 3088) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 3152) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 3216) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 3280) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 3344) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 3408) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 3472) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 3536) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 3600) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 3664) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 3728) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 3776) = v5;
  *(_OWORD *)(a1 + 2592) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 2656) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 2720) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 2784) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 2848) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 2912) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 2976) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 3040) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 3104) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 3168) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 3232) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 3296) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 3360) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 3424) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 3488) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 3552) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 3616) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 3680) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 3744) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 2528) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 3792) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 2608) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 2672) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 2736) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 2800) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 2864) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 2928) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 2992) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 3056) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 3120) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 3184) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 3248) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 3312) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 3376) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 3440) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 3504) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 3568) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 3632) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 3696) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 3760) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 2544) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 3808) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 3840) = v5;
  *(_OWORD *)(a1 + 3904) = v5;
  *(_OWORD *)(a1 + 3968) = v5;
  *(_OWORD *)(a1 + 4032) = v5;
  *(_OWORD *)(a1 + 4096) = v5;
  *(_OWORD *)(a1 + 4160) = v5;
  *(_OWORD *)(a1 + 4224) = v5;
  *(_OWORD *)(a1 + 4288) = v5;
  *(_OWORD *)(a1 + 4352) = v5;
  *(_OWORD *)(a1 + 4416) = v5;
  *(_OWORD *)(a1 + 4480) = v5;
  *(_OWORD *)(a1 + 4544) = v5;
  *(_OWORD *)(a1 + 4608) = v5;
  *(_OWORD *)(a1 + 4672) = v5;
  *(_OWORD *)(a1 + 4736) = v5;
  *(_OWORD *)(a1 + 4800) = v5;
  *(_OWORD *)(a1 + 4864) = v5;
  *(_OWORD *)(a1 + 4928) = v5;
  *(_OWORD *)(a1 + 4992) = v5;
  *(_OWORD *)(a1 + 5056) = v5;
  *(_OWORD *)(a1 + 3856) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 3920) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 3984) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 4048) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 4112) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 4176) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 4240) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 4304) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 4368) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 4432) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 4496) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 4560) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 4624) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 4688) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 4752) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 4816) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 4880) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 4944) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 5008) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 5072) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 3872) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 3936) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 4000) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 4064) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 4128) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 4192) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 4256) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 4320) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 4384) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 4448) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 4512) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 4576) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 4640) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 4704) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 4768) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 4832) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 4896) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 4960) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 5024) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 5088) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 3888) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 3952) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 4016) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 4080) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 4144) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 4208) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 4272) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 4336) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 4400) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 4464) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 4528) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 4592) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 4656) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 4720) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 4784) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 4848) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 4912) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 4976) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 5040) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 5104) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 3824) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 5120) = v5;
  *(_OWORD *)(a1 + 5184) = v5;
  *(_OWORD *)(a1 + 5248) = v5;
  *(_OWORD *)(a1 + 5312) = v5;
  *(_OWORD *)(a1 + 5376) = v5;
  *(_OWORD *)(a1 + 5440) = v5;
  *(_OWORD *)(a1 + 5504) = v5;
  *(_OWORD *)(a1 + 5568) = v5;
  *(_OWORD *)(a1 + 5632) = v5;
  *(_OWORD *)(a1 + 5696) = v5;
  *(_OWORD *)(a1 + 5760) = v5;
  *(_OWORD *)(a1 + 5824) = v5;
  *(_OWORD *)(a1 + 5888) = v5;
  *(_OWORD *)(a1 + 5952) = v5;
  *(_OWORD *)(a1 + 6016) = v5;
  *(_OWORD *)(a1 + 6080) = v5;
  *(_OWORD *)(a1 + 6144) = v5;
  *(_OWORD *)(a1 + 6208) = v5;
  *(_OWORD *)(a1 + 6272) = v5;
  *(_OWORD *)(a1 + 6336) = v5;
  *(_OWORD *)(a1 + 5136) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 5200) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 5264) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 5328) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 5392) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 5456) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 5520) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 5584) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 5648) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 5712) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 5776) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 5840) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 5904) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 5968) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 6032) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 6096) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 6160) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 6224) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 6288) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 6352) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 5152) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 5216) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 5280) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 5344) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 5408) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 5472) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 5536) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 5600) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 5664) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 5728) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 5792) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 5856) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 5920) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 5984) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 6048) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 6112) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 6176) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 6240) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 6304) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 6368) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 5168) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 5232) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 5296) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 5360) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 5424) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 5488) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 5552) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 5616) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 5680) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 5744) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 5808) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 5872) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 5936) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 6000) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 6064) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 6128) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 6192) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 6256) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 6320) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 6384) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 6400) = v5;
  *(_OWORD *)(a1 + 6464) = v5;
  *(_OWORD *)(a1 + 6528) = v5;
  *(_OWORD *)(a1 + 6592) = v5;
  *(_OWORD *)(a1 + 6656) = v5;
  *(_OWORD *)(a1 + 6720) = v5;
  *(_OWORD *)(a1 + 6784) = v5;
  *(_OWORD *)(a1 + 6848) = v5;
  *(_OWORD *)(a1 + 6912) = v5;
  *(_OWORD *)(a1 + 6976) = v5;
  *(_OWORD *)(a1 + 7040) = v5;
  *(_OWORD *)(a1 + 7104) = v5;
  *(_OWORD *)(a1 + 7168) = v5;
  *(_OWORD *)(a1 + 7232) = v5;
  *(_OWORD *)(a1 + 7296) = v5;
  *(_OWORD *)(a1 + 7360) = v5;
  *(_OWORD *)(a1 + 7424) = v5;
  *(_OWORD *)(a1 + 7488) = v5;
  *(_OWORD *)(a1 + 7552) = v5;
  *(_OWORD *)(a1 + 7616) = v5;
  *(_OWORD *)(a1 + 6480) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 6544) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 6608) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 6672) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 6736) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 6800) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 6864) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 6928) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 6992) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 7056) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 7120) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 7184) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 7248) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 7312) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 7376) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 7440) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 7504) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 7568) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 7632) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 6416) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 6496) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 6560) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 6624) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 6688) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 6752) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 6816) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 6880) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 6944) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 7008) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 7072) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 7136) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 7200) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 7264) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 7328) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 7392) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 7456) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 7520) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 7584) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 7648) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 6432) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 7680) = v5;
  *(_OWORD *)(a1 + 6512) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 6576) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 6640) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 6704) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 6768) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 6832) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 6896) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 6960) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 7024) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 7088) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 7152) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 7216) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 7280) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 7344) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 7408) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 7472) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 7536) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 7600) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 7664) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 6448) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 7696) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 7744) = v5;
  *(_OWORD *)(a1 + 7808) = v5;
  *(_OWORD *)(a1 + 7872) = v5;
  *(_OWORD *)(a1 + 7936) = v5;
  *(_OWORD *)(a1 + 8000) = v5;
  *(_OWORD *)(a1 + 8064) = v5;
  *(_OWORD *)(a1 + 8128) = v5;
  *(_OWORD *)(a1 + 7760) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 7824) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 7888) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 7952) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 8016) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 8080) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 8144) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 7776) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 7840) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 7904) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 7968) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 8032) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 8096) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 8160) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 7712) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 7792) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 7856) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 7920) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 7984) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 8048) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 8112) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 8176) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 7728) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 0x2000) = v5;
  *(_OWORD *)(a1 + 8208) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 8224) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 8240) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 8256) = v5;
  *(_OWORD *)(a1 + 8272) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 8288) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 8304) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 8320) = v5;
  *(_OWORD *)(a1 + 8336) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 8352) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 8368) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 8384) = v5;
  *(_OWORD *)(a1 + 8400) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 8416) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 8432) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 8448) = v5;
  *(_OWORD *)(a1 + 8464) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 8480) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 8496) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 8512) = v5;
  *(_OWORD *)(a1 + 8528) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 8544) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 8560) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 8576) = v5;
  *(_OWORD *)(a1 + 8592) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 8608) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 8624) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 8640) = v5;
  *(_OWORD *)(a1 + 8656) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 8672) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 8688) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 8704) = v5;
  *(_OWORD *)(a1 + 8720) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 8736) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 8752) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 8768) = v5;
  *(_OWORD *)(a1 + 8784) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 8800) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 8816) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 8832) = v5;
  *(_OWORD *)(a1 + 8848) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 8864) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 8880) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 8896) = v5;
  *(_OWORD *)(a1 + 8912) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 8928) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 8944) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 8960) = v5;
  *(_OWORD *)(a1 + 8976) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 8992) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 9008) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 9024) = v5;
  *(_OWORD *)(a1 + 9040) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 9056) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 9072) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 9088) = v5;
  *(_OWORD *)(a1 + 9104) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 9120) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 9136) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 9152) = v5;
  *(_OWORD *)(a1 + 9168) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 9184) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 9200) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 9216) = v5;
  *(_OWORD *)(a1 + 9232) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 9248) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 9264) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 9280) = v5;
  *(_OWORD *)(a1 + 9296) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 9312) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 9328) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 9344) = v5;
  *(_OWORD *)(a1 + 9360) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 9376) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 9392) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 9408) = v5;
  *(_OWORD *)(a1 + 9424) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 9440) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 9456) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 9472) = v5;
  *(_OWORD *)(a1 + 9488) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 9504) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 9520) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 9536) = v5;
  *(_OWORD *)(a1 + 9552) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 9568) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 9584) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 9600) = v5;
  *(_OWORD *)(a1 + 9616) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 9632) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 9648) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 9664) = v5;
  *(_OWORD *)(a1 + 9680) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 9696) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 9712) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 9728) = v5;
  *(_OWORD *)(a1 + 9744) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 9760) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 9776) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 9792) = v5;
  *(_OWORD *)(a1 + 9808) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 9824) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 9840) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 9856) = v5;
  *(_OWORD *)(a1 + 9872) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 9888) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 9904) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 9920) = v5;
  *(_OWORD *)(a1 + 9936) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 9952) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 9968) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 9984) = v5;
  *(_OWORD *)(a1 + 10000) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 10016) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 10032) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 10048) = v5;
  *(_OWORD *)(a1 + 10064) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 10080) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 10096) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 10112) = v5;
  *(_OWORD *)(a1 + 10128) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 10144) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 10160) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 10176) = v5;
  *(_OWORD *)(a1 + 10192) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 10208) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 10224) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 10240) = v5;
  *(_OWORD *)(a1 + 10256) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 10272) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 10288) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 10304) = v5;
  *(_OWORD *)(a1 + 10320) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 10336) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 10352) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 10368) = v5;
  *(_OWORD *)(a1 + 10384) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 10400) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 10416) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 10432) = v5;
  *(_OWORD *)(a1 + 10448) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 10464) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 10480) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 10496) = v5;
  *(_OWORD *)(a1 + 10512) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 10528) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 10544) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 10560) = v5;
  *(_OWORD *)(a1 + 10576) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 10592) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 10608) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 10624) = v5;
  *(_OWORD *)(a1 + 10640) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 10656) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 10672) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 10688) = v5;
  *(_OWORD *)(a1 + 10704) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 10720) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 10736) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 10752) = v5;
  *(_OWORD *)(a1 + 10768) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 10784) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 10800) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 10816) = v5;
  *(_OWORD *)(a1 + 10832) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 10848) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 10864) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 10880) = v5;
  *(_OWORD *)(a1 + 10896) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 10912) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 10928) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 10944) = v5;
  *(_OWORD *)(a1 + 10960) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 10976) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 10992) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 11008) = v5;
  *(_OWORD *)(a1 + 11024) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 11040) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 11056) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 11072) = v5;
  *(_OWORD *)(a1 + 11088) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 11104) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 11120) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 11136) = v5;
  *(_OWORD *)(a1 + 11152) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 11168) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 11184) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 11200) = v5;
  *(_OWORD *)(a1 + 11216) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 11232) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 11248) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 11264) = v5;
  *(_OWORD *)(a1 + 11280) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 11296) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 11312) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 11328) = v5;
  *(_OWORD *)(a1 + 11344) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 11360) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 11376) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 11392) = v5;
  *(_OWORD *)(a1 + 11408) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 11424) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 11440) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 11456) = v5;
  *(_OWORD *)(a1 + 11472) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 11488) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 11504) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 11520) = v5;
  *(_OWORD *)(a1 + 11536) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 11552) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 11568) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 11584) = v5;
  *(_OWORD *)(a1 + 11600) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 11616) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 11632) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 11648) = v5;
  *(_OWORD *)(a1 + 11664) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 11680) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 11696) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 11712) = v5;
  *(_OWORD *)(a1 + 11728) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 11744) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 11760) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 11776) = v5;
  *(_OWORD *)(a1 + 11792) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 11808) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 11824) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 11840) = v5;
  *(_OWORD *)(a1 + 11856) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 11872) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 11888) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 11904) = v5;
  *(_OWORD *)(a1 + 11920) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 11936) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 11952) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 11968) = v5;
  *(_OWORD *)(a1 + 11984) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 12000) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 12016) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 12032) = v5;
  *(_OWORD *)(a1 + 12048) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 12064) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 12080) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 12096) = v5;
  *(_OWORD *)(a1 + 12112) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 12128) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 12144) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 12160) = v5;
  *(_OWORD *)(a1 + 12176) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 12192) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 12208) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 12224) = v5;
  *(_OWORD *)(a1 + 12240) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 12256) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 12272) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 12288) = v5;
  *(_OWORD *)(a1 + 12304) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 12320) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 12336) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 12352) = v5;
  *(_OWORD *)(a1 + 12368) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 12384) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 12400) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 12416) = v5;
  *(_OWORD *)(a1 + 12432) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 12448) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 12464) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 12480) = v5;
  *(_OWORD *)(a1 + 12496) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 12512) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 12528) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 12544) = v5;
  *(_OWORD *)(a1 + 12560) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 12576) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 12592) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 12608) = v5;
  *(_OWORD *)(a1 + 12624) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 12640) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 12656) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 12672) = v5;
  *(_OWORD *)(a1 + 12688) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 12704) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 12720) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 12736) = v5;
  *(_OWORD *)(a1 + 12752) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 12768) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 12784) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 12800) = v5;
  *(_OWORD *)(a1 + 12816) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 12832) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 12848) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 12864) = v5;
  *(_OWORD *)(a1 + 12880) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 12896) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 12912) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 12928) = v5;
  *(_OWORD *)(a1 + 12944) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 12960) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 12976) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 12992) = v5;
  *(_OWORD *)(a1 + 13008) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 13024) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 13040) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 13056) = v5;
  *(_OWORD *)(a1 + 13072) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 13088) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 13104) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 13120) = v5;
  *(_OWORD *)(a1 + 13136) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 13152) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 13168) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 13184) = v5;
  *(_OWORD *)(a1 + 13200) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 13216) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 13232) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 13248) = v5;
  *(_OWORD *)(a1 + 13264) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 13280) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 13296) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 13312) = v5;
  *(_OWORD *)(a1 + 13328) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 13344) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 13360) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 13376) = v5;
  *(_OWORD *)(a1 + 13392) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 13408) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 13424) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 13440) = v5;
  *(_OWORD *)(a1 + 13456) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 13472) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 13488) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 13504) = v5;
  *(_OWORD *)(a1 + 13520) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 13536) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 13552) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 13568) = v5;
  *(_OWORD *)(a1 + 13584) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 13600) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 13616) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 13632) = v5;
  *(_OWORD *)(a1 + 13648) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 13664) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 13680) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 13696) = v5;
  *(_OWORD *)(a1 + 13712) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 13728) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 13744) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 13760) = v5;
  *(_OWORD *)(a1 + 13776) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 13792) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 13808) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 13824) = v5;
  *(_OWORD *)(a1 + 13840) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 13856) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 13872) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 13888) = v5;
  *(_OWORD *)(a1 + 13904) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 13920) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 13936) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 13952) = v5;
  *(_OWORD *)(a1 + 13968) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 13984) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 14000) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 14016) = v5;
  *(_OWORD *)(a1 + 14032) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 14048) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 14064) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 14080) = v5;
  *(_OWORD *)(a1 + 14096) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 14112) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 14128) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 14144) = v5;
  *(_OWORD *)(a1 + 14160) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 14176) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 14192) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 14208) = v5;
  *(_OWORD *)(a1 + 14224) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 14240) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 14256) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 14272) = v5;
  *(_OWORD *)(a1 + 14288) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 14304) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 14320) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 14336) = v5;
  *(_OWORD *)(a1 + 14352) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 14368) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 14384) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 14400) = v5;
  *(_OWORD *)(a1 + 14416) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 14432) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 14448) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 14464) = v5;
  *(_OWORD *)(a1 + 14480) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 14496) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 14512) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 14528) = v5;
  *(_OWORD *)(a1 + 14544) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 14560) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 14576) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 14592) = v5;
  *(_OWORD *)(a1 + 14608) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 14624) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 14640) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 14656) = v5;
  *(_OWORD *)(a1 + 14672) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 14688) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 14704) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 14720) = v5;
  *(_OWORD *)(a1 + 14736) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 14752) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 14768) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 14784) = v5;
  *(_OWORD *)(a1 + 14800) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 14816) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 14832) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 14848) = v5;
  *(_OWORD *)(a1 + 14864) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 14880) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 14896) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 14912) = v5;
  *(_OWORD *)(a1 + 14928) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 14944) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 14960) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 14976) = v5;
  *(_OWORD *)(a1 + 14992) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 15008) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 15024) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 15040) = v5;
  *(_OWORD *)(a1 + 15056) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 15072) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 15088) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 15104) = v5;
  *(_OWORD *)(a1 + 15120) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 15136) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 15152) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 15168) = v5;
  *(_OWORD *)(a1 + 15184) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 15200) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 15216) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 15232) = v5;
  *(_OWORD *)(a1 + 15248) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 15264) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 15280) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 15296) = v5;
  *(_OWORD *)(a1 + 15312) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 15328) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 15344) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 15360) = v5;
  *(_OWORD *)(a1 + 15376) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 15392) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 15408) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 15424) = v5;
  *(_OWORD *)(a1 + 15440) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 15456) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 15472) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 15488) = v5;
  *(_OWORD *)(a1 + 15504) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 15520) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 15536) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 15552) = v5;
  *(_OWORD *)(a1 + 15568) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 15584) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 15600) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 15616) = v5;
  *(_OWORD *)(a1 + 15632) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 15648) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 15664) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 15680) = v5;
  *(_OWORD *)(a1 + 15696) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 15712) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 15728) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 15744) = v5;
  *(_OWORD *)(a1 + 15760) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 15776) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 15792) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 15808) = v5;
  *(_OWORD *)(a1 + 15824) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 15840) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 15856) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 15872) = v5;
  *(_OWORD *)(a1 + 15888) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 15904) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 15920) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 15936) = v5;
  *(_OWORD *)(a1 + 15952) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 15968) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 15984) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 16000) = v5;
  *(_OWORD *)(a1 + 16016) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 16032) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 16048) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 16064) = v5;
  *(_OWORD *)(a1 + 16080) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 16096) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 16112) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 16128) = v5;
  *(_OWORD *)(a1 + 16144) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 16160) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 16176) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 16192) = v5;
  *(_OWORD *)(a1 + 16208) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 16224) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 16240) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 16256) = v5;
  *(_OWORD *)(a1 + 16272) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 16288) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 16304) = *(_OWORD *)&a4;
  *(_OWORD *)(a1 + 16320) = v5;
  *(_OWORD *)(a1 + 16336) = *(_OWORD *)&a2;
  *(_OWORD *)(a1 + 16352) = *(_OWORD *)&a3;
  *(_OWORD *)(a1 + 16368) = *(_OWORD *)&a4;
  *(_DWORD *)(a1 + 0x4000) = 0;
  *(_BYTE *)(a1 + 16388) = 1;
  return result;
}


===== DECOMPILE producer helper sub_140631F90 (cam-rel transform) ea=0x140631F90 rva=0x631F90 sub_140631F90 =====
  start=0x140631F90 end=0x14063228D size=0x2FD
  prologue AOB(48): 48 8B C4 48 81 EC 98 00 00 00 41 0F 10 50 20 0F 29 70 E8 0F 28 C2 41 0F C6 40 30 44 41 0F C6 50 30 EE 0F 29 78 D8 44 0F 29 40 C8 44 0F 29 48 B8
void sub_140631F90()
{
  JUMPOUT(0x140631F93LL);
}


===== DECOMPILE pose clone sub_14072CA00 (+0x350/+0x360/+0x550) ea=0x14072CA00 rva=0x72CA00 sub_14072CA00 =====
  start=0x14072CA00 end=0x14072D083 size=0x683
  prologue AOB(48): 48 8B 41 10 4C 8B D2 4C 8B D9 4C 63 48 04 48 8B 42 10 4C 63 40 04 41 8B 44 10 18 41 89 44 09 18 41 8B 44 10 1C 41 89 44 09 1C 41 0F B6 44 10 20
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


===== DECOMPILE pose/aggregate clone sub_140BD1F70 (+0x550/+0x660) ea=0x140BD1F70 rva=0xBD1F70 sub_140BD1F70 =====
  start=0x140BD1F70 end=0x140BD2562 size=0x5F2
  prologue AOB(48): 48 89 5C 24 10 48 89 6C 24 18 48 89 4C 24 08 56 57 41 56 48 83 EC 20 48 8B FA 48 8B D9 0F 10 02 0F 11 01 0F 10 4A 10 0F 11 49 10 0F 10 42 20 0F
void sub_140BD1F70()
{
  JUMPOUT(0x140BD1F75LL);
}


===== DECOMPILE f64 +0x5C8 getter/setter sub_1407A9210 ea=0x1407A9210 rva=0x7A9210 sub_1407A9210 =====
  start=0x1407A9210 end=0x1407A9305 size=0xF5
  prologue AOB(48): 40 53 48 83 EC 70 F2 0F 10 89 D0 05 00 00 48 8B D9 F2 0F 10 91 C8 05 00 00 4C 8B C2 F2 0F 10 81 C0 05 00 00 48 8D 54 24 20 F2 0F 10 99 B8 05 00
__int64 __fastcall sub_1407A9210(unsigned __int64 *a1, __int64 a2)
{
  unsigned __int64 v3; // rax
  double v4; // xmm1_8
  double v5; // xmm2_8
  double v6; // xmm3_8
  __m128 v8; // [rsp+20h] [rbp-58h] BYREF
  __int128 v9; // [rsp+30h] [rbp-48h] BYREF
  _BYTE v10[32]; // [rsp+40h] [rbp-38h] BYREF
  __int128 v11; // [rsp+60h] [rbp-18h]
  int v12; // [rsp+80h] [rbp+8h] BYREF
  char v13; // [rsp+90h] [rbp+18h] BYREF
  char v14; // [rsp+98h] [rbp+20h] BYREF

  v8 = _mm_movelh_ps(
         _mm_unpacklo_ps(_mm_cvtpd_ps((__m128d)a1[183]), _mm_cvtpd_ps((__m128d)a1[184])),
         _mm_unpacklo_ps(_mm_cvtpd_ps((__m128d)a1[185]), _mm_cvtpd_ps((__m128d)a1[186])));
  ((void (__fastcall *)(_BYTE *, __m128 *, __int64))unk_140FDF290)(v10, &v8, a2);
  v3 = *a1;
  v4 = v8.m128_f32[1];
  v5 = v8.m128_f32[2];
  v6 = v8.m128_f32[3];
  *((double *)a1 + 183) = v8.m128_f32[0];
  v9 = v11;
  *((double *)a1 + 184) = v4;
  *((double *)a1 + 185) = v5;
  *((double *)a1 + 186) = v6;
  (*(void (__fastcall **)(unsigned __int64 *, __int128 *))(v3 + 864))(a1, &v9);
  sub_140FDA810(v10, &v14, &v13, &v12);
  return (*(__int64 (__fastcall **)(unsigned __int64 *))(*a1 + 848))(a1);
}


===== DECOMPILE +0x540 writer sub_1407A9880 ea=0x1407A9880 rva=0x7A9880 sub_1407A9880 =====
  start=0x1407A9880 end=0x1407A99D4 size=0x154
  prologue AOB(48): 40 53 48 83 EC 20 44 0F B6 81 44 06 00 00 48 8D 05 9B 87 8C 07 48 8B D9 45 84 C0 48 8D 0D AE 24 7F 08 0F 57 C0 48 0F 45 C1 0F 10 28 0F 11 AB 40
__m128 *__fastcall sub_1407A9880(__int64 a1, __int64 a2)
{
  char v2; // r8
  __m128 *result; // rax
  __m128 v5; // xmm5
  __m128 v6; // xmm3
  __m128 v7; // xmm3
  __m128 v8; // xmm3
  __m128 v9; // xmm3
  __m128 v10; // xmm3

  v2 = *(_BYTE *)(a1 + 1604);
  result = (__m128 *)&unk_148072030;
  if ( v2 )
    result = (__m128 *)&unk_148F9BD50;
  v5 = *result;
  *(__m128 *)(a1 + 1344) = *result;
  *(_OWORD *)(a1 + 1584) = 0LL;
  *(_DWORD *)(a1 + 1600) = 0;
  if ( v2 )
  {
    v6 = (__m128)*(unsigned int *)(a2 + 72);
    if ( v6.m128_f32[0] != 0.0 || *(float *)(a2 + 76) != 0.0 || *(float *)(a2 + 80) != 0.0 )
      *(__m128 *)(a1 + 1344) = _mm_movelh_ps(
                                 _mm_unpacklo_ps(v6, (__m128)*(unsigned int *)(a2 + 76)),
                                 (__m128)*(unsigned int *)(a2 + 80));
    return result;
  }
  v7 = (__m128)*(unsigned int *)(a2 + 96);
  if ( v7.m128_f32[0] != 0.0
    || *(float *)(a2 + 100) != 0.0
    || *(float *)(a2 + 104) != 0.0
    || *(float *)(a2 + 108) != 0.0 )
  {
    *(__m128 *)(a1 + 1584) = _mm_movelh_ps(
                               _mm_unpacklo_ps(v7, (__m128)*(unsigned int *)(a2 + 100)),
                               (__m128)*(unsigned int *)(a2 + 104));
    *(_DWORD *)(a1 + 1600) = *(_DWORD *)(a2 + 108);
  }
  if ( *(_BYTE *)(a1 + 1536) )
  {
    v8 = (__m128)*(unsigned int *)(a2 + 84);
    if ( v8.m128_f32[0] == 0.0 && *(float *)(a2 + 88) == 0.0 && *(float *)(a2 + 92) == 0.0 )
    {
      *(__m128 *)(a1 + 1344) = _mm_add_ps(v5, (__m128)xmmword_148F9BD80);
      goto LABEL_24;
    }
    v9 = _mm_add_ps(
           _mm_movelh_ps(_mm_unpacklo_ps(v8, (__m128)*(unsigned int *)(a2 + 88)), (__m128)*(unsigned int *)(a2 + 92)),
           v5);
    goto LABEL_23;
  }
  v10 = (__m128)*(unsigned int *)(a2 + 60);
  if ( v10.m128_f32[0] != 0.0 || *(float *)(a2 + 64) != 0.0 || *(float *)(a2 + 68) != 0.0 )
  {
    v9 = _mm_movelh_ps(_mm_unpacklo_ps(v10, (__m128)*(unsigned int *)(a2 + 64)), (__m128)*(unsigned int *)(a2 + 68));
LABEL_23:
    *(__m128 *)(a1 + 1344) = v9;
  }
LABEL_24:
  result = (__m128 *)sub_1449FE470();
  if ( (_BYTE)result )
    *(float *)(a1 + 1352) = *(float *)&dword_148F9BD28 + *(float *)(a1 + 1352);
  return result;
}


===== PRODUCER sub_140BB1EE0 CALLERS =====
  0 call sites:

===== CCamDriver vtbl 0x145E3F290 pose-getter slots (read +0x320/+0x550) =====
  slot[4] 0x1407A9DD0 (rva 0x7A9DD0) sub_1407A9DD0 references +320
  slot[12] 0x1407A3C80 (rva 0x7A3C80) sub_1407A3C80 references +550

; DONE 18.6s -> E:/Github/vrframework/_agent_reports/fh5_empress_lever_finalize.md
