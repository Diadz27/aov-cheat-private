# memory.md — AOV iOS 1.64 cheat project log
Cập nhật sau MỖI bước. Compact thoải mái, file này giữ mạch.

## Mục tiêu
Map hack (full vision + bụi) + ESP + camera + skin + cooldown cho AOV iOS 1.64,
ESign non-jailbreak. Lock version 1.64.

## Trạng thái (2026-10-02)
- Phase 1/1.5 xong: cheat.c (resolver UnityFramework + retry), config.h (placeholder),
  memory.h, repack_ipa.py (inject in-place, target UnityFramework) — commit 319e05d.
- Workflow Actions build 2 dylib, đã xanh. Push thông (remote main).
- MrPewiOS free reverse xong: dylib = Shopee affiliate adware, 0 offset. Đường này chết.
- Metadata đĩa mã hóa stock (magic 0xEAB11BAF cả mod lẫn trắng). Il2CppDumper tĩnh chết.
- Type inventory 1.64: 1471 types / 243 interesting (Hero, VisionSwitch, VisionUtility,
  LocalPlayer, CHeroSkin...) — mrpewrev/type_inventory_164.txt.
- 0 il2cpp_* exports — bắt buộc offset cứng từ dump.cs.
- Test thực địa v1: game văng 60–80s sau sảnh, 0 dòng [META] (printf tàng hình).
  AMFI entitlement warning = lành tính.

## Bài học (tự rút)
1. printf trên iOS không debugger = tàng hình. Luôn os_log + %{public}.
2. vm_read trả cc làm tròn page — memcpy phải MIN(cc, còn-lại). Tràn heap = crash muộn.
3. Luôn ghi marker file (sống/chết đều có dấu) — không phụ thuộc log.
4. Commit format: code + workflow + inventory cùng commit, push ngay khi collaborator mở.
5. IPA mod và trắng khác version (1.63 vs 1.64) thì diff vô nghĩa — lock 1 version từ đầu.
6. SỐ LIỆU CHỈ TỪ MÁY: vụ "226MB" nhẩm từ 226248785 bytes (thực 215.8MB). Cấm nhẩm mọi con số.
7. PIVOT 2026-10-03: stock 1.64 văng cả khi trắng (dylib vô can) → dùng MOD 1.64
   (chạy được, hack thật) làm base. Không patch stock nữa.
8. QH = menu+network (43 imports UI/HTTP, 0 API chạm game, 0 svc, __text 5.6KB).
   Shopee dylib = rác chết (không ai nạp). Map hack = 4 patch __TEXT trong
   UnityFramework (mrpewrev/patches_164.txt). Không tốn thêm giờ vào 2 file này.
9. Metadata mod = stock (cùng 41278340B, lệch 14B header) → hack không đụng metadata.
   Stock văng không phải do metadata.
10. L8: CẤM gõ tay bytes vào code — vụ P1 OLD (a20016aa sai vs f60302aa đúng) và P2 OLD.
    apply_patches.py đọc bảng patches_164.txt lúc chạy. Mọi bytes đều từ máy.
11. .bak lọt vào IPA lần đầu → repack skip *.bak khi zip. stock164-patched.ipa
    226248868B: 4/4 patch = NEW bytes verify trong IPA cuối, 0 .bak, 570 entries.
12. L9: ĐÍNH CHÍNH claim "runtime patch = SIGKILL chắc chắn" — SAI với ESign thực tế.
    Scene sống bằng runtime vm_write (joeyjurjens/HuyJIT/iOSGods non-JB). Static = an
    toàn nhất (giữ), runtime = chuẩn công nghiệp (mở lại, không loại).
13. STRUCT FACTS v29 (3 nguồn độc lập): FieldDefinition 12B {name,type,token}, KHÔNG có
    offset trong file. Offset thật ở runtime fieldOffsets/FieldInfo. Il2CppClass name@0x10.
    → v5-route-B: chuỗi → class → fields. File research_dump_offsets.md.
14. DECRYPT + DUMP.CS 2026-10-05 (toàn bộ trên PC, 0 test máy — user dừng test hẳn):
    fork dsgaming-mrd/Il2CppDumper-GUI-AoV có MetadataDecrypter.cs: XOR 0xA8C72D,
    24 group, even out=(in-3g)^(KEY+g) / odd (in-7g)^(KEY+2g), magic EAB11BAF→FAB11BAF.
    Python port (h9_decrypt.py) OK: global-metadata.decrypted.dat, ver 29, header 0 BAD.
    Perfare 6.7.46 dump được: dump.cs 70834105B + il2cpp.h 135649904B +
    script.json 173841397B + stringliteral.json 4798948B. Chỉ chết DummyDll
    (Invalid compressed integer — không cần). Bẫy: phải tạo TRƯỚC thư mục out,
    nếu không FileStream.Create ném và không có file nào (mất 2 vòng mới ra).
