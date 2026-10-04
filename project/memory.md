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

## Quyết định đang hiệu lực
- Lock 1.64. ACE escalate theo lunar (stealth → RE anogs/anort → patch tĩnh).
- Dọn: MrPewiOS.ipa + 2 folder artifact (giữ installer 3uTools, giữ zip).
- Không xóa ngoài D:\LQMB BY CLAUDE + TEMP khi chưa hỏi. Push giữ pattern confirm.

## Hàng đợi
- [x] meta_extract v2 (os_log/heartbeat/MIN-copy/marker/region-cap)
- [x] push 7562627 → Actions xanh → v2 dylib 66936B ARM64 verify OK
- [x] repack mod164 v2 (dylib 66936B trong Frameworks, LC @rpath OK, IPA 226MB)
- [ ] user ESign + run + log [META] + pull global-metadata.decrypted.dat
- [ ] Il2CppDumper → dump.cs → mining offset → config.h 1.64
- [ ] cheat.c 1.64 + antiban cơ bản → test → iterate
- [ ] aimbot/macro + antiban nâng cao (conditional)
