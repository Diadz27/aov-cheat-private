# OFFSETS.md — live offset ledger (1.64 kgvn)
Status: GREEN = runtime-validated (needs 1 log) | YELLOW = disasm/dump, awaits log.
Dead values move to DEAD section, never delete.

| name | value | tier | status |
|---|---|---|---|
| SLOT_FWCLASS | 0xD0BBE00 | DISASM | YELLOW |
| CLASS_STATICFIELDS | 0xB8 | SRC+DISASM | YELLOW |
| FW_BATTLE | 0x68 | DUMP+DISASM | YELLOW |
| LBATTLE_GAMEMGR | 0xF8 | DUMP | YELLOW |
| L_MGR_HEROACTORS | 0x48 | DUMP+DISASM | YELLOW |
| LIST layouts | STD{10,18}/BIN{08,10} | CALIB | YELLOW |
| ARR layouts | STD{18,20}/BIN{10,18} | CALIB | YELLOW |
| HANDLE_OBJ/STRIDE | 0x08/16 | DISASM | YELLOW |
| L_ACT_TYPE/ISCOPY/LOC/VALUE/CONFIG | 68/2C/E0/338/378 | DUMP+DISASM | YELLOW |
| VPC_HP_A/B xor | 50/54 | DISASM+DUMP | YELLOW |
| CFG_CAMP | 0x38 | DUMP | YELLOW |
| STR layouts | BIN{08,0C} confirmed 3-source; STD fallback | CALIB | YELLOW |
| FN_ACTIVE_BATTLE 0x6E30808 | ref only (no calls) | DISASM | YELLOW |
| Skill CD fields | TBD | — | PHASE2 |
| Camera/W2S | TBD | — | PHASE2 |

## DEAD
- OFFSET_HERO_MANAGER 0x01B2F400 (placeholder era, replaced by chain)
- OFFSET_LOCAL_PLAYER 0x01A4B3C0 (placeholder era)
- cheat.dylib artifact name (renamed libUnityHelper.dylib, anti-signature)
- get_ActiveBattleLogic CALL design (killed L-trust: unattached thread)
