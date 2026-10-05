# RESEARCH: How to dump offset — iOS + MOBA (AOV/Liên Quân)
Ngày: 2026-10-05. Mục đích: mọi đường lấy offset il2cpp trên iOS, đặc biệt
non-jailbreak + game MOBA Unity (AOV). Kết luận áp dụng cho project ở cuối.

## 1. BẢN ĐỒ PHƯƠNG PHÁP (tổng hợp từ cộng đồng)

| # | Phương pháp | Cần gì | iOS 26 + ESign non-JB? | Ghi chú |
|---|---|---|---|---|
| M1 | Il2CppDumper tĩnh (binary + metadata đĩa) | file decrypt | CHẾT với Tencent (metadata mã hóa EAB11BAF) | Perfare: "file encrypt thì tự đi hỏi forum" |
| M2 | Frida dump từ RAM (k0tayan/FridaDumpGlobalMetadata, CameroonD) — pattern `af 1b b1 fa` | Jailbreak (frida-server) hoặc TrollStore | KHÔNG (iOS 26 không JB/TrollStore) |validate hướng magic-hunt của mình |
| M3 | Runtime dumper tweak JB (Leeksov/Il2CppDumper-iOS 2026, Zygisk, CrackerCat Auto) | Dopamine/palera1n/root Android | KHÔNG | full dump.cs + DummyDll, chuẩn vàng nếu có JB |
| M4 | frida-il2cpp-bridge (dump không cần metadata file) | Frida (JB) | KHÔNG | iOS mới "expect breakage" |
| M5 | Runtime DLL tự viết, đọc structures từ live process (PCIeTLP/il2cpp-runtime-dumper, Longno242/IL2CPP-Dumper → GameDump.hpp/.cs/.json offsets+RVA) | inject được vào process | CÓ — chính là dylib ESign của mình | kiến trúc đã có người chứng minh |
| M6 | Static bindiff mod-vs-stock (cùng version) | 2 IPA | CÓ — đã làm, ra 4 patch | chỉ ra CODE patch, không ra field offset |
| M7 | Mod-menu patchOffset theo version (joeyjurjens/unixape/ontrey228/silentninjabee-HuyJIT) | offsets mỗi bản game | CÓ — chuẩn công nghiệp ESign | `patchOffset(0x1002DB3C8, C0035FD6)`, getRealOffset()=base+offset, DobbyHook, KittyMemory/MSHookMemory |
| M8 | iOSGods AOV mods (trungtoan337 EU + ViP) | mua/tải | THAM KHẢO — bản EU (com.ngame.allstar.eu), khác build VN kgvn → offset khác, không copy được | feature set khả thi: camera +/-, aimbot slider, antiban auto-update, tool update offset/il2cpp |

## 2. PHÁT HIỆN QUAN TRỌNG NHẤT (tự đính chính)

Claim cũ của lunar ("runtime patch __TEXT trên stock iOS = SIGKILL chắc chắn")
là QUÁ MẠNH và SAI với thực tế ESign:
- Toàn bộ scene iOSGods non-jailbreak sống bằng runtime `vm_write`/`MSHookMemory`
  patch ngay lúc chạy trên app ESign resign — hoạt động đại trà nhiều năm.
- HuyJIT template ghi rõ: patching offsets/hexes cho Non-jailbreak (kèm JIT).
- Lý do nó sống: ESign resign (adhoc/enterprise, thường kèm get-task-allow hoặc
  CS config cho phép) + iOS chỉ kill khi page đã ký bị phát hiện sai hash lúc
  fault — trên thực tế với app resign thì vm_protect+write qua được.
- Kết luận sửa lại: STATIC patch (như modder + apply_patches.py của mình) là đường
  AN TOÀN NHẤT (đã verify). RUNTIME patch là CHUẨN CÔNG NGHIỆP trên ESign
  (xác suất sống cao, cho phép bật/tắt + menu). Cả hai đều đi được — không loại
  runtime nữa. Bài học L9.

## 3. ÁP CHO PROJECT (kgvn 1.64, ESign, iOS 26, không JB)

- Map hack: ĐÃ CÓ (4 patch tĩnh, replicate modder). Static trước (an toàn), runtime
  patcher dylib sau nếu muốn menu bật/tắt (đúng mẫu joeyjurjens).
- ESP offsets: M5 là đường duy nhất còn lại — dylib đọc runtime structures
  (Il2CppClass.name@0x10 → fields → FieldInfo.offset@24), tức v5-route-B đã duyệt.
  Không Frida, không JB, không metadata file — khớp 100% điều kiện của mình.
- Offset leak từ cộng đồng: bản EU khác build → không dùng được cho VN.
  iOSGods VN section + Telegram LQM là nguồn canh offset mỗi khi game update
  (dự phòng khi tự dump thất bại).
- Khi game update version: bindiff lại (M6, 30 phút) + v5 chạy lại trong game mới.
  Quy trình đã có tooling sẵn, không làm lại từ đầu.

## 4. NGUỒN (link đầy đủ)
- Perfare/Il2CppDumper (+ MetadataClass.cs struct v29, README.zh-CN FAQ encrypt)
- SamboyCoding/Cpp2IL (Il2CppFieldDefinition.cs — field KHÔNG có offset trong file)
- Jumboperson/il2cpp.h (Il2CppClass: image@0, gc_desc@8, name@0x10)
- MlgmXyysd/libil2cpp Unity_2022.1 GlobalMetadataFileInternals.h
- k0tayan/FridaDumpGlobalMetadata (pattern af 1b b1 fa, iOS+Android)
- CameroonD/Il2CppMetadataExtractor (Frida, cần JB)
- Leeksov/Il2CppDumper-iOS (2026, JB tweak, full dump.cs/DummyDll)
- rubenvereecken/vfsfitvnm frida-il2cpp-bridge (dump không cần metadata file)
- PCIeTLP/il2cpp-runtime-dumper, Longno242/IL2CPP-Dumper (runtime DLL → GameDump offsets)
- wanfengjiang/viegg69 Il2CppDumper forks (GameGuardian dump từ RAM Android)
- joeyjurjens/unixape/ontrey228 iOS mod menu templates (patchOffset + ENCRYPTHEX)
- silentninjabee/iOS-ImGui-ModMenu-Template HuyJIT (vm_writeData, getRealOffset, DobbyHook, non-JB + JIT)
- iosgods.com: topic/183939 AOV EU jailbreak (trungtoan337, aimbot/camera/antiban),
  topic/188661 AOV v1.56 non-jailbreak IPA
- MLuc24/lien-quan-data (dataset tướng/item, không phải offset — tham khảo tên)
- Frida-iOS-Hook noobpk (dump IPA + memory, cần JB), alonemonkey/incogbyte frida-ios-dump (cần JB)

## 5. QUYẾT ĐỊNH (không đổi hướng, chỉ mở thêm cửa)
1. Giữ static patch làm đường chính map hack (đã verify).
2. v5-route-B (string → Il2CppClass → FieldInfo) làm ngay để lấy offset ESP.
3. Sau ESP: cân nhắc runtime patcher dylib kiểu mod-menu (bật/tắt, đúng chuẩn scene).
4. Mỗi game update: bindiff M6 + chạy lại v5. Canh iOSGods/Telegram làm dự phòng.
