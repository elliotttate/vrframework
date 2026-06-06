# followup2

```
=============== Q2b: the 3 indirect/dispatch callers of the fold ===============

### caller 0x1407A6DC0 sub_1407A6DC0 ###
    void sub_1407A6DC0()
    {
      JUMPOUT(0x1407A6DC2LL);
    }
  callers-of-caller: 
  vtable slots holding it: ['0x145e3e8b0']

### caller 0x1407A6DF0 sub_1407A6DF0 ###
    void sub_1407A6DF0()
    {
      JUMPOUT(0x1407A6DF2LL);
    }
  callers-of-caller: 
  vtable slots holding it: ['0x145e3f5f0']

### caller 0x1407A6E30 sub_1407A6E30 ###
    void sub_1407A6E30()
    {
      JUMPOUT(0x1407A6E32LL);
    }
  callers-of-caller: 
  vtable slots holding it: ['0x145e3ef50']

=============== Q2c: callers of CCamDriver vtable[2] wrapper sub_1407A6430 ===============
  direct callers: (none direct; called via vtable slot 0x145E3F2A0)
  sub_1407A6430 vtable slots: ['0x145e3f2a0']

=============== Q3b: classify the +0x540 writers near the camera cluster ===============
  @0x1403D13F4 in fn 0x1403D0E50 sub_1403D0E50   | mov     [rdi+540h], rax
  @0x140581F8B in fn 0x140581F40 sub_140581F40   | movups  xmmword ptr [rsi+540h], xmm0
  @0x1405929A5 in fn 0x0 ?   | movups  xmmword ptr [rcx+540h], xmm0
  @0x14071BE31 in fn 0x14071BAF0 sub_14071BAF0   | mov     [r14+540h], rax
  @0x14079BCF0 in fn 0x14079BBC0 sub_14079BBC0   | movups  xmmword ptr [rdi+540h], xmm1
  @0x1407A98AC in fn 0x1407A9880 sub_1407A9880 CAM-CLUSTER  | movups  xmmword ptr [rbx+540h], xmm5
  @0x1407A9B85 in fn 0x1407A9B60 sub_1407A9B60 CAM-CLUSTER  | movups  xmmword ptr [rcx+540h], xmm0
  @0x1407AC240 in fn 0x1407ABF60 sub_1407ABF60 CAM-CLUSTER  | movups  xmmword ptr [rsi+540h], xmm1
  @0x140C96830 in fn 0x140C96630 sub_140C96630   | movups  xmmword ptr [r12+540h], xmm1
  @0x140C99830 in fn 0x140C98BA0 sub_140C98BA0   | movups  xmmword ptr [rcx+540h], xmm0
  @0x14103E13F in fn 0x14103E120 sub_14103E120   | movups  xmmword ptr [rcx+540h], xmm0
  @0x141033163 in fn 0x0 ?   | movups  xmmword ptr [rcx+540h], xmm0

  -- decompile cam-cluster movups writers of +0x540 to see if same-this as FOLD --

  ### 0x1407A9880 sub_1407A9880 ###
    entry: push    rbx
    +0x540 write @0x1407A98AC: movups  xmmword ptr [rbx+540h], xmm5
    +0x540 write @0x1407A98F0: movups  xmmword ptr [rbx+540h], xmm3
    +0x540 write @0x1407A9962: movups  xmmword ptr [rbx+540h], xmm5
    +0x540 write @0x1407A99A6: movups  xmmword ptr [rbx+540h], xmm3
    vtable slots: ['0x145e3f5c0']

  ### 0x1407A9B60 sub_1407A9B60 ###
    entry: push    rbx
    +0x540 write @0x1407A9B85: movups  xmmword ptr [rcx+540h], xmm0
    +0x540 write @0x1407A9BA7: mov     [rcx+540h], eax
    vtable slots: ['0x145e3ef20']

  ### 0x1407ABF60 sub_1407ABF60 ###
    entry: mov     rax, rsp
    +0x540 write @0x1407AC240: movups  xmmword ptr [rsi+540h], xmm1
    vtable slots: ['0x145e41978']

=============== Q3c: does the FOLD or its wrappers write +0x540 before reading? ===============
  0x1407A6300 sub_1407A6300: +0x540 writes=[] reads=['0x1407a6382']
  0x1407A6430 sub_1407A6430: +0x540 writes=[] reads=[]
  0x1407A6DC0 sub_1407A6DC0: +0x540 writes=[] reads=[]
  0x1407A6DF0 sub_1407A6DF0: +0x540 writes=[] reads=[]
  0x1407A6E30 sub_1407A6E30: +0x540 writes=[] reads=[]
```
