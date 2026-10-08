// config.h — AOV 1.64 (kgvn) ESP offsets
// Provenance tiers (rule: no number without a tier):
//   [DISASM] capstone ARM64 on stock 1.64 UnityFramework (function addrs,
//            load chains, xor formula, inline-Add stride)
//   [DUMP]   dump.cs (Il2CppDumper 6.7.46 on decrypted metadata v29)
//   [CALIB]  resolved at RUNTIME by dual-hypothesis validation (see below)
//   [STD]    universal il2cpp/mscorlib + runtime guards
// L17: static_off was miscomputed once (double-counted PC page) — every
//      disasm-derived address below was recomputed as ABSOLUTE page+imm
//      and segment-checked. Never hand-compute; use disx.py.
// L18: textbook (stock-Unity) layouts CONFLICT with this Tencent fork on
//      Array/List header. Code tries BOTH hypotheses at runtime and logs
//      which validates (LAYOUT=BIN|STD). Template files (il2cpp.h) are NOT
//      measurements — disasm + validation win.
//
// CORRECTED root chain [DISASM writer-hunt in ResetBattleLogic@0x6E304E4]:
//   P      = *(base + SLOT_FWCLASS)   // Il2CppClass* LFrameworkEditorProxy
//   S      = *(P + CLASS_STATICFIELDS) // static_fields region
//   FW     = *S                       // LFramework* (sole static `instance`)
//   battle = *(FW + FW_BATTLE)         // LMainBattleLogic : … : LBattleLogic
//   mgr    = *(battle + LBATTLE_GAMEMGR)
//   heroes = *(mgr + L_MGR_HEROACTORS)
// NOTE: slot does NOT hold LBattleLogic directly (earlier error, fixed L17).

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// ─── GAME VERSION ───
#define AOV_VERSION         "1.64.1.7-kgvn"
#define GAMEASSEMBLY_NAME   "UnityFramework"

// ─── ARTIFACT NAME (neutral naming rule) ───
#define DYLIB_NAME          "libUnityHelper.dylib"

// ─── STATIC SLOT (file offset in __DATA; +dyld base at runtime) ───
// [DISASM] adrp page 0xD0BB000 + 0xE00, segment-checked __DATA
#define SLOT_FWCLASS        0x0D0BBE00u
// [DISASM] Il2CppClass.static_fields == +0xB8 on Unity 2022.3.5f1
// (MlgmXyysd/libil2cpp 2022.3.5f1 il2cpp-class-internals.h)
#define CLASS_STATICFIELDS  0xB8u
// [DUMP+D] LFramework._battleLogic LMainBattleLogic @0x68
// (SetMainBattleLogic@0x4E04F10 stores arg at [x0,#0x68])
#define FW_BATTLE           0x68u

// ─── LBattleLogic ───
// [DUMP] LGameActorMgr <gameActorMgr>k__BackingField @0xF8 (owner LBattleLogic)
#define LBATTLE_GAMEMGR     0xF8u

// ─── LGameActorMgr (logic — all actors incl. hidden ones) ───
// [DUMP+DISASM] GetAllHeros = ldr x0,[x19,#0x48]; ret (no filter)
#define L_MGR_HEROACTORS    0x48u
// [DUMP] CampsHeroActors @0x90 (alt path, unused v1)
#define L_MGR_CAMPSHEROES   0x90u

// ─── NEW ROOT: KyriosFramework MonoSingleton (B2) ───
// [DISASM] get_actorManager @0x6E5368C fast path:
//   *0xD1082B0 → HasInstance, *0xD1082C0 → get_instance, then +0x28.
// S1/S2 hold RGCTX pointers (not class ptrs): resolved only after the game
// itself calls get_actorManager (prologue flag below). Pre-match S1==S2==0
// means NOT-INITED, not dead — log raw values to distinguish.
// 3-hop (same formula as old chain): slot=class → +0xB8=statics → +0x0=inst.
// NO minus-base variant (machine-verified: both slots are file offsets
// inside __DATA 0xCC98000-0xD554000; minus-0x100000000 is negative).
#define SLOT_KF_S1          0x0D1082B0ULL   // [DISASM] HasInstance rgctx slot
#define SLOT_KF             0x0D1082C0ULL   // [DISASM] get_instance rgctx slot
#define KF_PROLOGUE_FLAG    0x0D58F2C1ULL   // [DISASM] init-flag byte (prolog ran?)
#define KF_ACTOR_MGR        0x28u   // [DUMP] KyriosFramework._actorManager
#define KF_HERO_LIST        0x20u   // [DUMP] ActorManager.HeroActors (view list)
#define KF_HOST_LOGIC       0x50u   // [DUMP] KyriosFramework._hostLogic (H2 probe)

