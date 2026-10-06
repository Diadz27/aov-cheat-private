# RISK.md — barrier register (append-only)
| barrier | symptom | bypass tried | next test |
|---|---|---|---|
| unattached-thread call crash | SEGV on getter call | L-trust: static-reads only, zero calls | closed by design |
| export-trie absent (no il2cpp_*) | dlsym NULL | static slots from disasm | closed |
| out-dir trap (Perfare) | empty output | mkdir out first | closed |
| ghost names (v5 11 rounds 0 hits) | string hunt empty | real names (v6), metadata-only fact | 1 log: V6HIT? |
| white-IPA crash L7 | claim w/o forensics | WEAK verdict; mod base proven | 1 clean-white test IF ever needed |
| array/List header conflict | textbook vs binary | dual-hypothesis CALIB, log prints winner | 1 log: LAYOUT= |
| string layout unvalidated | names blank | dual-string, display-only | 1 log: nm=? |
| self unresolved | no enemy color | NO fake heuristic; view static not replicable | camera or deeper RE (phase 2) |
| ACE dyld-enum + hash upload | wave ban risk | innocuous name, strip, hidden, read-only, VERIFY≠RELEASE | alt account, OPSEC |
| free-cert revoke | install dies | paid cert (user has) | closed |
| version drift | offsets rot | user holds versions + SEASON.md | closed for now |
