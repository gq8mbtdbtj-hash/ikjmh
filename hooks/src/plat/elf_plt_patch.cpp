/**
 * @file elf_plt_patch.cpp
 * @brief ELF GOT 改写实现。
 *
 * 流程：
 * 1. dl_iterate_phdr 枚举映射中的 ELF
 * 2. 找 PT_DYNAMIC → DT_SYMTAB / DT_STRTAB / DT_JMPREL / DT_PLTRELSZ
 * 3. 遍历 .rela.plt / .rel.plt，类型为 JUMP_SLOT 且符号名匹配
 * 4. mprotect 所在页后写入 proxy，并 clear_cache（ARM 需要）
 *
 * 注意：
 * - 加固 / linker namespace / 只读 GOT 重定位（RELRO）可能导致 mprotect 失败
 * - AUTOMATIC 模式下新 dlopen 的 so 需再次 Patch（可由上层 hook dlopen）
 */

#include "plat/elf_plt_patch.hpp"

#if !defined(_WIN32)

#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <elf.h>

#if defined(__LP64__)
using Elf_Ehdr = Elf64_Ehdr;
using Elf_Phdr = Elf64_Phdr;
using Elf_Dyn = Elf64_Dyn;
using Elf_Sym = Elf64_Sym;
using Elf_Rel = Elf64_Rel;
using Elf_Rela = Elf64_Rela;
using Elf_Addr = Elf64_Addr;
#  define ELF_R_SYM ELF64_R_SYM
#  define ELF_R_TYPE ELF64_R_TYPE
#else
using Elf_Ehdr = Elf32_Ehdr;
using Elf_Phdr = Elf32_Phdr;
using Elf_Dyn = Elf32_Dyn;
using Elf_Sym = Elf32_Sym;
using Elf_Rel = Elf32_Rel;
using Elf_Rela = Elf32_Rela;
using Elf_Addr = Elf32_Addr;
#  define ELF_R_SYM ELF32_R_SYM
#  define ELF_R_TYPE ELF32_R_TYPE
#endif

// PLT 重定位类型：与 CPU 架构绑定
#if defined(__x86_64__)
#  define TRAY_JUMP_SLOT R_X86_64_JUMP_SLOT
#elif defined(__aarch64__)
#  define TRAY_JUMP_SLOT R_AARCH64_JUMP_SLOT
#elif defined(__arm__)
#  define TRAY_JUMP_SLOT R_ARM_JUMP_SLOT
#elif defined(__i386__)
#  define TRAY_JUMP_SLOT R_386_JMP_SLOT
#else
#  define TRAY_JUMP_SLOT 0  /* 未知架构：退化为「忽略类型，只比符号名」 */
#endif

