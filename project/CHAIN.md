# CHAIN.md — ESP root chain (verified, 1.64 kgvn, Unity 2022.3.5f1)
Boot-read file (<=80 lines). Format: edge + class:member + offset + tier + evidence.

```
base=dyld UnityFramework header (__TEXT.fileoff==0 → base+fileoff) [DISASM macho]
P      = *(base + 0xD0BBE00)      Il2CppClass* LFrameworkEditorProxy [DISASM adrp+segcheck]
S      = *(P + 0xB8)              static_fields (2022.3 class-internals) [SRC+DISASM writer]
FW     = *S                       LFramework* (sole static `instance`) [DUMP L2058 + writer Reset@0x6E304E4]
battle = *(FW + 0x68)             LMainBattleLogic:LFrameSync:LBattleLogic [DUMP+D SetMain@0x4E04F10 str+0x68]
mgr    = *(battle + 0xF8)         LGameActorMgr (gameActorMgr, owner LBattleLogic) [DUMP]
heroes = *(mgr + 0x48)            List<PoolObjHandle<LActorRoot>> (GetAllHeros=return field) [DUMP+DISASM]
items/size = List walk, DUAL layout, validated at runtime [CALIB]
array len/data DUAL, stride 16, handle.obj@+8 [DISASM GetCampHeroActors + inline-Add]
actor  = LActorRoot:
  type@0x68==0 (Hero)             [DISASM AddActor routing]
  copy@0x2C==0                    [DUMP bIsCopyActor]
  camp = *(cfg@0x378 + 0x38)      ActorConfigData.CmpType (not decoy@0x2C) [DUMP]
  hp   = *(u32*)(vpc@0x338+0x50) ^ *(u32*)(+0x54)  CrypticInt32.ToInt=eor [DISASM+DUMP]
  pos  = VInt3@0xE0 /1000 (12B: _forward@0xEC gap proof) [DUMP]
  cfg  = ConfigID@+0x30 (name key) [DUMP]; name dual-string (display only)
self   = OPEN (view static has bl-transforms, not replicable; no fake heuristic)
blue   = camp 1 (IsBlueCamp: cmp w19,#1) [DISASM]; self camp never hardcoded
```
Ghost names (0 defs in dump.cs — never hunt): HeroManager VisionSwitch VisionUtility SkillManager LocalPlayer.
Real names (metadata-only, 0 in binary): LActorRoot LGameActorMgr ActorManager LBattleLogic HeroActors.
