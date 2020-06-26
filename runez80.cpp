#include "cpu.h"
#include "emu.h"
#include "mem.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <unordered_map>

namespace {
using namespace std::literals::string_literals;

constexpr auto &r = cpu.registers;

using std::byte;
using size = std::size_t;
using u8 = uint8_t;
using s8 =  int8_t;
using u16 = uint16_t;
using s16 =  int16_t;
struct u24;
struct s24;
using u32 = uint32_t;
using s32 =  int32_t;
using u64 = uint64_t;
using s64 =  int64_t;

struct u24 final {
    u24(u32 x = {}) : bytes{byte(x >> 0), byte(x >> 8), byte(x >> 16)} {}
    operator s24() const;
    operator u32() const {
        return u8(bytes[0]) << 0 | u8(bytes[1]) << 8 | u8(bytes[2]) << 16;
    }
private:
    byte bytes[3];
};
struct s24 final {
    s24(s32 x = {}) : bytes{byte(x >> 0), byte(x >> 8), byte(x >> 16)} {}
    operator u24() const;
    operator s32() const {
        return u8(bytes[0]) << 0 | u8(bytes[1]) << 8 | s8(bytes[2]) << 16;
    }
private:
    byte bytes[3];
};
inline u24::operator s24() const { return operator u32(); }
inline s24::operator u24() const { return operator s32(); }

template<typename... Types>
class RegisterTuple {
    std::tuple<Types &&...> regs;
    template<typename Value, size Index = sizeof...(Types) - 1>
    Value get() {
        Value result = std::get<Index>(regs);
        if constexpr (bool(Index))
            result |= get<Value, Index - 1>() << 24;
        return result;
    }
public:
    RegisterTuple(Types &&...regs) : regs(std::forward<Types>(regs)...) {}
    template<typename Value, size Index = sizeof...(Types) - 1>
    RegisterTuple &operator=(Value value) {
        std::get<Index>(regs) = u24(value);
        if constexpr (bool(Index))
            return operator=<Value, Index - 1>(value >> Value(24));
        else
            return *this;
    }

