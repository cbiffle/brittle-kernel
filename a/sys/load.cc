/*
 * Program loading library.  This takes program images in an a.out-style format
 * and inflates them into runnable, isolated Contexts.
 */
#include "a/sys/load.h"

#include <cstdint>

#include "etl/algorithm.h"
#include "etl/armv7m/mpu.h"
#include "etl/armv7m/exception_frame.h"

#include "common/abi_sizes.h"

#include "a/sys/types.h"
#include "a/sys/alloc.h"
#include "a/sys/keys.h"
#include "a/k/memory.h"
#include "a/k/context.h"
#include "a/rt/keys.h"

using etl::min;
using etl::armv7m::Mpu;
using Rasr = Mpu::rasr_value_t;

namespace sys {

/*
 * Utility routine for rounding an integer up to the next power of two.
 */
static unsigned round_up_p2(unsigned v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return v + 1;
}


/*******************************************************************************
 * Program headers, operations thereon.
 */

/*
 * Header block prepended to loadable programs.  Describes the program's shape
 * to assist in allocation and GOT relocation.
 *
 * Several fields in the header are offsets.  These offsets are measured in
 * bytes from the address of the header itself.
 */
struct Header {
  // Size of image, in bytes.  (Equivalently: offset to end of initialized
  // data image.)
  uint32_t image_size;
  // Offset of end-of-text and start of the initialized data image.
  uint32_t text_end;
  // Offset of the end of the GOT prototype in the initialized data image.
  uint32_t got_end;
  // Offset to the end of the BSS section, assuming that RAM begins immediately
  // after text_end.
  uint32_t bss_end;
  // Number of bytes of stack required to execute this program.
  uint32_t stack_size;
  // Offset of entry point.
  uint32_t entry;
};

/*
 * Simple sanity checks of Header contents.  Any program whose Header does not
 * pass these checks will be refused.
 */
static bool is_valid(Header const & hdr) {
  // All addresses and sizes must be word-aligned.
  if (hdr.image_size & 3) return false;
  if (hdr.text_end & 3) return false;
  if (hdr.got_end & 3) return false;
  if (hdr.bss_end & 3) return false;
  if (hdr.stack_size & 3) return false;

  // Entry point must be halfword-aligned and fall within the text segment.
  if (hdr.entry & 1) return false;
  if (hdr.entry >= hdr.text_end) return false;

  // Check the relation text_end <= got_end <= image_size.
  if (hdr.got_end > hdr.image_size) return false;
  if (hdr.text_end > hdr.got_end) return false;

  // Check that the size of BSS is not negative.
  if (hdr.bss_end < hdr.image_size) return false;

  // Stack must be at least large enough to contain a hardware exception frame.
  if (hdr.stack_size < sizeof(etl::armv7m::ExceptionFrame)) return false;

  return true;
}

/*
 * Determines the size of Memory object required for the RAM segment
 * (data + BSS + stack) of a program, given its header.
 */
static unsigned get_ram_size(Header const & hdr) {
  return round_up_p2(hdr.bss_end - hdr.text_end + hdr.stack_size);
}


/*******************************************************************************
 * Program relocation and GOT management.
 */

/*
 * Translates a GOT prototype entry into a relocated GOT entry given information
 * about program instance geometry.
 */
static uintptr_t relocate_got_entry(Header const & hdr,
                                    uintptr_t hdr_addr,
                                    uintptr_t ram_addr,
                                    uintptr_t entry) {
  // The prototype address falls into one of three areas.
  if (entry < hdr.text_end) {
    // Text segment.
    return hdr_addr + entry;
  } else if (entry < hdr.bss_end) {
    // RAM segment.
    return entry - hdr.text_end + ram_addr;
  } else {
    // Outside the program's extent; assume it's an absolute address, e.g. a
    // peripheral address.
    return entry;
  }
}


/*
 * Attempts to load a program from an image at address 'img_addr'.  The image
 * must be completely contained within the Memory object designated by
 * 'img_key'.
 *
 * On success:
 * - Loads a service key to a Context into 'ctx_key_out'.  The Context is
 *   prepared to run the loaded program; it only needs to be given any
 *   additional authority and made runnable.
 * - Returns true.
 *
 * On failure:
 * - Returns false.
 */
Maybe<rt::AutoKey> load_program(uintptr_t img_addr, KeyIndex img_key) {
  // Require the img_addr to be word-aligned.
  if (img_addr & 3) return nothing;

  // TODO: img_key is presumably coming from an untrusted source.  It's not
  // safe (from a DoS perspective) to make blocking calls against it, and
  // particularly unsafe to assert on their results.  We need some way of
  // vetting this key.

  // Get the extent of the region containing the image, so we can verify that
  // it contains the image.
  auto img_region = memory::inspect(img_key);
  if (!img_region.contains(img_addr)) return nothing;

  // Since the region contains at least the first address -- and region
  // boundaries are always aligned on at least 32-byte boundaries -- we can now
  // safely read the image size.
  auto img_offset = (img_addr - img_region.get_base()) / sizeof(uint32_t);
  auto img_size = memory::peek(img_key, img_offset);
  if (!img_region.contains(img_addr + img_size)) return nothing;

  // The image is completely contained by the given Memory object.  Hooray!
  // Now we can load a copy of the header into local memory.
  auto hdr = Header {
    .image_size = img_size,
    .text_end   = memory::peek(img_key, img_offset + 1),
    .got_end    = memory::peek(img_key, img_offset + 2),
    .bss_end    = memory::peek(img_key, img_offset + 3),
    .stack_size = memory::peek(img_key, img_offset + 4),
    .entry      = memory::peek(img_key, img_offset + 5),
  };

  // Perform basic header validation.
  if (!is_valid(hdr)) return nothing;

  // Allocate the required amount of RAM.
  auto ram_bytes = get_ram_size(hdr);
  auto ram_l2_size = __builtin_ctz(ram_bytes);
  auto maybe_k_ram = alloc_mem(ram_l2_size - 1,
      uint32_t(Rasr().with_ap(Mpu::AccessPermissions::p_write_u_write)) >> 8);
  if (!maybe_k_ram) return nothing;
  auto & k_ram = maybe_k_ram.ref();

  // Allocate RAM for Context.
  auto maybe_k_ctx = alloc_mem(kabi::context_l2_size - 1, 0);
  if (!maybe_k_ctx) {
    free_mem(k_ram);
    return nothing;
  }
  auto & k_ctx = maybe_k_ctx.ref();

  memory::become(k_ctx, memory::ObjectType::context, 0);

  // Note: resources consumed, should not fail past this point if possible.

  // Copy the data initialization image (including the GOT image) into RAM.
  auto text_words = hdr.text_end / sizeof(uint32_t);
  auto data_words = (hdr.image_size / sizeof(uint32_t)) - text_words;
  for (unsigned d_off = 0; d_off < data_words; ++d_off) {
    auto word = memory::peek(img_key, img_offset + text_words + d_off);
    memory::poke(k_ram, d_off, word);
  }

  // Zero the rest.
  auto ram_words = ram_bytes / sizeof(uint32_t);
  for (unsigned d_off = data_words; d_off < ram_words; ++d_off) {
    memory::poke(k_ram, d_off, 0);
  }

  // Relocate the GOT.
  auto got_words = (hdr.got_end / sizeof(uint32_t)) - text_words;
  auto ram_region = memory::inspect(k_ram);
  for (unsigned d_off = 0; d_off < got_words; ++d_off) {
    auto entry = memory::peek(k_ram, d_off);
    memory::poke(k_ram, d_off,
        relocate_got_entry(hdr, img_addr, ram_region.get_base(), entry));
  }

  // Fill out the stack frame.
  memory::poke(k_ram, ram_words - 1, 1 << 24);  // PSR
  memory::poke(k_ram, ram_words - 2, hdr.entry + img_addr);  // PC

  // Fill out the Context.
  context::set_region(k_ctx, 0, img_key);
  context::set_region(k_ctx, 1, k_ram);

  context::set_register(k_ctx, context::Register::r9,
      ram_region.get_base());
  context::set_register(k_ctx, context::Register::sp,
      ram_region.get_base() + ram_bytes - sizeof(etl::armv7m::ExceptionFrame));

  return etl::move(k_ctx);
}

}  // namespace sys
