#include "cpu.h"
#include "emu.h"
#include "mem.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

namespace {
using namespace std::literals::string_literals;

auto &r = cpu.registers;

template<typename... Types>
class RegisterTuple {
    std::tuple<Types &&...> regs;
    template<typename Value, size_t Index = sizeof...(Types) - 1>
    Value get() {
        Value result = std::get<Index>(regs);
        if constexpr (bool(Index))
            result |= get<Value, Index - 1>() << 24;
        return result;
    }
public:
    RegisterTuple(Types &&...regs) : regs(std::forward<Types>(regs)...) {}
    template<typename Value, std::size_t Index = sizeof...(Types) - 1>
    RegisterTuple &operator=(Value value) {
        std::get<Index>(regs) = value & Value(0xFFFFFF);
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

template<typename Type> Type &memref(uint32_t address) {
    if (auto *ptr = static_cast<Type *>(phys_mem_ptr(address, sizeof(Type))))
        return *ptr;
    throw "invalid address";
}

bool ret() {
    cpu_flush(mem_peek_long(r.SPL), true);
    r.SPL += 3;
    cpu.cycles -= std::exchange(cpu.haltCycles, 0);
    cpu.halted = false;
    return true;
}
bool ret8(uint8_t result) {
    regs(r.A) = result;
    return ret();
}
bool ret16(uint16_t result) {
    regs(r.HL) = result;
    return ret();
}
bool ret24(uint32_t result) {
    regs(r.HL) = result;
    return ret();
}
bool ret32(uint32_t result) {
    regs(r.E, r.HL) = result;
    return ret();
}
bool ret64(uint64_t result) {
    regs(r.BC, r.DE, r.HL) = result;
    return ret();
}

const std::unordered_map<std::string, bool (*)()> libcall_handlers = {
    {"exit"s,       []{ return false; }},
    {"putchar"s,    []{ return ret32(putchar(memref<char>(r.SPL + 3))); }},
    {"_frameset0"s, []{
                        ret();
                        mem_poke_long(r.SPL -= 3, r.IX);
                        r.IX = r.SPL;
                        return true;
                    }},
    {"_frameset"s,  []{
                        ret();
                        mem_poke_long(r.SPL -= 3, r.IX);
                        r.IX = r.SPL;
                        r.SPL += (int32_t(r.HL) << 8 >> 8);
                        return true;
                    }},
    {"_sand"s,      []{ return ret16(r.HL & r.BC); }},
    {"_iand"s,      []{ return ret24(r.HL & r.BC); }},
    {"_land"s,      []{ return ret32(uint32_t(regs(r.E, r.HL)) & uint32_t(regs(r.A, r.BC))); }},
    {"_lland"s,     []{ return ret64(memref<uint64_t>(r.HL) & memref<uint64_t>(r.BC)); }},
    {"_sor"s,       []{ return ret16(r.HL | r.BC); }},
    {"_ior"s,       []{ return ret24(r.HL | r.BC); }},
    {"_lor"s,       []{ return ret32(uint32_t(regs(r.E, r.HL)) | uint32_t(regs(r.A, r.BC))); }},
    {"_llor"s,      []{ return ret64(memref<uint64_t>(r.HL) | memref<uint64_t>(r.BC)); }},
    {"_sxor"s,      []{ return ret16(r.HL ^ r.BC); }},
    {"_ixor"s,      []{ return ret24(r.HL ^ r.BC); }},
    {"_lxor"s,      []{ return ret32(uint32_t(regs(r.E, r.HL)) ^ uint32_t(regs(r.A, r.BC))); }},
    {"_llxor"s,     []{ return ret64(memref<uint64_t>(r.HL) ^ memref<uint64_t>(r.BC)); }},
    {"_bshl"s,      []{ return ret8(r.A << r.B); }},
    {"_bshrs"s,     []{ return ret8(int8_t(r.A) >> r.B); }},
    {"_bshru"s,     []{ return ret8(r.A >> r.B); }},
    {"_ishrs"s,     []{ return ret24(int32_t(r.HL) << 8 >> 8 >> r.B); }},
    {"_llshru"s,    []{ return ret64(memref<uint64_t>(r.HL) >> r.C); }},
    {"_lladd"s,     []{ return ret64(memref<uint64_t>(r.HL) + memref<uint64_t>(r.BC)); }},
};

bool emulate_libcall(void) {
    std::string name(memref<char[25]>(r.PC));
    if (auto handler = libcall_handlers.find(name); handler != libcall_handlers.end())
        return handler->second();
    fprintf(stderr, "Unimplemented libcall: %s\n", name.c_str());
    r.HL = 2;
    return false;
}
}

extern "C" {

void gui_console_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, format, args);
    va_end(args);
}

int main(int argc, char **argv) {
    if (argc != 2)
        return 1;
    asic_init();
    asic_reset();
    emu_load(EMU_DATA_RAM, argv[1]);
    r.SPL = 0xD40000;
    sched.event.cycle = 1000000;
    cpu_flush(0xD00000, true);
    do
        cpu_execute();
    while (cpu.halted && emulate_libcall());
    asic_free();
    return r.HL;
}

}
