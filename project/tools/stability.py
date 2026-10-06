#!/usr/bin/env python3
# stability.py — MUST-name lists + abort conditions (novelty detector)
# Imported by season_update.py and selftest_164.py. No guesses, only stops.
import sys
sys.path.insert(0, __import__('os').path.dirname(__file__))
from slot_mine import block  # noqa: E402

# classes that MUST exist in dump.cs or the season is too different
MUST_CLASSES = [
    'LBattleLogic', 'LGameActorMgr', 'LActorRoot', 'LFramework',
    'LFrameworkEditorProxy', 'KyriosFramework', 'ActorManager', 'ActorLinker',
    'ValuePropertyComponent',
]

# (class, [fields that MUST exist]) — aborts if any missing
MUST_FIELDS = {
    'LBattleLogic': ['gameActorMgr'],
    'LGameActorMgr': ['HeroActors'],
    'LActorRoot': ['_location', 'SkillControl', 'ValueComponent', 'actorConfig'],
    'LFramework': ['_battleLogic'],
    'LFrameworkEditorProxy': ['instance'],
    'ValuePropertyComponent': ['_nObjCurHp'],
    # SkillComponent/SkillSlot offsets are phase-2 (one SkillComponent revision
    # lacks offset comments); class existence above is enough for v1.
}

# (class, method-substring that MUST resolve to an RVA)
# NOTE: get_ActiveBattleLogic lives on LFrameworkEditorProxy (static holder),
# NOT on LBattleLogic (verified by writer-hunt disasm). get_actorManager lives
# on KyriosFramework (NOT ActorManager — verified by enclosing block).
MUST_METHODS = {
    'LFrameworkEditorProxy': ['get_ActiveBattleLogic'],
    'KyriosFramework': ['get_actorManager'],
    'LGameActorMgr': ['GetAllHeros'],
}


def check_dump(t):
    from slot_mine import fields, methods_rva
    bad = []
    for cls in MUST_CLASSES:
        if block(t, cls) is None:
            bad.append('missing class ' + cls)
    for cls, fs in MUST_FIELDS.items():
        b = block(t, cls)
        if b is None:
            continue
        have = fields(b)
        for f in fs:
            if f not in have and f.strip('<>') not in str(list(have)):
                # backing-field tolerance: <gameActorMgr>k__BackingField
                if not any(f in k for k in have):
                    bad.append('missing field %s.%s' % (cls, f))
    for cls, ms in MUST_METHODS.items():
        b = block(t, cls)
        if b is None:
            continue
        meths = methods_rva(b)
        for m in ms:
            if not any(m in k for k in meths):
                bad.append('missing method %s.%s' % (cls, m))
    return bad