namespace tray_hooks {
namespace elfplt {
namespace {

struct FindCtx {
  const char* caller_substr;
  const char* sym_name;
  void* new_fn;
  std::vector<Patch>* out;
  int (*allow)(const char*, void*);
  void* allow_arg;
  int count;
  int fails;
};

/** 放宽页保护以便写入 GOT；失败时再试 RXW */
bool WriteSlot(void** slot, void* value) {
  long page = sysconf(_SC_PAGESIZE);
  if (page <= 0) {
    page = 4096;
  }
  uintptr_t addr = reinterpret_cast<uintptr_t>(slot);
  uintptr_t page_start = addr & ~(static_cast<uintptr_t>(page) - 1);
  if (mprotect(reinterpret_cast<void*>(page_start), static_cast<size_t>(page),
               PROT_READ | PROT_WRITE) != 0) {
    if (mprotect(reinterpret_cast<void*>(page_start), static_cast<size_t>(page),
                 PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
      return false;
    }
  }
  *slot = value;
#if defined(__GNUC__)
  // ARM/ARM64：指令/数据缓存同步，避免读到旧 GOT
  __builtin___clear_cache(reinterpret_cast<char*>(slot),
                          reinterpret_cast<char*>(slot) + sizeof(void*));
#endif
  return true;
}

const char* GetStr(const char* strtab, Elf_Sym* sym) {
  if (!strtab || !sym) {
    return "";
  }
  return strtab + sym->st_name;
}

/** 处理单个已映射 ELF 的 PLT 重定位表 */
void PatchOneElf(dl_phdr_info* info, FindCtx* ctx) {
  if (!info) {
    return;
  }
  const char* path = info->dlpi_name ? info->dlpi_name : "";
  if (ctx->allow && !ctx->allow(path, ctx->allow_arg)) {
    return;
  }
  // 可选：按路径子串过滤调用者（例如只 hook libfoo.so）
  if (ctx->caller_substr && *ctx->caller_substr) {
    if (path[0] != '\0' && !std::strstr(path, ctx->caller_substr)) {
      return;
    }
  }

  const Elf_Dyn* dyn = 0;
  for (int i = 0; i < info->dlpi_phnum; ++i) {
    if (info->dlpi_phdr[i].p_type == PT_DYNAMIC) {
      dyn = reinterpret_cast<const Elf_Dyn*>(info->dlpi_addr +
                                             info->dlpi_phdr[i].p_vaddr);
      break;
    }
  }
  if (!dyn) {
    return;
  }

  Elf_Sym* symtab = 0;
  const char* strtab = 0;
  Elf_Rela* rela = 0;
  Elf_Rel* rel = 0;
  size_t rela_count = 0;
  size_t rel_count = 0;

  for (const Elf_Dyn* d = dyn; d->d_tag != DT_NULL; ++d) {
    switch (d->d_tag) {
      case DT_SYMTAB:
        symtab = reinterpret_cast<Elf_Sym*>(info->dlpi_addr + d->d_un.d_ptr);
        break;
      case DT_STRTAB:
        strtab = reinterpret_cast<const char*>(info->dlpi_addr + d->d_un.d_ptr);
        break;
      case DT_JMPREL:
        // .rela.plt 或 .rel.plt 的起始（由 DT_PLTREL 区分，此处两种都试）
        rela = reinterpret_cast<Elf_Rela*>(info->dlpi_addr + d->d_un.d_ptr);
        rel = reinterpret_cast<Elf_Rel*>(info->dlpi_addr + d->d_un.d_ptr);
        break;
      case DT_PLTRELSZ:
        rela_count = d->d_un.d_val / sizeof(Elf_Rela);
        rel_count = d->d_un.d_val / sizeof(Elf_Rel);
        break;
      default:
        break;
    }
  }
  if (!symtab || !strtab || (!rela && !rel)) {
    return;
  }

  // 64 位默认按 RELA；32 位 ARM 常见 REL
  bool use_rela = true;
#if !defined(__LP64__) && defined(__arm__)
  use_rela = false;
#endif

  if (use_rela && rela && rela_count > 0) {
    for (size_t i = 0; i < rela_count; ++i) {
      const Elf_Rela& r = rela[i];
      if (ELF_R_TYPE(r.r_info) != TRAY_JUMP_SLOT && TRAY_JUMP_SLOT != 0) {
        if (TRAY_JUMP_SLOT != 0) {
          continue;
        }
      }
      Elf_Sym* sym = symtab + ELF_R_SYM(r.r_info);
      const char* name = GetStr(strtab, sym);
      if (std::strcmp(name, ctx->sym_name) != 0) {
        continue;
      }
      // r_offset：相对 load bias 的 GOT 槽
      void** slot =
          reinterpret_cast<void**>(info->dlpi_addr + r.r_offset);
      void* original = *slot;
      if (original == ctx->new_fn) {
        continue;
      }
      if (!WriteSlot(slot, ctx->new_fn)) {
        ctx->fails++;
        continue;
      }
      if (ctx->out) {
        Patch p;
        p.slot = slot;
        p.original = original;
        p.replaced = ctx->new_fn;
        p.caller_path = info->dlpi_name ? info->dlpi_name : "";
        p.sym_name = ctx->sym_name;
        ctx->out->push_back(p);
      }
      ctx->count++;
    }
  } else if (rel && rel_count > 0) {
    for (size_t i = 0; i < rel_count; ++i) {
      const Elf_Rel& r = rel[i];
      if (TRAY_JUMP_SLOT != 0 && ELF_R_TYPE(r.r_info) != TRAY_JUMP_SLOT) {
        continue;
      }
      Elf_Sym* sym = symtab + ELF_R_SYM(r.r_info);
      const char* name = GetStr(strtab, sym);
      if (std::strcmp(name, ctx->sym_name) != 0) {
        continue;
      }
      void** slot =
          reinterpret_cast<void**>(info->dlpi_addr + r.r_offset);
      void* original = *slot;
      if (original == ctx->new_fn) {
        continue;
      }
      if (!WriteSlot(slot, ctx->new_fn)) {
        ctx->fails++;
        continue;
      }
      if (ctx->out) {
        Patch p;
        p.slot = slot;
        p.original = original;
        p.replaced = ctx->new_fn;
        p.caller_path = info->dlpi_name ? info->dlpi_name : "";
        p.sym_name = ctx->sym_name;
        ctx->out->push_back(p);
      }
      ctx->count++;
    }
  }
}

int PhdrCb(dl_phdr_info* info, size_t /*size*/, void* data) {
  PatchOneElf(info, static_cast<FindCtx*>(data));
  return 0;  // 继续迭代
}

}  // namespace

int PatchSymbol(const char* caller_substr,
                const char* sym_name,
                void* new_fn,
                std::vector<Patch>* out_patches,
                int (*allow)(const char* caller_path, void* arg),
                void* allow_arg,
                int* fail_out) {
  if (!sym_name || !*sym_name || !new_fn) {
    return 0;
  }
  FindCtx ctx;
  ctx.caller_substr = caller_substr;
  ctx.sym_name = sym_name;
  ctx.new_fn = new_fn;
  ctx.out = out_patches;
  ctx.allow = allow;
  ctx.allow_arg = allow_arg;
  ctx.count = 0;
  ctx.fails = 0;
  dl_iterate_phdr(PhdrCb, &ctx);
  if (fail_out) {
    *fail_out = ctx.fails;
  }
  if (ctx.fails > 0) {
    std::fprintf(stderr,
                 "[tray_hooks] elf_plt: '%s' write failed on %d GOT slot(s) "
                 "(RELRO/hardening?)\n",
                 sym_name, ctx.fails);
  }
  return ctx.count;
}

bool Restore(const Patch& p) {
  if (!p.slot || !p.original) {
    return false;
  }
  return WriteSlot(p.slot, p.original);
}

}  // namespace elfplt
}  // namespace tray_hooks

#endif
