/**
 * @file win_iat_patch.cpp
 * @brief IAT 改写实现：解析 PE 导入目录 → VirtualProtect → 写函数指针。
 *
 * 步骤概要：
 * 1. DOS/NT 头校验，取 IMAGE_DIRECTORY_ENTRY_IMPORT
 * 2. 遍历 IMAGE_IMPORT_DESCRIPTOR（每个描述一个依赖 DLL）
 * 3. 用 OriginalFirstThunk 取导入名，用 FirstThunk（IAT）取可写槽
 * 4. 名称匹配后 VirtualProtect(PAGE_EXECUTE_READWRITE) 再赋值
 */

#include "plat/win_iat_patch.hpp"

#ifdef _WIN32

#include <tlhelp32.h>

#include <cctype>
#include <cstring>

namespace tray_hooks {
namespace iat {
namespace {

/** 大小写不敏感字符串相等（DLL 名比较用） */
bool EqNoCase(const char* a, const char* b) {
  if (!a || !b) {
    return false;
  }
  while (*a && *b) {
    if (std::tolower(static_cast<unsigned char>(*a)) !=
        std::tolower(static_cast<unsigned char>(*b))) {
      return false;
    }
    ++a;
    ++b;
  }
  return *a == *b;
}

/**
 * @brief 判断导入 DLL 是否匹配过滤条件。
 * want 为空表示「不限」；否则比较完整名或 basename。
 */
bool DllMatch(const char* want, const char* have) {
  if (!want || !*want) {
    return true;
  }
  if (EqNoCase(want, have)) {
    return true;
  }
  const char* base = have;
  for (const char* p = have; *p; ++p) {
    if (*p == '\\' || *p == '/') {
      base = p + 1;
    }
  }
  return EqNoCase(want, base);
}

/** 可写地改写一个指针大小的槽，并尽量恢复原页保护属性 */
bool WriteSlot(void** slot, void* value) {
  DWORD old = 0;
  if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) {
    return false;
  }
  *slot = value;
  DWORD tmp = 0;
  VirtualProtect(slot, sizeof(void*), old, &tmp);
  return true;
}

}  // namespace

int PatchModule(HMODULE caller,
                const char* import_dll,
                const char* sym_name,
                void* new_fn,
                std::vector<Patch>* out_patches) {
  if (!caller || !sym_name || !*sym_name || !new_fn) {
    return 0;
  }

  auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(caller);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
    return 0;
  }
  auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(
      reinterpret_cast<BYTE*>(caller) + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE) {
    return 0;
  }

  IMAGE_DATA_DIRECTORY dir =
      nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (!dir.VirtualAddress || !dir.Size) {
    return 0;  // 无线导入表（少见，或纯资源模块）
  }

  auto* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
      reinterpret_cast<BYTE*>(caller) + dir.VirtualAddress);
  int count = 0;

  for (; imp->Name; ++imp) {
    const char* dll_name = reinterpret_cast<const char*>(
        reinterpret_cast<BYTE*>(caller) + imp->Name);
    if (!DllMatch(import_dll, dll_name)) {
      continue;
    }

    // INT：导入名称表；IAT：实际跳转使用的地址表（运行期可被绑定器改写）
    auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
        reinterpret_cast<BYTE*>(caller) +
        (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
    auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(
        reinterpret_cast<BYTE*>(caller) + imp->FirstThunk);

    for (; thunk->u1.AddressOfData; ++thunk, ++iat) {
      // 按序号导入的项没有名称，本实现跳过
      if (IMAGE_SNAP_BY_ORDINAL(thunk->u1.Ordinal)) {
        continue;
      }
      auto* ibn = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
          reinterpret_cast<BYTE*>(caller) + thunk->u1.AddressOfData);
      if (!ibn->Name || std::strcmp(reinterpret_cast<const char*>(ibn->Name),
                                    sym_name) != 0) {
        continue;
      }

      void** slot = reinterpret_cast<void**>(&iat->u1.Function);
      void* original = *slot;
      if (original == new_fn) {
        continue;  // 已 hook，避免重复写
      }
      if (!WriteSlot(slot, new_fn)) {
        continue;
      }
      if (out_patches) {
        Patch p;
        p.slot = slot;
        p.original = original;
        p.replaced = new_fn;
        p.caller = caller;
        p.dll_name = dll_name;
        p.sym_name = sym_name;
        out_patches->push_back(p);
      }
      ++count;
    }
  }
  return count;
}

int PatchAllModules(const char* import_dll,
                    const char* sym_name,
                    void* new_fn,
                    std::vector<Patch>* out_patches,
                    HMODULE skip_self) {
  // Toolhelp 枚举；失败时至少尝试主模块
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
                                         GetCurrentProcessId());
  if (snap == INVALID_HANDLE_VALUE) {
    HMODULE main_mod = GetModuleHandleW(NULL);
    return PatchModule(main_mod, import_dll, sym_name, new_fn, out_patches);
  }

  MODULEENTRY32W me;
  me.dwSize = sizeof(me);
  int total = 0;
  if (Module32FirstW(snap, &me)) {
    do {
      if (skip_self && me.hModule == skip_self) {
        continue;
      }
      total += PatchModule(me.hModule, import_dll, sym_name, new_fn, out_patches);
    } while (Module32NextW(snap, &me));
  }
  CloseHandle(snap);
  return total;
}

bool Restore(const Patch& p) {
  if (!p.slot || !p.original) {
    return false;
  }
  return WriteSlot(p.slot, p.original);
}

}  // namespace iat
}  // namespace tray_hooks

#endif
