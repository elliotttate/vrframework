# FH5 Empress camera-input-lane RE (raw script log)

```

############### QUERY 1: RTTI / string references ###############
[Q1] unique camera-name strings found: 20

  STR @0x145E42B08 (rva 0x5E42B08) [.rdata]: "HeadHomeOffsetX"
      ptr-slot @0x145E42350 xrefs=[]

  STR @0x145E42B18 (rva 0x5E42B18) [.rdata]: "HeadHomeOffsetY"
      ptr-slot @0x145E42360 xrefs=[]

  STR @0x145E42B28 (rva 0x5E42B28) [.rdata]: "HeadHomeOffsetZ"
      ptr-slot @0x145E42370 xrefs=[]

  STR @0x145E42B38 (rva 0x5E42B38) [.rdata]: "HeadTrackingX"
      ptr-slot @0x145E42380 xrefs=[]

  STR @0x145E42B48 (rva 0x5E42B48) [.rdata]: "HeadTrackingY"
      ptr-slot @0x145E42390 xrefs=[]

  STR @0x145E42B58 (rva 0x5E42B58) [.rdata]: "HeadTrackingZ"
      ptr-slot @0x145E423A0 xrefs=[]

  STR @0x145E42B68 (rva 0x5E42B68) [.rdata]: "HeadTrackingYaw"
      ptr-slot @0x145E423B0 xrefs=[]

  STR @0x145E42B78 (rva 0x5E42B78) [.rdata]: "HeadTrackingPitch"
      ptr-slot @0x145E423C0 xrefs=[]

  STR @0x145E43038 (rva 0x5E43038) [.rdata]: "CameraSpaceYPROffset"
      ptr-slot @0x145E41DF0 xrefs=[]

  STR @0x145E43050 (rva 0x5E43050) [.rdata]: "CameraSpaceYPRImpulse"
      ptr-slot @0x145E41E00 xrefs=[]

  STR @0x145E43068 (rva 0x5E43068) [.rdata]: "CameraSpaceXYZOffset"
      ptr-slot @0x145E41E10 xrefs=[]

  STR @0x145E43080 (rva 0x5E43080) [.rdata]: "CameraSpaceXYZImpulse"
      ptr-slot @0x145E41E20 xrefs=[]

  STR @0x145E43190 (rva 0x5E43190) [.rdata]: "CameraSpaceYPRMultiplier"
      ptr-slot @0x145E41F50 xrefs=[]

  STR @0x145E431B0 (rva 0x5E431B0) [.rdata]: "CameraSpaceXYZMultiplier"
      ptr-slot @0x145E41F60 xrefs=[]

  STR @0x145E431D0 (rva 0x5E431D0) [.rdata]: "CameraSpaceYPRSet"
      ptr-slot @0x145E41F70 xrefs=[]

  STR @0x145E431E8 (rva 0x5E431E8) [.rdata]: "CameraSpaceXYZSet"
      ptr-slot @0x145E41F80 xrefs=[]

  STR @0x145E59780 (rva 0x5E59780) [.rdata]: "HeadTrackingThread"
      xref<- 0x14009B89B  in func 0x14009B500 (rva 0x9B500)  | lea     r8, cs:145E59780h

  STR @0x145E9F238 (rva 0x5E9F238) [.rdata]: "MaxTraceDistanceIsCameraSpace"
      xref<- 0x140CD4D4B  in func 0x140CD4690 (rva 0xCD4690)  | lea     rdx, aMaxtracedistan; "MaxTraceDistanceIsCameraSpace"

  STR @0x145ED4E60 (rva 0x5ED4E60) [.rdata]: "CameraOffset"
      xref<- 0x141035F2E  in func 0x141033D80 (rva 0x1033D80)  | lea     rdx, cs:145ED4E60h

  STR @0x1461E4480 (rva 0x61E4480) [.rdata]: "CameraOffsetPerItem"
      xref<- 0x142D36545  in func 0x142D35FB0 (rva 0x2D35FB0)  | db  48h ; H

############### QUERY 5 (early): CCamDriver vtable sanity ###############

  vtable @0x145E3F290 (rva 0x5E3F290) in_rdata=True
    name='off_145E3F290'
    [0] @0x145E3F290 -> 0x1407A16A0 in_text=True  sub_1407A16A0
    [1] @0x145E3F298 -> 0x1407A6F70 in_text=True  sub_1407A6F70
    [2] @0x145E3F2A0 -> 0x1407A6430 in_text=True  sub_1407A6430
    [3] @0x145E3F2A8 -> 0x1407A6F00 in_text=True  sub_1407A6F00
    [4] @0x145E3F2B0 -> 0x1407A9DD0 in_text=True  sub_1407A9DD0
    [5] @0x145E3F2B8 -> 0x1407A3220 in_text=True  sub_1407A3220
    [-1] COL? @0x145E3F288 -> 0x146899FD0

  vtable @0x1465D6808 (rva 0x65D6808) in_rdata=True
    name=''
    [0] @0x1465D6808 -> 0x144A1D570 in_text=True  sub_144A1D570
    [1] @0x1465D6810 -> 0x1407A6F70 in_text=True  sub_1407A6F70
    [2] @0x1465D6818 -> 0x140602220 in_text=True  sub_140602220
    [3] @0x1465D6820 -> 0x140602BA0 in_text=True  
    [4] @0x1465D6828 -> 0x140609640 in_text=True  sub_140609640
    [5] @0x1465D6830 -> 0x144A24210 in_text=True  sub_144A24210
    [-1] COL? @0x1465D6800 -> 0x147317D38

  vtable @0x1465D6BA0 (rva 0x65D6BA0) in_rdata=True
    name=''
    [0] @0x1465D6BA0 -> 0x1405F4360 in_text=True  sub_1405F4360
    [1] @0x1465D6BA8 -> 0x140605520 in_text=True  sub_140605520
    [2] @0x1465D6BB0 -> 0x140605570 in_text=True  sub_140605570
    [3] @0x1465D6BB8 -> 0x140608BF0 in_text=True  sub_140608BF0
    [4] @0x1465D6BC0 -> 0x140608BA0 in_text=True  sub_140608BA0
    [5] @0x1465D6BC8 -> 0x140608B90 in_text=True  
    [-1] COL? @0x1465D6B98 -> 0x147317DD0

############### QUERY 2: per-frame CCamDriver build fn ###############
[Q2] seed candidate functions from vtable+string xrefs: 5
[Q2] candidates touching +0x320-family OR +0x540/+0x550: 1

  FUNC 0x141033D80 (rva 0x1033D80) sub_141033D80  [CAM2WORLD(+0x320) INPUT(+0x540/+0x550)]
      +0x320 w @0x141034960: mov     [rbp+320h], rax
      +0x320 r @0x141034985: lea     r8, [rbp+320h]
      +0x320 r @0x1410349A0: mov     rcx, [rbp+320h]
      +0x320 r @0x1410349B8: mov     rcx, [rbp+320h]
      +0x328 w @0x141034970: mov     dword ptr [rbp+328h], 5
      +0x32C w @0x14103497A: mov     qword ptr [rbp+32Ch], 4
      +0x340 w @0x141034A3C: mov     dword ptr [rbp+340h], 1
      +0x350 w @0x141034D61: mov     [rbp+350h], rax
      +0x350 r @0x141034D86: lea     r8, [rbp+350h]
      +0x350 r @0x141034DA1: mov     rcx, [rbp+350h]
      +0x350 r @0x141034DB9: mov     rcx, [rbp+350h]
      +0x368 w @0x141034E05: mov     [rbp+368h], rax
      +0x368 r @0x141034E2A: lea     r8, [rbp+368h]
      +0x368 r @0x141034E45: mov     rcx, [rbp+368h]
      +0x368 r @0x141034E5D: mov     rcx, [rbp+368h]
      +0x548 r @0x1410364CF: lea     rdx, [rbp+548h]
      +0x54C r @0x141036543: lea     rdx, [rbp+54Ch]
      +0x550 r @0x1410365B7: lea     rdx, [rbp+550h]
      +0x554 r @0x14103662B: lea     rdx, [rbp+554h]
      +0x558 r @0x1410366AB: lea     rdx, [rbp+558h]
      +0x55C r @0x1410368B7: lea     rdx, [rbp+55Ch]

############### QUERY 2b: global scan for fn with BOTH +0x320 write and +0x540/+0x550 read ###############
[Q2b] functions containing literal disp bytes 0x320/0x540/0x550: 2171
[Q2b] scanned=2171  fns with BOTH +0x320 WRITE and +0x540/+0x550 read: 141

  *** BOTH FUNC 0x145454000 (rva 0x5454000) sub_145454000
      +0x320 w @0x14545532A: movaps  [rbp+3450h+var_3130], xmm0
      +0x320 r @0x145455903: movaps  xmm1, [rbp+3450h+var_3130]
      +0x330 w @0x14545533B: movaps  [rbp+3450h+var_3120], xmm6
      +0x330 r @0x14545590A: movaps  xmm6, [rbp+3450h+var_3120]
      +0x340 w @0x14545534D: movaps  [rbp+3450h+var_3110], xmm15
      +0x340 r @0x145455911: movaps  xmm15, [rbp+3450h+var_3110]
      +0x350 w @0x145455366: movaps  [rbp+3450h+var_3100], xmm0
      +0x350 r @0x145455919: movaps  xmm3, [rbp+3450h+var_3100]
      +0x360 w @0x14545412B: movaps  [rbp+3450h+var_30F0], xmm11
      +0x360 r @0x1454543A8: movaps  xmm11, [rbp+3450h+var_30F0]
      +0x540 w @0x14545669F: movaps  [rbp+3450h+var_2F10], xmm14
      +0x540 r @0x145456D86: movaps  xmm14, [rbp+3450h+var_2F10]
      +0x550 w @0x145456710: movaps  [rbp+3450h+var_2F00], xmm15
      +0x550 r @0x145456D8E: movaps  xmm15, [rbp+3450h+var_2F00]

  *** BOTH FUNC 0x144EC8020 (rva 0x4EC8020) sub_144EC8020
      +0x330 w @0x144EC84B9: db  48h ; H
      +0x330 w @0x144EC84BA: db  89h
      +0x330 r @0x144EC851C: db  48h ; H
      +0x330 r @0x144EC851D: db  8Bh
      +0x340 w @0x144EC8C20: db    0
      +0x340 w @0x144EC8D2C: db    0
      +0x340 w @0x144EC9173: db  0Fh
      +0x340 w @0x144EC9174: db  11h
      +0x350 w @0x144EC9183: db  88h
      +0x360 w @0x144EC919A: db  48h ; H
      +0x360 w @0x144EC919B: db  89h
      +0x368 w @0x144EC91AA: db  48h ; H
      +0x368 w @0x144EC91AB: db  89h
      +0x540 r @0x144EC928A: db  48h ; H
      +0x540 r @0x144EC928B: db  8Dh
      +0x548 w @0x144EC80CF: db 0F0h
      +0x548 w @0x144EC80D0: db  48h ; H
      +0x548 w @0x144EC80D1: db  89h
      +0x548 r @0x144EC86C0: db  48h ; H
      +0x548 r @0x144EC86C1: db  8Bh
      +0x548 r @0x144EC8755: db  48h ; H
      +0x548 r @0x144EC8756: db  8Bh
      +0x548 r @0x144EC91A2: db  48h ; H
      +0x550 w @0x144EC92DF: db  48h ; H
      +0x550 w @0x144EC92E0: db  89h

  *** BOTH FUNC 0x141B4A090 (rva 0x1B4A090) sub_141B4A090
      +0x320 r @0x141B4A641: db  8Bh
      +0x320 w @0x141B4ACAA: db  0Fh
      +0x320 w @0x141B4ACAB: db  11h
      +0x324 r @0x141B4A64D: db  8Bh
      +0x328 r @0x141B4A659: db  8Bh
      +0x32C r @0x141B4A665: db  8Bh
      +0x330 r @0x141B4A671: db  8Bh
      +0x330 w @0x141B4ABF3: db  89h
      +0x340 r @0x141B4A6BC: db  8Bh
      +0x340 w @0x141B4ABCC: db 0F3h
      +0x340 w @0x141B4ABCD: db  44h ; D
      +0x340 w @0x141B4ABCE: db  0Fh
      +0x340 w @0x141B4ABCF: db  11h
      +0x340 w @0x141B4AD2F: db 0C7h
      +0x340 w @0x141B4ADDA: db  89h
      +0x350 r @0x141B4A6EC: db  8Bh
      +0x350 w @0x141B4ABA1: db  89h
      +0x360 r @0x141B4A71C: db  8Bh
      +0x360 w @0x141B4ACC3: db  89h
      +0x364 w @0x141B4A37D: db  89h
      +0x364 r @0x141B4A728: db  8Bh
      +0x368 w @0x141B4A389: db  89h
      +0x368 r @0x141B4A734: db  8Bh
      +0x544 r @0x141B4AAA7: db  8Bh
      +0x548 w @0x141B4A3E6: db  89h
      +0x548 r @0x141B4AAB3: db  8Bh
      +0x54C w @0x141B4A3F2: db  89h
      +0x54C r @0x141B4AABF: db  8Bh
      +0x550 w @0x141B4A3FE: db  89h
      +0x554 w @0x141B4A40A: db  89h
      +0x558 w @0x141B4A416: db  89h
      +0x55C w @0x141B4A422: db  89h

  *** BOTH FUNC 0x1452BA0C0 (rva 0x52BA0C0) sub_1452BA0C0
      +0x320 w @0x1452BA6C8: mov     [rbp+56D0h+var_53B0], rax
      +0x328 w @0x1452BA6BD: mov     [rbp+56D0h+var_53A8], rax
      +0x330 w @0x1452BA6D3: mov     [rbp+56D0h+var_53A0], rax
      +0x340 w @0x1452BA6F2: mov     [rbp+56D0h+var_5390], rax
      +0x350 w @0x1452BA71E: mov     [rbp+56D0h+var_5380], ax
      +0x368 r @0x1452BA7B4: lea     rcx, [rbp+56D0h+var_5368]
      +0x368 r @0x1452BA7C7: lea     rcx, [rbp+56D0h+var_5368]
      +0x368 r @0x1452BA7D3: mov     eax, [rbp+56D0h+var_5368]
      +0x540 w @0x1452BAC8A: mov     [rbp+56D0h+var_5190], rax
      +0x548 w @0x1452BACA6: mov     [rbp+56D0h+var_5188], rax
      +0x550 w @0x1452BACB3: mov     [rbp+56D0h+var_5180], eax
      +0x558 w @0x1452BACC0: mov     [rbp+56D0h+var_5178], rax

  *** BOTH FUNC 0x1410B4160 (rva 0x10B4160) sub_1410B4160
      +0x360 w @0x1410B4235: db  0Fh
      +0x360 w @0x1410B4236: db  29h ; )
      +0x540 w @0x1410B41F1: db  0Fh
      +0x540 w @0x1410B41F2: db  29h ; )
      +0x540 r @0x1410B424A: db  0Fh
      +0x540 w @0x1410B424B: db  28h ; (

  *** BOTH FUNC 0x142B001F0 (rva 0x2B001F0) sub_142B001F0
      +0x320 w @0x142B00D28: mov     qword ptr [rbp+1BA0h+var_1888+8], rbx
      +0x328 w @0x142B00E83: movdqu  [rbp+1BA0h+var_1878], xmm0
      +0x328 w @0x142B00E9B: mov     qword ptr [rbp+1BA0h+var_1878], rax
      +0x328 r @0x142B00EB0: lea     rdx, [rbp+1BA0h+var_1878]
      +0x330 w @0x142B00EA2: mov     qword ptr [rbp+1BA0h+var_1878+8], rbx
      +0x340 w @0x142B00E35: mov     qword ptr [rbp+1BA0h+var_1868+8], rax
      +0x340 r @0x142B00F21: mov     rcx, qword ptr [rbp+1BA0h+var_1868+8]
      +0x350 w @0x142B00C99: mov     [rbp+1BA0h+var_1850], rbx
      +0x360 w @0x142B00F49: mov     [rbp+1BA0h+var_1840], rbx
      +0x360 w @0x142B00F64: mov     [rbp+1BA0h+var_1840], rcx
      +0x360 r @0x142B00F81: mov     rcx, [rbp+1BA0h+var_1840]
      +0x368 w @0x142B01063: movdqu  [rbp+1BA0h+var_1838], xmm0
      +0x368 w @0x142B0107B: mov     qword ptr [rbp+1BA0h+var_1838], rax
      +0x368 r @0x142B01097: lea     rcx, [rbp+1BA0h+var_1838]
      +0x540 r @0x142B03AA9: lea     rcx, [rbp+1BA0h+var_1660]
      +0x540 r @0x142B03AC1: lea     rcx, [rbp+1BA0h+var_1660]
      +0x548 r @0x142B03BBD: lea     rcx, [rbp+1BA0h+var_1658]
      +0x548 r @0x142B03BD5: lea     rcx, [rbp+1BA0h+var_1658]
      +0x550 r @0x142B03BEE: lea     rcx, [rbp+1BA0h+var_1650]
      +0x550 r @0x142B03C06: lea     rcx, [rbp+1BA0h+var_1650]
      +0x558 r @0x142B040E6: lea     rcx, [rbp+1BA0h+var_1648]
      +0x558 r @0x142B04105: lea     rcx, [rbp+1BA0h+var_1648]

  *** BOTH FUNC 0x142FEE230 (rva 0x2FEE230) sub_142FEE230
      +0x320 w @0x142FEE6B9: db  0Fh
      +0x320 w @0x142FEE6BA: db  29h ; )
      +0x328 r @0x142FEE26D: db  4Ch ; L
      +0x328 r @0x142FEE26E: db  8Bh
      +0x328 r @0x142FEEC07: db  4Ch ; L
      +0x328 r @0x142FEEC08: db  8Bh
      +0x330 r @0x142FEE274: db  4Ch ; L
      +0x330 r @0x142FEE275: db  8Bh
      +0x330 r @0x142FEE55A: db  48h ; H
      +0x330 r @0x142FEE55B: db  8Bh
      +0x330 w @0x142FEE6AB: db  0Fh
      +0x330 w @0x142FEE6AC: db  29h ; )
      +0x330 r @0x142FEEC13: db  48h ; H
      +0x330 r @0x142FEEC14: db  8Bh
      +0x340 w @0x142FEE6D7: db  0Fh
      +0x340 w @0x142FEE6D8: db  29h ; )
      +0x350 w @0x142FEE632: db  45h ; E
      +0x350 w @0x142FEE633: db 0F0h
      +0x350 w @0x142FEE634: db  0Fh
      +0x350 w @0x142FEE635: db  29h ; )
      +0x350 r @0x142FEE849: db  4Ch ; L
      +0x350 r @0x142FEE84A: db  8Dh
      +0x360 w @0x142FEE65E: db  0Fh
      +0x360 w @0x142FEE65F: db  29h ; )
      +0x540 w @0x142FEE88B: db  40h ; @
      +0x540 w @0x142FEE88C: db  40h ; @
      +0x540 w @0x142FEE88D: db  0Fh
      +0x540 w @0x142FEE88E: db  29h ; )
      +0x550 w @0x142FEE898: db  0Fh
      +0x550 w @0x142FEE899: db  29h ; )

  *** BOTH FUNC 0x141226240 (rva 0x1226240) sub_141226240
      +0x320 r @0x14122633B: db  48h ; H
      +0x320 r @0x14122633C: db  8Dh
      +0x328 w @0x141226334: db  48h ; H
      +0x328 w @0x141226335: db  89h
      +0x328 w @0x141226348: db  48h ; H
      +0x328 w @0x141226349: db  89h
      +0x330 r @0x141226356: db  48h ; H
      +0x330 r @0x141226357: db  8Dh
      +0x340 r @0x141226371: db  48h ; H
      +0x340 r @0x141226372: db  8Dh
      +0x350 r @0x14122638C: db  48h ; H
      +0x350 r @0x14122638D: db  8Dh
      +0x360 r @0x1412263A7: db  48h ; H
      +0x360 r @0x1412263A8: db  8Dh
      +0x368 w @0x1412263A0: db  48h ; H
      +0x368 w @0x1412263A1: db  89h
      +0x368 w @0x1412263B4: db  48h ; H
      +0x368 w @0x1412263B5: db  89h
      +0x540 w @0x141226528: db  48h ; H
      +0x540 w @0x141226529: db  89h
      +0x548 w @0x14122652F: db  48h ; H
      +0x548 w @0x141226530: db  89h
      +0x550 w @0x141226536: db  48h ; H
      +0x550 w @0x141226537: db  89h
      +0x558 w @0x14122653D: db  48h ; H
      +0x558 w @0x14122653E: db  89h

  *** BOTH FUNC 0x143788250 (rva 0x3788250) sub_143788250
      +0x330 w @0x143788283: db 0C7h
      +0x330 r @0x14378A463: db  48h ; H
      +0x330 r @0x14378A464: db  8Dh
      +0x340 w @0x1437882B8: db  48h ; H
      +0x340 w @0x1437882B9: db  89h
      +0x340 r @0x1437882F3: db  48h ; H
      +0x340 r @0x1437882F4: db  8Dh
      +0x360 w @0x1437882EC: db  48h ; H
      +0x360 w @0x1437882ED: db  89h
      +0x540 w @0x143788583: db  4Ch ; L
      +0x540 w @0x143788584: db  89h
      +0x548 r @0x1437896F3: db  49h ; I
      +0x548 r @0x1437896F4: db  8Dh
      +0x558 w @0x143788591: db  48h ; H
      +0x558 w @0x143788592: db  89h

  *** BOTH FUNC 0x141024270 (rva 0x1024270) sub_141024270
      +0x320 w @0x141024BBE: cmp     byte ptr [rcx+320h], 0
      +0x558 w @0x141024331: mov     [rdi+558h], rax

  *** BOTH FUNC 0x141564290 (rva 0x1564290) sub_141564290
      +0x328 w @0x1415643C2: db  48h ; H
      +0x328 w @0x1415643C3: db  89h
      +0x330 r @0x1415643BB: db  48h ; H
      +0x330 r @0x1415643BC: db  8Dh
      +0x360 r @0x1415643CC: db  48h ; H
      +0x360 r @0x1415643CD: db  8Dh
      +0x540 r @0x141564476: db  48h ; H
      +0x540 r @0x141564477: db  8Dh

  *** BOTH FUNC 0x1407A6300 (rva 0x7A6300) sub_1407A6300
      +0x328 w @0x1407A635E: call    qword ptr [rax+328h]
      +0x330 r @0x1407A636A: mov     rbx, [rax+330h]
      +0x540 r @0x1407A6382: movups  xmm0, xmmword ptr [r14+540h]
      +0x550 w @0x1407A63B2: mov     [r14+550h], rcx
      +0x558 w @0x1407A63BD: mov     [r14+558h], rcx

  *** BOTH FUNC 0x1405FE310 (rva 0x5FE310) sub_1405FE310
      +0x320 w @0x1405FE710: db  48h ; H
      +0x320 w @0x1405FE711: db  89h
      +0x328 r @0x1405FE6ED: db  4Ch ; L
      +0x328 r @0x1405FE6EE: db  8Bh
      +0x328 w @0x1405FE717: db  48h ; H
      +0x328 w @0x1405FE718: db 0C7h
      +0x330 r @0x1405FF3B0: lea     rcx, [rbp+330h]
      +0x330 r @0x1405FF3C7: lea     rdx, [rbp+330h]
      +0x330 r @0x1405FF3EB: mov     rcx, [rbp+330h]
      +0x330 w @0x1405FF431: mov     [rbp+330h], r12b
      +0x340 w @0x1405FF41F: mov     [rbp+340h], r12
      +0x350 w @0x1405FE629: db 0C6h
      +0x350 r @0x1405FE767: db  48h ; H
      +0x350 r @0x1405FE768: db  8Dh
      +0x350 r @0x1405FE7CB: db  48h ; H
      +0x350 r @0x1405FE7CC: db  8Dh
      +0x350 r @0x1405FE8C2: lea     r9, [rbp+350h]
      +0x350 r @0x1405FE907: lea     rdx, [rbp+350h]
      +0x350 r @0x1405FE969: lea     rcx, [rbp+350h]
      +0x360 w @0x1405FE617: db  48h ; H
      +0x360 w @0x1405FE618: db  89h
      +0x368 w @0x1405FE61E: db  48h ; H
      +0x368 w @0x1405FE61F: db 0C7h
      +0x550 r @0x1405FEA11: lea     rcx, [rbp+550h]
      +0x550 r @0x1405FEA74: lea     rcx, [rbp+550h]

  *** BOTH FUNC 0x142FA2330 (rva 0x2FA2330) sub_142FA2330
      +0x320 r @0x142FA3EF7: db  48h ; H
      +0x320 r @0x142FA3EF8: db  8Bh
      +0x324 r @0x142FA2A70: db  8Bh
      +0x328 r @0x142FA2AB1: db  8Bh
      +0x32C r @0x142FA2689: db  8Bh
      +0x330 r @0x142FA26CA: db  8Bh
      +0x330 r @0x142FA42D6: db  48h ; H
      +0x330 r @0x142FA42D7: db  8Dh
      +0x330 r @0x142FA4332: db  48h ; H
      +0x330 r @0x142FA4333: db  8Bh
      +0x330 r @0x142FA4410: db  48h ; H
      +0x330 r @0x142FA4411: db  8Bh
      +0x330 r @0x142FA447B: db  48h ; H
      +0x360 r @0x142FA4520: db  48h ; H
      +0x360 r @0x142FA4521: db  8Dh
      +0x360 r @0x142FA454F: db  48h ; H
      +0x360 r @0x142FA4550: db  8Bh
      +0x360 r @0x142FA461D: db  48h ; H
      +0x360 r @0x142FA461E: db  8Dh
      +0x368 w @0x142FA4556: db  48h ; H
      +0x368 w @0x142FA4557: db  83h
      +0x550 r @0x142FA4D0B: db  4Ch ; L
      +0x550 r @0x142FA4D0C: db  8Dh

  *** BOTH FUNC 0x14495A330 (rva 0x495A330) sub_14495A330
      +0x320 w @0x14495A855: db  48h ; H
      +0x320 w @0x14495A856: db  89h
      +0x328 w @0x14495A85C: db  48h ; H
      +0x328 w @0x14495A85D: db  89h
      +0x330 w @0x14495A863: db 0C6h
      +0x340 r @0x14495A87E: db  48h ; H
      +0x340 r @0x14495A87F: db  8Dh
      +0x350 w @0x14495A898: db  48h ; H
      +0x350 w @0x14495A899: db  89h
      +0x360 w @0x14495A8B3: db  48h ; H
      +0x360 w @0x14495A8B4: db  89h
      +0x368 r @0x14495A8BA: db  48h ; H
      +0x368 r @0x14495A8BB: db  8Dh
      +0x550 r @0x14495AE79: db  48h ; H
      +0x550 r @0x14495AE7A: db  8Dh

  *** BOTH FUNC 0x141B22370 (rva 0x1B22370) sub_141B22370
      +0x320 w @0x141B22666: db  0Fh
      +0x320 w @0x141B22667: db  11h
      +0x330 w @0x141B2266D: db  48h ; H
      +0x330 w @0x141B2266E: db 0C7h
      +0x350 w @0x141B2269E: db 0C7h
      +0x360 w @0x141B226CA: db 0C7h
      +0x364 w @0x141B226D4: db 0C7h
      +0x368 w @0x141B226DE: db 0C7h
      +0x540 w @0x141B22984: db  48h ; H
      +0x540 w @0x141B22985: db 0C7h
      +0x548 w @0x141B2298F: db 0C7h
      +0x54C w @0x141B22999: db 0C7h
      +0x550 w @0x141B229A2: db  40h ; @
      +0x550 w @0x141B229A3: db 0C7h
      +0x554 w @0x141B229AD: db 0C7h
      +0x558 w @0x141B229B7: db 0C7h
      +0x55C w @0x141B229C1: db 0C7h

  *** BOTH FUNC 0x1434EC440 (rva 0x34EC440) sub_1434EC440
      +0x320 w @0x1434EE034: db  45h ; E
      +0x320 w @0x1434EE035: db  40h ; @
      +0x320 w @0x1434EE036: db  48h ; H
      +0x320 w @0x1434EE037: db  89h
      +0x328 w @0x1434EE061: db  4Ch ; L
      +0x328 w @0x1434EE062: db  89h
      +0x330 w @0x1434EE045: db  4Ch ; L
      +0x330 w @0x1434EE046: db  89h
      +0x340 w @0x1434EC745: db  48h ; H
      +0x340 w @0x1434EC746: db  89h
      +0x368 r @0x1434ED1E4: db  48h ; H
      +0x368 r @0x1434ED1E5: db  8Bh
      +0x540 r @0x1434EC49C: db  48h ; H
      +0x540 r @0x1434EC49D: db  8Dh

  *** BOTH FUNC 0x1418C2450 (rva 0x18C2450) sub_1418C2450
      +0x320 w @0x1418C2913: db  48h ; H
      +0x320 w @0x1418C2914: db  89h
      +0x328 r @0x1418C28D2: db  48h ; H
      +0x328 r @0x1418C28D3: db  8Bh
      +0x328 w @0x1418C291A: db  48h ; H
      +0x328 w @0x1418C291B: db 0C7h
      +0x330 r @0x1418C2885: db  48h ; H
      +0x330 r @0x1418C2886: db  8Bh
      +0x330 w @0x1418C28CB: db  40h ; @
      +0x330 w @0x1418C28CC: db  88h
      +0x340 w @0x1418C28B9: db  48h ; H
      +0x340 w @0x1418C28BA: db  89h
      +0x350 r @0x1418C282B: db  48h ; H
      +0x350 r @0x1418C282C: db  8Bh
      +0x350 w @0x1418C2871: db  40h ; @
      +0x350 w @0x1418C2872: db  88h
      +0x360 w @0x1418C285F: db  48h ; H
      +0x360 w @0x1418C2860: db  89h
      +0x368 r @0x1418C281E: db  48h ; H
      +0x368 r @0x1418C281F: db  8Bh
      +0x368 w @0x1418C2866: db  48h ; H
      +0x368 w @0x1418C2867: db 0C7h
      +0x548 r @0x1418C24EE: db  48h ; H
      +0x548 r @0x1418C24EF: db  8Dh

  *** BOTH FUNC 0x1407A6460 (rva 0x7A6460) sub_1407A6460
      +0x320 r @0x1407A658F: mov     rbx, [rax+320h]
      +0x328 w @0x1407A65E2: call    qword ptr [rax+328h]
      +0x330 r @0x1407A65BC: mov     rbx, [rax+330h]
      +0x558 w @0x1407A64A6: mov     [r15+558h], bl

  *** BOTH FUNC 0x14484A4C0 (rva 0x484A4C0) sub_14484A4C0
      +0x320 w @0x14484B705: db  4Ch ; L
      +0x320 w @0x14484B706: db  89h
      +0x320 w @0x14484B78A: db  4Ch ; L
      +0x320 w @0x14484B78B: db  89h
      +0x328 w @0x14484B70C: db  4Ch ; L
      +0x328 w @0x14484B70D: db  89h
      +0x328 r @0x14484B749: db  48h ; H
      +0x328 r @0x14484B74A: db  8Bh
      +0x328 w @0x14484B791: db  4Ch ; L
      +0x328 w @0x14484B792: db  89h
      +0x330 w @0x14484B7AD: db 0C6h
      +0x330 r @0x14484B7C1: db  48h ; H
      +0x330 r @0x14484B7C2: db  8Dh
      +0x330 r @0x14484B7CE: db  48h ; H
      +0x330 r @0x14484B7CF: db  8Dh
      +0x330 r @0x14484B7F3: db  48h ; H
      +0x330 r @0x14484B7F4: db  8Bh
      +0x330 w @0x14484B832: db 0C6h
      +0x340 w @0x14484B79F: db  4Ch ; L
      +0x340 w @0x14484B7A0: db  89h
      +0x340 w @0x14484B824: db  4Ch ; L
      +0x340 w @0x14484B825: db  89h
      +0x350 w @0x14484B847: db 0C6h
      +0x350 r @0x14484B85B: db  48h ; H
      +0x350 r @0x14484B85C: db  8Dh
      +0x350 r @0x14484B868: db  48h ; H
      +0x350 r @0x14484B869: db  8Dh
      +0x350 r @0x14484B88D: db  48h ; H
      +0x350 r @0x14484B88E: db  8Bh
      +0x350 w @0x14484B8CC: db 0C6h
      +0x360 w @0x14484B839: db  4Ch ; L
      +0x360 w @0x14484B83A: db  89h
      +0x360 w @0x14484B8BE: db  4Ch ; L
      +0x360 w @0x14484B8BF: db  89h
      +0x368 w @0x14484B840: db  4Ch ; L
      +0x368 w @0x14484B841: db  89h
      +0x368 r @0x14484B87D: db  48h ; H
      +0x368 r @0x14484B87E: db  8Bh
      +0x368 w @0x14484B8C5: db  4Ch ; L
      +0x368 w @0x14484B8C6: db  89h
      +0x550 r @0x14484BCC7: db  48h ; H
      +0x550 r @0x14484BCC8: db  8Dh
      +0x550 r @0x14484BCD4: db  48h ; H
      +0x550 r @0x14484BCD5: db  8Dh
      +0x550 r @0x14484BCE9: db  48h ; H
      +0x550 r @0x14484BCEA: db  8Dh

  *** BOTH FUNC 0x1402D64F0 (rva 0x2D64F0) sub_1402D64F0
      +0x350 w @0x1402D677A: db  48h ; H
      +0x350 w @0x1402D677B: db  89h
      +0x360 w @0x1402D679A: db  48h ; H
      +0x360 w @0x1402D679B: db  89h
      +0x360 r @0x1402D67A1: db  48h ; H
      +0x360 r @0x1402D67A2: db  8Dh
      +0x548 w @0x1402D68BC: mov     [rbp+548h], rax
      +0x550 w @0x1402D68CF: mov     [rbp+550h], eax
      +0x558 w @0x1402D68DC: mov     [rbp+558h], rax
      +0x558 r @0x1402D68E3: lea     rax, [rbp+558h]

  *** BOTH FUNC 0x144A5E510 (rva 0x4A5E510) sub_144A5E510
      +0x320 w @0x144A5E62A: db 0FFh
      +0x320 w @0x144A5F4E8: db  4Ch ; L
      +0x320 w @0x144A5F4E9: db  89h
      +0x320 r @0x144A5F935: db  48h ; H
      +0x320 r @0x144A5F936: db  8Bh
      +0x320 w @0x144A6047C: db 0FFh
      +0x328 r @0x144A5F4F6: db  48h ; H
      +0x328 r @0x144A5F4F7: db  8Dh
      +0x330 w @0x144A5F4EF: db  48h ; H
      +0x330 w @0x144A5F4F0: db  89h
      +0x330 w @0x144A5F52C: db  48h ; H
      +0x330 w @0x144A5F52D: db  89h
      +0x330 r @0x144A5F923: db  48h ; H
      +0x330 r @0x144A5F924: db  8Bh
      +0x340 w @0x144A5F533: db  48h ; H
      +0x340 w @0x144A5F534: db 0C7h
      +0x340 w @0x144A5F571: db  48h ; H
      +0x340 w @0x144A5F572: db  89h
      +0x340 r @0x144A5F911: db  48h ; H
      +0x340 r @0x144A5F912: db  8Bh
      +0x350 w @0x144A5F578: db  48h ; H
      +0x350 w @0x144A5F579: db 0C7h
      +0x350 w @0x144A5F5B9: db  48h ; H
      +0x350 w @0x144A5F5BA: db  89h
      +0x350 r @0x144A5F8FF: db  48h ; H
      +0x350 r @0x144A5F900: db  8Bh
      +0x360 w @0x144A5F5E9: db  48h ; H
      +0x360 w @0x144A5F5EA: db  89h
      +0x360 r @0x144A5F8CA: db  48h ; H
      +0x360 r @0x144A5F8CB: db  8Bh
      +0x368 r @0x144A5F5F7: db  48h ; H
      +0x368 r @0x144A5F5F8: db  8Dh
      +0x368 r @0x144A5F86D: db  48h ; H
      +0x368 r @0x144A5F86E: db  8Bh
      +0x368 w @0x144A5F8B4: db  48h ; H
      +0x368 w @0x144A5F8B5: db 0C7h
      +0x540 r @0x144A5ECD7: db  48h ; H
      +0x540 r @0x144A5ECD8: db  8Dh
      +0x548 r @0x144A5ECF7: db  48h ; H
      +0x548 r @0x144A5ECF8: db  8Bh
      +0x54C r @0x144A5EC6D: db  0Fh
      +0x550 r @0x144A5EDBE: db  48h ; H
      +0x550 r @0x144A5EDBF: db  8Bh
      +0x550 r @0x144A607D9: db  48h ; H
      +0x550 r @0x144A607DA: db  8Dh
      +0x550 r @0x144A607EE: db  48h ; H
      +0x550 r @0x144A607EF: db  8Dh
      +0x550 r @0x144A6085F: db  48h ; H
      +0x550 r @0x144A60860: db  8Dh
      +0x558 w @0x144A607E7: db  4Ch ; L
      +0x558 w @0x144A607E8: db  89h
      +0x558 w @0x144A6082A: db  48h ; H
      +0x558 w @0x144A6082B: db  89h

  *** BOTH FUNC 0x14538E5E0 (rva 0x538E5E0) sub_14538E5E0
      +0x320 w @0x14538E94F: db 0F3h
      +0x320 w @0x14538E950: db  0Fh
      +0x320 w @0x14538E951: db  11h
      +0x320 r @0x14538EA45: db 0F3h
      +0x320 r @0x14538EA46: db  0Fh
      +0x320 w @0x14538EA47: db  10h
      +0x328 r @0x14538E88E: db  48h ; H
      +0x328 r @0x14538E88F: db  8Dh
      +0x328 r @0x14538EA4D: db  48h ; H
      +0x328 r @0x14538EA4E: db  8Dh
      +0x340 w @0x14538EF2A: db  48h ; H
      +0x340 w @0x14538EF2B: db  89h
      +0x350 w @0x14538EF55: db  48h ; H
      +0x350 w @0x14538EF56: db  89h
      +0x360 w @0x14538EF7F: db  45h ; E
      +0x360 w @0x14538EF80: db 0F2h
      +0x360 w @0x14538EF81: db  48h ; H
      +0x360 w @0x14538EF82: db  89h
      +0x368 w @0x14538EF69: db  45h ; E
      +0x368 w @0x14538EF6A: db 0F0h
      +0x368 w @0x14538EF6B: db  48h ; H
      +0x368 w @0x14538EF6C: db  89h
      +0x540 r @0x14538F041: db  48h ; H
      +0x540 r @0x14538F042: db  8Dh
      +0x540 r @0x145390B61: db  48h ; H
      +0x540 r @0x145390B62: db  8Bh
      +0x540 w @0x145390B77: db  4Ch ; L
      +0x540 w @0x145390B78: db  89h
      +0x550 w @0x14538F1FC: db  89h
      +0x550 r @0x14538F2C6: db  48h ; H
      +0x550 r @0x14538F2C7: db  8Dh
      +0x550 r @0x145390265: db  48h ; H
      +0x550 r @0x145390266: db  8Dh
      +0x558 w @0x14538F225: db  48h ; H
      +0x558 w @0x14538F226: db  89h

  *** BOTH FUNC 0x1453945E0 (rva 0x53945E0) sub_1453945E0
      +0x320 r @0x145394DF9: db  48h ; H
      +0x320 r @0x145394DFA: db  8Bh
      +0x328 r @0x145394DEE: db  48h ; H
      +0x328 r @0x145394DEF: db  8Bh
      +0x330 r @0x145394E19: db  4Dh ; M
      +0x330 r @0x145394E1A: db  40h ; @
      +0x330 r @0x145394E1B: db  48h ; H
      +0x330 r @0x145394E1C: db  8Bh
      +0x340 r @0x145394E26: db  48h ; H
      +0x340 r @0x145394E27: db  8Bh
      +0x350 r @0x145394E3A: db  48h ; H
      +0x350 r @0x145394E3B: db  8Bh
      +0x360 r @0x145394E50: db  48h ; H
      +0x360 r @0x145394E51: db  8Bh
      +0x368 r @0x145394E02: db  4Dh ; M
      +0x368 r @0x145394E03: db 0F0h
      +0x368 r @0x145394E04: db 0F3h
      +0x368 r @0x145394E05: db  0Fh
      +0x368 w @0x145394E06: db  10h
      +0x540 r @0x1453947AF: db  48h ; H
      +0x540 r @0x1453947B0: db  8Bh
      +0x548 r @0x145394793: db  48h ; H
      +0x548 r @0x145394794: db  8Bh
      +0x550 r @0x145394741: db  48h ; H
      +0x550 r @0x145394742: db  8Bh
      +0x558 r @0x145394718: db  48h ; H
      +0x558 r @0x145394719: db  8Bh

  *** BOTH FUNC 0x14147C5F0 (rva 0x147C5F0) sub_14147C5F0
      +0x320 w @0x14147CE6C: db  44h ; D
      +0x320 w @0x14147CE6D: db  0Fh
      +0x320 w @0x14147CE6E: db  29h ; )
      +0x330 w @0x14147CE78: db  0Fh
      +0x330 w @0x14147CE79: db  29h ; )
      +0x340 w @0x14147CE83: db  0Fh
      +0x340 w @0x14147CE84: db  29h ; )
      +0x350 w @0x14147CE8E: db  0Fh
      +0x350 w @0x14147CE8F: db  29h ; )
      +0x360 w @0x14147CE9C: db  0Fh
      +0x360 w @0x14147CE9D: db  29h ; )
      +0x540 w @0x14147CD6A: db  0Fh
      +0x540 w @0x14147CD6B: db  29h ; )
      +0x550 w @0x14147CD75: db  0Fh
      +0x550 w @0x14147CD76: db  29h ; )

  *** BOTH FUNC 0x144684760 (rva 0x4684760) sub_144684760
      +0x320 w @0x144684839: db  48h ; H
      +0x320 w @0x14468483A: db  89h
      +0x320 w @0x144684851: db  48h ; H
      +0x320 w @0x144684852: db  89h
      +0x320 r @0x1446848D6: db  4Ch ; L
      +0x320 r @0x1446848D7: db  8Dh
      +0x320 r @0x144684A0F: db  48h ; H
      +0x320 r @0x144684A10: db  8Bh
      +0x328 w @0x14468480B: db  48h ; H
      +0x328 w @0x14468480C: db  89h
      +0x328 w @0x144684823: db  48h ; H
      +0x328 w @0x144684824: db  89h
      +0x328 r @0x1446848CA: db  48h ; H
      +0x328 r @0x1446848CB: db  8Dh
      +0x328 r @0x144684A35: db  48h ; H
      +0x328 r @0x144684A36: db  8Bh
      +0x330 w @0x144684B58: db  48h ; H
      +0x330 w @0x144684B59: db  89h
      +0x330 w @0x144684B70: db  48h ; H
      +0x330 w @0x144684B71: db  89h
      +0x330 r @0x144684B91: db  48h ; H
      +0x330 r @0x144684B92: db  8Dh
      +0x330 r @0x144684C74: db  48h ; H
      +0x330 r @0x144684C75: db  8Bh
      +0x340 w @0x144684AE6: db  48h ; H
      +0x340 w @0x144684AE7: db  89h
      +0x340 w @0x144684AFE: db  48h ; H
      +0x340 w @0x144684AFF: db  89h
      +0x340 r @0x144684B83: db  4Ch ; L
      +0x340 r @0x144684B84: db  8Dh
      +0x340 r @0x144684CC0: db  48h ; H
      +0x340 r @0x144684CC1: db  8Bh
      +0x350 w @0x144684E4E: db  48h ; H
      +0x350 w @0x144684E4F: db  89h
      +0x350 w @0x144684E66: db  48h ; H
      +0x350 w @0x144684E67: db  89h
      +0x350 r @0x144684E7E: db  48h ; H
      +0x350 r @0x144684E7F: db  8Dh
      +0x350 r @0x144684F61: db  48h ; H
      +0x350 r @0x144684F62: db  8Bh
      +0x360 w @0x144685877: db  4Ch ; L
      +0x360 w @0x144685878: db  89h
      +0x360 w @0x144685897: db  4Ch ; L
      +0x360 w @0x144685898: db  89h
      +0x360 r @0x1446858A5: db  48h ; H
      +0x360 r @0x1446858A6: db  8Bh
      +0x360 w @0x1446858AC: db  48h ; H
      +0x360 w @0x1446858AD: db  89h
      +0x368 r @0x144684AD9: db  48h ; H
      +0x368 r @0x144684ADA: db  8Dh
      +0x368 r @0x144684CD3: db  48h ; H
      +0x368 r @0x144684CD4: db  8Bh
      +0x540 r @0x14468658E: db  48h ; H
      +0x540 r @0x14468658F: db  8Dh
      +0x540 r @0x1446866A3: db  48h ; H
      +0x540 r @0x1446866A4: db  8Bh
      +0x548 r @0x144686555: db  48h ; H
      +0x548 r @0x144686556: db  8Dh
      +0x548 r @0x1446866C9: db  48h ; H
      +0x548 r @0x1446866CA: db  8Bh
      +0x550 r @0x14468685C: db  48h ; H
      +0x550 r @0x14468685D: db  8Dh
      +0x558 r @0x14468684C: db  48h ; H
      +0x558 r @0x14468684D: db  8Dh
      +0x558 r @0x14468689B: db  48h ; H
      +0x558 r @0x14468689C: db  8Bh

  *** BOTH FUNC 0x145392780 (rva 0x5392780) sub_145392780
      +0x320 w @0x1453929FB: db  66h ; f
      +0x320 w @0x1453929FC: db  89h
      +0x328 r @0x1453929BE: db  48h ; H
      +0x328 r @0x1453929BF: db  8Dh
      +0x328 r @0x1453932DC: db  48h ; H
      +0x328 r @0x1453932DD: db  8Bh
      +0x328 w @0x1453932F2: db  4Ch ; L
      +0x328 w @0x1453932F3: db  89h
      +0x340 w @0x145392815: db  48h ; H
      +0x340 w @0x145392816: db  89h
      +0x548 r @0x145393217: db  48h ; H
      +0x548 r @0x145393218: db  8Bh
      +0x548 w @0x14539322D: db  4Ch ; L
      +0x548 w @0x14539322E: db  89h
      +0x558 r @0x145392C24: db  48h ; H
      +0x558 r @0x145392C25: db  8Dh

  *** BOTH FUNC 0x1428507E0 (rva 0x28507E0) sub_1428507E0
      +0x320 w @0x142851F6C: db  66h ; f
      +0x320 w @0x142851F6D: db  0Fh
      +0x320 w @0x142851F85: db  66h ; f
      +0x320 w @0x142851F86: db  0Fh
      +0x320 r @0x142851F9B: db  48h ; H
      +0x320 r @0x142851F9C: db  8Dh
      +0x330 w @0x1428520D2: db  66h ; f
      +0x330 w @0x1428520D3: db  0Fh
      +0x330 w @0x1428520EB: db  66h ; f
      +0x330 w @0x1428520EC: db  0Fh
      +0x330 r @0x1428520FD: db  48h ; H
      +0x330 r @0x1428520FE: db  8Dh
      +0x340 w @0x142850B88: db  89h
      +0x340 r @0x142850BA3: db  48h ; H
      +0x340 r @0x142850BA4: db  8Dh
      +0x340 r @0x142850C2F: db  48h ; H
      +0x340 r @0x142850C30: db  8Bh
      +0x350 w @0x142850B77: db  48h ; H
      +0x350 w @0x142850B78: db 0C7h
      +0x360 r @0x14285279E: db  48h ; H
      +0x360 r @0x14285279F: db  8Dh
      +0x360 r @0x142852848: db  48h ; H
      +0x360 r @0x142852849: db  8Dh
      +0x368 r @0x1428529DC: db  48h ; H
      +0x368 r @0x1428529DD: db  8Dh
      +0x368 r @0x142852A4D: db  48h ; H
      +0x368 r @0x142852A4E: db  8Dh
      +0x540 w @0x142851771: db  48h ; H
      +0x540 w @0x142851772: db  89h
      +0x540 r @0x1428517A5: db  48h ; H
      +0x540 r @0x1428517A6: db  8Dh
      +0x548 w @0x14285178B: db  48h ; H
      +0x548 w @0x14285178C: db  89h
      +0x550 w @0x142851792: db  48h ; H
      +0x550 w @0x142851793: db  89h
      +0x550 r @0x1428517B1: db  48h ; H
      +0x550 r @0x1428517B2: db  8Bh
      +0x558 w @0x142851004: db  48h ; H
      +0x558 w @0x142851005: db  89h
      +0x558 r @0x14285102C: db  48h ; H
      +0x558 r @0x14285102D: db  8Dh

  *** BOTH FUNC 0x144D54820 (rva 0x4D54820) sub_144D54820
      +0x320 r @0x144D54B88: db  48h ; H
      +0x320 r @0x144D54B89: db  8Bh
      +0x320 w @0x144D54BA8: db  40h ; @
      +0x320 w @0x144D54BA9: db  88h
      +0x330 w @0x144D54B96: db  48h ; H
      +0x330 w @0x144D54B97: db  89h
      +0x340 r @0x144D54B57: db  48h ; H
      +0x340 r @0x144D54B58: db  8Bh
      +0x340 w @0x144D54B77: db  40h ; @
      +0x340 w @0x144D54B78: db  88h
      +0x350 w @0x144D54B65: db  48h ; H
      +0x350 w @0x144D54B66: db  89h
      +0x540 r @0x144D54923: db  48h ; H
      +0x540 r @0x144D54924: db  8Bh
      +0x540 w @0x144D54943: db  40h ; @
      +0x540 w @0x144D54944: db  88h
      +0x550 w @0x144D54931: db  48h ; H
      +0x550 w @0x144D54932: db  89h
      +0x558 w @0x144D54919: db  48h ; H
      +0x558 w @0x144D5491A: db  83h
      +0x558 w @0x144D54938: db  48h ; H
      +0x558 w @0x144D54939: db 0C7h

  *** BOTH FUNC 0x140BDC830 (rva 0xBDC830) sub_140BDC830
      +0x320 w @0x140BDD265: mov     [rbp+320h], rax
      +0x320 r @0x140BDD297: lea     r9, [rbp+320h]
      +0x328 w @0x140BDD26F: movdqu  xmmword ptr [rbp+328h], xmm0
      +0x340 w @0x140BDD4BD: movdqu  xmmword ptr [rbp+340h], xmm0
      +0x350 w @0x140BDD7BD: mov     [rbp+350h], rcx
      +0x350 r @0x140BDD7E8: lea     r9, [rbp+350h]
      +0x368 w @0x140BDDF2C: mov     [rbp+368h], rax
      +0x368 r @0x140BDDF60: lea     r9, [rbp+368h]
      +0x558 r @0x140BDDE65: lea     rcx, [rbp+558h]
      +0x558 r @0x140BDDE93: lea     r9, [rbp+558h]

  *** BOTH FUNC 0x140CEE860 (rva 0xCEE860) sub_140CEE860
      +0x320 w @0x140CEFCAE: db 0F3h
      +0x320 w @0x140CEFCAF: db  0Fh
      +0x320 r @0x140CEFCD7: db  48h ; H
      +0x320 r @0x140CEFCD8: db  8Dh
      +0x330 w @0x140CEFCB6: db  4Ch ; L
      +0x330 w @0x140CEFCB7: db  89h
      +0x340 r @0x140CEE9C1: lea     rax, [rbp+340h]
      +0x340 w @0x140CEEA23: mov     [rbp+340h], rax
      +0x340 r @0x140CEEBDB: lea     r8, [rbp+340h]
      +0x350 w @0x140CEEA31: mov     [rbp+350h], rax
      +0x360 w @0x140CEEA3F: mov     [rbp+360h], rax
      +0x368 w @0x140CEEA46: mov     [rbp+368h], rax
      +0x558 w @0x140CEF5C2: mov     [rbp+558h], r14
      +0x558 w @0x140CEF5DC: mov     [rbp+558h], r15
      +0x558 r @0x140CEF5E9: lea     rdx, [rbp+558h]
      +0x558 r @0x140CEF5FA: lea     rcx, [rbp+558h]

  *** BOTH FUNC 0x141248870 (rva 0x1248870) sub_141248870
      +0x320 w @0x141248876: db 0F6h
      +0x540 r @0x141248887: db  8Bh
      +0x540 w @0x14124889B: db  89h

  *** BOTH FUNC 0x1418F2880 (rva 0x18F2880) sub_1418F2880
      +0x328 w @0x1418F4577: db  48h ; H
      +0x328 w @0x1418F4578: db  89h
      +0x330 w @0x1418F457E: db  48h ; H
      +0x330 w @0x1418F457F: db 0C7h
      +0x350 w @0x1418F2913: db  48h ; H
      +0x350 w @0x1418F2914: db 0C7h
      +0x540 r @0x1418F303C: db  48h ; H
      +0x540 r @0x1418F303D: db  8Dh
      +0x540 r @0x1418F306D: db  48h ; H
      +0x540 r @0x1418F306E: db  8Dh
      +0x550 w @0x1418F2F4E: db  4Ch ; L
      +0x550 w @0x1418F2F4F: db  89h
      +0x558 w @0x1418F2F55: db  4Ch ; L
      +0x558 w @0x1418F2F56: db  89h

  *** BOTH FUNC 0x143B908D0 (rva 0x3B908D0) sub_143B908D0
      +0x320 w @0x143B91B02: db  48h ; H
      +0x320 w @0x143B91B03: db  89h
      +0x320 w @0x143B92803: db  48h ; H
      +0x320 w @0x143B92804: db  89h
      +0x328 w @0x143B91B09: db  48h ; H
      +0x328 w @0x143B91B0A: db 0C7h
      +0x328 w @0x143B9280A: db  48h ; H
      +0x328 w @0x143B9280B: db 0C7h
      +0x330 w @0x143B90EA2: db 0C7h
      +0x330 w @0x143B91B34: db 0C7h
      +0x330 w @0x143B92835: db 0C7h
      +0x330 w @0x143B9306C: db 0C7h
      +0x340 w @0x143B90957: db    0
      +0x350 w @0x143B91B45: db  48h ; H
      +0x350 w @0x143B91B46: db 0C7h
      +0x350 w @0x143B92846: db  48h ; H
      +0x350 w @0x143B92847: db 0C7h
      +0x360 r @0x143B90ED5: db  48h ; H
      +0x360 r @0x143B90ED6: db  8Dh
      +0x360 r @0x143B90F1D: db  48h ; H
      +0x360 r @0x143B90F1E: db  8Dh
      +0x360 r @0x143B90FFE: db  48h ; H
      +0x360 r @0x143B90FFF: db  8Dh
      +0x360 r @0x143B91098: db  48h ; H
      +0x360 r @0x143B91099: db  8Dh
      +0x540 r @0x143B90BEE: db  48h ; H
      +0x540 r @0x143B90BEF: db  8Dh
      +0x540 w @0x143B91EE4: db  66h ; f
      +0x540 w @0x143B91EE5: db  89h
      +0x540 r @0x143B91EF8: db  48h ; H
      +0x540 r @0x143B91EF9: db  8Dh
      +0x540 r @0x143B92B72: db  48h ; H
      +0x540 r @0x143B92B73: db  8Dh
      +0x550 w @0x143B91ED2: db  48h ; H
      +0x550 w @0x143B91ED3: db  89h
      +0x558 w @0x143B91ED9: db  48h ; H
      +0x558 w @0x143B91EDA: db 0C7h

  *** BOTH FUNC 0x1446CE930 (rva 0x46CE930) sub_1446CE930
      +0x320 r @0x1446CF2CA: db  48h ; H
      +0x320 r @0x1446CF2CB: db  8Bh
      +0x320 w @0x1446CF310: db  40h ; @
      +0x320 w @0x1446CF311: db  88h
      +0x330 w @0x1446CF2FE: db  48h ; H
      +0x330 w @0x1446CF2FF: db  89h
      +0x340 r @0x1446CF270: db  48h ; H
      +0x340 r @0x1446CF271: db  8Bh
      +0x340 w @0x1446CF2B6: db  40h ; @
      +0x340 w @0x1446CF2B7: db  88h
      +0x350 w @0x1446CF2A4: db  48h ; H
      +0x350 w @0x1446CF2A5: db  89h
      +0x368 r @0x1446CF216: db  48h ; H
      +0x368 r @0x1446CF217: db  8Bh
      +0x368 w @0x1446CF25C: db  40h ; @
      +0x368 w @0x1446CF25D: db  88h
      +0x540 r @0x1446CED77: db  48h ; H
      +0x540 r @0x1446CED78: db  8Bh
      +0x540 w @0x1446CEDBF: db  48h ; H
      +0x540 w @0x1446CEDC0: db 0C7h
      +0x548 r @0x1446CED2A: db  48h ; H
      +0x548 r @0x1446CED2B: db  8Bh
      +0x548 w @0x1446CED70: db  40h ; @
      +0x548 w @0x1446CED71: db  88h
      +0x558 w @0x1446CED5E: db  48h ; H
      +0x558 w @0x1446CED5F: db  89h

  *** BOTH FUNC 0x140D4A930 (rva 0xD4A930) sub_140D4A930
      +0x320 w @0x140D4B8C7: mov     [rbp+320h], eax
      +0x320 r @0x140D4B999: lea     r8, [rbp+320h]
      +0x324 w @0x140D4B8CD: mov     [rbp+324h], eax
      +0x328 r @0x140D4B627: lea     rcx, [r14+328h]
      +0x328 w @0x140D4B8D3: mov     dword ptr [rbp+328h], 1
      +0x32C w @0x140D4B8DD: mov     [rbp+32Ch], ecx
      +0x32C r @0x140D4B943: mov     eax, [rbp+32Ch]
      +0x330 w @0x140D4B8E3: mov     dword ptr [rbp+330h], 36h ; '6'
      +0x330 r @0x140D4B955: mov     r8d, [rbp+330h]
      +0x330 r @0x140D4BBDC: mov     r8d, [rbp+330h]
      +0x340 w @0x140D4B8FF: mov     qword ptr [rbp+340h], 88h
      +0x350 w @0x140D4B392: mov     [rbp+350h], eax
      +0x350 r @0x140D4B45D: lea     r8, [rbp+350h]
      +0x360 w @0x140D4B3AE: mov     dword ptr [rbp+360h], 36h ; '6'
      +0x360 r @0x140D4B420: mov     r8d, [rbp+360h]
      +0x360 r @0x140D4B6AD: mov     r8d, [rbp+360h]
      +0x364 w @0x140D4B3B8: mov     qword ptr [rbp+364h], 1
      +0x558 w @0x140D4C7CF: mov     qword ptr [rbp+558h], 20000h

  *** BOTH FUNC 0x1418C0940 (rva 0x18C0940) sub_1418C0940
      +0x320 w @0x1418C0A11: db  48h ; H
      +0x320 w @0x1418C0A12: db  89h
      +0x328 w @0x1418C0A18: db  48h ; H
      +0x328 w @0x1418C0A19: db 0C7h
      +0x330 w @0x1418C0A3B: db  88h
      +0x340 w @0x1418C0A29: db  48h ; H
      +0x340 w @0x1418C0A2A: db  89h
      +0x350 w @0x1418C0A53: db  88h
      +0x360 w @0x1418C0A41: db  48h ; H
      +0x360 w @0x1418C0A42: db  89h
      +0x368 w @0x1418C0A48: db  48h ; H
      +0x368 w @0x1418C0A49: db 0C7h
      +0x540 w @0x1418C0BC6: db  48h ; H
      +0x540 w @0x1418C0BC7: db  89h

  *** BOTH FUNC 0x140DBC9C0 (rva 0xDBC9C0) sub_140DBC9C0
      +0x320 w @0x140DBE0E6: mov     [rbp+0BB0h+var_890], rdi
      +0x328 w @0x140DBE0ED: mov     [rbp+0BB0h+var_888], 0Fh
      +0x328 r @0x140DBE16F: mov     rdx, [rbp+0BB0h+var_888]
      +0x330 w @0x140DBE1C5: mov     byte ptr [rbp+0BB0h+var_880], 0
      +0x330 r @0x140DBE1D9: lea     rcx, [rbp+0BB0h+var_880]
      +0x330 r @0x140DBE1ED: lea     rdx, [rbp+0BB0h+var_880]
      +0x330 r @0x140DBE245: mov     rcx, [rbp+0BB0h+var_880]
      +0x340 w @0x140DBE1B3: mov     [rbp+0BB0h+var_870], rdi
      +0x350 w @0x140DBE28B: mov     byte ptr [rbp+0BB0h+var_860], 0
      +0x350 r @0x140DBE29F: lea     rcx, [rbp+0BB0h+var_860]
      +0x350 r @0x140DBE2BA: lea     rdx, [rbp+0BB0h+var_860]
      +0x350 r @0x140DBE312: mov     rcx, [rbp+0BB0h+var_860]
      +0x360 w @0x140DBE279: mov     [rbp+0BB0h+var_850], rdi
      +0x368 w @0x140DBE280: mov     [rbp+0BB0h+var_848], 0Fh
      +0x368 r @0x140DBE302: mov     rdx, [rbp+0BB0h+var_848]
      +0x540 w @0x140DBCD1A: mov     [r14+540h], rdi
      +0x540 w @0x140DBD9C3: mov     [r14+540h], rcx
      +0x540 r @0x140DBDA66: mov     rcx, [r14+540h]
      +0x540 w @0x140DBE9FF: mov     [rbp+0BB0h+var_670], 1
      +0x544 w @0x140DBEA09: mov     [rbp+0BB0h+var_66C], 1Ah
      +0x548 w @0x140DBCD21: mov     [r14+548h], rdi
      +0x548 r @0x140DBD9CA: mov     rcx, [r14+548h]
      +0x548 w @0x140DBD9D1: mov     [r14+548h], rdx
      +0x548 w @0x140DBEA13: movups  [rbp+0BB0h+var_668], xmm0
      +0x550 w @0x140DBCD28: mov     [r14+550h], rdi
      +0x550 w @0x140DBD7CB: mov     [r14+550h], rcx
      +0x550 r @0x140DBD869: mov     rcx, [r14+550h]
      +0x558 w @0x140DBCD2F: mov     [r14+558h], rdi
      +0x558 r @0x140DBD7D2: mov     rcx, [r14+558h]
      +0x558 w @0x140DBD7D9: mov     [r14+558h], rdx
      +0x558 w @0x140DBEA1A: mov     [rbp+0BB0h+var_658], rax

  *** BOTH FUNC 0x144EC69E0 (rva 0x4EC69E0) sub_144EC69E0
      +0x330 w @0x144EC6E79: db  48h ; H
      +0x330 w @0x144EC6E7A: db  89h
      +0x330 r @0x144EC6EDC: db  48h ; H
      +0x330 r @0x144EC6EDD: db  8Bh
      +0x340 w @0x144EC75E0: db    0
      +0x340 w @0x144EC76EC: db    0
      +0x340 w @0x144EC7B33: db  0Fh
      +0x340 w @0x144EC7B34: db  11h
      +0x350 w @0x144EC7B43: db  88h
      +0x360 w @0x144EC7B5A: db  48h ; H
      +0x360 w @0x144EC7B5B: db  89h
      +0x368 w @0x144EC7B6A: db  48h ; H
      +0x368 w @0x144EC7B6B: db  89h
      +0x540 r @0x144EC7C4A: db  48h ; H
      +0x540 r @0x144EC7C4B: db  8Dh
      +0x548 w @0x144EC6A8F: db 0F0h
      +0x548 w @0x144EC6A90: db  48h ; H
      +0x548 w @0x144EC6A91: db  89h
      +0x548 r @0x144EC7080: db  48h ; H
      +0x548 r @0x144EC7081: db  8Bh
      +0x548 r @0x144EC7115: db  48h ; H
      +0x548 r @0x144EC7116: db  8Bh
      +0x548 r @0x144EC7B62: db  48h ; H
      +0x550 w @0x144EC7C9F: db  48h ; H
      +0x550 w @0x144EC7CA0: db  89h

  *** BOTH FUNC 0x1411BC9F0 (rva 0x11BC9F0) sub_1411BC9F0
      +0x328 w @0x1411BDE67: db 0FFh
      +0x328 w @0x1411BE8AE: db 0FFh
      +0x330 w @0x1411BD780: db 0FFh
      +0x330 w @0x1411BD796: db 0FFh
      +0x330 w @0x1411BD7B6: db 0FFh
      +0x330 w @0x1411BD7DF: db 0FFh
      +0x330 w @0x1411BD7FF: db 0FFh
      +0x330 r @0x1411BDD07: db  48h ; H
      +0x330 r @0x1411BDD08: db  8Dh
      +0x330 w @0x1411BEC0C: db 0FFh
      +0x350 w @0x1411BE6BB: db 0C7h
      +0x350 r @0x1411BE6D9: db  48h ; H
      +0x350 r @0x1411BE6DA: db  8Dh
      +0x360 w @0x1411BF32F: db 0FFh
      +0x544 r @0x1411BD40A: db  8Bh
      +0x550 r @0x1411BD41A: db  0Fh
      +0x550 w @0x1411BD41B: db  10h

  *** BOTH FUNC 0x14072CA00 (rva 0x72CA00) sub_14072CA00
      +0x320 r @0x14072CD3D: movups  xmm0, xmmword ptr [r10+320h]
      +0x320 w @0x14072CD45: movups  xmmword ptr [r11+320h], xmm0
      +0x330 r @0x14072CD4D: movups  xmm1, xmmword ptr [r10+330h]
      +0x330 w @0x14072CD55: movups  xmmword ptr [r11+330h], xmm1
      +0x340 r @0x14072CD5D: movups  xmm0, xmmword ptr [r10+340h]
      +0x340 w @0x14072CD65: movups  xmmword ptr [r11+340h], xmm0
      +0x350 r @0x14072CD6D: movups  xmm0, xmmword ptr [r10+350h]
      +0x350 w @0x14072CD75: movups  xmmword ptr [r11+350h], xmm0
      +0x360 r @0x14072CD7D: movups  xmm1, xmmword ptr [r10+360h]
      +0x360 w @0x14072CD85: movups  xmmword ptr [r11+360h], xmm1
      +0x540 r @0x14072CFA2: movups  xmm1, xmmword ptr [r10+540h]
      +0x540 w @0x14072CFAA: movups  xmmword ptr [r11+540h], xmm1
      +0x550 r @0x14072CFB2: movups  xmm0, xmmword ptr [r10+550h]
      +0x550 w @0x14072CFBA: movups  xmmword ptr [r11+550h], xmm0

  *** BOTH FUNC 0x141D42A40 (rva 0x1D42A40) sub_141D42A40
      +0x340 r @0x141D436BE: db  48h ; H
      +0x340 r @0x141D436BF: db  8Dh
      +0x340 r @0x141D43C13: db 0F3h
      +0x340 r @0x141D43C14: db  0Fh
      +0x340 w @0x141D43C15: db  10h
      +0x340 w @0x141D43C43: db 0F3h
      +0x340 w @0x141D43C44: db  0Fh
      +0x340 w @0x141D43C45: db  11h
      +0x350 r @0x141D43C55: db 0F3h
      +0x350 r @0x141D43C56: db  0Fh
      +0x350 w @0x141D43C57: db  10h
      +0x350 w @0x141D43C76: db 0F3h
      +0x350 w @0x141D43C77: db  0Fh
      +0x350 w @0x141D43C78: db  11h
      +0x364 r @0x141D43C85: db 0F3h
      +0x364 r @0x141D43C86: db  0Fh
      +0x364 w @0x141D43C87: db  10h
      +0x364 w @0x141D43C97: db 0F3h
      +0x364 w @0x141D43C98: db  0Fh
      +0x364 w @0x141D43C99: db  11h
      +0x548 r @0x141D43406: db 0F3h
      +0x548 r @0x141D43407: db  0Fh
      +0x548 w @0x141D43408: db  10h
      +0x548 w @0x141D43439: db 0F3h
      +0x548 w @0x141D4343A: db  0Fh
      +0x548 w @0x141D4343B: db  11h
      +0x54C r @0x141D43427: db 0F3h
      +0x54C r @0x141D43428: db  0Fh
      +0x54C w @0x141D43429: db  10h
      +0x54C w @0x141D4345A: db 0F3h
      +0x54C w @0x141D4345B: db  0Fh
      +0x54C w @0x141D4345C: db  11h
      +0x550 r @0x141D43448: db 0F3h
      +0x550 r @0x141D43449: db  0Fh
      +0x550 w @0x141D4344A: db  10h
      +0x550 w @0x141D4347B: db 0F3h
      +0x550 w @0x141D4347C: db  0Fh
      +0x550 w @0x141D4347D: db  11h
      +0x554 r @0x141D43469: db 0F3h
      +0x554 r @0x141D4346A: db  0Fh
      +0x554 w @0x141D4346B: db  10h
      +0x554 w @0x141D4349C: db 0F3h
      +0x554 w @0x141D4349D: db  0Fh
      +0x554 w @0x141D4349E: db  11h
      +0x558 r @0x141D4348A: db 0F3h
      +0x558 r @0x141D4348B: db  0Fh
      +0x558 w @0x141D4348C: db  10h
      +0x558 w @0x141D434BD: db 0F3h
      +0x558 w @0x141D434BE: db  0Fh
      +0x558 w @0x141D434BF: db  11h
      +0x55C r @0x141D434AB: db 0F3h
      +0x55C r @0x141D434AC: db  0Fh
      +0x55C w @0x141D434AD: db  10h
      +0x55C w @0x141D434DE: db 0F3h
      +0x55C w @0x141D434DF: db  0Fh
      +0x55C w @0x141D434E0: db  11h

  *** BOTH FUNC 0x145384AF0 (rva 0x5384AF0) sub_145384AF0
      +0x320 w @0x145384D14: db  48h ; H
      +0x320 w @0x145384D15: db  89h
      +0x328 w @0x145384D06: db  48h ; H
      +0x328 w @0x145384D07: db  89h
      +0x330 w @0x145384D75: db  48h ; H
      +0x330 w @0x145384D76: db  89h
      +0x340 w @0x145384DBA: db  48h ; H
      +0x340 w @0x145384DBB: db  89h
      +0x350 w @0x145384D4C: db  48h ; H
      +0x350 w @0x145384D4D: db  89h
      +0x360 w @0x145384D68: db  48h ; H
      +0x360 w @0x145384D69: db  89h
      +0x368 w @0x145384D83: db  89h
      +0x558 r @0x145384FAD: db  48h ; H
      +0x558 r @0x145384FAE: db  8Dh
      +0x558 w @0x145384FC7: db  4Ch ; L
      +0x558 w @0x145384FC8: db  89h
      +0x558 w @0x145384FF9: db  48h ; H
      +0x558 w @0x145384FFA: db  89h
      +0x558 r @0x145385C11: db  48h ; H
      +0x558 r @0x145385C12: db  8Bh

  *** BOTH FUNC 0x140AD2B00 (rva 0xAD2B00) sub_140AD2B00
      +0x320 w @0x140AD3860: mov     [rbp+320h], r12
      +0x328 w @0x140AD3867: mov     [rbp+328h], r8
      +0x328 w @0x140AD389B: mov     [rbp+328h], r15
      +0x328 r @0x140AD4414: mov     r8, [r15+328h]
      +0x328 r @0x140AD453A: lea     rcx, [r15+328h]
      +0x328 r @0x140AD454B: cmp     rax, [r15+328h]
      +0x328 r @0x140AD455F: lea     rcx, [r15+328h]
      +0x330 w @0x140AD43FE: cmp     qword ptr [r15+330h], 0
      +0x330 r @0x140AD4883: lea     rdx, [rbp+330h]
      +0x330 r @0x140AD4897: mov     rax, [rbp+330h]
      +0x330 r @0x140AD504E: lea     rcx, [rbp+330h]
      +0x340 r @0x140AD4A5B: lea     rcx, [rbp+340h]
      +0x340 r @0x140AD4AA5: lea     rax, [rbp+340h]
      +0x340 r @0x140AD5027: lea     rcx, [rbp+340h]
      +0x350 w @0x140AD4860: mov     [rbp+350h], r13
      +0x350 r @0x140AD4877: lea     r9, [rbp+350h]
      +0x360 w @0x140AD4120: mov     [rbp+360h], r12
      +0x368 w @0x140AD4127: mov     [rbp+368h], r8
      +0x368 w @0x140AD415B: mov     [rbp+368h], r13
      +0x558 r @0x140AD4EC8: lea     rcx, [rbp+558h]
      +0x558 r @0x140AD4F20: lea     r8, [rbp+558h]

  *** BOTH FUNC 0x140C12B00 (rva 0xC12B00) sub_140C12B00
      +0x320 w @0x140C1307D: db  0Fh
      +0x320 w @0x140C1307E: db  29h ; )
      +0x330 w @0x140C130A1: db  0Fh
      +0x330 w @0x140C130A2: db  29h ; )
      +0x340 w @0x140C130AC: db  0Fh
      +0x340 w @0x140C130AD: db  29h ; )
      +0x340 r @0x140C130F6: db  48h ; H
      +0x340 r @0x140C130F7: db  8Dh
      +0x340 r @0x140C13109: db  48h ; H
      +0x340 r @0x140C1310A: db  8Bh
      +0x340 r @0x140C13115: db  48h ; H
      +0x340 r @0x140C13116: db  8Dh
      +0x350 w @0x140C130BA: db  0Fh
      +0x350 w @0x140C130BB: db  29h ; )
      +0x360 w @0x140C130C5: db  0Fh
      +0x360 w @0x140C130C6: db  29h ; )
      +0x360 r @0x140C13142: db  48h ; H
      +0x360 r @0x140C13143: db  8Dh
      +0x550 r @0x140C133DA: db  4Ch ; L
      +0x550 r @0x140C133DB: db  8Dh

  *** BOTH FUNC 0x1411D2B00 (rva 0x11D2B00) sub_1411D2B00
      +0x320 r @0x1411D3B6C: db  0Fh
      +0x320 w @0x1411D3B6D: db  10h
      +0x330 w @0x1411D2B46: db 0FFh
      +0x330 w @0x1411D2B58: db 0FFh
      +0x330 r @0x1411D3209: db  49h ; I
      +0x330 r @0x1411D320A: db  8Dh
      +0x330 r @0x1411D3C08: db  0Fh
      +0x330 w @0x1411D3C09: db  10h
      +0x340 r @0x1411D3C17: db  0Fh
      +0x340 w @0x1411D3C18: db  10h
      +0x350 r @0x1411D3C26: db  0Fh
      +0x350 w @0x1411D3C27: db  10h
      +0x550 r @0x1411D336E: db  49h ; I
      +0x550 r @0x1411D336F: db  8Dh

  *** BOTH FUNC 0x140D5CB30 (rva 0xD5CB30) sub_140D5CB30
      +0x320 r @0x140D5CDA8: db  0Fh
      +0x320 w @0x140D5CDA9: db  10h
      +0x330 r @0x140D5CDB6: db  0Fh
      +0x330 w @0x140D5CDB7: db  10h
      +0x350 r @0x140D5CC34: db  8Bh
      +0x360 r @0x140D5CDE1: db 0F3h
      +0x360 r @0x140D5CDE2: db  0Fh
      +0x360 w @0x140D5CDE3: db  10h
      +0x360 r @0x140D5CEA7: db  8Bh
      +0x364 r @0x140D5CE11: db 0F3h
      +0x364 r @0x140D5CE12: db  0Fh
      +0x364 w @0x140D5CE13: db  10h
      +0x540 w @0x140D5CD07: db  0Fh
      +0x540 w @0x140D5CD08: db  11h
      +0x550 w @0x140D5CD15: db  0Fh
      +0x550 w @0x140D5CD16: db  11h

  *** BOTH FUNC 0x140C78B80 (rva 0xC78B80) sub_140C78B80
      +0x320 w @0x140C79C6A: mov     [rbp+0C40h+var_920], r15
      +0x320 r @0x140C79DA4: lea     rax, [rbp+0C40h+var_920]
      +0x328 w @0x140C79C71: mov     [rbp+0C40h+var_918], r13d
      +0x32C w @0x140C79C78: mov     [rbp+0C40h+var_914], 0Dh
      +0x340 w @0x140C79C94: mov     [rbp+0C40h+var_900], r14
      +0x350 w @0x140C79CAC: mov     [rbp+0C40h+var_8F0], 1
      +0x360 w @0x140C79CC7: mov     [rbp+0C40h+var_8E0], r12
      +0x368 w @0x140C79CCE: mov     [rbp+0C40h+var_8D8], r13d
      +0x540 w @0x140C793E6: mov     byte ptr [rbp+0C40h+var_700], 0
      +0x540 r @0x140C793FA: lea     rcx, [rbp+0C40h+var_700]
      +0x540 r @0x140C7940E: lea     rdx, [rbp+0C40h+var_700]
      +0x540 r @0x140C79463: mov     rcx, [rbp+0C40h+var_700]
      +0x550 w @0x140C793D4: mov     [rbp+0C40h+var_6F0], r13
      +0x558 w @0x140C793DB: mov     [rbp+0C40h+var_6E8], 0Fh
      +0x558 r @0x140C79453: mov     rdx, [rbp+0C40h+var_6E8]

  *** BOTH FUNC 0x1449A6B80 (rva 0x49A6B80) sub_1449A6B80
      +0x320 r @0x1449A82F6: db  4Dh ; M
      +0x320 r @0x1449A82F7: db  8Dh
      +0x320 w @0x1449A94F5: db  44h ; D
      +0x320 w @0x1449A94F6: db  89h
      +0x320 r @0x1449A9506: db  48h ; H
      +0x320 r @0x1449A9507: db  8Dh
      +0x324 w @0x1449A9558: db  44h ; D
      +0x324 w @0x1449A9559: db  89h
      +0x324 r @0x1449A9569: db  48h ; H
      +0x324 r @0x1449A956A: db  8Dh
      +0x328 w @0x1449A95BB: db  44h ; D
      +0x328 w @0x1449A95BC: db  89h
      +0x328 r @0x1449A95CC: db  48h ; H
      +0x328 r @0x1449A95CD: db  8Dh
      +0x32C w @0x1449A961E: db  44h ; D
      +0x32C w @0x1449A961F: db  89h
      +0x32C r @0x1449A962F: db  48h ; H
      +0x32C r @0x1449A9630: db  8Dh
      +0x330 w @0x1449A9681: db  44h ; D
      +0x330 w @0x1449A9682: db  89h
      +0x330 r @0x1449A9692: db  48h ; H
      +0x330 r @0x1449A9693: db  8Dh
      +0x340 r @0x1449A8359: db  4Dh ; M
      +0x340 r @0x1449A835A: db  8Dh
      +0x340 w @0x1449A97F2: db  44h ; D
      +0x340 w @0x1449A97F3: db  89h
      +0x340 r @0x1449A9803: db  48h ; H
      +0x340 r @0x1449A9804: db  8Dh
      +0x350 w @0x1449A9989: db  44h ; D
      +0x350 w @0x1449A998A: db  89h
      +0x350 r @0x1449A999A: db  48h ; H
      +0x350 r @0x1449A999B: db  8Dh
      +0x360 r @0x1449A83BC: db  4Dh ; M
      +0x360 r @0x1449A83BD: db  8Dh
      +0x360 w @0x1449A9B48: db  44h ; D
      +0x360 w @0x1449A9B49: db  89h
      +0x360 r @0x1449A9B59: db  48h ; H
      +0x360 r @0x1449A9B5A: db  8Dh
      +0x364 w @0x1449A9BA8: db  44h ; D
      +0x364 w @0x1449A9BA9: db  89h
      +0x364 r @0x1449A9BB9: db  48h ; H
      +0x364 r @0x1449A9BBA: db  8Dh
      +0x368 w @0x1449A9C08: db  44h ; D
      +0x368 w @0x1449A9C09: db  89h
      +0x368 r @0x1449A9C19: db  48h ; H
      +0x368 r @0x1449A9C1A: db  8Dh
      +0x540 r @0x1449A7EB7: db  48h ; H
      +0x540 r @0x1449A7EB8: db  8Dh
      +0x540 r @0x1449A7EC4: db  48h ; H
      +0x540 r @0x1449A7EC5: db  8Dh
      +0x540 r @0x1449A8A0D: db 0F3h
      +0x540 r @0x1449A8A0E: db  41h ; A
      +0x540 r @0x1449A8A0F: db  0Fh
      +0x540 w @0x1449A8A10: db  10h
      +0x544 r @0x1449A8A70: db 0F3h
      +0x544 r @0x1449A8A71: db  41h ; A
      +0x544 r @0x1449A8A72: db  0Fh
      +0x544 w @0x1449A8A73: db  10h
      +0x548 r @0x1449A7F10: db  48h ; H
      +0x548 r @0x1449A7F11: db  8Dh
      +0x548 r @0x1449A7F1D: db  48h ; H
      +0x548 r @0x1449A7F1E: db  8Dh
      +0x548 r @0x1449A8AD3: db 0F3h
      +0x548 r @0x1449A8AD4: db  41h ; A
      +0x548 r @0x1449A8AD5: db  0Fh
      +0x548 w @0x1449A8AD6: db  10h
      +0x54C r @0x1449A8B36: db 0F3h
      +0x54C r @0x1449A8B37: db  41h ; A
      +0x54C r @0x1449A8B38: db  0Fh
      +0x54C w @0x1449A8B39: db  10h
      +0x550 r @0x1449A7F62: db  48h ; H
      +0x550 r @0x1449A7F63: db  8Dh
      +0x550 r @0x1449A7F6F: db  48h ; H
      +0x550 r @0x1449A7F70: db  8Dh
      +0x554 r @0x1449A8BEB: db  45h ; E
      +0x554 r @0x1449A8BEC: db  8Bh
      +0x558 r @0x1449A7FC5: db  48h ; H
      +0x558 r @0x1449A7FC6: db  8Dh
      +0x558 r @0x1449A7FD2: db  48h ; H
      +0x558 r @0x1449A7FD3: db  8Dh
      +0x558 r @0x1449A8C44: db 0F2h
      +0x558 r @0x1449A8C45: db  41h ; A
      +0x558 r @0x1449A8C46: db  0Fh
      +0x558 w @0x1449A8C47: db  10h

  *** BOTH FUNC 0x141564B90 (rva 0x1564B90) sub_141564B90
      +0x328 w @0x141564CC2: db  48h ; H
      +0x328 w @0x141564CC3: db  89h
      +0x330 r @0x141564CBB: db  48h ; H
      +0x330 r @0x141564CBC: db  8Dh
      +0x360 r @0x141564CCC: db  48h ; H
      +0x360 r @0x141564CCD: db  8Dh
      +0x540 r @0x141564D76: db  48h ; H
      +0x540 r @0x141564D77: db  8Dh

  *** BOTH FUNC 0x145444B90 (rva 0x5444B90) sub_145444B90
      +0x320 w @0x145444CB1: movaps  [rbp+2EA0h+var_2B80], xmm4
      +0x320 r @0x145444F7B: movaps  xmm4, [rbp+2EA0h+var_2B80]
      +0x330 w @0x145444D22: movaps  [rbp+2EA0h+var_2B70], xmm3
      +0x330 r @0x145444F82: movaps  xmm3, [rbp+2EA0h+var_2B70]
      +0x340 w @0x1454472C2: movaps  [rbp+2EA0h+var_2B60], xmm0
      +0x340 r @0x145447625: minps   xmm10, [rbp+2EA0h+var_2B60]
      +0x350 w @0x145446F7C: movaps  [rbp+2EA0h+var_2B50], xmm6
      +0x350 r @0x1454477FC: movaps  xmm6, [rbp+2EA0h+var_2B50]
      +0x360 w @0x145446F70: movaps  [rbp+2EA0h+var_2B40], xmm8
      +0x360 r @0x145447803: movaps  xmm8, [rbp+2EA0h+var_2B40]
      +0x540 w @0x145444DB7: movaps  [rbp+2EA0h+var_2960], xmm7
      +0x550 w @0x145444DC1: movaps  [rbp+2EA0h+var_2950], xmm7

  *** BOTH FUNC 0x140C98BA0 (rva 0xC98BA0) sub_140C98BA0
      +0x320 w @0x140C98F9A: mov     [rdi+320h], rsi
      +0x320 w @0x140C98FAE: mov     [rdi+320h], rsi
      +0x320 w @0x140C99607: movups  xmmword ptr [rcx+320h], xmm2
      +0x320 r @0x140C9AF64: lea     rcx, [rbp+320h]
      +0x328 r @0x140C98FBC: lea     rcx, [rdi+328h]
      +0x328 r @0x140C9AC43: lea     rcx, [rdi+328h]
      +0x328 w @0x140C9AF5D: mov     [rbp+328h], r15
      +0x328 w @0x140C9AFA0: mov     [rbp+328h], rbx
      +0x328 r @0x140C9B4D8: lea     rdx, [rdi+328h]
      +0x330 w @0x140C98FB5: mov     [rdi+330h], rsi
      +0x330 w @0x140C98FC9: mov     [rdi+330h], rsi
      +0x330 w @0x140C9960E: movups  xmmword ptr [rcx+330h], xmm3
      +0x330 r @0x140C9AFAE: lea     rcx, [rbp+330h]
      +0x340 w @0x140C99638: movups  xmmword ptr [rcx+340h], xmm0
      +0x340 r @0x140C9AFF8: lea     rcx, [rbp+340h]
      +0x350 w @0x140C9963F: movups  xmmword ptr [rcx+350h], xmm1
      +0x350 r @0x140C9B042: lea     rcx, [rbp+350h]
      +0x360 w @0x140C99646: movups  xmmword ptr [rcx+360h], xmm2
      +0x360 r @0x140C9B08C: lea     rcx, [rbp+360h]
      +0x368 w @0x140C9B085: mov     [rbp+368h], r15
      +0x368 w @0x140C9B0C8: mov     [rbp+368h], rbx
      +0x540 w @0x140C99830: movups  xmmword ptr [rcx+540h], xmm0
      +0x540 r @0x140C99EA4: lea     rcx, [rbp+540h]
      +0x550 w @0x140C99837: movups  xmmword ptr [rcx+550h], xmm1

  *** BOTH FUNC 0x1418C0C10 (rva 0x18C0C10) sub_1418C0C10
      +0x320 w @0x1418C0CD9: db  48h ; H
      +0x320 w @0x1418C0CDA: db  89h
      +0x328 w @0x1418C0CE0: db  48h ; H
      +0x328 w @0x1418C0CE1: db 0C7h
      +0x330 w @0x1418C0D03: db  88h
      +0x340 w @0x1418C0CF1: db  48h ; H
      +0x340 w @0x1418C0CF2: db  89h
      +0x350 w @0x1418C0D1B: db  88h
      +0x360 w @0x1418C0D09: db  48h ; H
      +0x360 w @0x1418C0D0A: db  89h
      +0x368 w @0x1418C0D10: db  48h ; H
      +0x368 w @0x1418C0D11: db 0C7h
      +0x540 w @0x1418C0E8E: db  48h ; H
      +0x540 w @0x1418C0E8F: db  89h

  *** BOTH FUNC 0x140CFCC30 (rva 0xCFCC30) sub_140CFCC30
      +0x320 r @0x140CFD25A: db  48h ; H
      +0x320 r @0x140CFD25B: db  8Dh
      +0x320 r @0x140CFD8D1: db  48h ; H
      +0x320 r @0x140CFD8D2: db  8Dh
      +0x324 r @0x140CFD28C: db  48h ; H
      +0x324 r @0x140CFD28D: db  8Dh
      +0x328 r @0x140CFD2BE: db  48h ; H
      +0x328 r @0x140CFD2BF: db  8Dh
      +0x32C r @0x140CFD2F0: db  48h ; H
      +0x32C r @0x140CFD2F1: db  8Dh
      +0x330 r @0x140CFD322: db  48h ; H
      +0x330 r @0x140CFD323: db  8Dh
      +0x330 r @0x140CFD91C: db  48h ; H
      +0x330 r @0x140CFD91D: db  8Dh
      +0x340 r @0x140CFD41C: db  48h ; H
      +0x340 r @0x140CFD41D: db  8Dh
      +0x340 r @0x140CFD967: db  48h ; H
      +0x340 r @0x140CFD968: db  8Dh
      +0x350 r @0x140CFD4E4: db  48h ; H
      +0x350 r @0x140CFD4E5: db  8Dh
      +0x350 r @0x140CFDA30: db  48h ; H
      +0x350 r @0x140CFDA31: db  8Dh
      +0x360 w @0x140CFD591: db  89h
      +0x360 r @0x140CFDA73: db  48h ; H
      +0x360 r @0x140CFDA74: db  8Dh
      +0x364 w @0x140CFD59D: db  89h
      +0x368 w @0x140CFD5A7: db  89h
      +0x540 r @0x140CFD2E9: db  48h ; H
      +0x540 r @0x140CFD2EA: db  8Dh
      +0x544 r @0x140CFD31B: db  48h ; H
      +0x544 r @0x140CFD31C: db  8Dh
      +0x548 r @0x140CFD34D: db  48h ; H
      +0x548 r @0x140CFD34E: db  8Dh
      +0x54C r @0x140CFD447: db  48h ; H
      +0x54C r @0x140CFD448: db  8Dh
      +0x550 r @0x140CFD479: db  48h ; H
      +0x550 r @0x140CFD47A: db  8Dh
      +0x554 r @0x140CFD37F: db  48h ; H
      +0x554 r @0x140CFD380: db  8Dh
      +0x558 r @0x140CFD3B1: db  48h ; H
      +0x558 r @0x140CFD3B2: db  8Dh
      +0x55C r @0x140CFD3E3: db  48h ; H
      +0x55C r @0x140CFD3E4: db  8Dh

  *** BOTH FUNC 0x140B56C30 (rva 0xB56C30) sub_140B56C30
      +0x320 w @0x140B593FF: mov     [rbp+0D50h+var_A30], edi
      +0x324 w @0x140B59405: mov     [rbp+0D50h+var_A2C], 3
      +0x328 w @0x140B5944B: mov     [rbp+0D50h+var_A28], 2
      +0x328 r @0x140B59497: lea     r8, [rbp+0D50h+var_A28]
      +0x330 w @0x140B59456: mov     [rbp+0D50h+var_A20], r13d
      +0x340 w @0x140B57A7A: mov     [rbp+0D50h+var_A10], rax
      +0x340 r @0x140B57A8F: lea     rdx, [rbp+0D50h+var_A10]
      +0x340 r @0x140B57B38: mov     rcx, [rbp+0D50h+var_A10]
      +0x340 w @0x140B57B7E: mov     byte ptr [rbp+0D50h+var_A10], 0
      +0x350 w @0x140B57A42: mov     [rbp+0D50h+var_A00], 16h
      +0x350 w @0x140B57B6C: mov     [rbp+0D50h+var_A00], r12
      +0x360 w @0x140B57776: mov     [rbp+0D50h+var_9F0], rax
      +0x360 r @0x140B57784: lea     rdx, [rbp+0D50h+var_9F0]
      +0x360 r @0x140B57831: mov     rcx, [rbp+0D50h+var_9F0]
      +0x360 w @0x140B57879: mov     byte ptr [rbp+0D50h+var_9F0], cl
      +0x544 w @0x140B578FE: mov     [rbp+0D50h+var_80C], 0FFFFFFFFh
      +0x548 w @0x140B57908: mov     [rbp+0D50h+var_808], rcx
      +0x550 w @0x140B5762F: mov     [rbp+0D50h+var_800], r14
      +0x550 w @0x140B57650: mov     [rbp+0D50h+var_800], rax
      +0x550 r @0x140B5765D: lea     rdx, [rbp+0D50h+var_800]
      +0x550 w @0x140B57671: mov     [rbp+0D50h+var_800], r14
      +0x558 r @0x140B5763D: lea     rcx, [rbp+0D50h+var_7F8]
      +0x558 r @0x140B57688: mov     rcx, [rbp+0D50h+var_7F8]
      +0x558 w @0x140B576CC: mov     byte ptr [rbp+0D50h+var_7F8], 0

  *** BOTH FUNC 0x14461CC40 (rva 0x461CC40) sub_14461CC40
      +0x320 w @0x14461D17F: db  48h ; H
      +0x320 w @0x14461D180: db  89h
      +0x328 w @0x14461D186: db  48h ; H
      +0x328 w @0x14461D187: db 0C7h
      +0x330 w @0x14461D1C1: db  88h
      +0x330 r @0x14461D1D2: db  48h ; H
      +0x330 r @0x14461D1D3: db  8Dh
      +0x340 w @0x14461D1AF: db  48h ; H
      +0x340 w @0x14461D1B0: db  89h
      +0x350 w @0x14461D1F1: db  88h
      +0x350 r @0x14461D202: db  48h ; H
      +0x350 r @0x14461D203: db  8Dh
      +0x360 w @0x14461D1DF: db  48h ; H
      +0x360 w @0x14461D1E0: db  89h
      +0x368 w @0x14461D1E6: db  48h ; H
      +0x368 w @0x14461D1E7: db 0C7h
      +0x540 w @0x14461D4AF: db  48h ; H
      +0x540 w @0x14461D4B0: db  89h
      +0x548 w @0x14461D4B6: db  48h ; H
      +0x548 w @0x14461D4B7: db 0C7h
      +0x550 w @0x14461D4F1: db  88h
      +0x550 r @0x14461D502: db  48h ; H
      +0x550 r @0x14461D503: db  8Dh

  *** BOTH FUNC 0x1453FCC60 (rva 0x53FCC60) sub_1453FCC60
      +0x320 w @0x1453FCDE0: db 0C5h
      +0x320 w @0x1453FCDE2: db  11h
      +0x320 r @0x1453FCF4D: db 0C5h
      +0x320 w @0x1453FCF4F: db  10h
      +0x320 w @0x1453FD42A: db 0C5h
      +0x320 w @0x1453FD42C: db  11h
      +0x340 w @0x1453FCDCE: db 0C5h
      +0x340 w @0x1453FCDD0: db  11h
      +0x340 r @0x1453FCEA0: db 0C5h
      +0x340 w @0x1453FCEA2: db  10h
      +0x340 w @0x1453FD432: db 0C5h
      +0x340 w @0x1453FD434: db  11h
      +0x340 r @0x1453FD675: db 0C5h
      +0x340 w @0x1453FD677: db  10h
      +0x360 w @0x1453FCD63: db 0C5h
      +0x360 r @0x1453FD6D3: db 0C5h
      +0x550 r @0x1453FCCE1: db  48h ; H
      +0x550 r @0x1453FCCE2: db  8Bh
      +0x558 r @0x1453FCCBB: db  48h ; H
      +0x558 r @0x1453FCCBC: db  8Bh

  *** BOTH FUNC 0x145112C70 (rva 0x5112C70) sub_145112C70
      +0x320 w @0x145113107: db  48h ; H
      +0x320 w @0x145113108: db  89h
      +0x328 w @0x14511311B: db  48h ; H
      +0x328 w @0x14511311C: db  89h
      +0x330 r @0x145112C85: db 0F3h
      +0x330 r @0x145112C86: db  0Fh
      +0x340 r @0x145112DB4: db 0F3h
      +0x340 r @0x145112DB5: db  0Fh
      +0x558 w @0x14511306B: db  4Ch ; L
      +0x558 w @0x14511306C: db  89h

  *** BOTH FUNC 0x142A3ACC0 (rva 0x2A3ACC0) sub_142A3ACC0
      +0x320 r @0x142A3B071: db  48h ; H
      +0x320 r @0x142A3B072: db  8Bh
      +0x320 w @0x142A3B0B9: db  48h ; H
      +0x320 w @0x142A3B0BA: db 0C7h
      +0x328 r @0x142A3B024: db  48h ; H
      +0x328 r @0x142A3B025: db  8Bh
      +0x328 w @0x142A3B06A: db  40h ; @
      +0x328 w @0x142A3B06B: db  88h
      +0x340 r @0x142A3B017: db  48h ; H
      +0x340 r @0x142A3B018: db  8Bh
      +0x340 w @0x142A3B05F: db  48h ; H
      +0x340 w @0x142A3B060: db 0C7h
      +0x360 r @0x142A3AFBD: db  48h ; H
      +0x360 r @0x142A3AFBE: db  8Bh
      +0x360 w @0x142A3B005: db  48h ; H
      +0x360 w @0x142A3B006: db 0C7h
      +0x368 r @0x142A3AF70: db  48h ; H
      +0x368 r @0x142A3AF71: db  8Bh
      +0x368 w @0x142A3AFB6: db  40h ; @
      +0x368 w @0x142A3AFB7: db  88h
      +0x548 r @0x142A3AD4E: db  48h ; H
      +0x548 r @0x142A3AD4F: db  8Dh

  *** BOTH FUNC 0x14275ECE0 (rva 0x275ECE0) sub_14275ECE0
      +0x340 r @0x14275F087: db  48h ; H
      +0x340 r @0x14275F088: db  8Dh
      +0x360 w @0x14275F048: db    8
      +0x540 r @0x14275EDAC: db  0Fh
      +0x540 r @0x14275EE47: db  0Fh
      +0x540 w @0x14275EF2C: db  4Ch ; L
      +0x540 w @0x14275EF2D: db  89h
      +0x540 r @0x14275F173: db  4Ch ; L
      +0x540 r @0x14275F174: db  8Bh
      +0x540 w @0x14275F280: db  48h ; H
      +0x540 w @0x14275F281: db  89h
      +0x550 r @0x14275EFDE: db  44h ; D
      +0x550 r @0x14275EFDF: db  0Fh
      +0x550 r @0x14275F310: db  44h ; D
      +0x550 r @0x14275F311: db  0Fh
      +0x558 r @0x14275EFCF: db  8Bh
      +0x558 r @0x14275F306: db  8Bh

  *** BOTH FUNC 0x142F9ACE0 (rva 0x2F9ACE0) sub_142F9ACE0
      +0x320 w @0x142F9AF26: db  40h ; @
      +0x320 w @0x142F9AF27: db 0C7h
      +0x324 w @0x142F9AF30: db  44h ; D
      +0x324 w @0x142F9AF31: db 0C7h
      +0x328 w @0x142F9AF3B: db 0C7h
      +0x32C w @0x142F9AF45: db 0C7h
      +0x330 w @0x142F9AF4F: db 0C7h
      +0x340 w @0x142F9AF6E: db 0C7h
      +0x350 w @0x142F9AF96: db 0C7h
      +0x360 r @0x142F9AFBE: db  48h ; H
      +0x360 r @0x142F9AFBF: db  8Dh
      +0x550 r @0x142F9B2E1: db  48h ; H
      +0x550 r @0x142F9B2E2: db  8Dh

  *** BOTH FUNC 0x140DFED10 (rva 0xDFED10) sub_140DFED10
      +0x320 w @0x140E01302: movups  xmmword ptr [rbp+320h], xmm0
      +0x320 r @0x140E01309: lea     rcx, [rbp+320h]
      +0x330 w @0x140E01394: movups  xmmword ptr [rbp+330h], xmm0
      +0x330 r @0x140E0139B: lea     rcx, [rbp+330h]
      +0x340 w @0x140E01426: movups  xmmword ptr [rbp+340h], xmm0
      +0x340 r @0x140E0142D: lea     rcx, [rbp+340h]
      +0x350 w @0x140E014B8: movups  xmmword ptr [rbp+350h], xmm0
      +0x350 r @0x140E014BF: lea     rcx, [rbp+350h]
      +0x360 w @0x140E0154A: movups  xmmword ptr [rbp+360h], xmm0
      +0x360 r @0x140E01551: lea     rcx, [rbp+360h]
      +0x540 w @0x140E02690: movups  xmmword ptr [rbp+540h], xmm0
      +0x540 r @0x140E02697: lea     rcx, [rbp+540h]
      +0x550 w @0x140E02724: movups  xmmword ptr [rbp+550h], xmm0
      +0x550 r @0x140E0272B: lea     rcx, [rbp+550h]

  *** BOTH FUNC 0x141B3AD20 (rva 0x1B3AD20) sub_141B3AD20
      +0x320 w @0x141B3C1B3: mov     [rbp+13F0h+var_10D0], r15
      +0x328 w @0x141B3C1BA: mov     [rbp+13F0h+var_10C8], r14
      +0x328 r @0x141B3C22B: mov     rdx, [rbp+13F0h+var_10C8]
      +0x328 r @0x141B3CFFB: lea     rcx, [r12+328h]
      +0x330 w @0x141B3C282: mov     byte ptr [rbp+13F0h+var_10C0], 0
      +0x330 r @0x141B3C296: lea     rcx, [rbp+13F0h+var_10C0]
      +0x330 r @0x141B3C2AA: lea     rdx, [rbp+13F0h+var_10C0]
      +0x330 r @0x141B3C2F7: mov     rcx, [rbp+13F0h+var_10C0]
      +0x340 w @0x141B3C274: mov     [rbp+13F0h+var_10B0], r15
      +0x350 w @0x141B3C339: mov     byte ptr [rbp+13F0h+var_10A0], 0
      +0x350 r @0x141B3C34D: lea     rcx, [rbp+13F0h+var_10A0]
      +0x350 r @0x141B3C361: lea     rdx, [rbp+13F0h+var_10A0]
      +0x350 r @0x141B3C3B0: mov     rcx, [rbp+13F0h+var_10A0]
      +0x360 w @0x141B3C32B: mov     [rbp+13F0h+var_1090], r15
      +0x368 w @0x141B3C332: mov     [rbp+13F0h+var_1088], r14
      +0x368 r @0x141B3C3A0: mov     rdx, [rbp+13F0h+var_1088]
      +0x368 r @0x141B3DA34: lea     rcx, [r12+368h]
      +0x540 w @0x141B3CE26: mov     [rbp+13F0h+var_EB0], r15
      +0x548 w @0x141B3CE2D: mov     [rbp+13F0h+var_EA8], r14
      +0x548 r @0x141B3CE9E: mov     rdx, [rbp+13F0h+var_EA8]
      +0x550 w @0x141B3CEF0: mov     byte ptr [rbp+13F0h+var_EA0], 0
      +0x550 r @0x141B3CF04: lea     rcx, [rbp+13F0h+var_EA0]
      +0x550 r @0x141B3CF18: lea     rdx, [rbp+13F0h+var_EA0]
      +0x550 r @0x141B3CF6A: mov     rcx, [rbp+13F0h+var_EA0]

  *** BOTH FUNC 0x145250D80 (rva 0x5250D80) sub_145250D80
      +0x328 w @0x1452513F5: db  89h
      +0x328 r @0x14525149B: db  48h ; H
      +0x328 r @0x14525149C: db  8Dh
      +0x328 r @0x14525235D: db  48h ; H
      +0x328 r @0x14525235E: db  8Dh
      +0x330 w @0x145251415: db  48h ; H
      +0x330 w @0x145251416: db  89h
      +0x340 w @0x14525144A: db  48h ; H
      +0x340 w @0x14525144B: db  89h
      +0x350 w @0x145251476: db  48h ; H
      +0x350 w @0x145251477: db  89h
      +0x360 w @0x145251420: db  48h ; H
      +0x360 w @0x145251421: db  89h
      +0x368 w @0x14525140A: db  48h ; H
      +0x368 w @0x14525140B: db  89h
      +0x548 w @0x1452519AF: db  89h
      +0x548 r @0x145251A79: db  48h ; H
      +0x548 r @0x145251A7A: db  8Dh
      +0x548 r @0x1452522D8: db  48h ; H
      +0x548 r @0x1452522D9: db  40h ; @
      +0x548 r @0x1452522DA: db  48h ; H
      +0x548 r @0x1452522DB: db  8Dh
      +0x550 w @0x1452519D8: db  48h ; H
      +0x550 w @0x1452519D9: db  89h
      +0x558 w @0x1452519BC: db  48h ; H
      +0x558 w @0x1452519BD: db  89h

  *** BOTH FUNC 0x14254CE40 (rva 0x254CE40) sub_14254CE40
      +0x320 r @0x14254D095: db  48h ; H
      +0x320 r @0x14254D096: db  8Dh
      +0x360 w @0x14254D0A9: db  48h ; H
      +0x360 w @0x14254D0AA: db 0C7h
      +0x368 r @0x14254D0BB: db  48h ; H
      +0x368 r @0x14254D0BC: db  8Dh
      +0x548 r @0x14254D31D: db  48h ; H
      +0x548 r @0x14254D31E: db  8Dh

  *** BOTH FUNC 0x1403D0E50 (rva 0x3D0E50) sub_1403D0E50
      +0x320 r @0x1403D11A6: lea     rcx, [rdi+320h]
      +0x320 w @0x1403D11B2: mov     [rdi+320h], r13
      +0x340 r @0x1403D11C7: lea     rcx, [rdi+340h]
      +0x340 w @0x1403D11D3: mov     [rdi+340h], r15
      +0x360 r @0x1403D11E8: lea     rcx, [rdi+360h]
      +0x360 w @0x1403D11F4: mov     [rdi+360h], r15
      +0x540 r @0x1403D13E1: lea     rcx, [rdi+540h]
      +0x540 w @0x1403D13F4: mov     [rdi+540h], rax
      +0x558 w @0x1403D13FB: mov     [rdi+558h], r12

  *** BOTH FUNC 0x1422AAE80 (rva 0x22AAE80) sub_1422AAE80
      +0x328 w @0x1422AC163: db  4Ch ; L
      +0x328 w @0x1422AC164: db  89h
      +0x328 w @0x1422AC3BE: db  4Ch ; L
      +0x328 w @0x1422AC3BF: db  89h
      +0x330 w @0x1422AC16A: db  48h ; H
      +0x330 w @0x1422AC16B: db 0C7h
      +0x330 r @0x1422AC37D: db  48h ; H
      +0x330 r @0x1422AC37E: db  8Bh
      +0x330 w @0x1422AC3C5: db  48h ; H
      +0x330 w @0x1422AC3C6: db 0C7h
      +0x350 w @0x1422AC57F: db  48h ; H
      +0x350 w @0x1422AC580: db 0C7h
      +0x350 r @0x1422AC5BD: db  48h ; H
      +0x350 r @0x1422AC5BE: db  8Bh
      +0x350 w @0x1422AC64A: db  48h ; H
      +0x350 w @0x1422AC64B: db 0C7h
      +0x360 w @0x1422ABBCD: db 0FFh
      +0x368 w @0x1422AD27A: db  4Ch ; L
      +0x368 w @0x1422AD27B: db  89h
      +0x368 w @0x1422AD296: db  0Fh
      +0x368 w @0x1422AD297: db  11h
      +0x368 w @0x1422AD310: db  4Ch ; L
      +0x368 w @0x1422AD311: db  89h
      +0x540 w @0x1422ACDD0: db  4Ch ; L
      +0x540 w @0x1422ACDD1: db  89h
      +0x548 w @0x1422ACDD7: db  48h ; H
      +0x548 w @0x1422ACDD8: db 0C7h
      +0x548 r @0x1422ACE34: db  48h ; H
      +0x548 r @0x1422ACE35: db  8Bh
      +0x550 r @0x1422AD0A9: db  48h ; H
      +0x550 r @0x1422AD0AA: db  8Dh
      +0x550 r @0x1422AD104: db  4Ch ; L
      +0x550 r @0x1422AD105: db  8Dh
      +0x550 r @0x1422AD146: db  48h ; H
      +0x550 r @0x1422AD147: db  8Dh
      +0x550 r @0x1422AD155: db  48h ; H
      +0x550 r @0x1422AD156: db  0Fh

  *** BOTH FUNC 0x1426F2E90 (rva 0x26F2E90) sub_1426F2E90
      +0x320 r @0x1426F48E7: db  48h ; H
      +0x320 r @0x1426F48E8: db  8Dh
      +0x320 r @0x1426F4904: db  0Fh
      +0x320 w @0x1426F4905: db  28h ; (
      +0x328 r @0x1426F48F4: db  48h ; H
      +0x328 r @0x1426F48F5: db  8Bh
      +0x328 r @0x1426F4964: db  48h ; H
      +0x328 r @0x1426F4965: db  8Bh
      +0x330 r @0x1426F4B32: db  48h ; H
      +0x330 r @0x1426F4B33: db  8Dh
      +0x330 r @0x1426F4B4F: db  0Fh
      +0x330 w @0x1426F4B50: db  28h ; (
      +0x340 r @0x1426F4BC8: db  48h ; H
      +0x340 r @0x1426F4BC9: db  8Dh
      +0x340 r @0x1426F4BE5: db  0Fh
      +0x340 w @0x1426F4BE6: db  28h ; (
      +0x350 r @0x1426F4C5E: db  48h ; H
      +0x350 r @0x1426F4C5F: db  8Dh
      +0x350 r @0x1426F4C7B: db  0Fh
      +0x350 w @0x1426F4C7C: db  28h ; (
      +0x360 r @0x1426F4CF4: db  48h ; H
      +0x360 r @0x1426F4CF5: db  8Dh
      +0x360 r @0x1426F4D11: db  0Fh
      +0x360 w @0x1426F4D12: db  28h ; (
      +0x368 r @0x1426F4D01: db  48h ; H
      +0x368 r @0x1426F4D02: db  8Bh
      +0x368 r @0x1426F4D71: db  48h ; H
      +0x368 r @0x1426F4D72: db  8Bh
      +0x540 w @0x1426F3E9D: db  0Fh
      +0x540 w @0x1426F3E9E: db  11h

  *** BOTH FUNC 0x141B54EE0 (rva 0x1B54EE0) sub_141B54EE0
      +0x320 w @0x141B56961: db 0F3h
      +0x320 w @0x141B56962: db  0Fh
      +0x320 w @0x141B56963: db  11h
      +0x324 w @0x141B56993: db 0F3h
      +0x324 w @0x141B56994: db  0Fh
      +0x324 w @0x141B56995: db  11h
      +0x328 w @0x141B569C5: db 0F3h
      +0x328 w @0x141B569C6: db  0Fh
      +0x328 w @0x141B569C7: db  11h
      +0x32C w @0x141B569F7: db 0F3h
      +0x32C w @0x141B569F8: db  0Fh
      +0x32C w @0x141B569F9: db  11h
      +0x330 w @0x141B56A29: db 0F3h
      +0x330 w @0x141B56A2A: db  0Fh
      +0x330 w @0x141B56A2B: db  11h
      +0x340 w @0x141B56AF1: db 0F3h
      +0x340 w @0x141B56AF2: db  0Fh
      +0x340 w @0x141B56AF3: db  11h
      +0x350 w @0x141B56BC5: db 0F3h
      +0x350 w @0x141B56BC6: db  0Fh
      +0x350 w @0x141B56BC7: db  11h
      +0x350 r @0x141B56D43: db  8Bh
      +0x360 w @0x141B56C91: db 0F3h
      +0x360 w @0x141B56C92: db  0Fh
      +0x360 w @0x141B56C93: db  11h
      +0x364 w @0x141B56CC3: db 0F3h
      +0x364 w @0x141B56CC4: db  0Fh
      +0x364 w @0x141B56CC5: db  11h
      +0x364 w @0x141B56D31: db  89h
      +0x368 w @0x141B56CF6: db 0F3h
      +0x368 w @0x141B56CF7: db  0Fh
      +0x368 w @0x141B56CF8: db  11h
      +0x368 w @0x141B56D3D: db  89h
      +0x540 w @0x141B57EEA: db 0F3h
      +0x540 w @0x141B57EEB: db  0Fh
      +0x540 w @0x141B57EEC: db  11h
      +0x544 w @0x141B57F1C: db 0F3h
      +0x544 w @0x141B57F1D: db  0Fh
      +0x544 w @0x141B57F1E: db  11h
      +0x548 w @0x141B57F53: db 0F3h
      +0x548 w @0x141B57F54: db  0Fh
      +0x548 w @0x141B57F55: db  11h
      +0x54C w @0x141B57F85: db 0F3h
      +0x54C w @0x141B57F86: db  0Fh
      +0x54C w @0x141B57F87: db  11h
      +0x550 r @0x141B55472: db  48h ; H
      +0x550 r @0x141B55473: db  8Dh
      +0x550 r @0x141B55490: db  48h ; H
      +0x550 r @0x141B55491: db  8Dh
      +0x550 w @0x141B57EB9: db  0Fh
      +0x550 w @0x141B57EBA: db  11h

  *** BOTH FUNC 0x140D52F40 (rva 0xD52F40) sub_140D52F40
      +0x320 r @0x140D53824: lea     rcx, [rbp+320h]
      +0x320 w @0x140D5549B: cmp     byte ptr [rbx+320h], 0
      +0x330 w @0x140D552F7: movaps  xmmword ptr [rbp+330h], xmm0
      +0x330 r @0x140D55389: lea     r8, [rbp+330h]
      +0x340 w @0x140D55306: movdqa  xmmword ptr [rbp+340h], xmm1
      +0x350 w @0x140D55312: movdqa  xmmword ptr [rbp+350h], xmm1
      +0x360 w @0x140D5531A: movdqa  xmmword ptr [rbp+360h], xmm1
      +0x540 r @0x140D52FD3: db  48h ; H
      +0x540 r @0x140D52FD4: db  8Bh
      +0x548 r @0x140D52FF1: db  48h ; H
      +0x548 r @0x140D52FF2: db  8Bh
      +0x550 r @0x140D53012: mov     rax, [rbp+550h]

  *** BOTH FUNC 0x140A52F50 (rva 0xA52F50) sub_140A52F50
      +0x320 w @0x140A533BE: mov     qword ptr [rbp+320h], 26h ; '&'
      +0x328 w @0x140A533C9: mov     qword ptr [rbp+328h], 2Fh ; '/'
      +0x328 r @0x140A53486: mov     rdx, [rbp+328h]
      +0x330 w @0x140A543C2: movdqu  xmmword ptr [rbp+330h], xmm0
      +0x330 r @0x140A543E0: lea     rax, [rbp+330h]
      +0x340 w @0x140A543CA: mov     [rbp+340h], rbx
      +0x360 w @0x140A54661: mov     [rbp+360h], rax
      +0x360 r @0x140A54697: lea     r9, [rbp+360h]
      +0x368 w @0x140A5466B: movdqu  xmmword ptr [rbp+368h], xmm0
      +0x540 w @0x140A54601: movaps  xmmword ptr [rbp+540h], xmm1
      +0x550 w @0x140A5460F: movaps  xmmword ptr [rbp+550h], xmm0

  *** BOTH FUNC 0x1450E4F50 (rva 0x50E4F50) sub_1450E4F50
      +0x320 w @0x1450E523F: db  48h ; H
      +0x320 w @0x1450E5240: db  89h
      +0x320 r @0x1450E5DCD: db  48h ; H
      +0x320 r @0x1450E5DCE: db  8Bh
      +0x328 w @0x1450E5547: db  4Ch ; L
      +0x328 w @0x1450E5548: db  89h
      +0x328 r @0x1450E5DB5: db  4Ch ; L
      +0x328 r @0x1450E5DB6: db  8Bh
      +0x540 w @0x1450E5412: db  4Ch ; L
      +0x540 w @0x1450E5413: db  89h
      +0x548 w @0x1450E5419: db  4Ch ; L
      +0x548 w @0x1450E541A: db  89h
      +0x550 w @0x1450E5420: db  4Ch ; L
      +0x550 w @0x1450E5421: db  89h
      +0x558 w @0x1450E5427: db  4Ch ; L
      +0x558 w @0x1450E5428: db  89h

  *** BOTH FUNC 0x14495CF70 (rva 0x495CF70) sub_14495CF70
      +0x320 r @0x14495D581: db  49h ; I
      +0x320 r @0x14495D582: db  8Dh
      +0x320 r @0x14495D588: db  48h ; H
      +0x320 r @0x14495D589: db  8Dh
      +0x330 r @0x14495D594: db  41h ; A
      +0x330 r @0x14495D595: db  0Fh
      +0x330 w @0x14495D5A3: db  88h
      +0x340 r @0x14495D59C: db  49h ; I
      +0x340 r @0x14495D59D: db  8Dh
      +0x340 r @0x14495D5A9: db  48h ; H
      +0x340 r @0x14495D5AA: db  8Dh
      +0x360 r @0x14495D5F2: db  49h ; I
      +0x360 r @0x14495D5F3: db  8Bh
      +0x360 w @0x14495D5F9: db  48h ; H
      +0x360 w @0x14495D5FA: db  89h
      +0x368 r @0x14495D5DE: db  49h ; I
      +0x368 r @0x14495D5DF: db  8Dh
      +0x368 r @0x14495D5EB: db  48h ; H
      +0x368 r @0x14495D5EC: db  8Dh
      +0x550 r @0x14495D808: db  49h ; I
      +0x550 r @0x14495D809: db  8Dh
      +0x550 r @0x14495D80F: db  48h ; H
      +0x550 r @0x14495D810: db  8Dh

  *** BOTH FUNC 0x14186EF70 (rva 0x186EF70) sub_14186EF70
      +0x320 w @0x14186F8A1: db  0Fh
      +0x320 w @0x14186F8A2: db  11h
      +0x330 w @0x14186F8A8: db  0Fh
      +0x330 w @0x14186F8A9: db  11h
      +0x340 w @0x14186F8AF: db  0Fh
      +0x340 w @0x14186F8B0: db  11h
      +0x350 w @0x14186F8B6: db  0Fh
      +0x350 w @0x14186F8B7: db  11h
      +0x360 w @0x14186F8BD: db  48h ; H
      +0x360 w @0x14186F8BE: db  89h
      +0x368 w @0x14186F8C4: db  89h
      +0x558 r @0x14186FF0C: db  48h ; H
      +0x558 r @0x14186FF0D: db  8Dh

  *** BOTH FUNC 0x140C08F90 (rva 0xC08F90) sub_140C08F90
      +0x320 r @0x140C0A2F6: mov     rax, [rbp+320h]
      +0x328 w @0x140C09E75: cmp     [r14+328h], esi
      +0x328 r @0x140C0A1AA: lea     rcx, [rbp+328h]
      +0x328 r @0x140C0AB5D: db  48h ; H
      +0x328 r @0x140C0AB5E: db  8Dh
      +0x340 w @0x140C090DF: movaps  xmmword ptr [rbp+340h], xmm3
      +0x340 r @0x140C0958C: lea     rcx, [r14+340h]
      +0x340 r @0x140C0A017: lea     rdi, [r14+340h]
      +0x340 r @0x140C0A094: lea     rdi, [r14+340h]
      +0x350 w @0x140C090EA: movaps  xmmword ptr [rbp+350h], xmm4
      +0x360 w @0x140C090F5: movaps  xmmword ptr [rbp+360h], xmm5
      +0x360 r @0x140C0A53D: lea     rcx, [r14+360h]
      +0x540 w @0x140C09126: movaps  xmmword ptr [rbp+540h], xmm15
      +0x550 w @0x140C0912E: movaps  xmmword ptr [rbp+550h], xmm12

  *** BOTH FUNC 0x1418F4FB0 (rva 0x18F4FB0) sub_1418F4FB0
      +0x320 r @0x1418F5362: db  48h ; H
      +0x320 r @0x1418F5363: db  8Bh
      +0x320 r @0x1418F537A: db  48h ; H
      +0x320 r @0x1418F537B: db  8Bh
      +0x328 w @0x1418F540C: db  48h ; H
      +0x328 w @0x1418F540D: db  89h
      +0x328 r @0x1418F549A: db  4Ch ; L
      +0x328 r @0x1418F549B: db  8Dh
      +0x330 w @0x1418F5413: db 0C7h
      +0x350 r @0x1418F54D6: db  48h ; H
      +0x350 r @0x1418F54D7: db  8Bh
      +0x360 w @0x1418F5438: db 0F3h
      +0x360 w @0x1418F5439: db  0Fh
      +0x368 r @0x1418F54B1: db  48h ; H
      +0x368 r @0x1418F54B2: db  8Bh
      +0x368 r @0x1418F54C9: db  48h ; H
      +0x368 r @0x1418F54CA: db  8Bh
      +0x550 w @0x1418F5E75: db 0C6h
      +0x558 w @0x1418F5E7F: db 0F3h
      +0x558 w @0x1418F5E80: db  0Fh

  *** BOTH FUNC 0x1450F1010 (rva 0x50F1010) sub_1450F1010
      +0x330 w @0x1450F2359: db  49h ; I
      +0x330 w @0x1450F235A: db  89h
      +0x368 w @0x1450F2360: db  49h ; I
      +0x368 w @0x1450F2361: db  89h
      +0x540 w @0x1450F1288: db  4Ch ; L
      +0x540 w @0x1450F1289: db  89h
      +0x548 w @0x1450F128F: db  4Ch ; L
      +0x548 w @0x1450F1290: db  89h
      +0x550 w @0x1450F1296: db  4Ch ; L
      +0x550 w @0x1450F1297: db  89h
      +0x558 w @0x1450F129D: db  4Ch ; L
      +0x558 w @0x1450F129E: db  89h

  *** BOTH FUNC 0x1453FB080 (rva 0x53FB080) sub_1453FB080
      +0x320 w @0x1453FB219: db 0C5h
      +0x320 w @0x1453FB21B: db  11h
      +0x320 r @0x1453FB2BC: db 0C5h
      +0x320 w @0x1453FB2BE: db  10h
      +0x320 w @0x1453FB85A: db 0C5h
      +0x320 w @0x1453FB85C: db  11h
      +0x340 w @0x1453FB1FA: db 0C5h
      +0x340 w @0x1453FB1FC: db  11h
      +0x340 r @0x1453FB2CF: db 0C5h
      +0x340 w @0x1453FB2D1: db  10h
      +0x340 w @0x1453FB862: db 0C5h
      +0x340 w @0x1453FB864: db  11h
      +0x340 r @0x1453FBA4F: db 0C5h
      +0x340 w @0x1453FBA51: db  10h
      +0x360 w @0x1453FB1DE: db 0C5h
      +0x360 w @0x1453FB1E0: db  11h
      +0x360 r @0x1453FB2A9: db 0C5h
      +0x360 w @0x1453FB2AB: db  10h
      +0x360 w @0x1453FB86A: db 0C5h
      +0x360 w @0x1453FB86C: db  11h
      +0x360 r @0x1453FBA42: db 0C5h
      +0x360 w @0x1453FBA44: db  10h
      +0x550 r @0x1453FB0FE: db  48h ; H
      +0x550 r @0x1453FB0FF: db  8Bh
      +0x558 r @0x1453FB0DB: db  48h ; H
      +0x558 r @0x1453FB0DC: db  8Bh

  *** BOTH FUNC 0x140BAF080 (rva 0xBAF080) sub_140BAF080
      +0x320 w @0x140BAF8D5: movaps  xmmword ptr [rbp+320h], xmm0
      +0x330 w @0x140BAF8E0: movaps  xmmword ptr [rbp+330h], xmm5
      +0x340 w @0x140BAF8EB: movaps  xmmword ptr [rbp+340h], xmm1
      +0x350 w @0x140BAF956: movaps  xmmword ptr [rbp+350h], xmm4
      +0x360 w @0x140BAF961: movaps  xmmword ptr [rbp+360h], xmm0
      +0x558 r @0x140BB0592: lea     r11, [rsp+558h]

  *** BOTH FUNC 0x141B310E0 (rva 0x1B310E0) sub_141B310E0
      +0x320 r @0x141B3154B: db  0Fh
      +0x320 w @0x141B3154C: db  10h
      +0x320 w @0x141B31552: db  0Fh
      +0x320 w @0x141B31553: db  11h
      +0x330 r @0x141B31559: db  8Bh
      +0x330 w @0x141B3155F: db  89h
      +0x340 r @0x141B31589: db  8Bh
      +0x340 w @0x141B3158F: db  89h
      +0x350 r @0x141B315B9: db  8Bh
      +0x350 w @0x141B315BF: db  89h
      +0x360 r @0x141B31604: db  8Bh
      +0x360 w @0x141B3160A: db  89h
      +0x364 r @0x141B31610: db  8Bh
      +0x364 w @0x141B31616: db  89h
      +0x368 r @0x141B3161C: db  8Bh
      +0x368 w @0x141B31622: db  89h
      +0x540 r @0x141B31A5E: db 0F2h
      +0x540 r @0x141B31A5F: db  0Fh
      +0x540 w @0x141B31A60: db  10h
      +0x540 w @0x141B31A66: db 0F2h
      +0x540 w @0x141B31A67: db  0Fh
      +0x540 w @0x141B31A68: db  11h
      +0x548 r @0x141B31A6E: db  0Fh
      +0x548 w @0x141B31A6F: db  10h
      +0x548 w @0x141B31A75: db  0Fh
      +0x548 w @0x141B31A76: db  11h
      +0x558 r @0x141B31A7C: db  0Fh
      +0x558 w @0x141B31A7D: db  10h
      +0x558 w @0x141B31A83: db  0Fh
      +0x558 w @0x141B31A84: db  11h

  *** BOTH FUNC 0x140D69160 (rva 0xD69160) sub_140D69160
      +0x320 w @0x140D69684: db  0Fh
      +0x320 w @0x140D69685: db  11h
      +0x330 w @0x140D6968B: db  0Fh
      +0x330 w @0x140D6968C: db  11h
      +0x340 w @0x140D69692: db  0Fh
      +0x340 w @0x140D69693: db  11h
      +0x540 w @0x140D693FE: db  0Fh
      +0x540 w @0x140D693FF: db  11h
      +0x550 w @0x140D69405: db  0Fh
      +0x550 w @0x140D69406: db  11h

  *** BOTH FUNC 0x1411AB180 (rva 0x11AB180) sub_1411AB180
      +0x320 r @0x1411AC095: movups  xmm1, xmmword ptr [rax+rcx+320h]
      +0x320 r @0x1411AD542: mov     rcx, [rbx+320h]
      +0x328 w @0x1411AD236: call    qword ptr [rax+328h]
      +0x328 w @0x1411ADA17: call    qword ptr [rax+328h]
      +0x328 w @0x1411ADA29: call    qword ptr [rax+328h]
      +0x330 w @0x1411AB530: db 0FFh
      +0x330 r @0x1411ABA89: lea     rdx, [r10+330h]
      +0x330 r @0x1411AC0A5: movups  xmm0, xmmword ptr [rax+rcx+330h]
      +0x330 w @0x1411ACB79: call    qword ptr [rax+330h]
      +0x340 r @0x1411AC135: movups  xmm0, xmmword ptr [rax+rcx+340h]
      +0x340 r @0x1411AD760: lea     rax, [r8+340h]
      +0x350 r @0x1411AC145: movups  xmm1, xmmword ptr [rax+rcx+350h]
      +0x360 r @0x1411AC155: movups  xmm0, xmmword ptr [rax+rcx+360h]
      +0x360 r @0x1411AD12F: lea     rdx, [rbp+360h]
      +0x364 w @0x1411AB705: db  89h
      +0x550 r @0x1411ABBE9: lea     rdx, [r10+550h]

  *** BOTH FUNC 0x142833190 (rva 0x2833190) sub_142833190
      +0x330 w @0x1428346E1: db  66h ; f
      +0x330 w @0x1428346E2: db  0Fh
      +0x330 r @0x1428346ED: db  66h ; f
      +0x330 r @0x1428346EE: db  0Fh
      +0x330 w @0x14283470D: db  66h ; f
      +0x330 w @0x14283470E: db  0Fh
      +0x340 w @0x14283491F: db  66h ; f
      +0x340 w @0x142834920: db  0Fh
      +0x340 r @0x14283492B: db  66h ; f
      +0x340 r @0x14283492C: db  0Fh
      +0x340 w @0x14283494B: db  66h ; f
      +0x340 w @0x14283494C: db  0Fh
      +0x350 w @0x14283353A: db  4Ch ; L
      +0x350 w @0x14283353B: db  89h
      +0x350 r @0x14283355E: db  0Fh
      +0x350 w @0x14283355F: db  10h
      +0x360 w @0x142833E82: db  48h ; H
      +0x360 w @0x142833E83: db  89h
      +0x360 r @0x142833EC7: db  48h ; H
      +0x360 r @0x142833EC8: db  8Bh
      +0x368 w @0x142833E89: db  48h ; H
      +0x368 w @0x142833E8A: db  89h
      +0x540 w @0x142834D33: db  48h ; H
      +0x540 w @0x142834D34: db  89h

  *** BOTH FUNC 0x140B29210 (rva 0xB29210) sub_140B29210
      +0x320 r @0x140B29C5B: lea     rax, [rbp+320h]
      +0x320 w @0x140B29C74: mov     [rbp+320h], rcx
      +0x320 r @0x140B29DE3: lea     rax, [rbp+320h]
      +0x328 r @0x140B29C87: lea     rax, [rbp+328h]
      +0x328 w @0x140B29C9D: mov     [rbp+328h], rcx
      +0x328 r @0x140B29DEF: lea     rax, [rbp+328h]
      +0x330 r @0x140B29CB0: lea     rax, [rbp+330h]
      +0x330 w @0x140B29CC6: mov     [rbp+330h], rcx
      +0x330 r @0x140B29DFB: lea     rax, [rbp+330h]
      +0x340 r @0x140B29CFF: lea     rax, [rbp+340h]
      +0x340 w @0x140B29D12: mov     [rbp+340h], rcx
      +0x340 r @0x140B29E13: lea     rax, [rbp+340h]
      +0x350 r @0x140B292FA: lea     rcx, [rbp+350h]
      +0x350 w @0x140B2AEEC: mov     [rbp+350h], rbx
      +0x360 r @0x140B29C69: mov     rax, [rbp+360h]
      +0x360 r @0x140B29F70: mov     rax, [rbp+360h]
      +0x360 w @0x140B2AF1D: mov     [rbp+360h], r14
      +0x368 r @0x140B292E3: lea     rcx, [rbp+368h]
      +0x368 w @0x140B2AF37: mov     [rbp+368h], rbx
      +0x540 r @0x140B293F9: lea     rcx, [rbp+540h]
      +0x540 r @0x140B2AD2F: lea     rcx, [rbp+540h]
      +0x550 r @0x140B299D4: mov     rax, [rbp+rbx+550h]
      +0x550 r @0x140B29D74: mov     rax, [rbp+550h]
      +0x550 r @0x140B2A061: mov     rax, [rbp+550h]
      +0x558 r @0x140B29410: lea     rcx, [rbp+558h]

  *** BOTH FUNC 0x145441210 (rva 0x5441210) sub_145441210
      +0x320 w @0x145442709: movaps  [rbp+3410h+var_30F0], xmm6
      +0x320 r @0x145442D48: movaps  xmm6, [rbp+3410h+var_30F0]
      +0x330 w @0x145442720: movaps  [rbp+3410h+var_30E0], xmm0
      +0x330 r @0x145442D4F: movaps  xmm2, [rbp+3410h+var_30E0]
      +0x340 w @0x14544278E: movaps  [rbp+3410h+var_30D0], xmm11
      +0x340 r @0x145442D56: movaps  xmm11, [rbp+3410h+var_30D0]
      +0x350 w @0x1454427FC: movaps  [rbp+3410h+var_30C0], xmm13
      +0x350 r @0x145442D5E: movaps  xmm13, [rbp+3410h+var_30C0]
      +0x360 w @0x14544135F: movaps  [rbp+3410h+var_30B0], xmm4
      +0x360 r @0x14544164D: movaps  xmm4, [rbp+3410h+var_30B0]
      +0x540 w @0x145443D0E: movaps  [rbp+3410h+var_2ED0], xmm4
      +0x540 r @0x14544430D: movaps  xmm0, [rbp+3410h+var_2ED0]
      +0x550 w @0x14544453C: movaps  [rbp+3410h+var_2EC0], xmm0
      +0x550 r @0x14544497F: movaps  xmm0, [rbp+3410h+var_2EC0]

  *** BOTH FUNC 0x141143220 (rva 0x1143220) sub_141143220
      +0x320 r @0x14114371A: db  0Fh
      +0x320 w @0x14114371B: db  10h
      +0x320 w @0x141143721: db  0Fh
      +0x320 w @0x141143722: db  11h
      +0x330 r @0x141143728: db  8Bh
      +0x330 w @0x14114372E: db  89h
      +0x340 r @0x141143758: db  8Bh
      +0x340 w @0x14114375E: db  89h
      +0x350 r @0x141143788: db  8Bh
      +0x350 w @0x14114378E: db  89h
      +0x360 r @0x1411437D3: db  8Bh
      +0x360 w @0x1411437D9: db  89h
      +0x364 r @0x1411437DF: db  8Bh
      +0x364 w @0x1411437E5: db  89h
      +0x368 r @0x1411437EB: db  8Bh
      +0x368 w @0x1411437F1: db  89h
      +0x540 r @0x141143C02: movsd   xmm1, qword ptr [rdi+540h]
      +0x540 w @0x141143C0A: movsd   qword ptr [rbx+540h], xmm1
      +0x548 r @0x141143C12: movups  xmm0, xmmword ptr [rdi+548h]
      +0x548 w @0x141143C19: movups  xmmword ptr [rbx+548h], xmm0
      +0x558 r @0x141143C20: movups  xmm1, xmmword ptr [rdi+558h]
      +0x558 w @0x141143C27: movups  xmmword ptr [rbx+558h], xmm1

  *** BOTH FUNC 0x140B5D230 (rva 0xB5D230) sub_140B5D230
      +0x320 w @0x140B5E038: movups  xmmword ptr [rdi+320h], xmm9
      +0x330 w @0x140B5E040: movups  xmmword ptr [rdi+330h], xmm4
      +0x330 r @0x140B5EA3A: lea     rcx, [rbp+7F0h+var_4C0]
      +0x340 w @0x140B5E047: movups  xmmword ptr [rdi+340h], xmm3
      +0x340 r @0x140B5EA24: lea     rcx, [rbp+7F0h+var_4B0]
      +0x350 w @0x140B5E04E: movups  xmmword ptr [rdi+350h], xmm2
      +0x350 r @0x140B5EAF8: lea     rdx, [rbp+7F0h+var_4A0]
      +0x360 r @0x140B5EAB2: lea     rcx, [rbp+7F0h+var_490]
      +0x540 r @0x140B5E161: shufps  xmm11, [rbp+7F0h+var_2B0], 44h ; 'D'
      +0x540 r @0x140B5E16A: shufps  xmm10, [rbp+7F0h+var_2B0], 0EEh
      +0x550 r @0x140B5E173: movaps  xmm2, [rbp+7F0h+var_2A0]

  *** BOTH FUNC 0x140687290 (rva 0x687290) sub_140687290
      +0x320 r @0x14068926C: lea     rcx, [rbp+320h]
      +0x320 r @0x14068929B: movaps  xmm0, xmmword ptr [rbp+320h]
      +0x320 w @0x1406894A4: call    qword ptr [rax+320h]
      +0x328 r @0x140689284: mov     rbx, [rbp+328h]
      +0x328 r @0x140689294: mov     rbx, [rbp+328h]
      +0x330 w @0x14068927C: movdqa  xmmword ptr [rbp+330h], xmm0
      +0x330 w @0x1406892A2: movdqa  xmmword ptr [rbp+330h], xmm0
      +0x330 r @0x1406892C6: lea     r8, [rbp+330h]
      +0x340 w @0x1406892AD: mov     [rbp+340h], rax
      +0x340 r @0x1406892CD: lea     rdx, [rbp+340h]
      +0x350 r @0x140689815: lea     rcx, [rbp+350h]
      +0x350 r @0x140689844: movaps  xmm0, xmmword ptr [rbp+350h]
      +0x360 w @0x140689825: movdqa  xmmword ptr [rbp+360h], xmm0
      +0x360 w @0x14068984B: movdqa  xmmword ptr [rbp+360h], xmm0
      +0x360 r @0x14068986F: lea     r8, [rbp+360h]
      +0x540 r @0x140689C13: lea     rcx, [rbp+540h]
      +0x540 r @0x140689C32: mov     rcx, [rbp+540h]
      +0x540 r @0x140689C67: lea     rcx, [rbp+540h]
      +0x550 w @0x140687C3E: mov     [rbp+550h], rax
      +0x550 r @0x140687C48: lea     r8, [rbp+550h]
      +0x550 r @0x140687DBC: mov     rcx, [rbp+550h]

  *** BOTH FUNC 0x1446CD2B0 (rva 0x46CD2B0) sub_1446CD2B0
      +0x320 r @0x1446CD3DF: db  48h ; H
      +0x320 r @0x1446CD3E0: db  8Dh
      +0x320 r @0x1446CD3E6: db  48h ; H
      +0x320 r @0x1446CD3E7: db  8Dh
      +0x340 r @0x1446CD3F3: db  48h ; H
      +0x340 r @0x1446CD3F4: db  8Dh
      +0x340 r @0x1446CD3FA: db  48h ; H
      +0x340 r @0x1446CD3FB: db  8Dh
      +0x360 r @0x1446CD407: db  0Fh
      +0x360 w @0x1446CD40E: db  88h
      +0x368 r @0x1446CD414: db  48h ; H
      +0x368 r @0x1446CD415: db  8Dh
      +0x368 r @0x1446CD41B: db  48h ; H
      +0x368 r @0x1446CD41C: db  8Dh
      +0x548 r @0x1446CD5A9: db  48h ; H
      +0x548 r @0x1446CD5AA: db  8Dh
      +0x548 r @0x1446CD5B0: db  48h ; H
      +0x548 r @0x1446CD5B1: db  8Dh

  *** BOTH FUNC 0x140CCD2C0 (rva 0xCCD2C0) sub_140CCD2C0
      +0x320 r @0x140CCD520: mov     ecx, [rbx+320h]
      +0x320 w @0x140CCD552: movups  xmmword ptr [rbx+320h], xmm0
      +0x320 r @0x140CCD581: mov     ecx, [rbx+320h]
      +0x320 w @0x140CCD5B9: movups  xmmword ptr [rbx+320h], xmm0
      +0x320 r @0x140CCD736: mov     ecx, [rbx+320h]
      +0x320 w @0x140CCD768: movups  xmmword ptr [rbx+320h], xmm0
      +0x320 r @0x140CCD797: mov     ecx, [rbx+320h]
      +0x320 w @0x140CCD7C6: movups  xmmword ptr [rbx+320h], xmm0
      +0x324 r @0x140CCD526: mov     r8d, [rbx+324h]
      +0x324 r @0x140CCD587: mov     edx, [rbx+324h]
      +0x324 r @0x140CCD73C: mov     r8d, [rbx+324h]
      +0x324 r @0x140CCD79D: mov     edx, [rbx+324h]
      +0x328 r @0x140CCD530: mov     ecx, [rbx+328h]
      +0x328 r @0x140CCD599: mov     ecx, [rbx+328h]
      +0x328 r @0x140CCD746: mov     ecx, [rbx+328h]
      +0x328 r @0x140CCD7A6: mov     ecx, [rbx+328h]
      +0x32C r @0x140CCD539: mov     ecx, [rbx+32Ch]
      +0x32C r @0x140CCD5A2: mov     ecx, [rbx+32Ch]
      +0x32C r @0x140CCD74F: mov     ecx, [rbx+32Ch]
      +0x32C r @0x140CCD7AF: mov     ecx, [rbx+32Ch]
      +0x550 r @0x140CCD6AF: lea     rdx, [r9+550h]

  *** BOTH FUNC 0x141B232C0 (rva 0x1B232C0) sub_141B232C0
      +0x320 r @0x141B262F7: db  48h ; H
      +0x320 r @0x141B262F8: db  8Dh
      +0x320 w @0x141B26303: db  48h ; H
      +0x320 w @0x141B26304: db  89h
      +0x320 r @0x141B26FF4: db  48h ; H
      +0x320 r @0x141B26FF5: db  8Dh
      +0x320 r @0x141B27233: db  48h ; H
      +0x320 r @0x141B27234: db  8Dh
      +0x550 r @0x141B23648: db  48h ; H
      +0x550 r @0x141B23649: db  8Dh
      +0x550 w @0x141B2365B: db  48h ; H
      +0x550 w @0x141B2365C: db  89h
      +0x550 r @0x141B261D3: db  48h ; H
      +0x550 r @0x141B261D4: db  8Dh
      +0x550 w @0x141B261E6: db  48h ; H
      +0x550 w @0x141B261E7: db  89h

  *** BOTH FUNC 0x1411C1300 (rva 0x11C1300) sub_1411C1300
      +0x328 w @0x1411C16C4: db 0FFh
      +0x328 w @0x1411C16F4: db 0FFh
      +0x350 r @0x1411C18D9: lea     rax, [rbp+350h]
      +0x350 r @0x1411C18FA: lea     rdx, [rbp+350h]
      +0x350 r @0x1411C190A: lea     rax, [rbp+350h]
      +0x350 r @0x1411C1976: lea     rdx, [rbp+350h]
      +0x540 w @0x1411C1388: db  66h ; f
      +0x540 w @0x1411C1389: db  0Fh
      +0x550 w @0x1411C1390: db  66h ; f
      +0x550 w @0x1411C1391: db  0Fh

  *** BOTH FUNC 0x1450C7330 (rva 0x50C7330) sub_1450C7330
      +0x320 r @0x1450C75E3: db  41h ; A
      +0x320 r @0x1450C75E4: db  8Bh
      +0x320 w @0x1450C75F5: db  41h ; A
      +0x320 w @0x1450C75F6: db  89h
      +0x324 r @0x1450C7622: db 0F3h
      +0x324 r @0x1450C7623: db  41h ; A
      +0x324 r @0x1450C7624: db  0Fh
      +0x324 w @0x1450C7625: db  10h
      +0x324 r @0x1450C7693: db 0F3h
      +0x324 r @0x1450C7694: db  41h ; A
      +0x324 r @0x1450C7695: db  0Fh
      +0x328 r @0x1450C7619: db 0F3h
      +0x328 r @0x1450C761A: db  41h ; A
      +0x328 r @0x1450C761B: db  0Fh
      +0x328 w @0x1450C761C: db  10h
      +0x328 r @0x1450C76A5: db 0F3h
      +0x328 r @0x1450C76A6: db  41h ; A
      +0x328 r @0x1450C76A7: db  0Fh
      +0x32C r @0x1450C7610: db 0F3h
      +0x32C r @0x1450C7611: db  41h ; A
      +0x32C r @0x1450C7612: db  0Fh
      +0x32C w @0x1450C7613: db  10h
      +0x32C r @0x1450C76B7: db 0F3h
      +0x32C r @0x1450C76B8: db  41h ; A
      +0x32C r @0x1450C76B9: db  0Fh
      +0x330 r @0x1450C7607: db 0F3h
      +0x330 r @0x1450C7608: db  41h ; A
      +0x330 r @0x1450C7609: db  0Fh
      +0x330 w @0x1450C760A: db  10h
      +0x330 r @0x1450C76C9: db 0F3h
      +0x330 r @0x1450C76CA: db  41h ; A
      +0x330 r @0x1450C76CB: db  0Fh
      +0x340 r @0x1450C7654: db 0F3h
      +0x340 r @0x1450C7655: db  41h ; A
      +0x340 r @0x1450C7656: db  0Fh
      +0x340 w @0x1450C7657: db  10h
      +0x350 r @0x1450C764B: db 0F3h
      +0x350 r @0x1450C764C: db  41h ; A
      +0x350 r @0x1450C764D: db  0Fh
      +0x350 w @0x1450C769C: db 0F3h
      +0x350 w @0x1450C769D: db  41h ; A
      +0x350 w @0x1450C769E: db  0Fh
      +0x350 w @0x1450C769F: db  11h
      +0x360 w @0x1450C7CF6: db 0F3h
      +0x360 w @0x1450C7CF7: db  41h ; A
      +0x360 w @0x1450C7CF8: db  0Fh
      +0x360 w @0x1450C7CF9: db  11h
      +0x364 w @0x1450C7D0C: db 0F3h
      +0x364 w @0x1450C7D0D: db  41h ; A
      +0x364 w @0x1450C7D0E: db  0Fh
      +0x364 w @0x1450C7D0F: db  11h
      +0x368 w @0x1450C7D1C: db  41h ; A
      +0x368 w @0x1450C7D1D: db  89h
      +0x540 w @0x1450C7FCC: db 0F3h
      +0x540 w @0x1450C7FCD: db  41h ; A
      +0x540 w @0x1450C7FCE: db  0Fh
      +0x540 w @0x1450C7FCF: db  11h
      +0x540 r @0x1450C7FEB: db 0F3h
      +0x540 r @0x1450C7FEC: db  41h ; A
      +0x540 r @0x1450C7FED: db  0Fh
      +0x540 w @0x1450C7FEE: db  10h
      +0x550 r @0x1450C7EA7: db  41h ; A
      +0x550 r @0x1450C7EA8: db  8Bh
      +0x550 w @0x1450C7EB9: db  41h ; A
      +0x550 w @0x1450C7EBA: db  89h
      +0x554 r @0x1450C7EE6: db 0F3h
      +0x554 r @0x1450C7EE7: db  41h ; A
      +0x554 r @0x1450C7EE8: db  0Fh
      +0x554 w @0x1450C7EE9: db  10h
      +0x554 r @0x1450C7F57: db 0F3h
      +0x554 r @0x1450C7F58: db  41h ; A
      +0x554 r @0x1450C7F59: db  0Fh
      +0x558 r @0x1450C7EDD: db 0F3h
      +0x558 r @0x1450C7EDE: db  41h ; A
      +0x558 r @0x1450C7EDF: db  0Fh
      +0x558 w @0x1450C7EE0: db  10h
      +0x558 r @0x1450C7F69: db 0F3h
      +0x558 r @0x1450C7F6A: db  41h ; A
      +0x558 r @0x1450C7F6B: db  0Fh
      +0x55C r @0x1450C7ED4: db 0F3h
      +0x55C r @0x1450C7ED5: db  41h ; A
      +0x55C r @0x1450C7ED6: db  0Fh
      +0x55C w @0x1450C7ED7: db  10h
      +0x55C r @0x1450C7F7B: db 0F3h
      +0x55C r @0x1450C7F7C: db  41h ; A
      +0x55C r @0x1450C7F7D: db  0Fh

  *** BOTH FUNC 0x1418C5410 (rva 0x18C5410) sub_1418C5410
      +0x364 w @0x1418C55F3: db 0C6h
      +0x540 w @0x1418C55A3: db  0Fh
      +0x540 w @0x1418C55A4: db  11h
      +0x550 w @0x1418C55AE: db  0Fh
      +0x550 w @0x1418C55AF: db  11h

  *** BOTH FUNC 0x143CB5420 (rva 0x3CB5420) sub_143CB5420
      +0x320 w @0x143CB592D: db 0C6h
      +0x324 w @0x143CB5934: db  44h ; D
      +0x324 w @0x143CB5935: db  89h
      +0x328 r @0x143CB593E: db  48h ; H
      +0x328 r @0x143CB593F: db  8Dh
      +0x32C w @0x143CB594A: db 0C7h
      +0x330 r @0x143CB59B5: db  48h ; H
      +0x330 r @0x143CB59B6: db  8Dh
      +0x330 r @0x143CB59C8: db  48h ; H
      +0x330 r @0x143CB59C9: db  8Dh
      +0x368 w @0x143CB59C1: db  4Ch ; L
      +0x368 w @0x143CB59C2: db  89h
      +0x368 w @0x143CB59DC: db  48h ; H
      +0x368 w @0x143CB59DD: db  89h
      +0x540 w @0x143CB5D36: db  4Ch ; L
      +0x540 w @0x143CB5D37: db  89h
      +0x540 w @0x143CB5DB2: db  48h ; H
      +0x540 w @0x143CB5DB3: db  89h
      +0x540 w @0x143CB5DDE: db  48h ; H
      +0x540 w @0x143CB5DDF: db  89h
      +0x548 w @0x143CB5D3D: db  4Ch ; L
      +0x548 w @0x143CB5D3E: db  89h
      +0x548 w @0x143CB5DBC: db  48h ; H
      +0x548 w @0x143CB5DBD: db  89h
      +0x550 w @0x143CB5DEC: db  48h ; H
      +0x550 w @0x143CB5DED: db  89h
      +0x558 w @0x143CB5DF3: db  4Ch ; L
      +0x558 w @0x143CB5DF4: db  89h

  *** BOTH FUNC 0x140C57490 (rva 0xC57490) sub_140C57490
      +0x320 w @0x140C5750D: mov     dword ptr [rcx+320h], 3F800000h
      +0x324 w @0x140C57517: mov     dword ptr [rcx+324h], 3F800000h
      +0x328 w @0x140C57521: mov     dword ptr [rcx+328h], 3F800000h
      +0x32C w @0x140C5752B: mov     dword ptr [rcx+32Ch], 3F800000h
      +0x330 w @0x140C57535: mov     dword ptr [rcx+330h], 3E99999Ah
      +0x340 w @0x140C57904: mov     dword ptr [rcx+340h], 41C80000h
      +0x350 w @0x140C57D8F: mov     [rcx+350h], bpl
      +0x360 w @0x140C57DBE: movups  xmmword ptr [rcx+360h], xmm0
      +0x544 w @0x140C5792C: mov     dword ptr [rcx+544h], 3F800000h
      +0x548 w @0x140C57936: mov     dword ptr [rcx+548h], 3E4CCCCDh
      +0x550 w @0x140C579F5: movss   dword ptr [rcx+550h], xmm0
      +0x554 w @0x140C57A1F: movss   dword ptr [rcx+554h], xmm0
      +0x558 w @0x140C57A27: movss   dword ptr [rcx+558h], xmm4
      +0x55C w @0x140C57A32: mov     dword ptr [rcx+55Ch], 3F800000h

  *** BOTH FUNC 0x140649550 (rva 0x649550) sub_140649550
      +0x328 w @0x140649615: db 0FFh
      +0x330 w @0x14064967D: db 0FFh
      +0x360 r @0x140649763: db  49h ; I
      +0x360 r @0x140649764: db  8Bh
      +0x548 w @0x14064958C: db 0FFh
      +0x550 r @0x14064962F: db  48h ; H
      +0x550 r @0x140649630: db  8Bh
      +0x558 w @0x1406495AC: db 0FFh
      +0x558 w @0x140649696: db 0FFh

  *** BOTH FUNC 0x1412DF590 (rva 0x12DF590) sub_1412DF590
      +0x320 w @0x1412E0258: db  41h ; A
      +0x320 w @0x1412E0259: db  83h
      +0x320 w @0x1412E0263: db  41h ; A
      +0x320 w @0x1412E0264: db 0C7h
      +0x324 r @0x1412E0240: db  41h ; A
      +0x324 r @0x1412E0241: db  8Bh
      +0x324 w @0x1412E026F: db  41h ; A
      +0x324 w @0x1412E0270: db  89h
      +0x328 w @0x1412DF89F: db 0FFh
      +0x328 w @0x1412DFFA4: db 0FFh
      +0x328 r @0x1412E0248: db  41h ; A
      +0x328 r @0x1412E0249: db  8Bh
      +0x328 w @0x1412E0277: db  41h ; A
      +0x328 w @0x1412E0278: db  89h
      +0x32C r @0x1412E0250: db  41h ; A
      +0x32C r @0x1412E0251: db  8Bh
      +0x32C w @0x1412E027F: db  41h ; A
      +0x32C w @0x1412E0280: db  89h
      +0x540 w @0x1412DFD82: db  48h ; H
      +0x540 w @0x1412DFD83: db  89h
      +0x548 w @0x1412DFD4D: db 0C7h
      +0x54C w @0x1412DFD5F: db 0F3h
      +0x54C w @0x1412DFD60: db  0Fh
      +0x54C w @0x1412DFD61: db  11h
      +0x550 w @0x1412DFDB0: db 0C7h
      +0x558 w @0x1412DFD89: db  48h ; H
      +0x558 w @0x1412DFD8A: db  89h

  *** BOTH FUNC 0x140C0D660 (rva 0xC0D660) sub_140C0D660
      +0x320 w @0x140C0DFD7: db  44h ; D
      +0x320 w @0x140C0DFD8: db  0Fh
      +0x320 w @0x140C0DFD9: db  29h ; )
      +0x330 w @0x140C0DFDF: db  0Fh
      +0x330 w @0x140C0DFE0: db  29h ; )
      +0x340 r @0x140C0E09B: db  48h ; H
      +0x340 r @0x140C0E09C: db  8Dh
      +0x340 r @0x140C0E0AE: db  48h ; H
      +0x340 r @0x140C0E0AF: db  8Bh
      +0x340 r @0x140C0E0BC: db  48h ; H
      +0x340 r @0x140C0E0BD: db  8Dh
      +0x340 w @0x140C0E45D: db  0Fh
      +0x340 w @0x140C0E45E: db  11h
      +0x350 w @0x140C0E464: db  0Fh
      +0x350 w @0x140C0E465: db  11h
      +0x360 r @0x140C0E0ED: db  48h ; H
      +0x360 r @0x140C0E0EE: db  8Dh
      +0x360 w @0x140C0E49D: db  0Fh
      +0x360 w @0x140C0E49E: db  11h
      +0x540 w @0x140C0DEB1: db  0Fh
      +0x540 w @0x140C0DEB2: db  29h ; )
      +0x550 w @0x140C0DEB8: db  44h ; D
      +0x550 w @0x140C0DEB9: db  0Fh
      +0x550 w @0x140C0DEBA: db  29h ; )

  *** BOTH FUNC 0x1410EB670 (rva 0x10EB670) sub_1410EB670
      +0x368 w @0x1410EC25C: movss   dword ptr [rbx+368h], xmm0
      +0x550 w @0x1410EC035: movss   dword ptr [rbx+550h], xmm1

  *** BOTH FUNC 0x142553690 (rva 0x2553690) sub_142553690
      +0x320 w @0x1425537D1: db  48h ; H
      +0x320 w @0x1425537D2: db  89h
      +0x320 r @0x1425537F7: db  48h ; H
      +0x320 r @0x1425537F8: db  8Dh
      +0x340 r @0x1425537D8: db  48h ; H
      +0x340 r @0x1425537D9: db  8Dh
      +0x340 r @0x1425537DF: db  48h ; H
      +0x340 r @0x1425537E0: db  8Bh
      +0x360 r @0x14255376D: db  48h ; H
      +0x360 r @0x14255376E: db  8Bh
      +0x360 w @0x1425537B8: db  48h ; H
      +0x360 w @0x1425537B9: db 0C7h
      +0x368 r @0x142553761: db  48h ; H
      +0x368 r @0x142553762: db  8Dh
      +0x548 r @0x142553719: db  48h ; H
      +0x548 r @0x14255371A: db  8Dh

  *** BOTH FUNC 0x1412256C0 (rva 0x12256C0) sub_1412256C0
      +0x320 r @0x141225AF1: db  48h ; H
      +0x320 r @0x141225AF2: db  8Dh
      +0x320 r @0x141225AFE: db  48h ; H
      +0x320 r @0x141225AFF: db  8Dh
      +0x320 r @0x141225B20: db  48h ; H
      +0x320 r @0x141225B21: db  8Dh
      +0x328 w @0x141225AEA: db  4Ch ; L
      +0x328 w @0x141225AEB: db  89h
      +0x328 r @0x141225B0B: db  48h ; H
      +0x328 r @0x141225B0C: db  8Bh
      +0x328 w @0x141225B2D: db  48h ; H
      +0x328 w @0x141225B2E: db  89h
      +0x330 r @0x141225B3B: db  48h ; H
      +0x330 r @0x141225B3C: db  8Dh
      +0x330 r @0x141225B48: db  48h ; H
      +0x330 r @0x141225B49: db  8Dh
      +0x330 r @0x141225B6A: db  48h ; H
      +0x330 r @0x141225B6B: db  8Dh
      +0x340 r @0x141225B85: db  48h ; H
      +0x340 r @0x141225B86: db  8Dh
      +0x340 r @0x141225B92: db  48h ; H
      +0x340 r @0x141225B93: db  8Dh
      +0x340 r @0x141225BB4: db  48h ; H
      +0x340 r @0x141225BB5: db  8Dh
      +0x350 r @0x141225BCF: db  48h ; H
      +0x350 r @0x141225BD0: db  8Dh
      +0x350 r @0x141225BDC: db  48h ; H
      +0x350 r @0x141225BDD: db  8Dh
      +0x350 r @0x141225BFE: db  48h ; H
      +0x350 r @0x141225BFF: db  8Dh
      +0x360 r @0x141225C19: db  48h ; H
      +0x360 r @0x141225C1A: db  8Dh
      +0x360 r @0x141225C26: db  48h ; H
      +0x360 r @0x141225C27: db  8Dh
      +0x360 r @0x141225C48: db  48h ; H
      +0x360 r @0x141225C49: db  8Dh
      +0x368 w @0x141225C12: db  4Ch ; L
      +0x368 w @0x141225C13: db  89h
      +0x368 r @0x141225C33: db  48h ; H
      +0x368 r @0x141225C34: db  8Bh
      +0x368 w @0x141225C55: db  48h ; H
      +0x368 w @0x141225C56: db  89h
      +0x540 r @0x1412261D2: db  0Fh
      +0x540 w @0x1412261D3: db  10h
      +0x540 w @0x1412261D9: db  0Fh
      +0x540 w @0x1412261DA: db  11h
      +0x550 r @0x1412261E0: db  0Fh
      +0x550 w @0x1412261E1: db  10h
      +0x550 w @0x1412261E7: db  0Fh
      +0x550 w @0x1412261E8: db  11h

  *** BOTH FUNC 0x141B356C0 (rva 0x1B356C0) sub_141B356C0
      +0x320 r @0x141B369EE: db 0F3h
      +0x320 r @0x141B369EF: db  0Fh
      +0x320 w @0x141B369F0: db  10h
      +0x320 r @0x141B369F6: db 0F3h
      +0x320 r @0x141B369F7: db  0Fh
      +0x320 w @0x141B369F8: db  10h
      +0x320 w @0x141B36A0A: db 0F3h
      +0x320 w @0x141B36A0B: db  0Fh
      +0x324 r @0x141B36A12: db 0F3h
      +0x324 r @0x141B36A13: db  0Fh
      +0x324 w @0x141B36A14: db  10h
      +0x324 r @0x141B36A1A: db 0F3h
      +0x324 r @0x141B36A1B: db  0Fh
      +0x324 w @0x141B36A1C: db  10h
      +0x324 w @0x141B36A2E: db 0F3h
      +0x324 w @0x141B36A2F: db  0Fh
      +0x328 r @0x141B36A36: db 0F3h
      +0x328 r @0x141B36A37: db  0Fh
      +0x328 w @0x141B36A38: db  10h
      +0x328 r @0x141B36A3E: db 0F3h
      +0x328 r @0x141B36A3F: db  0Fh
      +0x328 w @0x141B36A40: db  10h
      +0x328 w @0x141B36A52: db 0F3h
      +0x328 w @0x141B36A53: db  0Fh
      +0x32C r @0x141B36A5A: db 0F3h
      +0x32C r @0x141B36A5B: db  0Fh
      +0x32C w @0x141B36A5C: db  10h
      +0x32C r @0x141B36A62: db 0F3h
      +0x32C r @0x141B36A63: db  0Fh
      +0x32C w @0x141B36A64: db  10h
      +0x32C w @0x141B36A76: db 0F3h
      +0x32C w @0x141B36A77: db  0Fh
      +0x330 r @0x141B36A7E: db 0F3h
      +0x330 r @0x141B36A7F: db  0Fh
      +0x330 w @0x141B36A80: db  10h
      +0x330 r @0x141B36A86: db 0F3h
      +0x330 r @0x141B36A87: db  0Fh
      +0x330 w @0x141B36A88: db  10h
      +0x330 w @0x141B36A9A: db 0F3h
      +0x330 w @0x141B36A9B: db  0Fh
      +0x340 r @0x141B36B0E: db 0F3h
      +0x340 r @0x141B36B0F: db  0Fh
      +0x340 w @0x141B36B10: db  10h
      +0x340 r @0x141B36B16: db 0F3h
      +0x340 r @0x141B36B17: db  0Fh
      +0x340 w @0x141B36B18: db  10h
      +0x340 w @0x141B36B2A: db 0F3h
      +0x340 w @0x141B36B2B: db  0Fh
      +0x350 r @0x141B36B9E: db 0F3h
      +0x350 r @0x141B36B9F: db  0Fh
      +0x350 w @0x141B36BA0: db  10h
      +0x350 r @0x141B36BA6: db 0F3h
      +0x350 r @0x141B36BA7: db  0Fh
      +0x350 w @0x141B36BA8: db  10h
      +0x350 w @0x141B36BBA: db 0F3h
      +0x350 w @0x141B36BBB: db  0Fh
      +0x360 r @0x141B36C2E: db 0F3h
      +0x360 r @0x141B36C2F: db  0Fh
      +0x360 w @0x141B36C30: db  10h
      +0x360 r @0x141B36C36: db 0F3h
      +0x360 r @0x141B36C37: db  0Fh
      +0x360 w @0x141B36C38: db  10h
      +0x360 w @0x141B36C4A: db 0F3h
      +0x360 w @0x141B36C4B: db  0Fh
      +0x364 r @0x141B36C52: db 0F3h
      +0x364 r @0x141B36C53: db  0Fh
      +0x364 w @0x141B36C54: db  10h
      +0x364 r @0x141B36C5A: db 0F3h
      +0x364 r @0x141B36C5B: db  0Fh
      +0x364 w @0x141B36C5C: db  10h
      +0x364 w @0x141B36C6E: db 0F3h
      +0x364 w @0x141B36C6F: db  0Fh
      +0x368 r @0x141B36C76: db 0F3h
      +0x368 r @0x141B36C77: db  0Fh
      +0x368 w @0x141B36C78: db  10h
      +0x368 r @0x141B36C7E: db 0F3h
      +0x368 r @0x141B36C7F: db  0Fh
      +0x368 w @0x141B36C80: db  10h
      +0x368 w @0x141B36C92: db 0F3h
      +0x368 w @0x141B36C93: db  0Fh
      +0x540 r @0x141B3790C: db 0F3h
      +0x540 r @0x141B3790D: db  0Fh
      +0x540 w @0x141B3790E: db  10h
      +0x540 r @0x141B37914: db 0F3h
      +0x540 r @0x141B37915: db  0Fh
      +0x540 w @0x141B37916: db  10h
      +0x540 w @0x141B37928: db 0F3h
      +0x540 w @0x141B37929: db  0Fh
      +0x544 r @0x141B37930: db 0F3h
      +0x544 r @0x141B37931: db  0Fh
      +0x544 w @0x141B37932: db  10h
      +0x544 r @0x141B37938: db 0F3h
      +0x544 r @0x141B37939: db  0Fh
      +0x544 w @0x141B3793A: db  10h
      +0x544 w @0x141B3794C: db 0F3h
      +0x544 w @0x141B3794D: db  0Fh
      +0x548 r @0x141B37954: db 0F3h
      +0x548 r @0x141B37955: db  0Fh
      +0x548 w @0x141B37956: db  10h
      +0x548 r @0x141B3795C: db 0F3h
      +0x548 r @0x141B3795D: db  0Fh
      +0x548 w @0x141B3795E: db  10h
      +0x548 w @0x141B37970: db 0F3h
      +0x548 w @0x141B37971: db  0Fh
      +0x54C r @0x141B37978: db 0F3h
      +0x54C r @0x141B37979: db  0Fh
      +0x54C w @0x141B3797A: db  10h
      +0x54C r @0x141B37980: db 0F3h
      +0x54C r @0x141B37981: db  0Fh
      +0x54C w @0x141B37982: db  10h
      +0x54C w @0x141B37994: db 0F3h
      +0x54C w @0x141B37995: db  0Fh
      +0x550 r @0x141B378EE: db  0Fh
      +0x550 w @0x141B378EF: db  10h
      +0x550 r @0x141B378F5: db  0Fh
      +0x550 w @0x141B378F6: db  10h
      +0x550 w @0x141B37905: db  0Fh
      +0x550 w @0x141B37906: db  11h

  *** BOTH FUNC 0x1449596D0 (rva 0x49596D0) sub_1449596D0
      +0x320 w @0x144959E90: db  4Dh ; M
      +0x320 w @0x144959E91: db  89h
      +0x320 r @0x144959EAE: db  48h ; H
      +0x320 r @0x144959EAF: db  8Bh
      +0x320 w @0x144959EB5: db  49h ; I
      +0x320 w @0x144959EB6: db  89h
      +0x328 w @0x144959E97: db  4Dh ; M
      +0x328 w @0x144959E98: db  89h
      +0x328 r @0x144959E9E: db  48h ; H
      +0x328 r @0x144959E9F: db  8Bh
      +0x328 r @0x144959EBC: db  48h ; H
      +0x328 r @0x144959EBD: db  8Bh
      +0x328 w @0x144959EC3: db  49h ; I
      +0x328 w @0x144959EC4: db  89h
      +0x330 r @0x144959ECA: db  0Fh
      +0x330 w @0x144959ED1: db  41h ; A
      +0x330 w @0x144959ED2: db  88h
      +0x340 r @0x144959EE6: db  49h ; I
      +0x340 r @0x144959EE7: db  8Dh
      +0x340 r @0x144959EED: db  48h ; H
      +0x340 r @0x144959EEE: db  8Dh
      +0x350 w @0x144959F00: db  4Dh ; M
      +0x350 w @0x144959F01: db  89h
      +0x350 r @0x144959F07: db  48h ; H
      +0x350 r @0x144959F08: db  8Bh
      +0x350 r @0x144959F25: db  48h ; H
      +0x350 r @0x144959F26: db  8Bh
      +0x350 w @0x144959F2C: db  49h ; I
      +0x350 w @0x144959F2D: db  89h
      +0x360 r @0x144959F41: db  48h ; H
      +0x360 r @0x144959F42: db  8Bh
      +0x360 w @0x144959F48: db  49h ; I
      +0x360 w @0x144959F49: db  89h
      +0x368 r @0x144959F4F: db  49h ; I
      +0x368 r @0x144959F50: db  8Dh
      +0x368 r @0x144959F56: db  48h ; H
      +0x368 r @0x144959F57: db  8Dh
      +0x550 r @0x14495A257: db  49h ; I
      +0x550 r @0x14495A258: db  8Dh
      +0x550 r @0x14495A25E: db  48h ; H
      +0x550 r @0x14495A25F: db  8Dh

  *** BOTH FUNC 0x145115700 (rva 0x5115700) sub_145115700
      +0x320 w @0x1451157AE: db  48h ; H
      +0x320 w @0x1451157AF: db  89h
      +0x328 w @0x1451157B5: db  48h ; H
      +0x328 w @0x1451157B6: db  89h
      +0x558 r @0x14511573C: db  48h ; H
      +0x558 r @0x14511573D: db  8Bh
      +0x558 w @0x145115768: db  48h ; H
      +0x558 w @0x145115769: db  89h

  *** BOTH FUNC 0x140C11710 (rva 0xC11710) sub_140C11710
      +0x320 w @0x140C12104: db  0Fh
      +0x320 w @0x140C12105: db  29h ; )
      +0x330 w @0x140C12110: db  0Fh
      +0x330 w @0x140C12111: db  29h ; )
      +0x340 w @0x140C1211F: db  0Fh
      +0x340 w @0x140C12120: db  29h ; )
      +0x340 r @0x140C124A1: db  48h ; H
      +0x340 r @0x140C124A2: db  8Dh
      +0x340 r @0x140C124B4: db  48h ; H
      +0x340 r @0x140C124B5: db  8Bh
      +0x340 r @0x140C124C2: db  48h ; H
      +0x340 r @0x140C124C3: db  8Dh
      +0x350 w @0x140C1212E: db  0Fh
      +0x350 w @0x140C1212F: db  29h ; )
      +0x360 w @0x140C1213D: db  0Fh
      +0x360 w @0x140C1213E: db  29h ; )
      +0x360 r @0x140C124F3: db  48h ; H
      +0x360 r @0x140C124F4: db  8Dh
      +0x540 w @0x140C12454: db  0Fh
      +0x540 w @0x140C12455: db  29h ; )
      +0x550 w @0x140C12462: db  0Fh
      +0x550 w @0x140C12463: db  29h ; )

  *** BOTH FUNC 0x14545D750 (rva 0x545D750) sub_14545D750
      +0x320 w @0x14545F2D6: movaps  [rbp+32F0h+var_2FD0], xmm11
      +0x320 r @0x14545F86A: movaps  xmm11, [rbp+32F0h+var_2FD0]
      +0x330 w @0x14545DA7D: movaps  [rbp+32F0h+var_2FC0], xmm1
      +0x330 r @0x14545DAC2: movaps  xmm1, [rbp+32F0h+var_2FC0]
      +0x340 w @0x14545DAAD: movaps  [rbp+32F0h+var_2FB0], xmm3
      +0x340 r @0x14545DACD: movaps  xmm0, [rbp+32F0h+var_2FB0]
      +0x350 w @0x14545FD90: movaps  [rbp+32F0h+var_2FA0], xmm0
      +0x350 r @0x14546009F: minps   xmm10, [rbp+32F0h+var_2FA0]
      +0x360 w @0x1454601BA: movaps  [rbp+32F0h+var_2F90], xmm2
      +0x360 r @0x14546026D: movaps  xmm0, [rbp+32F0h+var_2F90]
      +0x540 w @0x14545D905: movaps  [rbp+32F0h+var_2DB0], xmm6
      +0x550 w @0x14545D90C: movaps  [rbp+32F0h+var_2DA0], xmm6

  *** BOTH FUNC 0x140BEF760 (rva 0xBEF760) sub_140BEF760
      +0x340 w @0x140BEF7D0: db  49h ; I
      +0x340 w @0x140BEF7D1: db  89h
      +0x350 w @0x140BEF7BB: db  49h ; I
      +0x350 w @0x140BEF7BC: db  89h
      +0x360 w @0x140BEF7DE: db  49h ; I
      +0x360 w @0x140BEF7DF: db  89h
      +0x368 w @0x140BEF7E5: db  49h ; I
      +0x368 w @0x140BEF7E6: db  89h
      +0x540 w @0x140BEF977: mov     [r14+540h], rdi
      +0x548 w @0x140BEF97E: mov     [r14+548h], rdi
      +0x550 w @0x140BEF985: mov     [r14+550h], rdi
      +0x558 w @0x140BEF98C: mov     [r14+558h], rdi

  *** BOTH FUNC 0x1405EF780 (rva 0x5EF780) sub_1405EF780
      +0x320 w @0x1405F0523: mov     byte ptr [rbp+320h], 0
      +0x320 r @0x1405F0537: lea     rcx, [rbp+320h]
      +0x330 w @0x1405F0511: mov     [rbp+330h], rsi
      +0x340 w @0x1405F0543: mov     qword ptr [rbp+340h], 3
      +0x350 w @0x1405F0558: movdqu  xmmword ptr [rbp+350h], xmm0
      +0x360 w @0x1405F0572: mov     byte ptr [rbp+360h], 0
      +0x360 r @0x1405F0586: lea     rcx, [rbp+360h]
      +0x540 w @0x1405EF7E1: mov     [r15+540h], rsi
      +0x540 w @0x1405F08E3: db  89h
      +0x544 w @0x1405F08EC: db  89h
      +0x548 w @0x1405EF7E8: mov     [r15+548h], rsi
      +0x548 w @0x1405F08F4: db  43h ; C
      +0x548 w @0x1405F08F5: db 0F0h
      +0x548 w @0x1405F08F6: db  88h
      +0x550 r @0x1405EF7EF: lea     rbx, [r15+550h]
      +0x550 w @0x1405F08FF: db 0F3h
      +0x550 w @0x1405F0900: db  0Fh
      +0x550 w @0x1405F091A: db  48h ; H
      +0x550 w @0x1405F091B: db  89h
      +0x558 w @0x1405F0921: db  48h ; H
      +0x558 w @0x1405F0922: db  89h

  *** BOTH FUNC 0x1402BF810 (rva 0x2BF810) sub_1402BF810
      +0x320 w @0x1402BFF92: mov     [rbp+320h], bl
      +0x320 r @0x1402BFFA3: lea     rcx, [rbp+320h]
      +0x330 w @0x1402BFF80: mov     [rbp+330h], rbx
      +0x340 w @0x1402BFFC2: mov     [rbp+340h], bl
      +0x340 r @0x1402BFFD3: lea     rcx, [rbp+340h]
      +0x350 w @0x1402BFFB0: mov     [rbp+350h], rbx
      +0x360 w @0x1402BFFF2: mov     [rbp+360h], bl
      +0x360 r @0x1402C0003: lea     rcx, [rbp+360h]
      +0x540 w @0x1402C02C2: mov     [rbp+540h], bl
      +0x540 r @0x1402C02D3: lea     rcx, [rbp+540h]
      +0x550 w @0x1402C02B0: mov     [rbp+550h], rbx
      +0x558 w @0x1402C02B7: mov     qword ptr [rbp+558h], 0Fh

  *** BOTH FUNC 0x141229840 (rva 0x1229840) sub_141229840
      +0x320 w @0x141229C6E: db  41h ; A
      +0x320 w @0x141229C6F: db  80h
      +0x328 r @0x141229C76: db  49h ; I
      +0x328 r @0x141229C77: db  8Dh
      +0x350 w @0x141229C9A: db  4Dh ; M
      +0x350 w @0x141229C9B: db  89h
      +0x360 w @0x141229CA8: db  4Dh ; M
      +0x360 w @0x141229CA9: db  89h
      +0x368 w @0x141229CAF: db  4Dh ; M
      +0x368 w @0x141229CB0: db  89h
      +0x540 w @0x141229EB1: db  45h ; E
      +0x540 w @0x141229EB2: db  89h
      +0x548 r @0x141229EB8: db  49h ; I
      +0x548 r @0x141229EB9: db  8Dh

  *** BOTH FUNC 0x143775890 (rva 0x3775890) sub_143775890
      +0x328 w @0x14377598E: db  41h ; A
      +0x328 w @0x14377598F: db  83h
      +0x540 r @0x14377595A: db  49h ; I
      +0x540 r @0x14377595B: db  8Bh
      +0x540 r @0x143775987: db  49h ; I
      +0x540 r @0x143775988: db  8Bh
      +0x540 r @0x1437759EC: db  49h ; I
      +0x540 r @0x1437759ED: db  8Bh
      +0x540 r @0x143775A1D: db  49h ; I
      +0x540 r @0x143775A1E: db  8Bh

  *** BOTH FUNC 0x140B418D0 (rva 0xB418D0) sub_140B418D0
      +0x320 w @0x140B4278D: db  4Ch ; L
      +0x320 w @0x140B4278E: db  89h
      +0x328 w @0x140B427D2: db  4Ch ; L
      +0x328 w @0x140B427D3: db  89h
      +0x330 w @0x140B427D9: db  48h ; H
      +0x330 w @0x140B427DA: db  89h
      +0x350 r @0x140B420B0: db  48h ; H
      +0x350 r @0x140B420B1: db  8Bh
      +0x540 r @0x140B42757: db  48h ; H
      +0x540 r @0x140B42758: db  8Bh
      +0x540 r @0x140B42873: db  48h ; H
      +0x540 r @0x140B42874: db  4Ch ; L
      +0x540 r @0x140B42875: db  8Bh
      +0x550 w @0x140B41912: db  44h ; D
      +0x550 w @0x140B41913: db  89h
      +0x550 r @0x140B4288B: db  8Bh
      +0x550 w @0x140B429AC: db  48h ; H
      +0x550 w @0x140B429AD: db  89h
      +0x550 w @0x140B42A79: db  89h
      +0x550 w @0x140B42B7C: db  89h
      +0x558 w @0x140B41B6E: db  4Ch ; L
      +0x558 w @0x140B41B6F: db  89h
      +0x558 w @0x140B41B82: db  48h ; H
      +0x558 w @0x140B41B83: db  89h
      +0x558 r @0x140B41B93: db  4Ch ; L
      +0x558 r @0x140B41B94: db  8Dh
      +0x558 r @0x140B41BA2: db  48h ; H
      +0x558 r @0x140B41BA3: db  8Bh

  *** BOTH FUNC 0x1447E7990 (rva 0x47E7990) sub_1447E7990
      +0x320 w @0x1447E916D: db  48h ; H
      +0x320 w @0x1447E916E: db  83h
      +0x328 r @0x1447E8B48: db  48h ; H
      +0x328 r @0x1447E8B49: db  8Dh
      +0x328 r @0x1447E9196: db  48h ; H
      +0x328 r @0x1447E9197: db  8Dh
      +0x328 r @0x1447E91A5: db  48h ; H
      +0x328 r @0x1447E91A6: db  0Fh
      +0x340 w @0x1447E919D: db  48h ; H
      +0x340 w @0x1447E919E: db  83h
      +0x360 w @0x1447E91CD: db  48h ; H
      +0x360 w @0x1447E91CE: db  83h
      +0x368 r @0x1447E8BD6: db  48h ; H
      +0x368 r @0x1447E8BD7: db  8Dh
      +0x368 r @0x1447E91F6: db  48h ; H
      +0x368 r @0x1447E91F7: db  8Dh
      +0x368 r @0x1447E9205: db  48h ; H
      +0x368 r @0x1447E9206: db  0Fh
      +0x540 r @0x1447E8AEF: db  48h ; H
      +0x540 r @0x1447E8AF0: db  8Dh
      +0x540 r @0x1447E8B1D: db  48h ; H
      +0x540 r @0x1447E8B1E: db  8Bh
      +0x540 r @0x1447E8B24: db  48h ; H
      +0x540 r @0x1447E8B25: db  8Dh
      +0x558 r @0x1447E8B0D: db  4Ch ; L
      +0x558 r @0x1447E8B0E: db  8Bh

  *** BOTH FUNC 0x1446CD9D0 (rva 0x46CD9D0) sub_1446CD9D0
      +0x320 r @0x1446CDC2B: db  48h ; H
      +0x320 r @0x1446CDC2C: db  8Dh
      +0x340 r @0x1446CDC52: db  48h ; H
      +0x340 r @0x1446CDC53: db  8Dh
      +0x360 w @0x1446CDC79: db 0C6h
      +0x368 r @0x1446CDC80: db  48h ; H
      +0x368 r @0x1446CDC81: db  8Dh
      +0x548 r @0x1446CDEF5: db  48h ; H
      +0x548 r @0x1446CDEF6: db  8Dh

  *** BOTH FUNC 0x14058FA20 (rva 0x58FA20) sub_14058FA20
      +0x320 w @0x140591048: mov     [rbp+320h], r15
      +0x320 w @0x14059108B: mov     [rbp+320h], rbx
      +0x328 r @0x140591099: lea     rcx, [rbp+328h]
      +0x328 r @0x1405912DE: lea     rax, [rbp+328h]
      +0x330 w @0x140591092: mov     [rbp+330h], r15
      +0x330 w @0x1405910D5: mov     [rbp+330h], rbx
      +0x340 w @0x1405910DC: mov     [rbp+340h], r15
      +0x340 w @0x14059111F: mov     [rbp+340h], rbx
      +0x350 w @0x140591126: mov     [rbp+350h], r15
      +0x350 w @0x140591169: mov     [rbp+350h], rbx
      +0x360 w @0x140591170: mov     [rbp+360h], r15
      +0x360 w @0x1405911AA: mov     [rbp+360h], rbx
      +0x368 r @0x1405911B8: lea     rcx, [rbp+368h]
      +0x368 r @0x14059130E: lea     rax, [rbp+368h]
      +0x540 w @0x14058FBDC: db  41h ; A
      +0x540 w @0x14058FBDD: db  80h
      +0x540 r @0x14058FD5C: db  41h ; A
      +0x540 r @0x14058FD5D: db  0Fh
      +0x540 w @0x14058FD5E: db  10h
      +0x540 w @0x140591714: cmp     [r14+540h], dil
      +0x544 r @0x14058FBF7: db  41h ; A
      +0x544 r @0x14058FBF8: db  0Fh
      +0x548 r @0x14059181F: lea     rcx, [rbp+548h]
      +0x548 r @0x14059186D: lea     rax, [rbp+548h]
      +0x550 r @0x14058FD6B: db  41h ; A
      +0x550 r @0x14058FD6C: db  0Fh
      +0x550 w @0x14058FD6D: db  10h
      +0x550 w @0x14059179B: cmp     dword ptr [r14+550h], 1
      +0x558 r @0x14059182F: lea     rcx, [rbp+558h]
      +0x558 r @0x140591879: lea     r9, [rbp+558h]

  *** BOTH FUNC 0x1418D1A70 (rva 0x18D1A70) sub_1418D1A70
      +0x320 w @0x1418D304B: db  4Ch ; L
      +0x320 w @0x1418D304C: db  89h
      +0x328 w @0x1418D3052: db  48h ; H
      +0x328 w @0x1418D3053: db 0C7h
      +0x330 r @0x1418D2025: db  48h ; H
      +0x330 r @0x1418D2026: db  8Dh
      +0x330 w @0x1418D30D9: db 0C6h
      +0x330 r @0x1418D30ED: db  48h ; H
      +0x330 r @0x1418D30EE: db  8Dh
      +0x330 r @0x1418D3105: db  4Ch ; L
      +0x330 r @0x1418D3106: db  8Dh
      +0x340 w @0x1418D30C7: db  4Ch ; L
      +0x340 w @0x1418D30C8: db  89h
      +0x350 r @0x1418D21B3: db  48h ; H
      +0x350 r @0x1418D21B4: db  8Dh
      +0x350 w @0x1418D3231: db 0C6h
      +0x350 r @0x1418D3245: db  48h ; H
      +0x350 r @0x1418D3246: db  8Dh
      +0x350 r @0x1418D325D: db  4Ch ; L
      +0x350 r @0x1418D325E: db  8Dh
      +0x360 w @0x1418D321F: db  4Ch ; L
      +0x360 w @0x1418D3220: db  89h
      +0x368 w @0x1418D3226: db  48h ; H
      +0x368 w @0x1418D3227: db 0C7h
      +0x540 r @0x1418D1AF0: db  48h ; H
      +0x540 r @0x1418D1AF1: db  8Dh
      +0x540 r @0x1418D1BBE: db  48h ; H
      +0x540 r @0x1418D1BBF: db  8Bh
      +0x558 r @0x1418D1BAE: db  48h ; H
      +0x558 r @0x1418D1BAF: db  8Bh

  *** BOTH FUNC 0x1418BFA80 (rva 0x18BFA80) sub_1418BFA80
      +0x364 w @0x1418BFBEB: db  66h ; f
      +0x364 w @0x1418BFBEC: db  89h
      +0x540 w @0x1418BFB18: db  0Fh
      +0x540 w @0x1418BFB19: db  11h
      +0x550 w @0x1418BFB26: db  0Fh
      +0x550 w @0x1418BFB27: db  11h

  *** BOTH FUNC 0x144D53A80 (rva 0x4D53A80) sub_144D53A80
      +0x320 w @0x144D53B72: db  4Ch ; L
      +0x320 w @0x144D53B73: db  89h
      +0x320 w @0x144D53B8B: db  44h ; D
      +0x320 w @0x144D53B8C: db  88h
      +0x330 w @0x144D53B79: db  4Ch ; L
      +0x330 w @0x144D53B7A: db  89h
      +0x340 w @0x144D53B92: db  4Ch ; L
      +0x340 w @0x144D53B93: db  89h
      +0x340 w @0x144D53BAB: db  44h ; D
      +0x340 w @0x144D53BAC: db  88h
      +0x350 w @0x144D53B99: db  4Ch ; L
      +0x350 w @0x144D53B9A: db  89h
      +0x360 w @0x144D53BB2: db  0Fh
      +0x360 w @0x144D53BB3: db  11h
      +0x540 w @0x144D53D2B: db  4Ch ; L
      +0x540 w @0x144D53D2C: db  89h
      +0x540 w @0x144D53D44: db  44h ; D
      +0x540 w @0x144D53D45: db  88h
      +0x550 w @0x144D53D32: db  4Ch ; L
      +0x550 w @0x144D53D33: db  89h
      +0x558 w @0x144D53D39: db  48h ; H
      +0x558 w @0x144D53D3A: db 0C7h

  *** BOTH FUNC 0x142CD3AC0 (rva 0x2CD3AC0) sub_142CD3AC0
      +0x320 w @0x142CD3DB9: db  48h ; H
      +0x320 w @0x142CD3DBA: db  89h
      +0x328 w @0x142CD3DC0: db  48h ; H
      +0x328 w @0x142CD3DC1: db  89h
      +0x330 w @0x142CD3DC7: db  88h
      +0x340 w @0x142CD3DD4: db  48h ; H
      +0x340 w @0x142CD3DD5: db  89h
      +0x350 w @0x142CD3DE2: db  48h ; H
      +0x350 w @0x142CD3DE3: db  89h
      +0x360 w @0x142CD3DF0: db  48h ; H
      +0x360 w @0x142CD3DF1: db  89h
      +0x368 w @0x142CD3DF7: db 0C7h
      +0x540 r @0x142CD4323: db  48h ; H
      +0x540 r @0x142CD4324: db  8Dh

  *** BOTH FUNC 0x1446CFAE0 (rva 0x46CFAE0) sub_1446CFAE0
      +0x320 r @0x1446CFCD7: db  48h ; H
      +0x320 r @0x1446CFCD8: db  8Dh
      +0x320 r @0x1446CFCDE: db  48h ; H
      +0x320 r @0x1446CFCDF: db  8Dh
      +0x340 r @0x1446CFCFD: db  48h ; H
      +0x340 r @0x1446CFCFE: db  8Dh
      +0x340 r @0x1446CFD04: db  48h ; H
      +0x340 r @0x1446CFD05: db  8Dh
      +0x360 r @0x1446CFD23: db  0Fh
      +0x360 w @0x1446CFD38: db  88h
      +0x368 r @0x1446CFD2A: db  48h ; H
      +0x368 r @0x1446CFD2B: db  8Dh
      +0x368 r @0x1446CFD31: db  48h ; H
      +0x368 r @0x1446CFD32: db  8Dh
      +0x548 r @0x1446CFFC1: db  48h ; H
      +0x548 r @0x1446CFFC2: db  8Dh
      +0x548 r @0x1446CFFC8: db  48h ; H
      +0x548 r @0x1446CFFC9: db  8Dh

  *** BOTH FUNC 0x140C07B00 (rva 0xC07B00) sub_140C07B00
      +0x320 w @0x140C0847F: db  44h ; D
      +0x320 w @0x140C08480: db  0Fh
      +0x320 w @0x140C08481: db  29h ; )
      +0x330 w @0x140C084C3: db  44h ; D
      +0x330 w @0x140C084C4: db  0Fh
      +0x330 w @0x140C084C5: db  29h ; )
      +0x340 w @0x140C08504: db  0Fh
      +0x340 w @0x140C08505: db  29h ; )
      +0x340 r @0x140C0889E: db  48h ; H
      +0x340 r @0x140C0889F: db  8Dh
      +0x340 r @0x140C088B1: db  48h ; H
      +0x340 r @0x140C088B2: db  8Bh
      +0x340 r @0x140C088BF: db  48h ; H
      +0x340 r @0x140C088C0: db  8Dh
      +0x350 w @0x140C0850F: db  0Fh
      +0x350 w @0x140C08510: db  29h ; )
      +0x360 w @0x140C0851D: db  0Fh
      +0x360 w @0x140C0851E: db  29h ; )
      +0x360 r @0x140C088F0: db  48h ; H
      +0x360 r @0x140C088F1: db  8Dh
      +0x540 w @0x140C08822: db  0Fh
      +0x540 w @0x140C08823: db  29h ; )
      +0x550 w @0x140C08846: db  0Fh
      +0x550 w @0x140C08847: db  29h ; )

  *** BOTH FUNC 0x1411CDB50 (rva 0x11CDB50) sub_1411CDB50
      +0x320 w @0x1411CE934: db  0Fh
      +0x320 w @0x1411CE935: db  29h ; )
      +0x330 w @0x1411CE942: db  0Fh
      +0x330 w @0x1411CE943: db  29h ; )
      +0x340 w @0x1411CE950: db  0Fh
      +0x340 w @0x1411CE951: db  29h ; )
      +0x350 w @0x1411CE747: movaps  xmmword ptr [rbp+350h], xmm0
      +0x360 w @0x1411CE755: movaps  xmmword ptr [rbp+360h], xmm0
      +0x368 r @0x1411CE607: mov     rcx, [rsi+368h]
      +0x544 r @0x1411CE9D9: db 0F3h
      +0x544 r @0x1411CE9DA: db  41h ; A
      +0x544 r @0x1411CE9DB: db  0Fh

  *** BOTH FUNC 0x1409E3B80 (rva 0x9E3B80) sub_1409E3B80
      +0x320 w @0x1409E3E85: mov     [rsi+320h], r15
      +0x320 r @0x1409E5C94: lea     rcx, [rsi+320h]
      +0x320 r @0x1409E61ED: mov     rcx, [rsi+320h]
      +0x320 r @0x1409E6217: mov     rcx, [rsi+320h]
      +0x328 w @0x1409E3E8C: mov     [rsi+328h], r15
      +0x328 r @0x1409E67CC: lea     rcx, [rbp+328h]
      +0x330 w @0x1409E3E93: mov     [rsi+330h], r15
      +0x330 w @0x1409E5C3A: mov     [rsi+330h], rax
      +0x340 w @0x1409E3EA1: mov     [rsi+340h], r15
      +0x340 w @0x1409E5CF2: mov     [rsi+340h], rax
      +0x350 w @0x1409E3EAF: mov     [rsi+350h], r15
      +0x350 w @0x1409E5DB2: mov     [rsi+350h], rax
      +0x360 w @0x1409E3EBD: mov     [rsi+360h], r15
      +0x360 r @0x1409E5DE0: lea     rcx, [rsi+360h]
      +0x360 r @0x1409E5FBF: mov     rcx, [rsi+360h]
      +0x360 r @0x1409E5FE8: mov     rcx, [rsi+360h]
      +0x368 w @0x1409E3EC4: mov     [rsi+368h], r15
      +0x368 r @0x1409E68B7: db  48h ; H
      +0x368 r @0x1409E68B8: db  8Dh
      +0x540 w @0x1409E4801: mov     [rbp+540h], r15
      +0x540 w @0x1409E4822: mov     [rbp+540h], rax
      +0x540 r @0x1409E482F: lea     rdx, [rbp+540h]
      +0x540 r @0x1409E4840: lea     rcx, [rbp+540h]
      +0x548 r @0x1409E480F: lea     rcx, [rbp+548h]
      +0x550 w @0x1409E3F9B: mov     [rsi+550h], r15
      +0x558 r @0x1409E3FA2: lea     rcx, [rsi+558h]
      +0x558 r @0x1409E528E: lea     rcx, [rsi+558h]

  *** BOTH FUNC 0x145459C10 (rva 0x5459C10) sub_145459C10
      +0x320 w @0x14545B77E: movaps  [rbp+3CD0h+var_39B0], xmm4
      +0x320 r @0x14545B7C9: movaps  xmm0, [rbp+3CD0h+var_39B0]
      +0x330 w @0x14545B11B: movaps  [rbp+3CD0h+var_39A0], xmm4
      +0x330 r @0x14545B7F2: movaps  xmm4, [rbp+3CD0h+var_39A0]
      +0x340 w @0x14545B129: movaps  [rbp+3CD0h+var_3990], xmm5
      +0x340 r @0x14545B7F9: movaps  xmm5, [rbp+3CD0h+var_3990]
      +0x350 w @0x14545B10B: movdqa  [rbp+3CD0h+var_3980], xmm0
      +0x350 r @0x14545B7EA: movdqa  xmm0, [rbp+3CD0h+var_3980]
      +0x360 w @0x14545B138: movaps  [rbp+3CD0h+var_3970], xmm6
      +0x360 r @0x14545B800: movaps  xmm6, [rbp+3CD0h+var_3970]
      +0x540 w @0x14545C49C: movdqa  [rbp+3CD0h+var_3790], xmm0
      +0x540 r @0x14545CDC0: movdqa  xmm0, [rbp+3CD0h+var_3790]
      +0x550 w @0x14545C4D2: movaps  [rbp+3CD0h+var_3780], xmm0
      +0x550 r @0x14545CDF3: movaps  xmm3, [rbp+3CD0h+var_3780]

  *** BOTH FUNC 0x140A5DCB0 (rva 0xA5DCB0) sub_140A5DCB0
      +0x364 w @0x140A5DDF4: mov     dword ptr [rbp+364h], 0Dh
      +0x540 r @0x140A5DD2C: lea     rdx, [rbp+540h]
      +0x540 r @0x140A5DD5D: mov     rcx, [rbp+540h]

  *** BOTH FUNC 0x140587D00 (rva 0x587D00) sub_140587D00
      +0x320 r @0x14058975E: lea     rdx, [rbp+320h]
      +0x320 r @0x140589A0F: lea     rax, [rbp+320h]
      +0x328 w @0x14058976F: cmp     qword ptr [rbp+328h], 0
      +0x328 r @0x140589D96: mov     rcx, [rbp+328h]
      +0x330 w @0x140588567: call    qword ptr [rax+330h]
      +0x330 r @0x140588C05: lea     rdx, [rbp+330h]
      +0x330 r @0x140589548: lea     rdx, [rbp+330h]
      +0x330 r @0x140589931: lea     rdx, [rbp+330h]
      +0x340 r @0x140588B2B: lea     rdx, [rbp+340h]
      +0x340 r @0x140589535: lea     rdx, [rbp+340h]
      +0x340 r @0x14058991E: lea     rdx, [rbp+340h]
      +0x350 r @0x140588B0B: lea     rdx, [rbp+350h]
      +0x350 r @0x140589581: lea     rdx, [rbp+350h]
      +0x350 r @0x14058996A: lea     rdx, [rbp+350h]
      +0x360 r @0x140588ACB: lea     rdx, [rbp+360h]
      +0x360 r @0x1405895BA: lea     rdx, [rbp+360h]
      +0x360 r @0x1405899A3: lea     rdx, [rbp+360h]
      +0x368 r @0x140589EA1: mov     rcx, [rbp+368h]
      +0x540 r @0x1405899BD: lea     rcx, [rbp+540h]
      +0x540 r @0x140589B04: lea     rax, [rbp+540h]
      +0x550 r @0x1405899D0: lea     rcx, [rbp+550h]
      +0x550 r @0x140589B10: lea     r9, [rbp+550h]

  *** BOTH FUNC 0x1418D3D10 (rva 0x18D3D10) sub_1418D3D10
      +0x320 w @0x1418D4F51: db  4Ch ; L
      +0x320 w @0x1418D4F52: db  89h
      +0x328 w @0x1418D4F58: db  48h ; H
      +0x328 w @0x1418D4F59: db 0C7h
      +0x330 w @0x1418D4FDD: db 0C6h
      +0x330 r @0x1418D4FF1: db  48h ; H
      +0x330 r @0x1418D4FF2: db  8Dh
      +0x330 r @0x1418D5009: db  4Ch ; L
      +0x330 r @0x1418D500A: db  8Dh
      +0x340 w @0x1418D4FCB: db  4Ch ; L
      +0x340 w @0x1418D4FCC: db  89h
      +0x350 w @0x1418D5057: db 0C6h
      +0x350 r @0x1418D506B: db  48h ; H
      +0x350 r @0x1418D506C: db  8Dh
      +0x350 r @0x1418D5083: db  4Ch ; L
      +0x350 r @0x1418D5084: db  8Dh
      +0x360 w @0x1418D5045: db  4Ch ; L
      +0x360 w @0x1418D5046: db  89h
      +0x368 w @0x1418D504C: db  48h ; H
      +0x368 w @0x1418D504D: db 0C7h
      +0x540 w @0x1418D401E: db  48h ; H
      +0x540 w @0x1418D401F: db  89h
      +0x540 r @0x1418D40AF: db  4Ch ; L
      +0x540 r @0x1418D40B0: db  8Dh
      +0x548 w @0x1418D4025: db 0C7h
      +0x54C w @0x1418D4052: db 0C7h
      +0x550 r @0x1418D4034: db  48h ; H
      +0x550 r @0x1418D4035: db  8Dh
      +0x550 r @0x1418D40FB: db  48h ; H
      +0x550 r @0x1418D40FC: db  8Bh

  *** BOTH FUNC 0x141033D80 (rva 0x1033D80) sub_141033D80
      +0x320 w @0x141034960: mov     [rbp+320h], rax
      +0x320 r @0x141034985: lea     r8, [rbp+320h]
      +0x320 r @0x1410349A0: mov     rcx, [rbp+320h]
      +0x320 r @0x1410349B8: mov     rcx, [rbp+320h]
      +0x328 w @0x141034970: mov     dword ptr [rbp+328h], 5
      +0x32C w @0x14103497A: mov     qword ptr [rbp+32Ch], 4
      +0x340 w @0x141034A3C: mov     dword ptr [rbp+340h], 1
      +0x350 w @0x141034D61: mov     [rbp+350h], rax
      +0x350 r @0x141034D86: lea     r8, [rbp+350h]
      +0x350 r @0x141034DA1: mov     rcx, [rbp+350h]
      +0x350 r @0x141034DB9: mov     rcx, [rbp+350h]
      +0x368 w @0x141034E05: mov     [rbp+368h], rax
      +0x368 r @0x141034E2A: lea     r8, [rbp+368h]
      +0x368 r @0x141034E45: mov     rcx, [rbp+368h]
      +0x368 r @0x141034E5D: mov     rcx, [rbp+368h]
      +0x548 r @0x1410364CF: lea     rdx, [rbp+548h]
      +0x54C r @0x141036543: lea     rdx, [rbp+54Ch]
      +0x550 r @0x1410365B7: lea     rdx, [rbp+550h]
      +0x554 r @0x14103662B: lea     rdx, [rbp+554h]
      +0x558 r @0x1410366AB: lea     rdx, [rbp+558h]
      +0x55C r @0x1410368B7: lea     rdx, [rbp+55Ch]

  *** BOTH FUNC 0x140C0BDA0 (rva 0xC0BDA0) sub_140C0BDA0
      +0x320 w @0x140C0C6A3: movaps  xmmword ptr [rbp+320h], xmm11
      +0x330 w @0x140C0C6AB: movaps  xmmword ptr [rbp+330h], xmm13
      +0x340 w @0x140C0C6B3: movaps  xmmword ptr [rbp+340h], xmm12
      +0x340 r @0x140C0CB83: lea     rcx, [r14+340h]
      +0x340 r @0x140C0CB96: mov     rax, [r14+340h]
      +0x340 r @0x140C0CBA4: lea     rcx, [r14+340h]
      +0x350 w @0x140C0C6BB: movaps  xmmword ptr [rbp+350h], xmm14
      +0x360 w @0x140C0C6FF: movaps  xmmword ptr [rbp+360h], xmm8
      +0x360 r @0x140C0CBD5: lea     rcx, [r14+360h]
      +0x540 w @0x140C0CA79: movaps  xmmword ptr [rbp+540h], xmm0
      +0x550 w @0x140C0CA84: movaps  xmmword ptr [rbp+550h], xmm4

  *** BOTH FUNC 0x1412D7DB0 (rva 0x12D7DB0) sub_1412D7DB0
      +0x320 w @0x1412D7F71: mov     dword ptr [rbp+6A0h+var_380], eax
      +0x320 r @0x1412D7FC3: movaps  xmm2, [rbp+6A0h+var_380]
      +0x324 w @0x1412D7F77: mov     dword ptr [rbp+6A0h+var_380+4], 1Ah
      +0x330 r @0x1412D7FD1: movaps  xmm1, [rbp+6A0h+var_370]
      +0x340 r @0x1412D7FDF: movaps  xmm0, xmmword ptr [rbp+340h]
      +0x350 r @0x1412D7FED: mov     eax, [rbp+350h]
      +0x364 w @0x1412D7FD8: movups  [rbp+6A0h+var_33C], xmm1
      +0x548 w @0x1412D8389: movups  [rbp+6A0h+var_158], xmm0
      +0x558 w @0x1412D8396: mov     [rbp+6A0h+var_148], eax
      +0x55C w @0x1412D83A3: movups  [rbp+6A0h+var_144], xmm0
      +0x55C w @0x1412D83DE: mov     dword ptr [rbp+6A0h+var_144], eax

  *** BOTH FUNC 0x1407A9DD0 (rva 0x7A9DD0) sub_1407A9DD0
      +0x320 w @0x1407A9E29: call    qword ptr [rax+320h]
      +0x540 r @0x1407A9E6D: movups  xmm0, xmmword ptr [rbx+540h]
      +0x550 w @0x1407A9FF2: mov     [rbx+550h], rcx
      +0x558 w @0x1407A9FFD: mov     [rbx+558h], rcx

  *** BOTH FUNC 0x140583E30 (rva 0x583E30) sub_140583E30
      +0x320 r @0x14058468A: lea     rax, [rbp+320h]
      +0x320 w @0x1405846AA: mov     byte ptr [rbp+320h], 0
      +0x320 r @0x1405846BE: lea     rcx, [rbp+320h]
      +0x320 r @0x14058474B: lea     r9, [rbp+320h]
      +0x330 w @0x140584698: mov     [rbp+330h], r13
      +0x340 r @0x1405846CB: lea     rax, [rbp+340h]
      +0x340 w @0x1405846EB: mov     byte ptr [rbp+340h], 0
      +0x340 r @0x1405846FF: lea     rcx, [rbp+340h]
      +0x340 r @0x140584752: lea     r8, [rbp+340h]
      +0x350 w @0x1405846D9: mov     [rbp+350h], r13
      +0x360 w @0x14058471E: mov     byte ptr [rbp+360h], 0
      +0x360 r @0x140584732: lea     rcx, [rbp+360h]
      +0x360 r @0x140584759: lea     rdx, [rbp+360h]
      +0x550 r @0x140585401: lea     rcx, [rbp+550h]
      +0x550 r @0x1405854AA: lea     rcx, [rbp+550h]

  *** BOTH FUNC 0x1453FBE60 (rva 0x53FBE60) sub_1453FBE60
      +0x320 w @0x1453FBFF9: db 0C5h
      +0x320 w @0x1453FBFFB: db  11h
      +0x320 r @0x1453FC09C: db 0C5h
      +0x320 w @0x1453FC09E: db  10h
      +0x320 w @0x1453FC5EA: db 0C5h
      +0x320 w @0x1453FC5EC: db  11h
      +0x340 w @0x1453FBFDA: db 0C5h
      +0x340 w @0x1453FBFDC: db  11h
      +0x340 r @0x1453FC0AF: db 0C5h
      +0x340 w @0x1453FC0B1: db  10h
      +0x340 w @0x1453FC5F2: db 0C5h
      +0x340 w @0x1453FC5F4: db  11h
      +0x340 r @0x1453FC813: db 0C5h
      +0x340 w @0x1453FC815: db  10h
      +0x360 w @0x1453FBFBE: db 0C5h
      +0x360 w @0x1453FBFC0: db  11h
      +0x360 r @0x1453FC089: db 0C5h
      +0x360 w @0x1453FC08B: db  10h
      +0x360 w @0x1453FC5FA: db 0C5h
      +0x360 w @0x1453FC5FC: db  11h
      +0x360 r @0x1453FC806: db 0C5h
      +0x360 w @0x1453FC808: db  10h
      +0x550 r @0x1453FBEDE: db  48h ; H
      +0x550 r @0x1453FBEDF: db  8Bh
      +0x558 r @0x1453FBEBB: db  48h ; H
      +0x558 r @0x1453FBEBC: db  8Bh

  *** BOTH FUNC 0x140C4FE80 (rva 0xC4FE80) sub_140C4FE80
      +0x320 r @0x140C50263: movss   xmm0, dword ptr [rsi+320h]
      +0x320 w @0x140C5028F: mov     [rsi+320h], ecx
      +0x324 r @0x140C50295: movss   xmm0, dword ptr [rsi+324h]
      +0x324 w @0x140C502C1: mov     [rsi+324h], ecx
      +0x328 r @0x140C502C7: movss   xmm0, dword ptr [rsi+328h]
      +0x328 w @0x140C502F3: mov     [rsi+328h], ecx
      +0x32C r @0x140C502F9: movss   xmm0, dword ptr [rsi+32Ch]
      +0x32C w @0x140C50325: mov     [rsi+32Ch], ecx
      +0x330 r @0x140C50032: movss   xmm0, dword ptr [rsi+330h]
      +0x330 w @0x140C5005E: mov     [rsi+330h], ecx
      +0x340 r @0x140C54CF3: movss   xmm0, dword ptr [rsi+340h]
      +0x340 w @0x140C54D1F: mov     [rsi+340h], ecx
      +0x360 r @0x140C525C5: movss   xmm0, dword ptr [rsi+360h]
      +0x360 w @0x140C525F1: mov     [rsi+360h], ecx
      +0x364 r @0x140C525F7: movss   xmm0, dword ptr [rsi+364h]
      +0x364 w @0x140C52623: mov     [rsi+364h], ecx
      +0x368 r @0x140C52629: movss   xmm0, dword ptr [rsi+368h]
      +0x368 w @0x140C52655: mov     [rsi+368h], ecx
      +0x540 r @0x140C552FE: movss   xmm0, dword ptr [rsi+540h]
      +0x540 w @0x140C5532A: mov     [rsi+540h], ecx
      +0x544 r @0x140C515A7: movss   xmm6, dword ptr [rsi+544h]
      +0x544 w @0x140C515C6: mov     [rsi+544h], ecx
      +0x544 r @0x140C5163C: movss   xmm0, dword ptr [rsi+544h]
      +0x544 w @0x140C5165B: mov     [rsi+544h], ecx
      +0x548 r @0x140C5168C: movss   xmm0, dword ptr [rsi+548h]
      +0x548 w @0x140C516B8: mov     [rsi+548h], ecx
      +0x550 r @0x140C51E3B: movss   xmm0, dword ptr [rsi+550h]
      +0x550 w @0x140C51E67: mov     [rsi+550h], ecx
      +0x554 r @0x140C51E6D: movss   xmm0, dword ptr [rsi+554h]
      +0x554 w @0x140C51E99: mov     [rsi+554h], ecx
      +0x558 r @0x140C51E9F: movss   xmm0, dword ptr [rsi+558h]
      +0x558 w @0x140C51ECB: mov     [rsi+558h], ecx
      +0x55C r @0x140C51ED1: movss   xmm0, dword ptr [rsi+55Ch]
      +0x55C w @0x140C51EFD: mov     [rsi+55Ch], ecx

  *** BOTH FUNC 0x140BB1EE0 (rva 0xBB1EE0) sub_140BB1EE0
      +0x320 w @0x140BB23E6: movaps  xmmword ptr [rbp+320h], xmm3
      +0x330 w @0x140BB23F1: movaps  xmmword ptr [rbp+330h], xmm4
      +0x340 w @0x140BB23FC: movaps  xmmword ptr [rbp+340h], xmm6
      +0x350 w @0x140BB2579: movaps  xmmword ptr [rbp+350h], xmm2
      +0x360 w @0x140BB2584: movaps  xmmword ptr [rbp+360h], xmm3
      +0x540 w @0x140BB3996: movups  xmmword ptr [rbp+rdi+540h], xmm1
      +0x540 w @0x140BB3F2B: movss   dword ptr [rbp+rax*8+540h], xmm0
      +0x544 w @0x140BB3F39: movss   dword ptr [rbp+rax*8+544h], xmm1
      +0x548 w @0x140BB3F47: movss   dword ptr [rbp+rax*8+548h], xmm0
      +0x54C w @0x140BB3F55: movss   dword ptr [rbp+rax*8+54Ch], xmm1
      +0x550 r @0x140BB3F72: lea     rbx, [rbp+550h]
      +0x550 w @0x140BB4009: db  0Fh
      +0x550 w @0x140BB400A: db  11h

  *** BOTH FUNC 0x1452BDF40 (rva 0x52BDF40) sub_1452BDF40
      +0x360 r @0x1452BE2D4: db  48h ; H
      +0x360 r @0x1452BE2D5: db  8Dh
      +0x360 w @0x1452BE2EE: db  4Ch ; L
      +0x360 w @0x1452BE2EF: db  89h
      +0x360 w @0x1452BE31D: db  48h ; H
      +0x360 w @0x1452BE31E: db  89h
      +0x360 r @0x1452BE9F4: db  48h ; H
      +0x360 r @0x1452BE9F5: db  8Bh
      +0x368 w @0x1452BE300: db  4Ch ; L
      +0x368 w @0x1452BE301: db  89h
      +0x368 w @0x1452BE333: db 0C7h
      +0x540 w @0x1452BE236: db  48h ; H
      +0x540 w @0x1452BE237: db  89h

  *** BOTH FUNC 0x14288DF50 (rva 0x288DF50) sub_14288DF50
      +0x320 w @0x14288E8D4: db  40h ; @
      +0x320 w @0x14288E8D5: db  0Fh
      +0x320 w @0x14288E8D6: db  11h
      +0x330 w @0x14288E8DC: db  48h ; H
      +0x330 w @0x14288E8DD: db  89h
      +0x340 w @0x14288E295: db    0
      +0x350 w @0x14288E8EA: db  48h ; H
      +0x350 w @0x14288E8EB: db  89h
      +0x350 r @0x14288ED5E: db  48h ; H
      +0x350 r @0x14288ED5F: db  8Bh
      +0x350 r @0x14288F355: db  48h ; H
      +0x350 r @0x14288F356: db  8Bh
      +0x360 w @0x14288E993: db  40h ; @
      +0x360 w @0x14288E994: db  0Fh
      +0x360 w @0x14288E995: db  11h
      +0x550 r @0x14288E7C5: db  48h ; H
      +0x550 r @0x14288E7C6: db  8Dh
      +0x550 w @0x14288EF90: db  48h ; H
      +0x550 w @0x14288EF91: db  89h
      +0x558 r @0x14288EFBD: db  48h ; H
      +0x558 r @0x14288EFBE: db  8Dh
      +0x558 w @0x14288EFDB: db  48h ; H
      +0x558 w @0x14288EFDC: db  4Ch ; L
      +0x558 w @0x14288EFDD: db  89h
      +0x558 r @0x14288EFF6: db  48h ; H
      +0x558 r @0x14288EFF7: db  8Dh
      +0x558 r @0x14288F011: db  4Ch ; L

  *** BOTH FUNC 0x14092FF70 (rva 0x92FF70) sub_14092FF70
      +0x320 w @0x140930427: mov     [rbp+320h], rax
      +0x328 w @0x14093042E: mov     dword ptr [rbp+328h], 5
      +0x330 w @0x140930438: mov     [rbp+330h], rcx
      +0x340 w @0x14093044D: mov     [rbp+340h], rax
      +0x350 w @0x14093045E: mov     [rbp+350h], rax
      +0x360 w @0x140930473: mov     [rbp+360h], rax
      +0x368 w @0x14093047A: mov     dword ptr [rbp+368h], 5
      +0x540 w @0x1409306A6: mov     [rbp+540h], rax
      +0x548 w @0x1409306AD: mov     dword ptr [rbp+548h], 10h
      +0x550 w @0x1409306B7: mov     [rbp+550h], r13
      +0x558 w @0x1409306BE: mov     [rbp+558h], r15b

  *** BOTH FUNC 0x144D53FA0 (rva 0x4D53FA0) sub_144D53FA0
      +0x320 w @0x144D540F4: db  48h ; H
      +0x320 w @0x144D540F5: db  89h
      +0x320 w @0x144D5410D: db  40h ; @
      +0x320 w @0x144D5410E: db  88h
      +0x330 w @0x144D540FB: db  48h ; H
      +0x330 w @0x144D540FC: db  89h
      +0x340 w @0x144D54114: db  48h ; H
      +0x340 w @0x144D54115: db  89h
      +0x340 w @0x144D5412D: db  40h ; @
      +0x340 w @0x144D5412E: db  88h
      +0x350 w @0x144D5411B: db  48h ; H
      +0x350 w @0x144D5411C: db  89h
      +0x360 w @0x144D54134: db  0Fh
      +0x360 w @0x144D54135: db  11h
      +0x540 w @0x144D542AC: db  48h ; H
      +0x540 w @0x144D542AD: db  89h
      +0x540 w @0x144D542C5: db  40h ; @
      +0x540 w @0x144D542C6: db  88h
      +0x550 w @0x144D542B3: db  48h ; H
      +0x550 w @0x144D542B4: db  89h
      +0x558 w @0x144D542BA: db  48h ; H
      +0x558 w @0x144D542BB: db 0C7h

  *** BOTH FUNC 0x140C0FFD0 (rva 0xC0FFD0) sub_140C0FFD0
      +0x320 w @0x140C10D1F: db  44h ; D
      +0x320 w @0x140C10D20: db  0Fh
      +0x320 w @0x140C10D21: db  29h ; )
      +0x330 w @0x140C10D27: db  44h ; D
      +0x330 w @0x140C10D28: db  0Fh
      +0x330 w @0x140C10D29: db  29h ; )
      +0x340 w @0x140C10D2F: db  44h ; D
      +0x340 w @0x140C10D30: db  0Fh
      +0x340 w @0x140C10D31: db  29h ; )
      +0x340 r @0x140C10DEC: db  48h ; H
      +0x340 r @0x140C10DED: db  8Dh
      +0x340 r @0x140C10DFF: db  48h ; H
      +0x340 r @0x140C10E00: db  8Bh
      +0x340 r @0x140C10E0D: db  48h ; H
      +0x350 w @0x140C1087E: db  44h ; D
      +0x350 w @0x140C1087F: db  0Fh
      +0x350 w @0x140C10880: db  29h ; )
      +0x360 w @0x140C1088E: db  44h ; D
      +0x360 w @0x140C1088F: db  0Fh
      +0x360 w @0x140C10890: db  29h ; )
      +0x360 r @0x140C10E3E: db  48h ; H
      +0x360 r @0x140C10E3F: db  8Dh
      +0x540 w @0x140C10C15: db  44h ; D
      +0x540 w @0x140C10C16: db  0Fh
      +0x540 w @0x140C10C17: db  29h ; )
      +0x550 w @0x140C10C40: db  0Fh
      +0x550 w @0x140C10C41: db  29h ; )

############### QUERY 3+4: decompile + AOBs for top build candidates ###############
[Q3/Q4] decompile targets: ['0x145454000', '0x144ec8020', '0x141b4a090', '0x1452ba0c0', '0x1410b4160', '0x142b001f0']

========== DECOMP 0x145454000 (rva 0x5454000) sub_145454000 ==========
  AOB prologue (25 bytes): 48 89 5C 24 10 55 56 57 41 56 41 57 48 8D AC 24 D0 CB FF FF B8 30 35 00 00
     0x145454000: mov     qword ptr [rsp-8+arg_0+8], rbx
     0x145454005: push    rbp
     0x145454006: push    rsi
     0x145454007: push    rdi
     0x145454008: push    r14
     0x14545400A: push    r15
     0x14545400C: lea     rbp, [rsp-3430h]
     0x145454014: mov     eax, 3530h
  CALLERS (0): 
  DECOMP FAIL:  @0x145454000

========== DECOMP 0x144EC8020 (rva 0x4EC8020) sub_144EC8020 ==========
  AOB prologue (26 bytes): 48 89 5C 24 18 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 81 EC 80 06 00 00
     0x144EC8020: mov     [rsp+18h], rbx
     0x144EC8025: mov     [rsp+20h], rsi
     0x144EC8026: mov     [rsp+20h], rsi
     0x144EC802A: push    rdi
     0x144EC802B: push    r12
     0x144EC802C: push    r12
     0x144EC802D: push    r13
     0x144EC802E: push    r13
  CALLERS (0): 
void sub_144EC8020()
{
  JUMPOUT(0x144EC803ALL);
}


========== DECOMP 0x141B4A090 (rva 0x1B4A090) sub_141B4A090 ==========
  AOB prologue (26 bytes): 48 89 5C 24 18 48 89 74 24 20 57 48 83 EC 60 80 3D D2 47 58 07 00 41 0F B6 F0
     0x141B4A090: mov     [rsp+18h], rbx
     0x141B4A095: mov     [rsp+20h], rsi
     0x141B4A096: mov     [rsp+20h], rsi
     0x141B4A09A: push    rdi
     0x141B4A09B: sub     rsp, 60h
     0x141B4A09C: sub     rsp, 60h
     0x141B4A09F: cmp     cs:byte_1490CE878, 0
     0x141B4A0A0: cmp     cs:byte_1490CE878, 0
  CALLERS (0): 
void sub_141B4A090()
{
  JUMPOUT(0x141B4A0AALL);
}


========== DECOMP 0x1452BA0C0 (rva 0x52BA0C0) sub_1452BA0C0 ==========
  AOB prologue (26 bytes): 40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 68 A9 FF FF B8 98 57 00 00
     0x1452BA0C0: push    rbp
     0x1452BA0C2: push    rbx
     0x1452BA0C3: push    rsi
     0x1452BA0C4: push    rdi
     0x1452BA0C5: push    r12
     0x1452BA0C7: push    r13
     0x1452BA0C9: push    r14
     0x1452BA0CB: push    r15
  CALLERS (0): 
// positive sp value has been detected, the output may be wrong!
void __fastcall sub_1452BA0C0(__int64 a1)
{
  __int64 v2; // rax
  __int64 v3; // r15
  __int64 v4; // rbx
  __int64 v5; // rax
  __int64 v6; // rbx
  __int64 v7; // rax
  __int64 v8; // rdx
  int v9; // ecx
  __int64 v10; // rcx
  __int64 v11; // rax
  __int64 v12; // rax
  __int64 v13; // rbx
  __int64 v14; // rax
  __int64 v15; // rax
  __int64 v16; // rbx
  __int64 v17; // rbx
  __int64 v18; // rax
  __int64 v19; // rbx
  __int64 v20; // rbx
  __int64 v21; // rax
  __int64 v22; // rbx
  __int64 v23; // rax
  __int64 v24; // rbx
  __int64 v25; // rbx
  __int64 v26; // rax
  __int64 v27; // rbx
  __int64 v28; // rax
  __int64 v29; // rbx
  __int64 v30; // rbx
  __int64 v31; // rax
  __int64 v32; // rax
  __int64 v33; // rbx
  __int64 v34; // rax
  __int64 v35; // rax
  __int64 v36; // rbx
  __int64 v37; // rax
  __int64 v38; // rax
  __int64 v39; // rbx
  __int64 v40; // rax
  __int64 v41; // rax
  __int64 v42; // rbx
  __int64 v43; // rax
  __int64 v44; // rbx
  __int64 v45; // rbx
  __int64 v46; // rax
  __int64 v47; // rax
  __int64 v48; // rbx
  __int64 v49; // rax
  __int64 v50; // rbx
  __int64 v51; // rbx
  __int64 v52; // rax
  __int64 v53; // rax
  __int64 v54; // rbx
  __int64 v55; // rax
  __int64 v56; // rbx
  __int64 v57; // rax
  __int64 v58; // rax
  __int64 v59; // rbx
  __int64 v60; // rax
  __int64 v61; // rbx
  __int64 v62; // rbx
  __int64 v63; // rax
  __int64 v64; // rax
  __int64 v65; // rax
  __int64 v66; // rax
  __int64 v67; // rax
  __int64 v68; // rax
  __int64 v69; // rax
  __int64 v70; // rax
  __int64 v71; // rax
  __int64 v72; // rax
  __int64 v73; // rax
  __int64 v74; // rbx
  __int64 v75; // rbx
  __int64 v76; // rbx
  __int64 v77; // rax
  __int64 v78; // rbx
  __int64 v79; // rax
  __int64 v80; // rax
  __int64 v81; // rbx
  __int64 v82; // rdi
  __int64 v83; // rax
  __int64 v84; // rbx
  __int64 v85; // rbx
  __int64 v86; // r13
  _QWORD *v87; // r14
  __int64 v88; // rdi
  __int64 v89; // rbx
  __int64 v90; // rax
  __int64 v91; // rax
  __int64 v92; // rax
  __int64 v93; // rbx
  __int64 v94; // rax
  __int64 v95; // rax
  __int64 v96; // rax
  __int64 v97; // rbx
  __int64 v98; // rdi
  __int64 v99; // rbx
  __int64 v100; // rax
  __int64 v101; // rax
  __int64 v102; // rax
  __int64 v103; // rbx
  __int64 v104; // rdi
  __int64 v105; // rbx
  __int64 v106; // rax
  __int64 v107; // rax
  __int64 v108; // rax
  __int64 v109; // rbx
  __int64 v110; // rdi
  __int64 v111; // rbx
  __int64 v112; // rax
  __int64 v113; // rax
  __int64 v114; // rax
  __int64 v115; // rbx
  __int64 v116; // r13
  _QWORD *v117; // r14
  __int64 v118; // rbx
  __int64 v119; // rbx
  __int64 v120; // rbx
  __int64 v121; // r14
  _QWORD *v122; // rdi
  __int64 v123; // rax
  __int64 v124; // rbx
  __int64 v125; // rbx
  __int64 v126; // rdi
  __int64 v127; // rax
  __int64 v128; // rbx
  __int64 v129; // rbx
  __int64 v130; // rbx
  __int64 v131; // rbx
  __int64 v132; // rax
  char v133[8]; // [rsp+30h] [rbp-D0h] BYREF
  __int128 v134; // [rsp+38h] [rbp-C8h]
  int v135; // [rsp+48h] [rbp-B8h] BYREF
  int v136; // [rsp+4Ch] [rbp-B4h] BYREF
  int v137; // [rsp+50h] [rbp-B0h] BYREF
  int v138; // [rsp+54h] [rbp-ACh] BYREF
  int v139; // [rsp+58h] [rbp-A8h] BYREF
  int v140; // [rsp+5Ch] [rbp-A4h] BYREF
  int v141; // [rsp+60h] [rbp-A0h] BYREF
  int v142; // [rsp+64h] [rbp-9Ch] BYREF
  int v143; // [rsp+68h] [rbp-98h] BYREF
  int v144; // [rsp+6Ch] [rbp-94h] BYREF
  int v145; // [rsp+70h] [rbp-90h] BYREF
  int v146; // [rsp+74h] [rbp-8Ch] BYREF
  int v147; // [rsp+78h] [rbp-88h] BYREF
  int v148; // [rsp+7Ch] [rbp-84h] BYREF
  int v149; // [rsp+80h] [rbp-80h] BYREF
  int v150; // [rsp+84h] [rbp-7Ch] BYREF
  int v151; // [rsp+88h] [rbp-78h] BYREF
  int v152; // [rsp+8Ch] [rbp-74h] BYREF
  int v153; // [rsp+90h] [rbp-70h] BYREF
  int v154; // [rsp+94h] [rbp-6Ch] BYREF
  int v155; // [rsp+98h] [rbp-68h] BYREF
  int v156; // [rsp+9Ch] [rbp-64h] BYREF
  int v157; // [rsp+A0h] [rbp-60h] BYREF
  int v158; // [rsp+A4h] [rbp-5Ch] BYREF
  int v159; // [rsp+A8h] [rbp-58h] BYREF
  int v160; // [rsp+ACh] [rbp-54h] BYREF
  int v161; // [rsp+B0h] [rbp-50h] BYREF
  int v162; // [rsp+B4h] [rbp-4Ch] BYREF
  int v163; // [rsp+B8h] [rbp-48h] BYREF
  int v164; // [rsp+C0h] [rbp-40h] BYREF
  __int128 v165; // [rsp+C8h] [rbp-38h]
  __int128 v166; // [rsp+D8h] [rbp-28h]
  __int128 v167; // [rsp+E8h] [rbp-18h]
  __int128 v168; // [rsp+F8h] [rbp-8h]
  __int64 v169; // [rsp+108h] [rbp+8h]
  int v170; // [rsp+110h] [rbp+10h]
  __int64 v171; // [rsp+118h] [rbp+18h]
  __int64 v172; // [rsp+120h] [rbp+20h]
  __int16 v173; // [rsp+128h] [rbp+28h]
  char v174; // [rsp+12Ah] [rbp+2Ah]
  _BYTE v175[16]; // [rsp+130h] [rbp+30h] BYREF
  int v176; // [rsp+140h] [rbp+40h] BYREF
  __int64 v177; // [rsp+148h] [rbp+48h]
  __int64 v178; // [rsp+150h] [rbp+50h]
  __int64 v179; // [rsp+158h] [rbp+58h]
  __int64 v180; // [rsp+160h] [rbp+60h]
  __int64 v181; // [rsp+168h] [rbp+68h]
  __int64 v182; // [rsp+170h] [rbp+70h]
  __int64 v183; // [rsp+178h] [rbp+78h]
  __int64 v184; // [rsp+180h] [rbp+80h]
  __int64 v185; // [rsp+188h] [rbp+88h]
  int v186; // [rsp+190h] [rbp+90h]
  __int64 v187; // [rsp+198h] [rbp+98h]
  __int64 v188; // [rsp+1A0h] [rbp+A0h]
  __int64 v189; // [rsp+1A8h] [rbp+A8h]
  int v190; // [rsp+1B0h] [rbp+B0h]
  _BYTE v191[16]; // [rsp+1B8h] [rbp+B8h] BYREF
  int v192; // [rsp+1C8h] [rbp+C8h] BYREF
  __int64 v193; // [rsp+1D0h] [rbp+D0h]
  __int64 v194; // [rsp+1D8h] [rbp+D8h]
  __int64 v195; // [rsp+1E0h] [rbp+E0h]
  __int64 v196; // [rsp+1E8h] [rbp+E8h]
  __int64 v197; // [rsp+1F0h] [rbp+F0h]
  __int64 v198; // [rsp+1F8h] [rbp+F8h]
  __int64 v199; // [rsp+200h] [rbp+100h]
  __int64 v200; // [rsp+208h] [rbp+108h]
  __int64 v201; // [rsp+210h] [rbp+110h]
  int v202; // [rsp+218h] [rbp+118h]
  __int64 v203; // [rsp+220h] [rbp+120h]
  __int64 v204; // [rsp+228h] [rbp+128h]
  __int64 v205; // [rsp+230h] [rbp+130h]
  int v206; // [rsp+238h] [rbp+138h]
  _BYTE v207[16]; // [rsp+240h] [rbp+140h] BYREF
  int v208; // [rsp+250h] [rbp+150h] BYREF
  __int64 v209; // [rsp+258h] [rbp+158h]
  __int64 v210; // [rsp+260h] [rbp+160h]
  __int64 v211; // [rsp+268h] [rbp+168h]
  __int64 v212; // [rsp+270h] [rbp+170h]
  __int64 v213; // [rsp+278h] [rbp+178h]
  __int64 v214; // [rsp+280h] [rbp+180h]
  __int64 v215; // [rsp+288h] [rbp+188h]
  __int64 v216; // [rsp+290h] [rbp+190h]
  __int64 v217; // [rsp+298h] [rbp+198h]
  int v218; // [rsp+2A0h] [rbp+1A0h]
  __int64 v219; // [rsp+2A8h] [rbp+1A8h]
  __int64 v220; // [rsp+2B0h] [rbp+1B0h]
  __int64 v221; // [rsp+2B8h] [rbp+1B8h]
  int v222; // [rsp+2C0h] [rbp+1C0h]
  _BYTE v223[16]; // [rsp+2C8h] [rbp+1C8h] BYREF
  int v224; // [rsp+2D8h] [rbp+1D8h] BYREF
  __int64 v225; // [rsp+2E0h] [rbp+1E0h]
  __int64 v226; // [rsp+2E8h] [rbp+1E8h]
  __int64 v227; // [rsp+2F0h] [rbp+1F0h]
  __int64 v228; // [rsp+2F8h] [rbp+1F8h]
  __int64 v229; // [rsp+300h] [rbp+200h]
  __int64 v230; // [rsp+308h] [rbp+208h]
  __int64 v231; // [rsp+310h] [rbp+210h]
  __int64 v232; // [rsp+318h] [rbp+218h]
  __int64 v233; // [rsp+320h] [rbp+220h]
  int v234; // [rsp+328h] [rbp+228h]
  __int64 v235; // [rsp+330h] [rbp+230h]
  __int64 v236; // [rsp+338h] [rbp+238h]
  __int64 v237; // [rsp+340h] [rbp+240h]
  int v238; // [rsp+348h] [rbp+248h]
  _BYTE v239[16]; // [rsp+350h] [rbp+250h] BYREF
  int v240; // [rsp+360h] [rbp+260h] BYREF
  __int64 v241; // [rsp+368h] [rbp+268h]
  __int64 v242; // [rsp+370h] [rbp+270h]
  __int64 v243; // [rsp+378h] [rbp+278h]
  __int64 v244; // [rsp+380h] [rbp+280h]
  __int64 v245; // [rsp+388h] [rbp+288h]
  __int64 v246; // [rsp+390h] [rbp+290h]
  __int64 v247; // [rsp+398h] [rbp+298h]
  __int64 v248; // [rsp+3A0h] [rbp+2A0h]
  __int64 v249; // [rsp+3A8h] [rbp+2A8h]
  int v250; // [rsp+3B0h] [rbp+2B0h]
  __int64 v251; // [rsp+3B8h] [rbp+2B8h]
  __int64 v252; // [rsp+3C0h] [rbp+2C0h]
  __int64 v253; // [rsp+3C8h] [rbp+2C8h]
  int v254; // [rsp+3D0h] [rbp+2D0h]
  _BYTE v255[16]; // [rsp+3D8h] [rbp+2D8h] BYREF
  int v256; // [rsp+3E8h] [rbp+2E8h] BYREF
  __int128 v257; // [rsp+3F0h] [rbp+2F0h]
  __int128 v258; // [rsp+400h] [rbp+300h]
  __int128 v259; // [rsp+410h] [rbp+310h]
  __int128 v260; // [rsp+420h] [rbp+320h]
  __int64 v261; // [rsp+430h] [rbp+330h]
  int v262; // [rsp+438h] [rbp+338h]
  __int64 v263; // [rsp+440h] [rbp+340h]
  __int64 v264; // [rsp+448h] [rbp+348h]
  __int16 v265; // [rsp+450h] [rbp+350h]
  char v266; // [rsp+452h] [rbp+352h]
  _BYTE v267[16]; // [rsp+458h] [rbp+358h] BYREF
  int v268; // [rsp+468h] [rbp+368h] BYREF
  __int64 v269; // [rsp+470h] [rbp+370h]
  __int64 v270; // [rsp+478h] [rbp+378h]
  __int64 v271; // [rsp+480h] [rbp+380h]
  __int64 v272; // [rsp+488h] [rbp+388h]
  __int64 v273; // [rsp+490h] [rbp+390h]
  __int64 v274; // [rsp+498h] [rbp+398h]
  __int64 v275; // [rsp+4A0h] [rbp+3A0h]
  __int64 v276; // [rsp+4A8h] [rbp+3A8h]
  __int64 v277; // [rsp+4B0h] [rbp+3B0h]
  int v278; // [rsp+4B8h] [rbp+3B8h]
  __int64 v279; // [rsp+4C0h] [rbp+3C0h]
  __int64 v280; // [rsp+4C8h] [rbp+3C8h]
  __int64 v281; // [rsp+4D0h] [rbp+3D0h]
  int v282; // [rsp+4D8h] [rbp+3D8h]
  _BYTE v283[16]; // [rsp+4E0h] [rbp+3E0h] BYREF
  int v284; // [rsp+4F0h] [rbp+3F0h] BYREF
  __int64 v285; // [rsp+4F8h] [rbp+3F8h]
  __int64 v286; // [rsp+500h] [rbp+400h]
  __int64 v287; // [rsp+508h] [rbp+408h]
  __int64 v288; // [rsp+510h] [rbp+410h]
  __int64 v289; // [rsp+518h] [rbp+418h]
  __int64 v290; // [rsp+520h] [rbp+420h]
  __int64 v291; // [rsp+528h] [rbp+428h]
  __int64 v292; // [rsp+530h] [rbp+430h]
  __int64 v293; // [rsp+538h] [rbp+438h]
  int v294; // [rsp+540h] [rbp+440h]
  __int64 v295; // [rsp+548h] [rbp+448h]
  __int64 v296; // [rsp+550h] [rbp+450h]
  __int64 v297; // [rsp+558h] [rbp+458h]
  int v298; // [rsp+560h] [rbp+460h]
  _BYTE v299[16]; // [rsp+568h] [rbp+468h] BYREF
  int v300; // [rsp+578h] [rbp+478h] BYREF
  __int64 v301; // [rsp+580h] [rbp+480h]
  __int64 v302; // [rsp+588h] [rbp+488h]
  __int64 v303; // [rsp+590h] [rbp+490h]
  __int64 v304; // [rsp+598h] [rbp+498h]
  __int64 v305; // [rsp+5A0h] [rbp+4A0h]
  __int64 v306; // [rsp+5A8h] [rbp+4A8h]
  __int64 v307; // [rsp+5B0h] [rbp+4B0h]
  __int64 v308; // [rsp+5B8h] [rbp+4B8h]
  __int64 v309; // [rsp+5C0h] [rbp+4C0h]
  int v310; // [rsp+5C8h] [rbp+4C8h]
  __int64 v311; // [rsp+5D0h] [rbp+4D0h]
  __int64 v312; // [rsp+5D8h] [rbp+4D8h]
  __int64 v313; // [rsp+5E0h] [rbp+4E0h]
  int v314; // [rsp+5E8h] [rbp+4E8h]
  _BYTE v315[16]; // [rsp+5F0h] [rbp+4F0h] BYREF
  int v316; // [rsp+600h] [rbp+500h] BYREF
  __int64 v317; // [rsp+608h] [rbp+508h]
  __int64 v318; // [rsp+610h] [rbp+510h]
  __int64 v319; // [rsp+618h] [rbp+518h]
  __int64 v320; // [rsp+620h] [rbp+520h]
  __int64 v321; // [rsp+628h] [rbp+528h]
  __int64 v322; // [rsp+630h] [rbp+530h]
  __int64 v323; // [rsp+638h] [rbp+538h]
  __int64 v324; // [rsp+640h] [rbp+540h]
  __int64 v325; // [rsp+648h] [rbp+548h]
  int v326; // [rsp+650h] [rbp+550h]
  __int64 v327; // [rsp+658h] [rbp+558h]
  __int64 v328; // [rsp+660h] [rbp+560h]
  __int64 v329; // [rsp+668h] [rbp+568h]
  int v330; // [rsp+670h] [rbp+570h]
  _BYTE v331[16]; // [rsp+678h] [rbp+578h] BYREF
  int v332; // [rsp+688h] [rbp+588h] BYREF
  __int64 v333; // [rsp+690h] [rbp+590h]
  __int64 v334; // [rsp+698h] [rbp+598h]
  __int64 v335; // [rsp+6A0h] [rbp+5A0h]
  __int64 v336; // [rsp+6A8h] [rbp+5A8h]
  __int64 v337; // [rsp+6B0h] [rbp+5B0h]
  __int64 v338; // [rsp+6B8h] [rbp+5B8h]
  __int64 v339; // [rsp+6C0h] [rbp+5C0h]
  __int64 v340; // [rsp+6C8h] [rbp+5C8h]
  __int64 v341; // [rsp+6D0h] [rbp+5D0h]
  int v342; // [rsp+6D8h] [rbp+5D8h]
  __int64 v343; // [rsp+6E0h] [rbp+5E0h]
  __int64 v344; // [rsp+6E8h] [rbp+5E8h]
  __int64 v345; // [rsp+6F0h] [rbp+5F0h]
  int v346; // [rsp+6F8h] [rbp+5F8h]
  _BYTE v347[16]; // [rsp+700h] [rbp+600h] BYREF
  int v348; // [rsp+710h] [rbp+610h] BYREF
  __int64 v349; // [rsp+718h] [rbp+618h]
  __int64 v350; // [rsp+720h] [rbp+620h]
  __int64 v351; // [rsp+728h] [rbp+628h]
  __int64 v352; // [rsp+730h] [rbp+630h]
  __int64 v353; // [rsp+738h] [rbp+638h]
  __int64 v354; // [rsp+740h] [rbp+640h]
  __int64 v355; // [rsp+748h] [rbp+648h]
  __int64 v356; // [rsp+750h] [rbp+650h]
  __int64 v357; // [rsp+758h] [rbp+658h]
  int v358; // [rsp+760h] [rbp+660h]
  __int64 v359; // [rsp+768h] [rbp+668h]
  __int64 v360; // [rsp+770h] [rbp+670h]
  __int64 v361; // [rsp+778h] [rbp+678h]
  int v362; // [rsp+780h] [rbp+680h]
  _BYTE v363[16]; // [rsp+788h] [rbp+688h] BYREF
  int v364; // [rsp+798h] [rbp+698h] BYREF
  __int64 v365; // [rsp+7A0h] [rbp+6A0h]
  __int64 v366; // [rsp+7A8h] [rbp+6A8h]
  __int64 v367; // [rsp+7B0h] [rbp+6B0h]
  __int64 v368; // [rsp+7B8h] [rbp+6B8h]
  __int64 v369; // [rsp+7C0h] [rbp+6C0h]
  __int64 v370; // [rsp+7C8h] [rbp+6C8h]
  __int64 v371; // [rsp+7D0h] [rbp+6D0h]
  __int64 v372; // [rsp+7D8h] [rbp+6D8h]
  __int64 v373; // [rsp+7E0h] [rbp+6E0h]
  int v374; // [rsp+7E8h] [rbp+6E8h]
  __int64 v375; // [rsp+7F0h] [rbp+6F0h]
  __int64 v376; // [rsp+7F8h] [rbp+6F8h]
  __int64 v377; // [rsp+800h] [rbp+700h]
  int v378; // [rsp+808h] [rbp+708h]
  _BYTE v379[16]; // [rsp+810h] [rbp+710h] BYREF
  int v380; // [rsp+820h] [rbp+720h] BYREF
  __int64 v381; // [rsp+828h] [rbp+728h]
  __int64 v382; // [rsp+830h] [rbp+730h]
  __int64 v383; // [rsp+838h] [rbp+738h]
  __int64 v384; // [rsp+840h] [rbp+740h]
  __int64 v385; // [rsp+848h] [rbp+748h]
  __int64 v386; // [rsp+850h] [rbp+750h]
  __int64 v387; // [rsp+858h] [rbp+758h]
  __int64 v388; // [rsp+860h] [rbp+760h]
  __int64 v389; // [rsp+868h] [rbp+768h]
  int v390; // [rsp+870h] [rbp+770h]
  __int64 v391; // [rsp+878h] [rbp+778h]
  __int64 v392; // [rsp+880h] [rbp+780h]
  __int64 v393; // [rsp+888h] [rbp+788h]
  int v394; // [rsp+890h] [rbp+790h]
  _BYTE v395[16]; // [rsp+898h] [rbp+798h] BYREF
  _BYTE v396[120]; // [rsp+8A8h] [rbp+7A8h] BYREF
  _OWORD *v397; // [rsp+920h] [rbp+820h] BYREF
  __int64 v398; // [rsp+928h] [rbp+828h]
  ...[truncated 1364 lines]

========== DECOMP 0x1410B4160 (rva 0x10B4160) sub_1410B4160 ==========
  AOB prologue (26 bytes): 48 81 EC 58 05 00 00 F3 0F 10 81 6C 02 00 00 48 8B C2 F3 0F 10 A9 9C 09 00 00
     0x1410B4160: sub     rsp, 558h
     0x1410B4167: movss   xmm0, dword ptr [rcx+26Ch]
     0x1410B4168: movss   xmm0, dword ptr [rcx+26Ch]
     0x1410B416F: mov     rax, rdx
     0x1410B4170: mov     rax, rdx
     0x1410B4172: movss   xmm5, dword ptr [rcx+99Ch]
     0x1410B4173: movss   xmm5, dword ptr [rcx+99Ch]
  CALLERS (0): 
void sub_1410B4160()
{
  JUMPOUT(0x1410B417ALL);
}


========== DECOMP 0x142B001F0 (rva 0x2B001F0) sub_142B001F0 ==========
  AOB prologue (29 bytes): 48 89 5C 24 08 48 89 54 24 10 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 90 E4 FF FF
     0x142B001F0: mov     qword ptr [rsp-8+arg_0], rbx
     0x142B001F5: mov     qword ptr [rsp-8+arg_0+8], rdx
     0x142B001FA: push    rbp
     0x142B001FB: push    rsi
     0x142B001FC: push    rdi
     0x142B001FD: push    r12
     0x142B001FF: push    r13
     0x142B00201: push    r14
  CALLERS (0): 
// positive sp value has been detected, the output may be wrong!
void __fastcall sub_142B001F0(__int64 a1, __int64 *a2)
{
  _QWORD *v4; // r14
  __int64 v5; // rdi
  __int64 v6; // rcx
  __int64 *v7; // rax
  __int64 v8; // rbx
  __int64 *v9; // rax
  __int64 v10; // rbx
  __int64 v11; // rax
  __int64 v12; // r12
  __int64 v13; // rax
  __int64 v14; // rcx
  char *v15; // rax
  int i; // edi
  int v17; // ebx
  __int64 v18; // rsi
  __int64 v19; // rbx
  __int64 v20; // r13
  __int64 v21; // rdx
  signed __int32 v22; // eax
  signed __int32 v23; // ett
  __int64 v24; // rax
  __int64 v25; // r8
  __int64 v26; // rbx
  __int64 v27; // rcx
  __int64 v28; // rax
  volatile signed __int32 *v29; // rbx
  _OWORD *v30; // rdx
  __int64 v31; // rax
  __int64 v32; // rbx
  __int64 v33; // rdi
  __int64 v34; // rcx
  __int64 *v35; // rax
  __int64 v36; // rsi
  __int64 v37; // rdi
  __int64 v38; // rcx
  __int64 v39; // rdx
  signed __int32 v40; // eax
  signed __int32 v41; // ett
  __int64 v42; // rax
  __int64 v43; // r8
  __int64 v44; // rdi
  __int64 *v45; // rdx
  __int64 v46; // rcx
  __int64 v47; // rax
  __int64 v48; // rbx
  __int64 v49; // rdx
  signed __int32 v50; // eax
  signed __int32 v51; // ett
  __int64 v52; // rax
  __int64 v53; // r8
  __int64 *v54; // rdx
  __int64 v55; // rcx
  __int64 v56; // rax
  __int64 v57; // rbx
  __int64 v58; // rdx
  signed __int32 v59; // eax
  signed __int32 v60; // ett
  __int64 v61; // rax
  __int64 v62; // r8
  __int64 *v63; // rdx
  __int64 v64; // rcx
  __int64 v65; // rax
  __int64 v66; // rbx
  __int64 v67; // rdx
  signed __int32 v68; // eax
  signed __int32 v69; // ett
  __int64 v70; // rax
  __int64 v71; // r8
  __int64 *v72; // rdx
  __int64 v73; // rcx
  __int64 v74; // rax
  __int64 v75; // rbx
  __int64 v76; // rdx
  signed __int32 v77; // eax
  signed __int32 v78; // ett
  __int64 v79; // rax
  __int64 v80; // r8
  __int64 *v81; // rdx
  __int64 v82; // rcx
  __int64 v83; // rbx
  __int64 v84; // rdx
  signed __int32 v85; // eax
  signed __int32 v86; // ett
  __int64 v87; // rax
  __int64 v88; // r8
  _QWORD *v89; // rdx
  __int64 v90; // rcx
  __int64 v91; // rax
  __int64 v92; // rbx
  __int64 v93; // rax
  __int64 v94; // rax
  __int64 v95; // rax
  __int64 v96; // rbx
  __int64 v97; // rax
  __int64 v98; // rax
  __int64 v99; // rax
  __int64 v100; // rax
  __int64 v101; // rax
  __int64 v102; // rax
  __int64 v103; // rbx
  __int64 v104; // rax
  __int64 v105; // rax
  __int64 v106; // rax
  __int64 v107; // rax
  __int64 v108; // rax
  __int64 v109; // rax
  __int64 v110; // rdi
  __int64 v111; // rbx
  __int64 v112; // rax
  __int64 v113; // rax
  _DWORD *v114; // rax
  __int64 v115; // rax
  __int64 v116; // rax
  int v117; // edi
  __int64 v118; // r13
  __int64 v119; // rax
  unsigned int j; // ebx
  __int64 v121; // rbx
  __int64 v122; // rax
  __int64 v123; // rax
  _DWORD *v124; // rax
  __int64 v125; // rax
  __int64 v126; // rax
  int v127; // edi
  __int64 v128; // r13
  __int64 v129; // rax
  unsigned int k; // ebx
  __int64 v131; // rbx
  __int64 v132; // rax
  __int64 v133; // rax
  _DWORD *v134; // rax
  __int64 v135; // rax
  __int64 v136; // rax
  __int64 v137; // rbx
  __int64 v138; // rax
  __int64 v139; // rax
  __int64 v140; // rax
  __int64 v141; // rbx
  __int64 v142; // rax
  __int64 v143; // rax
  __int64 v144; // rax
  __int64 v145; // rbx
  __int64 v146; // rax
  __int64 v147; // rax
  __int64 v148; // rax
  __int64 v149; // rbx
  __int64 v150; // rax
  __int64 v151; // rax
  __int64 v152; // rax
  __int64 v153; // rbx
  __int64 v154; // rax
  __int64 v155; // rax
  __int64 v156; // rax
  __int64 v157; // rbx
  __int64 v158; // rax
  __int64 v159; // rax
  __int64 v160; // rax
  __int64 v161; // rbx
  __int64 v162; // rax
  __int64 v163; // rax
  __int64 v164; // rax
  __int64 v165; // rbx
  __int64 v166; // rax
  __int64 v167; // rax
  __int64 v168; // rax
  __int64 v169; // rbx
  __int64 v170; // rax
  __int64 v171; // rax
  __int64 v172; // rax
  __int64 v173; // rsi
  __int64 v174; // rax
  int m; // edi
  __int64 v176; // rbx
  __int64 v177; // rax
  __int64 v178; // rax
  __int64 v179; // rbx
  __int64 v180; // rax
  __int64 v181; // rax
  __int64 v182; // rbx
  _DWORD *v183; // rsi
  __int64 v184; // rax
  __int64 v185; // rdi
  __int64 v186; // rax
  __int64 v187; // rbx
  __int64 v188; // rax
  __int64 v189; // rax
  __int64 v190; // rax
  __int64 v191; // rdi
  __int64 v192; // rax
  __int64 v193; // rbx
  __int64 v194; // rax
  __int64 v195; // rax
  __int64 v196; // rax
  __int64 v197; // rdi
  __int64 v198; // rax
  __int64 v199; // rbx
  __int64 v200; // rax
  __int64 v201; // rax
  __int64 v202; // rax
  __int64 v203; // rax
  __int64 v204; // rbx
  __int64 v205; // rax
  __int64 v206; // rax
  __int64 v207; // rbx
  __int64 v208; // rax
  __int64 v209; // rax
  __int64 v210; // rbx
  __int64 v211; // rax
  __int64 v212; // rax
  __int64 v213; // rbx
  __int64 v214; // rax
  __int64 v215; // rax
  __int64 v216; // rbx
  __int64 v217; // rax
  __int64 v218; // rax
  __int64 v219; // rax
  __int64 v220; // rbx
  __int64 v221; // rax
  __int64 v222; // rax
  __int64 v223; // rax
  __int64 v224; // rbx
  __int64 v225; // rax
  __int64 v226; // rax
  __int64 v227; // rax
  __int64 v228; // rbx
  __int64 v229; // rax
  __int64 v230; // rax
  __int64 v231; // rax
  __int64 v232; // rsi
  __int64 v233; // rax
  __int64 v234; // rdi
  __int64 v235; // rbx
  __int64 v236; // rax
  __int64 v237; // rax
  __int64 v238; // rax
  __int64 v239; // rbx
  __int64 v240; // rax
  __int64 v241; // rax
  __int64 v242; // rax
  __int64 v243; // rax
  __int64 v244; // rax
  __int64 v245; // rax
  __int64 v246; // rsi
  __int64 v247; // rax
  __int64 v248; // rdi
  __int64 v249; // rbx
  __int64 v250; // rax
  __int64 v251; // rax
  __int64 v252; // rax
  __int64 v253; // rax
  __int64 v254; // rax
  __int64 v255; // rax
  __int64 v256; // rax
  __int64 v257; // [rsp+0h] [rbp-100h] BYREF
  int v258; // [rsp+20h] [rbp-E0h]
  __int64 v259; // [rsp+28h] [rbp-D8h]
  __int128 v260; // [rsp+30h] [rbp-D0h] BYREF
  __int128 v261; // [rsp+40h] [rbp-C0h] BYREF
  _QWORD v262[2]; // [rsp+50h] [rbp-B0h] BYREF
  __int128 *v263; // [rsp+60h] [rbp-A0h]
  _QWORD v264[2]; // [rsp+68h] [rbp-98h] BYREF
  __int128 v265; // [rsp+78h] [rbp-88h] BYREF
  __int128 v266; // [rsp+88h] [rbp-78h]
  __int64 v267; // [rsp+98h] [rbp-68h] BYREF
  int v268; // [rsp+A0h] [rbp-60h]
  _QWORD v269[2]; // [rsp+A8h] [rbp-58h] BYREF
  _QWORD v270[2]; // [rsp+B8h] [rbp-48h] BYREF
  _QWORD v271[2]; // [rsp+C8h] [rbp-38h] BYREF
  _QWORD v272[2]; // [rsp+D8h] [rbp-28h] BYREF
  _QWORD v273[2]; // [rsp+E8h] [rbp-18h] BYREF
  char *v274; // [rsp+F8h] [rbp-8h] BYREF
  __int64 v275; // [rsp+108h] [rbp+8h]
  unsigned __int64 v276; // [rsp+110h] [rbp+10h]
  _QWORD v277[2]; // [rsp+118h] [rbp+18h] BYREF
  _QWORD v278[2]; // [rsp+128h] [rbp+28h] BYREF
  _QWORD v279[2]; // [rsp+138h] [rbp+38h] BYREF
  _QWORD v280[2]; // [rsp+148h] [rbp+48h] BYREF
  _QWORD v281[2]; // [rsp+158h] [rbp+58h] BYREF
  _QWORD v282[2]; // [rsp+168h] [rbp+68h] BYREF
  _QWORD v283[2]; // [rsp+178h] [rbp+78h] BYREF
  _QWORD v284[2]; // [rsp+188h] [rbp+88h] BYREF
  _QWORD v285[2]; // [rsp+198h] [rbp+98h] BYREF
  char v286[16]; // [rsp+1A8h] [rbp+A8h] BYREF
  __int64 v287; // [rsp+1B8h] [rbp+B8h]
  unsigned __int64 v288; // [rsp+1C0h] [rbp+C0h]
  _BYTE v289[16]; // [rsp+1C8h] [rbp+C8h] BYREF
  _BYTE v290[16]; // [rsp+1D8h] [rbp+D8h] BYREF
  _QWORD v291[2]; // [rsp+1E8h] [rbp+E8h] BYREF
  _QWORD v292[2]; // [rsp+1F8h] [rbp+F8h] BYREF
  _QWORD v293[2]; // [rsp+208h] [rbp+108h] BYREF
  _BYTE v294[16]; // [rsp+218h] [rbp+118h] BYREF
  _BYTE v295[16]; // [rsp+228h] [rbp+128h] BYREF
  _QWORD v296[2]; // [rsp+238h] [rbp+138h] BYREF
  _QWORD v297[2]; // [rsp+248h] [rbp+148h] BYREF
  _QWORD v298[2]; // [rsp+258h] [rbp+158h] BYREF
  __int64 v299; // [rsp+268h] [rbp+168h]
  unsigned __int64 v300; // [rsp+270h] [rbp+170h]
  _QWORD v301[2]; // [rsp+278h] [rbp+178h] BYREF
  __int64 v302; // [rsp+288h] [rbp+188h]
  unsigned __int64 v303; // [rsp+290h] [rbp+190h]
  _QWORD v304[2]; // [rsp+298h] [rbp+198h] BYREF
  __int64 v305; // [rsp+2A8h] [rbp+1A8h]
  unsigned __int64 v306; // [rsp+2B0h] [rbp+1B0h]
  _QWORD v307[2]; // [rsp+2B8h] [rbp+1B8h] BYREF
  __int64 v308; // [rsp+2C8h] [rbp+1C8h]
  unsigned __int64 v309; // [rsp+2D0h] [rbp+1D0h]
  _QWORD v310[2]; // [rsp+2D8h] [rbp+1D8h] BYREF
  __int64 v311; // [rsp+2E8h] [rbp+1E8h]
  unsigned __int64 v312; // [rsp+2F0h] [rbp+1F0h]
  _QWORD v313[2]; // [rsp+2F8h] [rbp+1F8h] BYREF
  __int64 v314; // [rsp+308h] [rbp+208h]
  unsigned __int64 v315; // [rsp+310h] [rbp+210h]
  _QWORD v316[2]; // [rsp+318h] [rbp+218h] BYREF
  __int64 v317; // [rsp+328h] [rbp+228h]
  unsigned __int64 v318; // [rsp+330h] [rbp+230h]
  _QWORD v319[2]; // [rsp+338h] [rbp+238h] BYREF
  __int64 v320; // [rsp+348h] [rbp+248h]
  unsigned __int64 v321; // [rsp+350h] [rbp+250h]
  _QWORD v322[2]; // [rsp+358h] [rbp+258h] BYREF
  __int64 v323; // [rsp+368h] [rbp+268h]
  unsigned __int64 v324; // [rsp+370h] [rbp+270h]
  _QWORD v325[2]; // [rsp+378h] [rbp+278h] BYREF
  __int64 v326; // [rsp+388h] [rbp+288h]
  unsigned __int64 v327; // [rsp+390h] [rbp+290h]
  _QWORD v328[2]; // [rsp+398h] [rbp+298h] BYREF
  __int64 v329; // [rsp+3A8h] [rbp+2A8h]
  unsigned __int64 v330; // [rsp+3B0h] [rbp+2B0h]
  _QWORD v331[2]; // [rsp+3B8h] [rbp+2B8h] BYREF
  __int64 v332; // [rsp+3C8h] [rbp+2C8h]
  unsigned __int64 v333; // [rsp+3D0h] [rbp+2D0h]
  __int128 v334; // [rsp+3D8h] [rbp+2D8h] BYREF
  __int128 v335; // [rsp+3E8h] [rbp+2E8h]
  __int64 v336; // [rsp+3F8h] [rbp+2F8h]
  __int64 v337; // [rsp+400h] [rbp+300h]
  __int64 v338; // [rsp+408h] [rbp+308h] BYREF
  __int64 v339; // [rsp+410h] [rbp+310h]
  __int128 v340; // [rsp+418h] [rbp+318h] BYREF
  __int128 v341; // [rsp+428h] [rbp+328h] BYREF
  __int128 v342; // [rsp+438h] [rbp+338h]
  __int64 v343; // [rsp+448h] [rbp+348h]
  __int64 v344; // [rsp+450h] [rbp+350h]
  __int64 v345; // [rsp+458h] [rbp+358h] BYREF
  __int64 v346; // [rsp+460h] [rbp+360h]
  __int128 v347; // [rsp+468h] [rbp+368h] BYREF
  __int128 v348; // [rsp+478h] [rbp+378h] BYREF
  __int128 v349; // [rsp+488h] [rbp+388h]
  __int64 v350; // [rsp+498h] [rbp+398h]
  __int64 v351; // [rsp+4A0h] [rbp+3A0h]
  __int64 v352; // [rsp+4A8h] [rbp+3A8h] BYREF
  __int64 v353; // [rsp+4B0h] [rbp+3B0h]
  __int128 v354; // [rsp+4B8h] [rbp+3B8h] BYREF
  __int128 v355; // [rsp+4C8h] [rbp+3C8h] BYREF
  __int128 v356; // [rsp+4D8h] [rbp+3D8h]
  __int64 v357; // [rsp+4E8h] [rbp+3E8h]
  __int64 v358; // [rsp+4F0h] [rbp+3F0h]
  __int64 v359; // [rsp+4F8h] [rbp+3F8h] BYREF
  __int64 v360; // [rsp+500h] [rbp+400h]
  __int128 v361; // [rsp+508h] [rbp+408h] BYREF
  __int128 v362; // [rsp+518h] [rbp+418h] BYREF
  __int128 v363; // [rsp+528h] [rbp+428h]
  __int64 v364; // [rsp+538h] [rbp+438h]
  __int64 v365; // [rsp+540h] [rbp+440h]
  __int64 v366; // [rsp+548h] [rbp+448h] BYREF
  __int64 v367; // [rsp+550h] [rbp+450h]
  __int128 v368; // [rsp+558h] [rbp+458h]
  _BYTE v369[16]; // [rsp+568h] [rbp+468h] BYREF
  _BYTE v370[16]; // [rsp+578h] [rbp+478h] BYREF
  _BYTE v371[16]; // [rsp+588h] [rbp+488h] BYREF
  _BYTE v372[16]; // [rsp+598h] [rbp+498h] BYREF
  _BYTE v373[16]; // [rsp+5A8h] [rbp+4A8h] BYREF
  __int64 v374; // [rsp+5B8h] [rbp+4B8h] BYREF
  int v375; // [rsp+5C0h] [rbp+4C0h]
  __int64 v376; // [rsp+5C8h] [rbp+4C8h]
  int v377; // [rsp+5D0h] [rbp+4D0h]
  __int64 v378; // [rsp+5D8h] [rbp+4D8h] BYREF
  __int64 v379; // [rsp+5E0h] [rbp+4E0h] BYREF
  __int64 v380; // [rsp+5E8h] [rbp+4E8h] BYREF
  char v381[8]; // [rsp+5F0h] [rbp+4F0h] BYREF
  char v382[8]; // [rsp+5F8h] [rbp+4F8h] BYREF
  char v383[8]; // [rsp+600h] [rbp+500h] BYREF
  char v384[8]; // [rsp+608h] [rbp+508h] BYREF
  char v385[8]; // [rsp+610h] [rbp+510h] BYREF
  char v386[8]; // [rsp+618h] [rbp+518h] BYREF
  char v387[8]; // [rsp+620h] [rbp+520h] BYREF
  char v388[8]; // [rsp+628h] [rbp+528h] BYREF
  char v389[8]; // [rsp+630h] [rbp+530h] BYREF
  char v390[8]; // [rsp+638h] [rbp+538h] BYREF
  char v391[8]; // [rsp+640h] [rbp+540h] BYREF
  char v392[8]; // [rsp+648h] [rbp+548h] BYREF
  char v393[8]; // [rsp+650h] [rbp+550h] BYREF
  char v394[8]; // [rsp+658h] [rbp+558h] BYREF
  char v395[8]; // [rsp+660h] [rbp+560h] BYREF
  char v396[8]; // [rsp+668h] [rbp+568h] BYREF
  _BYTE v397[16]; // [rsp+670h] [rbp+570h] BYREF
  __int128 v398; // [rsp+680h] [rbp+580h] BYREF
  __int128 v399; // [rsp+690h] [rbp+590h] BYREF
  char v400[8]; // [rsp+6A0h] [rbp+5A0h] BYREF
  ...[truncated 1899 lines]

############### QUERY 5b: dump candidate camera object field layout ###############
(see Q2/Q2b offset listings above for field usage)
```
