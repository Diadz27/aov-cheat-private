// config.h — AOV Cheat Configuration
// PLACEHOLDER OFFSETS — cần dump Il2CppDumper thay thế
// Version: AOV Latest (update khi game patch)

#ifndef CONFIG_H
#define CONFIG_H

// ─── GAME VERSION ───
#define AOV_VERSION         "1.0.0-placeholder"

// ─── IL2CPP BASE ───
// Resolved at runtime — không hardcode
#define GAMEASSEMBLY_NAME   "UnityFramework"

// ─── HERO MANAGER ───
#define OFFSET_HERO_MANAGER      0x01B2F400
#define OFFSET_HERO_LIST_COUNT   0x18
#define OFFSET_HERO_LIST_ITEMS   0x10
#define OFFSET_HERO_ARRAY_START  0x20

// ─── HERO OBJECT ───
#define OFFSET_HERO_POS_X        0x0230
#define OFFSET_HERO_POS_Y        0x0234
#define OFFSET_HERO_POS_Z        0x0238
#define OFFSET_HERO_IS_ENEMY     0x0244
#define OFFSET_HERO_HP           0x0248
#define OFFSET_HERO_MAX_HP       0x024C
#define OFFSET_HERO_NAME         0x025C
#define OFFSET_HERO_TEAM_ID      0x0260

// ─── LOCAL PLAYER ───
#define OFFSET_LOCAL_PLAYER      0x01A4B3C0
#define OFFSET_MOVE_SPEED        0x01A0
#define OFFSET_PLAYER_MANA       0x015C
#define OFFSET_PLAYER_MAX_MANA   0x0160
#define OFFSET_ATTACK_TARGET     0x02A0
#define OFFSET_ATTACK_RANGE      0x02A8

// ─── SKILL MANAGER ───
#define OFFSET_SKILL_MANAGER     0x0310
#define OFFSET_SKILL_CD_BASE     0x0040
#define OFFSET_SKILL_CD_STRIDE   0x10
#define OFFSET_CASTSKILL_METHOD  0x00C4B200

// ─── CAMERA ───
#define OFFSET_CAMERA_MGR        0x01C3A000
#define OFFSET_CAMERA_HEIGHT     0x0080

// ─── SKIN ───
#define OFFSET_SKIN_ID           0x0190

// ─── CHEAT CONFIG ───
#define SPEED_MULTIPLIER         1.3f
#define MANA_FREEZE_VALUE        9999.0f
#define CAMERA_HEIGHT_VALUE      0.7f
#define TICK_RATE_US             100000
#define INIT_DELAY_SEC           6

#endif