// ─── SKILL CD, LOGIC TREE (B1): enemy-safe (no fog gate on these fields) ───
// [DUMP+DISASM] actor+0x328 (LSkillComponent) → +0x88 (SkillSlot[] ref-array,
// stride 8, count=array header) → slot: ready u8 @0x6D, CD xor @0xFC/0x100
// (same ToInt eor as HP). View-tree @0x21/@0x50 is FOG-GATED — do not use
// for enemies. Ulti = slot idx 2 (idx 3 iff bIsFourSkillType); slot 3 =
// summoner. Scan slots 0..3 on device to confirm per hero.
#define L_ACT_SKILL         0x328u
#define LSKILL_SLOTS        0x88u
#define SKILL_CD_READY      0x6Du
#define SKILL_CD_CRYPTIC    0xFCu
#define V6_LOG_NAME         "v6.txt"

// ─── List<T> — DUAL hypothesis ([CALIB] picks winner at runtime) ───
// H_STD {items@0x10,size@0x18} (mscorlib textbook)
// H_BIN {items@0x08,size@0x10} (this fork: inlined-Add site evidence)
#define LIST_STD_ITEMS      0x10u
#define LIST_STD_SIZE       0x18u
#define LIST_BIN_ITEMS      0x08u
#define LIST_BIN_SIZE       0x10u
#define LIST_MAX_COUNT      64

// ─── Managed SZARRAY header — DUAL hypothesis ([CALIB]) ───
// H_STD {len@0x18,data@0x20} (textbook) / H_BIN {len@0x10,data@0x18} (disasm)
#define ARR_STD_LEN         0x18u
#define ARR_STD_DATA        0x20u
#define ARR_BIN_LEN         0x10u
#define ARR_BIN_DATA        0x18u

// ─── PoolObjHandle<T> (ref T) ───
// [DISASM] get_handle: ldr x0,[x0,#8]; ret. {u32 seq@0, pad, ptr@8}, stride 16.
// [DISASM] inlined List.Add: stp pair,[arr+idx*16+#0x18] (stride 16 confirmed)
#define HANDLE_OBJ          0x08u
#define HANDLE_STRIDE       16u

// ─── LActorRoot (logic actor) — [DUMP] ───
#define L_ACT_NAME          0x20u   // string (layout: see STR_* [CALIB])
#define L_ACT_TYPE          0x68u   // ActorTypeDef int32: Hero==0 (AddActor routing)
#define L_ACT_ISCOPY        0x2Cu   // u8 bIsCopyActor (0 = real)
#define L_ACT_LOCATION      0xE0u   // VInt3 fixed-point (x@0,y@4,z@8, 12B)
#define L_ACT_VALUE         0x338u  // ValuePropertyComponent
#define L_ACT_CONFIG        0x378u  // ActorConfigData (logic side)

// ─── VInt3 ───
// [DUMP] Precision=1000; [DUMP] _location@0xE0,_forward@0xEC (12B size proof)
#define VINT_SCALE          1000.0f

// ─── ValuePropertyComponent — [DUMP] _nObjCurHp @0x50 (CrypticInt32 8B) ───
// [DISASM] ToInt: ldp w8,w9,[x0]; eor w0,w9,w8 → value = A ^ B
#define VPC_HP_A            0x50u
#define VPC_HP_B            0x54u

// ─── ActorConfigData (logic, via L_ACT_CONFIG) — [DUMP] CmpType @0x38 ───
// (NOT the decoy RecycleableMsgBase ActorConfigData with CmpType@0x2C)
#define CFG_CAMP            0x38u
#define CFG_CONFIGID        0x30u   // ConfigID: hero-name key (phase 2)

// ─── ActorLinker (view) — [DUMP] ───
#define V_LINK_HOSTFLAG     0x1B0u  // mIsHostCtrlActor u8
#define V_LINK_OBJLINKER    0x128u  // ActorConfig
#define V_LINK_LOCATION     0x18Cu  // VInt3
// ─── ActorConfig (view, via ObjLinker) — [DUMP] CmpType @0x20 ───
#define V_CFG_CAMP          0x20u

// ─── Il2CppString — DUAL hypothesis ([CALIB]) ───
// H_STD {len@0x10,chars@0x14} / H_BIN {len@0x8,chars@0xC}
#define STR_STD_LEN         0x10u
#define STR_STD_CHARS       0x14u
#define STR_BIN_LEN         0x08u
#define STR_BIN_CHARS       0x0Cu
#define STR_MAX             64

// ─── COM_PLAYERCAMP — [DUMP] enum + [DISASM] IsBlueCamp: camp==1 ───
#define CAMP_MID            0u
#define CAMP_BLUE           1u
#define CAMP_RED            2u
// self resolved at runtime (never hardcoded); enemy = other of {1,2}

// ─── Skill slots (PHASE 2; offsets known, cooldown fields TBD) ───
// [DUMP] ActorLinker.SkillControl @0x18; SkillComponent.SkillSlotArray @0x30;
// SkillSlot.SkillObj @0xB8
#define V_LINK_SKILL        0x18u
#define SKILL_SLOTARRAY     0x30u
#define SLOT_SKILLOBJ       0xB8

// ─── LOG (VERIFY builds only; RELEASE defines RELEASE_BUILD) ───
// Neutral filename (neutral filename rule).
#define SYNC_LOG_NAME        "sync_state.txt"
#define SYNC_LOG_CAP         65536
#define TICK_RATE_US        200000
#define INIT_DELAY_SEC      6
#define SELF_REFRESH_TICKS  25

#endif
