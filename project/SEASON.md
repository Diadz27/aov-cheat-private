# SEASON.md — re-derivation recipe (copy-paste, PC only, ~30min/season)
1. Decrypt: `python h9_decrypt.py <meta_enc> <meta_dec>` (key 0xA8C72D; if magic
   != 0xFAB11BAF after: brute-force 1B key, 256 tries, re-validate header).
2. Dump: `Il2CppDumper.exe UnityFramework <meta_dec> out` (mkdir out FIRST;
   config: GenerateDummyDll=false). Expect dump.cs + script.json.
3. Mine: `python dumpq.py LGameActorMgr LActorRoot LBattleLogic LFramework LFrameworkEditorProxy`
   → refresh LBATTLE_GAMEMGR / L_MGR_HEROACTORS / L_ACT_* / FW_BATTLE.
4. Grep dump.cs for `get_ActiveBattleLogic` RVA → `python disx.py 0xRVA 60`
   → adrp page+imm = new SLOT_FWCLASS (ABSOLUTE page, never +PC page; segment-check).
   Same for get_actorManager (view, best-effort).
5. Grep `class LMainBattleLogic :` chain + `SetMainBattleLogic` RVA → confirm +0x68 store.
6. Update OFFSETS.md table + CHAIN.md edges. Rebuild via CI. Done — no device needed
   until the 1 verification log.
Holes: wrong CR/MR → garbage dump (validate 1 known method disasm first);
inlined getters (no standalone RVA → pick nearest caller); static reorder (re-derive all).
