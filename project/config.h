// config.h — AOV 1.64 (kgvn) ESP offsets — MACHINE VERIFIED
// Sources:
//   [DUMP]   dump.cs via Il2CppDumper 6.7.46 on DECRYPTED metadata
//            (XOR 0xA8C72D + group transforms, magic EAB11BAF->FAB11BAF)
//   [DISASM] capstone ARM64 disassembly of stock 1.64 UnityFramework
//   [STD]    universal il2cpp/mscorlib layout + runtime guards in cheat.c
// Rule: every number below traces to machine output. No hand-typed guesses.
//
// ESP chain (logic layer = all actors, fog-independent):
//   battle = get_ActiveBattleLogic()          // CALL base+FN_ACTIVE_BATTLE
//   mgr    = *(battle + LBATTLE_GAMEMGR)      // LGameActorMgr
//   heroes = *(mgr + L_MGR_HEROACTORS)        // List<PoolObjHandle<LActorRoot>>
//   walk List -> items array -> handle._handleObj -> LActorRoot
//   pos  = VInt3(actor+L_ACT_LOCATION)/1000
//   camp = *(u32*)(*(actor+L_ACT_CONFIG)+CFG_CAMP)
//   hp   = *(u32*)(vpc+VPC_HP_A) ^ *(u32*)(vpc+VPC_HP_B), vpc=*(actor+L_ACT_VALUE)
// Self (view layer): ActorManager.HeroActors -> ActorLinker.mIsHostCtrlActor

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// ─── GAME VERSION ───
#define AOV_VERSION         "1.64.1.7-kgvn"
#define GAMEASSEMBLY_NAME   "UnityFramework"

// ─── RUNTIME FUNCTION ENTRY POINTS (file offsets; +dyld slide at runtime) ───
// [DISASM] LBattleLogic.get_ActiveBattleLogic @0x6E30808 — static, no args,
//          returns LBattleLogic* (null when no battle)
#define FN_ACTIVE_BATTLE      0x06E30808u
// [DISASM] ActorManager.get_actorManager @0x6E6C5C8 — static, no args
#define FN_ACTOR_MANAGER      0x06E6C5C8u

// ─── LBattleLogic ───
// [DUMP] LGameActorMgr <gameActorMgr>k__BackingField // 0xF8 (owner LBattleLogic)
#define LBATTLE_GAMEMGR       0xF8u

// ─── LGameActorMgr (logic, sees ALL actors incl. fogged) ───
// [DUMP] + [DISASM] GetAllHeros fast path: ldr x0,[x19,#0x48]; ret
#define L_MGR_HEROACTORS      0x48u
// [DUMP] CampsHeroActors // 0x90  (List[] per camp, alt path)
#define L_MGR_CAMPSHEROES     0x90u
// [DUMP] AllActorList // 0x40
#define L_MGR_ALLACTORS       0x40u

// ─── ActorManager (view, visible actors only) ───
// [DUMP] HeroActors // 0x20
#define V_MGR_HEROACTORS      0x20u

// ─── List<T> (mscorlib, universal) ───
// [STD] _items@0x10 _size@0x18 — guarded at runtime (count 0..64, ptr probe)
#define LIST_ITEMS            0x10u
#define LIST_SIZE             0x18u
#define LIST_MAX_COUNT        64

// ─── Managed SZARRAY header (this binary's il2cpp) ───
// [DISASM] GetCampHeroActors: len=ldr w9,[x8,#0x10]; elem=ldr x0,[x8+idx*8,#0x18]
#define ARRAY_LENGTH          0x10u
#define ARRAY_DATA            0x18u

// ─── PoolObjHandle<T> (reference T) ───
// [DISASM] PoolObjHandle<object>.get_handle: ldr x0,[x0,#8]; ret
// layout {u32 seq @0, pad, ptr obj @8}, stride 16
#define HANDLE_OBJ            0x08u
#define HANDLE_STRIDE         16u

// ─── LActorRoot (logic actor) ───
// [DUMP] all below
#define L_ACT_NAME            0x20u   // string (Il2CppString)
#define L_ACT_LOCATION        0xE0u   // VInt3 fixed-point (x@0,y@4,z@8)
#define L_ACT_SKILL           0x328u  // LSkillComponent
#define L_ACT_VALUE           0x338u  // ValuePropertyComponent
#define L_ACT_CONFIG          0x378u  // ActorConfigData (logic side)
#define L_ACT_GAMEOBJ         0x410u  // LEngineGameObject (Unity pos, phase 2)

// ─── VInt3 fixed point ───
// [DUMP] Precision = 1000; [DISASM-consistent] {x@0,y@4,z@8} size 12
#define VINT_SCALE            1000.0f

// ─── ValuePropertyComponent ───
// [DUMP] _nObjCurHp @0x50 (CrypticInt32, 8B)
// [DISASM] CrypticInt32.ToInt: ldp w8,w9,[x0]; eor w0,w9,w8; ret
//          => value = *(u32*)(p) ^ *(u32*)(p+4)
#define VPC_HP_A              0x50u
#define VPC_HP_B              0x54u
#define VPC_EP_A              0x58u
#define VPC_EP_B              0x5Cu

// ─── ActorConfigData (logic side, via L_ACT_CONFIG) ───
// [DUMP] CmpType // 0x38
#define CFG_CAMP              0x38u

// ─── ActorLinker (view side) ───
// [DUMP] mIsHostCtrlActor // 0x1B0 ; ObjLinker(ActorConfig) // 0x128
// [DUMP] _location VInt3 // 0x18C (view position, fixed-point)
#define V_LINK_HOSTFLAG       0x1B0u
#define V_LINK_OBJLINKER      0x128u
#define V_LINK_LOCATION       0x18Cu
// ─── ActorConfig (view side, via ObjLinker) ───
// [DUMP] CmpType // 0x20 ; IsHostPlayer // 0x26
#define V_CFG_CAMP            0x20u
#define V_CFG_ISHOST          0x26u

// ─── Il2CppString ───
// [STD] len u32 @0x10, UTF-16 chars @0x14
#define STR_LEN               0x10u
#define STR_CHARS             0x14u
#define STR_MAX               64

// ─── COM_PLAYERCAMP (5v5: 1 vs 2) ───
// [DUMP] enum: MID=0, CAMP_1=1, CAMP_2=2
#define CAMP_NONE             0u
#define CAMP_1                1u
#define CAMP_2                2u

// ─── Skill slots (PHASE 2 — offsets known, cooldown fields TBD) ───
// [DUMP] ActorLinker.SkillControl @0x18 (SkillComponent)
// [DUMP] SkillComponent.SkillSlotArray @0x30 ; SkillSlot.SkillObj @0xB8
#define V_LINK_SKILL          0x18u
#define SKILL_SLOTARRAY       0x30u
#define SLOT_SKILLOBJ         0xB8

// ─── CHEAT CONFIG ───
#define TICK_RATE_US             200000
#define INIT_DELAY_SEC           6

#endif