    template<typename Value>
    operator Value() { return get<Value>(); }
};
template<typename... Types>
RegisterTuple<Types...> regs(Types &&...regs) {
    return RegisterTuple<Types...>(std::forward<Types>(regs)...);
}

template<typename Type> Type &memref(u32 address) {
    if (auto *ptr = static_cast<Type *>(phys_mem_ptr(address, sizeof(Type))))
        return *ptr;
    throw "invalid address";
}

bool ret() {
    cpu_flush(memref<u24>(r.SPL), true);
    r.SPL += 3;
    cpu.cycles -= std::exchange(cpu.haltCycles, 0);
    cpu.halted = false;
    return true;
}
bool ret8(u8 result) {
    regs(r.A) = result;
    return ret();
}
bool ret16(u16 result) {
    regs(r.HL) = result;
    return ret();
}
bool ret24(u24 result) {
    regs(r.HL) = result;
    return ret();
}
bool ret32(u32 result) {
    regs(r.E, r.HL) = result;
    return ret();
}
bool retL(u32 result) {
    regs(r.A, r.BC) = result;
    return ret();
}
bool ret64(u64 result) {
    regs(r.BC, r.DE, r.HL) = result;
    return ret();
}
template<typename Value>
bool cmp(Value x, Value y = Value()) {
    typedef typename std::make_signed<Value>::type Signed;
    typedef typename std::make_unsigned<Value>::type Unsigned;
    constexpr Unsigned half_mask = std::numeric_limits<Unsigned>::max() >> Unsigned(4);
    Signed signed_result;
    Unsigned unsigned_result;
    auto f = r.F & (1 << 5 | 1 << 3);
    f |= __builtin_sub_overflow(Unsigned(x), Unsigned(y), &unsigned_result) << 0;
    f |= true << 1;
    f |= __builtin_sub_overflow(Signed(x), Signed(y), &signed_result) << 2;
    f |= __builtin_sub_overflow(Unsigned(x & half_mask), Unsigned(y & half_mask), &unsigned_result) << 4;
    f |= (x == y) << 6;
    f |= (signed_result < 0) << 7;
    r.F = f;
    return ret();
}
template<typename Value>
Value div(Value x, Value y) {
    return y ? x / y : x < Value() ? 1 : -1;
}
template<typename Value>
Value rem(Value x, Value y) {
    return y ? x % y : x < Value() ? -x : +x;
}

const std::unordered_map<std::string, bool (*)()> libcall_handlers = {
    {"exit"s,       []{ return false; }},
    {"putchar"s,    []{ return ret32(putchar(memref<char>(r.SPL + 3))); }},
    {"memcpy"s,     []{
                        u24 dst = memref<u24>(r.SPL + 3);
                        u24 src = memref<u24>(r.SPL + 6);
                        u24 len = memref<u24>(r.SPL + 9);
                        void *pdst = phys_mem_ptr(dst, len);
                        void *psrc = phys_mem_ptr(src, len);
                        if (!pdst || !psrc) {
                            fprintf(stderr, "Couldn't perform memcpy\n");
                            r.HL = -1;
                            return false;
                        }
                        std::memcpy(pdst, psrc, len);
                        return ret24(dst);
                    }},
    {"memset"s,     []{
                        u24 dst = memref<u24>(r.SPL + 3);
                        u24 src = memref<u24>(r.SPL + 6);
                        u24 len = memref<u24>(r.SPL + 9);
                        void *pdst = phys_mem_ptr(dst, len);
                        if (!pdst) {
                            fprintf(stderr, "Couldn't perform memset\n");
                            r.HL = -1;
                            return false;
                        }
                        std::memset(pdst, src, len);
                        return ret24(dst);
                    }},
    {"_dump"s,      []{
                        fprintf(stderr, "AF %04X     %04X AF'\nBC %06X %06X BC'\nDE %06X %06X DE'\n"
                                "HL %06X %06X HL'\nIX %06X %06X SPS\nIY %06X %06X SPL\n\n",
                                r.AF, r._AF, r.BC, r._BC, r.DE, r._DE,
                                r.HL, r._HL, r.IX, r.SPS, r.IY, r.SPL);
                        return ret();
                    }},
    {"_frameset0"s, []{
                        ret();
                        memref<u24>(r.SPL -= 3) = r.IX;
                        r.IX = r.SPL;
                        return true;
                    }},
    {"_frameset"s,  []{
                        ret();
                        memref<u24>(r.SPL -= 3) = r.IX;
                        r.IX = r.SPL;
                        r.SPL += s24(r.HL);
                        return true;
                    }},
    {"_sand"s,      []{ return ret16(r.HL & r.BC); }},
    {"_iand"s,      []{ return ret24(r.HL & r.BC); }},
    {"_land"s,      []{ return ret32(u32(regs(r.E, r.HL)) & u32(regs(r.A, r.BC))); }},
    {"_lland"s,     []{ return ret64(memref<u64>(r.HL) & memref<u64>(r.BC)); }},
    {"_sor"s,       []{ return ret16(r.HL | r.BC); }},
    {"_ior"s,       []{ return ret24(r.HL | r.BC); }},
    {"_lor"s,       []{ return ret32(u32(regs(r.E, r.HL)) | u32(regs(r.A, r.BC))); }},
    {"_llor"s,      []{ return ret64(memref<u64>(r.HL) | memref<u64>(r.BC)); }},
    {"_sxor"s,      []{ return ret16(r.HL ^ r.BC); }},
    {"_ixor"s,      []{ return ret24(r.HL ^ r.BC); }},
    {"_lxor"s,      []{ return ret32(u32(regs(r.E, r.HL)) ^ u32(regs(r.A, r.BC))); }},
    {"_llxor"s,     []{ return ret64(memref<u64>(r.HL) ^ memref<u64>(r.BC)); }},
    {"_bshl"s,      []{ return ret8(r.A << r.B); }},
    {"_sshl"s,      []{ return ret16(r.HL << r.C); }},
    {"_ishl"s,      []{ return ret24(r.HL << r.C); }},
    {"_lshl"s,      []{ return retL(u32(regs(r.A, r.BC)) << r.L); }},
    {"_llshl"s,     []{ return ret64(memref<u64>(r.HL) << r.C); }},
    {"_bshrs"s,     []{ return ret8(s8(r.A) >> r.B); }},
    {"_sshrs"s,     []{ return ret16(s16(r.HL) >> r.C); }},
    {"_ishrs"s,     []{ return ret24(s24(r.HL) >> r.C); }},
    {"_lshrs"s,     []{ return retL(s32(regs(r.A, r.BC)) >> r.L); }},
    {"_llshrs"s,    []{ return ret64(memref<s64>(r.HL) >> r.C); }},
    {"_bshru"s,     []{ return ret8(u8(r.A) >> r.B); }},
    {"_sshru"s,     []{ return ret16(u16(r.HL) >> r.C); }},
    {"_ishru"s,     []{ return ret24(u24(r.HL) >> r.C); }},
    {"_lshru"s,     []{ return retL(u32(regs(r.A, r.BC)) >> r.L); }},
    {"_llshru"s,    []{ return ret64(memref<u64>(r.HL) >> r.C); }},
    {"_lcmpu"s,     []{ return cmp(u32(regs(r.E, r.HL)), u32(regs(r.A, r.BC))); }},
    {"_llcmpu"s,    []{ return cmp(memref<u64>(r.HL), memref<u64>(r.BC)); }},
    {"_lcmpzero"s,  []{ return cmp(u32(regs(r.E, r.HL))); }},
    {"_llcmpzero"s, []{ return cmp(memref<u64>(r.HL)); }},
    {"_setflag"s,   []{ if (r.F & 1 << 2) r.F ^= 1 << 7; return ret(); }},
    {"_ladd"s,      []{ return ret32(u32(regs(r.E, r.HL)) + u32(regs(r.A, r.BC))); }},
    {"_lladd"s,     []{ return ret64(memref<u64>(r.HL) + memref<u64>(r.BC)); }},
    {"_lsub"s,      []{ return ret32(u32(regs(r.E, r.HL)) - u32(regs(r.A, r.BC))); }},
    {"_llsub"s,     []{ return ret64(memref<u64>(r.HL) - memref<u64>(r.BC)); }},
    {"_smulu"s,     []{ return ret16(u16(r.HL) * u16(r.BC)); }},
    {"_imulu"s,     []{ return ret24(u24(r.HL) * u24(r.BC)); }},
    {"_lmulu"s,     []{ return ret24(u32(regs(r.E, r.HL)) * u32(regs(r.A, r.BC))); }},
    {"_llmulu"s,    []{ return ret64(memref<u64>(r.HL) * memref<u64>(r.BC)); }},
    {"_bdivs"s,     []{ return ret8(div(s8(r.B), s8(r.C))); }},
    {"_sdivs"s,     []{ return ret16(div(s16(r.HL), s16(r.BC))); }},
    {"_idivs"s,     []{ return ret24(div(s24(r.HL), s24(r.BC))); }},
    {"_ldivs"s,     []{ return ret32(div(s32(regs(r.E, r.HL)), s32(regs(r.A, r.BC)))); }},
    {"_lldivs"s,    []{ return ret64(div(memref<s64>(r.HL), memref<s64>(r.BC))); }},
    {"_bdivu"s,     []{ return ret8(div(u8(r.B), u8(r.C))); }},
    {"_sdivu"s,     []{ return ret16(div(u16(r.HL), u16(r.BC))); }},
    {"_idivu"s,     []{ return ret24(div(u24(r.HL), u24(r.BC))); }},
    {"_ldivu"s,     []{ return ret32(div(u32(regs(r.E, r.HL)), u32(regs(r.A, r.BC)))); }},
    {"_lldivu"s,    []{ return ret64(div(memref<u64>(r.HL), memref<u64>(r.BC))); }},
    {"_brems"s,     []{ return ret8(rem(s8(r.A), s8(r.C))); }},
    {"_srems"s,     []{ return ret16(rem(s16(r.HL), s16(r.BC))); }},
    {"_irems"s,     []{ return ret24(rem(s24(r.HL), s24(r.BC))); }},
    {"_lrems"s,     []{ return ret32(rem(s32(regs(r.E, r.HL)), s32(regs(r.A, r.BC)))); }},
    {"_llrems"s,    []{ return ret64(rem(memref<s64>(r.HL), memref<s64>(r.BC))); }},
    {"_bremu"s,     []{ return ret8(rem(u8(r.A), u8(r.C))); }},
    {"_sremu"s,     []{ return ret16(rem(u16(r.HL), u16(r.BC))); }},
    {"_iremu"s,     []{ return ret24(rem(u24(r.HL), u24(r.BC))); }},
    {"_lremu"s,     []{ return ret32(rem(u32(regs(r.E, r.HL)), u32(regs(r.A, r.BC)))); }},
    {"_llremu"s,    []{ return ret64(rem(memref<u64>(r.HL), memref<u64>(r.BC))); }},
};

bool emulate_libcall(void) {
    std::string name(memref<char[25]>(r.PC));
    //std::fprintf(stderr, "libcall: %10s(0x%08X, 0x%08X)\n", name.c_str(), u32(regs(r.E, r.HL)), u32(regs(r.A, r.BC)));
    if (auto handler = libcall_handlers.find(name); handler != libcall_handlers.end())
        return handler->second();
    std::fprintf(stderr, "Unimplemented libcall: %s\n", name.c_str());
    r.HL = -1;
    return false;
}
}

extern "C" {

void gui_console_printf(const char *format, ...) {
    (void)format;
    //std::va_list args;
    //va_start(args, format);
    //std::fprintf(stderr, format, args);
    //va_end(args);
}

int main(int argc, char **argv) {
    if (argc != 2)
        return 1;
    asic_init();
    asic_reset();
    emu_load(EMU_DATA_RAM, argv[1]);
    r.SPL = 0xD65800;
    sched.event.cycle = 48000000 * 10;
    cpu_flush(0xD00000, true);
    do
        cpu_execute();
    while (cpu.halted && emulate_libcall());
    if (!cpu.halted)
        r.HL = 124;
    asic_free();
    return r.HL;
}

}