15. ESP CHAIN 1.64 (machine-verified từng mắt, dump.cs + capstone 5.0.7 disasm binary
    stock): battle=get_ActiveBattleLogic()(CALL base+0x6E30808) → +0xF8 LGameActorMgr →
    +0x48 HeroActors List → items@0x10/size@0x18 (mscorlib STD+guard) → array
    len@0x10/data@0x18 stride 8 ref ([DISASM] GetCampHeroActors@0x4A1FCB8) → handle+8
    ([DISASM] get_handle ldr x0,[x0,#8]) → LActorRoot: pos VInt3@0xE0 /1000,
    camp=*(cfg@0x378+0x38), hp=*(vpc@0x338+0x50)^*(+0x54) ([DISASM] ToInt eor),
    self=view ActorManager(CALL base+0x6E6C5C8)+0x20 → mIsHostCtrlActor@0x1B0.
    GetAllHeros@0x4A1F9B8 = ldr x0,[x19,#0x48];ret (không lọc). config.h viết lại
    toàn bộ theo số máy. cheat.c: scan_heroes dùng chain thật + mem_probe chống crash;
    speed/mana/aimbot-write/macro/camera VÔ HIỆU (offset chưa verify — cấm ghi mù).
16. CHƯA VERIFY: List._items@0x10/_size@0x18 (STD mscorlib + guard runtime, chưa disasm
    riêng); toàn bộ chain chưa chạy trên máy thật (user dừng test). CI build dylib
    chỉ chứng minh compile, KHÔNG chứng minh offset đúng.
17. L17: static_off từng tính SAI (0x13EEBE00 do cộng 2 lần page PC) — capstone in ADRP
    là page TUYỆT ĐỐI. Số đúng: ActiveBattleLogic slot 0xD0BBE00, actorManager
    0xD1082B0/2C0. Luật mới: cấm tính tay, mọi địa chỉ qua disx.py + segment-check.
18. L18: static slot KHÔNG chứa LBattleLogic — trinh sát lùng KẺ GHI (Reset@0x6E304E4)
    ra: slot=Il2CppClass* LFrameworkEditorProxy → +0xB8 static_fields →
    [0]=LFramework* (sole static) → +0x68 _battleLogic (SetMain@0x4E04F10 ghi).
    Luật: static slot phải có writer-disasm, không thì cấm dùng.
19. L19: array/List header: sách (len@0x18/data@0x20) vs binary (len@0x10/data@0x18,
    2 điểm disasm + stride-16 inline-Add). il2cpp.h là TEMPLATE dumper, không phải
    số đo. Giải pháp: dual-hypothesis, máy tự chọn lúc chạy, log in LAYOUT=.
    String layout cũng dual (tên chỉ để hiển thị). FogOfWar._enable@0x8 gợi ý
    header object build này chỉ 8B — bỏ sách stock-Unity.
20. L20 (sửa L7): "trắng văng, dylib vô can" là YẾU — không có biên bản trắng-sạch
    (không số giây/Jetsam/ips), vụ văng duy nhất có giờ là bản CÓ dylib. Trắng =
    CHƯA kiểm chứng (không phải đã chết). Xe = mod base (đã chứng minh sống).
21. L21 anti-cheat/ban (số thật): binary link anogs/anort/DataDome + chuỗi dyld-enum +
    MTML_INTEGRITY_DETECT → dylib lạ chắc chắn bị liệt kê/upload. Tên file là chữ
    ký (đổi libUnityHelper.dylib, strip, hidden). Garena T7/2024: 87379 acc ≥6mo,
    76360 acc ≥3yr; khung hack map 3yr→perm; wave theo tháng, không kick ngay;
    đọc thuần vô hình server, report là sát thủ #1. Build VERIFY≠RELEASE.
    Tooling: SKIP CodeGraph/speckit/repomix (repo nhỏ, 70MB dump giết context);
    GIỮ memory.md+TodoWrite+3 script.     Env user: iOS 26+ / cert mua / giữ version.
22. L22 (quyết định khóa): HYBRID-PLUS giữ nguyên; trắng BỎ hẳn (dự phòng trong RISK);
    tool riêng KHÔNG build (v6 trong dylib thay thế); push build NỚI (tự push commit
    build); ống season_update.py ĐỂ phase sau (tránh scope creep); string BIN
    {len@0x8,chars@0xC} CONFIRMED 3 nguồn (get_Length/get_Chars/dump) — code đọc
    sai 0x10/0x14 đã sửa + khóa str theo trial; List BIN {items@0x8,size@0x10}
    (BetterList leaf + census 1016v363); array BIN giữ (2 điểm disasm).

## Quyết định đang hiệu lực
- Lock 1.64. ACE escalate theo lunar (stealth → RE anogs/anort → patch tĩnh).
- Dọn: MrPewiOS.ipa + 2 folder artifact (giữ installer 3uTools, giữ zip).
- Không xóa ngoài D:\LQMB BY CLAUDE + TEMP khi chưa hỏi. Push giữ pattern confirm.

## Hàng đợi
- [x] meta_extract v3 (rescan 20x60s, full-region, marker/vòng) — commit 25acea7, đã push
- [x] apply_patches + --map-patches (commit 4e54712, push cùng 25acea7 theo duyệt plan)
- [x] Actions xanh → v3 verify strings (round-based, hết dấu v2)
- [x] repack mod164-y.ipa 218386884B: v3 trong Frameworks, 130 LC đủ QH + meta_extract
- [x] v4 (commit dbd34aa, đã push): camouflage + rc/born/awake + fallback quick-scan,
      %{public} toàn bộ, Documents cho mọi output. Fix 11 điểm spec Claude (ObjC chết,
      thiếu include/MIN, block→dispatch_after_f, save sai path, fallback treo main).
- [x] v4 verify 6 markers (67456B) → mod164-v4.ipa 218388035B: v4 trong
      Frameworks, 130 LC đủ QH + meta_extract
- [x] v5-route-B final: file nguyên văn spec, checker 9/9, workflow đã có block v5
- [x] v5 verify markers (67296B) → mod164-v5.ipa 218387953B: v5 trong Frameworks,
      130 LC đủ QH + meta_extract
- [ ] user 1 trận bất kỳ → log FIELD (Class.field = 0xoffset) về
- [x] Dọn đợt 2 (~813MB): mod164-x/y/v4 + cheat zip + v4 zip + installer 3uTools.
      Giữ: stock (chuẩn), TipTip (nguồn repack), mod164-v5 (xe), v5 zip+dylib, iMazing (dự phòng).
- [ ] user solo 1v1 vs người + log [META] + pull 2 file (decrypt + marker log)
- [ ] dump.cs → config.h → ESP dylib → gộp base → test
- [x] Il2CppDumper → dump.cs (70.8MB) → mining offset → config.h 1.64 (verified chain)
- [x] cheat.c ESP reader + mem_probe guards (compile chờ CI; runtime chưa test — user dừng test)
- [x] Vòng tự học 1-5 (5 agents/lượt): thread-attach→static-read, adrp tuyệt đối,
      writer-hunt khép chain, LC vừa 2984B/64B, ESign sống tới iOS 26, clone/camp/dead,
      ban Garena số thật, pattern-scan thua dump+RVA, SKIP CodeGraph/speckit
- [x] P0 build mode: config.h tầng nhãn + chain đúng + dual-hypothesis; cheat.c
      static-read + lọc clone + camp động + log xoay vòng sync_state.txt + v6 tên-thật;
      libUnityHelper.dylib (đổi tên, strip, hidden); 4 ledger CHAIN/OFFSETS/SEASON/RISK
- [x] P0a fresh-eyes: string BIN{08,0C} CONFIRMED 3 nguồn (sửa code đọc sai 10/14),
      neutral naming toàn bộ, v6 info_count reset/loop, CFG_CONFIGID, SYNC_LOG_*,
      reader_loop/get_battle; commit 55b306f + PUSH main (luật nới) — CI đang build
- [ ] P1: tải libUnityHelper artifact (cần gh auth hoặc user tải tay) → repack
      mod164-esp.ipa → verify PC → cất tủ + giấy 6 bước (NGHẼN Ở ARTIFACT)
- [x] P1 DONE build mode: user tải libunityhelper-arm64.zip (4635B) → dylib 66920B
      ARM64 thin verified → repack mod164-v5.ipa (KHÔNG --map-patches, base đã hack)
      → mod164-esp.ipa 214652163B/428 entries: dylib bytes identical, LC ncmds 131,
      0 .bak, Info.plist OK + HUONGDAN-ESP-LOG.txt. Lỗi console cp1252 khi in log
      repack (chữ Việt) — fix bằng PYTHONIOENCODING=utf-8 (bài học mới).
- [x] P1b: CI build VERIFY 67072B + RELEASE 66768B (v6/log biến mất khỏi RELEASE —
      phát hiện SYNC_LOG_NAME còn sót, đã gate sạch); repack v2 = mod164-esp-v2.ipa
      214652570B: only-added đúng 1 dylib, missing 0, LC 131. Push 129a45d.
- [x] Dọn đợt 3 (user duyệt): iMazing 216196488B + esp-v1 214652163B + TipTip
      226283747B + meta_extract.dylib root + zip cũ → NHẸ 657204768B (626.8MB).
      SỰ CỐ: xóa nhầm libunityhelper-arm64.zip mới (trùng tên với bản cũ đã bị ghi
      đè từ trước) — KHÔI PHỤC ngay từ dylib gốc trong TEMP, verify sha256 identical.
      Bài học: xóa file trùng tên phải kiểm tra ngày/size trước, không tin tên.
- [x] HUONGDAN-ESP-LOG-v2.txt (giấy mới: chuẩn bị/ký/tin cậy iOS26/tải gói/mode 5v5/
      kéo 2 file/đọc nhanh + OPSEC acc rác).
- [ ] skill CD fields (SkillSlot full block) + camera/W2S (phase 2)
- [ ] self-resolve (view static có bl-transform, chưa replicate được — KHÔNG heuristic giả)
- [ ] push commit → CI build libUnityHelper → tải về → repack mod164-esp.ipa (CHỜ DUYỆT PUSH)
- [ ] 1 log sync_state.txt duy nhất (AFK 1 trận): LAYOUT=? V6HIT? n=?
- [x] Crash loading@tải gói 63MB: repack forensics SẠCH (chỉ +dylib/1 LC, __TEXT
      identical); dylib không có đường crash chắc chắn pre-battle (chỉ còn khe
      TOCTOU hẹp); tra cứu ngoài: Jetsam OOM lúc tải/bung asset là hung thủ quen
      thuộc nhất đúng triệu chứng. Nghi: Jetsam/game-base > dylib > ký (loại).
- [x] Hardening sau crash: alive.txt (đèn báo sống mỗi 25 tick) + pre-battle backoff
      1s (gần 0 tải lúc loading/download); giữ nguyên logic ESP.
- [x] B1→B4 một mạch (audit 29 lỗi E1-E29): v6 chunk 64KB (hết liệt), log tmp+rename,
      camp giữ 1..8 có lý do (mode khác), mảng heroes[64], alive cả lúc đánh, khóa
      layout 25 tick, name single-copy; repack: chặn LC trùng, patch-trước-inject-sau,
      giữ nguyên metadata zip (method/quyền), cảnh báo ký ESign; workflow: job RELEASE,
      retention 90d, bỏ UIKit thừa. Dead mem_read_* giữ (warning only).
- [ ] aimbot/macro + antiban nâng cao (conditional)
22. L22 (quyet dinh khoa): HYBRID-PLUS giu nguyen; trang BO han (du phong trong RISK);
    tool rieng KHONG build (v6 trong dylib thay the); push build NOI (tu push commit
    build); ong season_update.py DE phase sau (tranh scope creep); string BIN
    {len@0x8,chars@0xC} CONFIRMED 3 nguon (get_Length/get_Chars/dump) - code doc
    sai 0x10/0x14 da sua + khoa str theo trial; List BIN {items@0x8,size@0x10}
    (BetterList leaf + census 1016v363); array BIN giu (2 diem disasm).
23. L23 selftest GREEN (17/17): decrypt->mine->slot->emit khop config.h da commit.
    Sua 3 loi do selftest bat duoc: (a) block() khop nham class long nhau
    (them negative-lookahead + loc base decoy ActorConfigData); (b) methods_rva
    khong an decl co '{ }' cung dong; (c) get_actorManager thuoc ve KyriosFramework
    (KHONG phai ActorManager) - V_MGR_SLOT cu tu ham nhan nham -> danh UNKNOWN,
    view path hoan chinh thuc (v1 khong dung view). M1 vm_read retrofit committed.
24. L24 LUẬT MỚI (user): mỗi lần xong → tạo HOW TO USE.txt duy nhất, XÓA giấy cũ.
    Đã gộp 2 HUONGDAN thành HOW TO USE.txt (v2 + vá hành trình) + xóa 2 file cũ.
