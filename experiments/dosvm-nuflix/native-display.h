#pragma once
#include "native-fit.h"

namespace dosvm_nuflix {
template<class Value, unsigned Capacity> struct FixedList {
    Value values[Capacity];
    unsigned count = 0;
    bool valid = true;
    Value &operator[](unsigned index) { return values[index]; }
    const Value &operator[](unsigned index) const { return values[index]; }
    void add(const Value &value) { if (count < Capacity) values[count++] = value; else valid = false; }
    void remove(unsigned index) { for (++index; index < count; ++index) values[index - 1] = values[index]; --count; }
    void clear() { count = 0; valid = true; }
    template<class Compare> void sort(Compare compare) {
        for (unsigned index = 1; index < count; ++index) {
            Value value = values[index]; unsigned position = index;
            while (position && compare(value, values[position - 1])) { values[position] = values[position - 1]; --position; }
            values[position] = value;
        }
    }
};
enum UpdateType : uint8_t { Underlay, Bug, Border, Background, Screen, Fli, SpriteY };
struct Update {
    int16_t value, screen, lastScreen;
    UpdateType type;
    uint8_t slot;
    int address() const {
        static constexpr int bugRegisters[]{0xd027,0xd025,0xd02e,0xd026};
        switch (type) {
            case Underlay: return 0xd028 + slot;
            case Bug: return bugRegisters[slot];
            case Border: return 0xd020;
            case Background: return 0xd021;
            case Screen: return value < 0 ? 0xdd00 : 0xd018;
            case Fli: return 0xd011;
            default: return 0xd001 + slot * 2;
        }
    }
    int first() const {
        if (type == Underlay && screen >= 0 && !(screen & 1)) return 20 + (slot + 1) * 6 < 55 ? 20 + (slot + 1) * 6 : 55;
        if (type == Bug && screen >= 0) return 20;
        if (type == Border) return 14;
        if (type == Fli) return !(screen & 6) ? (screen < 200 ? 10 : 56) : 58;
        return 10;
    }
    int last() const {
        if (type == Underlay && screen >= 0 && (screen & 1)) return 19 + slot * 6;
        if (type == Fli && first() > 55) return first();
        return 55;
    }
    bool nybble() const { return type <= Background; }
    bool accepts(int candidate) const { return candidate == value || (candidate >= 0 && nybble() && (candidate & 15) == (value & 15)); }
    bool early() const { return last() < 55; }
    bool late(int cycle) const { return first() > cycle + 3; }
};
using Updates = FixedList<Update, 32>;
inline void sortUpdates(Updates &updates) {
    updates.sort([](const Update &left, const Update &right) { return left.first() < right.first() || (left.first() == right.first() && left.last() < right.last()); });
}
enum Op : uint8_t { Nop, Rts, Lda, Ldx, Ldy, Sta, Stx, Sty };
struct Instruction {
    int operand = 0;
    Op op = Nop;
    bool extra = false;
    unsigned length() const { return op == Nop ? (extra ? 2 : 1) : op == Rts ? 1 : op < Sta ? 2 : 3; }
};
struct Snippet {
    FixedList<Instruction, 64> code;
    Updates effective;
    FixedList<unsigned, 32> included;
    int regs[3], uses[3], cycle, fliRegister, lastRegister, extras;
    bool overflow;
    bool covered(const Update &update) const { return update.accepts(regs[0]) || update.accepts(regs[1]) || update.accepts(regs[2]); }
    void load(int selected, int value, const Updates &future) {
        if (selected < 0) { overflow = true; return; }
        regs[selected] = value; uses[selected] = 0; cycle += 2;
        for (unsigned index = 0; index < future.count; ++index) uses[selected] += future[index].accepts(value);
        code.add({int16_t(value), Op(Lda + selected), false});
    }
    void store(int selected, const Update &update) {
        cycle += 4; code.add({update.address(), Op(Sta + selected), false}); effective.add(update);
        if (update.last() < cycle - 1 && !(update.last() == 58 && code.count > 1 && code[code.count - 2].op >= Sta)) overflow = true;
    }
    bool immediate(const Update &update) {
        bool emitted = false;
        for (int selected = 0; selected < 3; ++selected) if (update.accepts(regs[selected])) {
            if (!emitted) { store(selected, update); emitted = true; }
            --uses[selected];
        }
        return emitted;
    }
    int bestLoad(int value, const Updates &remaining, bool nybble = false) {
        int selected = -1, minimum = 32767;
        for (int reg = 0; reg < 3; ++reg) if (reg != fliRegister && reg != lastRegister && uses[reg] < minimum) { selected = reg; minimum = uses[reg]; }
        if (nybble) for (unsigned index = 0; index < remaining.count; ++index)
            if (!remaining[index].nybble() && (remaining[index].value & 15) == value) { value = remaining[index].value; break; }
        for (int reg = 0; reg < 3; ++reg) if (regs[reg] == value) return reg;
        load(selected, value, remaining); return selected;
    }
    void nop(bool extra = false) { cycle += extra ? 3 : 2; code.add({0, Nop, extra}); }
    NUFLIX_CODE void generate(unsigned row, const int *initial, unsigned extraLimit, const Updates &updates,
                  const Updates &following, const Updates &deferred) {
        code.clear(); effective.clear(); included.clear(); overflow = false; extras = 0;
        cycle = row && !(row & 3) ? 10 : 11;
        memcpy(regs, initial, sizeof(regs));
        Updates remaining = updates;
        for (unsigned index = 0; index < deferred.count; ++index) {
            auto update = deferred[index];
            if (update.type != SpriteY || update.lastScreen != int(row * 2 + 1)) {
                if (unsigned(extras) >= extraLimit) break;
                ++extras;
            }
            if (update.screen < int(row * 2 + 1)) update.screen = -1;
            update.lastScreen = update.screen; remaining.add(update); included.add(index);
        }
        sortUpdates(remaining);
        for (unsigned reg = 0; reg < 3; ++reg) {
            uses[reg] = 0;
            for (unsigned index = 0; index < remaining.count; ++index) uses[reg] += remaining[index].accepts(regs[reg]);
        }
        fliRegister = lastRegister = -1;
        while (remaining.count) {
            Update update = remaining[0];
            if (update.early()) {
                remaining.remove(0);
                if (!immediate(update)) store(bestLoad(update.value, remaining, update.nybble()), update);
            } else if (!update.late(cycle)) {
                bool emitted = false; unsigned selected = 0;
                for (unsigned index = 0; index < remaining.count; ++index) {
                    if (remaining[index].late(cycle)) break;
                    if (immediate(remaining[index])) { selected = index; emitted = true; break; }
                }
                update = remaining[selected]; remaining.remove(selected);
                if (!emitted) store(bestLoad(update.value, remaining, update.nybble()), update);
            } else {
                if (fliRegister < 0 && remaining.count > 1 && remaining[remaining.count-1].address() == 0xd011 && remaining[remaining.count-2].address() == 0xd02d) {
                    fliRegister = 0; load(0, remaining[remaining.count-1].value, remaining);
                    lastRegister = bestLoad(remaining[remaining.count-2].value, remaining, true);
                }
                if (!covered(update)) bestLoad(update.value, remaining, update.nybble());
                int wait = update.first() - 3 - cycle;
                while (wait > 1) {
                    bool loaded = false;
                    for (int reg = 0; reg < 3; ++reg) {
                        if (uses[reg] || reg == fliRegister || reg == lastRegister) continue;
                        Updates future = remaining;
                        if (covered(remaining[remaining.count-1])) for (unsigned index = 0; index < following.count; ++index) future.add(following[index]);
                        for (unsigned index = 0; index < future.count; ++index) if (!covered(future[index])) {
                            load(reg, future[index].value, future); loaded = true; break;
                        }
                        break;
                    }
                    if (!loaded) nop();
                    wait -= 2;
                }
                if (wait == 1) for (unsigned index = code.count; index-- > 0;) {
                    auto &instruction = code[index];
                    if (!instruction.extra && instruction.operand >= 0 && instruction.op < Sta) { instruction.extra = true; ++cycle; break; }
                }
                remaining.remove(0);
                if (!immediate(update)) overflow = true;
            }
        }
        while (cycle < 55) nop(cycle == 52);
        overflow |= !code.valid || !effective.valid || !remaining.valid;
    }
};
struct NativeDisplay {
    NativeFit fit;
    uint8_t bitmap[8000], sprites[4800], code[4352];
    uint16_t codeBytes, bankOperand;
    uint8_t initialX, initialY;
    uint16_t codeBase;
};
class DisplayConverter {
    const NativeConverter &colors;
    NativeSource source;
    NativeFit *fit;
public:
    unsigned deferredUnderlayUpdates=0;
    DisplayConverter(const NativeConverter &converter, NativeSource pixels) : colors(converter), source(pixels), fit(nullptr) {}
    NUFLIX_CODE unsigned midPair(unsigned left, unsigned right, int paper, int ink, int sprite, unsigned &pattern) const {
        unsigned best = UINT32_MAX; pattern = 0;
        for (unsigned choice = 0; choice < (sprite < 0 ? 4u : 7u); ++choice) {
            const unsigned first = choice < 4 ? ((choice & 2) ? ink : paper) : choice == 6 ? ink : sprite;
            const unsigned second = choice < 4 ? ((choice & 1) ? ink : paper) : choice == 5 ? ink : sprite;
            const unsigned error = colors.colorDistance(left, first) + colors.colorDistance(right, second);
            if (error < best) { best = error; pattern = choice; }
        }
        return best;
    }
    NUFLIX_CODE unsigned bugPair(unsigned left, unsigned right, const uint8_t *palette, unsigned &background, unsigned &first, unsigned &second) const {
        unsigned best = UINT32_MAX; background = first = second = 0;
        for (unsigned slot = 0; slot < 4; ++slot) {
            unsigned options[]{slot ? unsigned(palette[slot] & 15) : 15, unsigned(palette[0] & 15), 15};
            unsigned selectLeft = 0, selectRight = 0;
            for (unsigned choice = 1; choice < 3; ++choice) {
                if (colors.colorDistance(left, options[choice]) < colors.colorDistance(left, options[selectLeft])) selectLeft = choice;
                if (colors.colorDistance(right, options[choice]) < colors.colorDistance(right, options[selectRight])) selectRight = choice;
            }
            unsigned error = colors.colorDistance(left, options[selectLeft]) + colors.colorDistance(right, options[selectRight]);
            if (error < best) { best = error; background = slot; first = selectLeft; second = selectRight; }
        }
        return best;
    }
    unsigned bugDistance(unsigned row, const uint8_t *palette) const {
        unsigned error = 0, background, first, second;
        for (unsigned vertical = row * 2; vertical < row * 2 + 2; ++vertical) for (unsigned column = 0; column < 24; column += 2)
            error += bugPair(source.read(source.context,column,vertical), source.read(source.context,column+1,vertical), palette, background, first, second);
        return error;
    }
    unsigned midDistance(unsigned row, unsigned block, unsigned sprite) const {
        unsigned error = 0, pattern;
        for (unsigned column = 24 + block * 48; column < 72 + block * 48; column += 2) {
            unsigned attributes = fit->attributes[row/2*40+column/8];
            error += midPair(source.read(source.context,column,row), source.read(source.context,column+1,row), attributes & 15, attributes >> 4, sprite & 15, pattern);
        }
        return error;
    }
    NUFLIX_CODE void markBugUnused() {
        for (int row = 98; row >= 0; --row) {
            unsigned used = 0;
            for (unsigned vertical = row * 2; vertical < unsigned(row * 2 + 2); ++vertical) for (unsigned column = 0; column < 24; column += 2) {
                unsigned background, first, second;
                bugPair(source.read(source.context,column,vertical), source.read(source.context,column+1,vertical), fit->bug+row*4, background, first, second);
                if (background) used |= 1u << background;
                if (first == 1 || second == 1) used |= 1;
            }
            for (unsigned slot = 0; slot < 4; ++slot) if (!(used & (1u << slot))) fit->bug[row*4+slot] = uint8_t((fit->bug[(row+1)*4+slot] & 15) - 16);
        }
    }
    NUFLIX_CODE bool resolve(unsigned row, Updates &current, Updates &next) {
        if (row >= 99) return false;
        int hires = -1, multi = -1;
        for (unsigned index = 0; index < current.count; ++index) if (current[index].type == Bug && current[index].slot == 0) hires = index;
        if (hires >= 0) {
            int slot = -1;
            for (unsigned column = 1; column < 4; ++column) if ((fit->bug[row*4+column]&15) == (current[hires].value&15)) { slot = column; break; }
            for (unsigned index = 0; index < current.count; ++index) if (current[index].type == Bug && current[index].slot == slot) multi = index;
            if (multi >= 0 && (fit->bug[row*4]&15) == (current[multi].value&15)) {
                const Update first = current[hires], second = current[multi];
                if (row < 98 && (first.value&15) == (fit->bug[(row+2)*4]&15)) next.add(first);
                if (row < 98 && (second.value&15) == (fit->bug[(row+2)*4+slot]&15)) next.add(second);
                current.remove(hires > multi ? hires : multi); current.remove(hires < multi ? hires : multi); sortUpdates(next); return true;
            }
        }
        unsigned best = UINT32_MAX; int selected = -1;
        for (unsigned index = 0; index < current.count; ++index) if (current[index].type == Bug) {
            uint8_t palette[4]; memcpy(palette, fit->bug+(row+1)*4, 4); palette[current[index].slot] = fit->bug[row*4+current[index].slot];
            unsigned error = bugDistance(row+1, palette);
            if (error < best) { best = error; selected = index; }
        }
        if (selected >= 0) {
            const Update update = current[selected]; current.remove(selected);
            if (row < 98 && update.value == int8_t(fit->bug[(row+2)*4+update.slot])) next.add(update);
            sortUpdates(next); return true;
        }
        int early[6], late[6];
        for (unsigned slot = 0; slot < 6; ++slot) early[slot] = late[slot] = -1;
        for (unsigned index = 0; index < current.count; ++index) if (current[index].type == Underlay)
            (current[index].early() ? early : late)[current[index].slot] = index;
        best = UINT32_MAX; selected = -1;
        for (unsigned slot = 0; slot < 6; ++slot) if (early[slot] >= 0 && late[slot] >= 0) {
            unsigned previous = fit->underlay[row*12+slot]&15, upcoming = fit->underlay[row*12+12+slot]&15;
            unsigned first = midDistance(row*2+1,slot,previous), second = midDistance(row*2+1,slot,upcoming);
            if (previous == upcoming) second >>= 2;
            if (second < best) { best = second; selected = slot + 6; }
            if (first < best) { best = first; selected = slot; }
        }
        if (selected >= 0) {
            unsigned slot = selected % 6, offset = row*12+slot;
            if (selected < 6) { current.remove(early[slot]); fit->underlay[offset+6] = fit->underlay[offset]; }
            else {
                if (fit->underlay[offset] == fit->underlay[offset+12]) {
                    current.remove(early[slot] > late[slot] ? early[slot] : late[slot]); current.remove(early[slot] < late[slot] ? early[slot] : late[slot]);
                    fit->underlay[offset+6] = fit->underlay[offset];
                } else { current[early[slot]].value = current[late[slot]].value; current.remove(late[slot]); fit->underlay[offset+6] = fit->underlay[offset+12]; }
            }
            return true;
        }
        best = UINT32_MAX;
        for (unsigned slot = 0; slot < 6; ++slot) {
            int index = early[slot] >= 0 ? early[slot] : late[slot];
            if (index < 0 || (fit->underlay[row*12+12+slot]&15) == (fit->underlay[row*12+18+slot]&15)) continue;
            unsigned error = midDistance(row*2+2,slot,fit->underlay[row*12+slot]);
            if (current[index].early()) error += midDistance(row*2+1,slot,fit->underlay[row*12+slot]);
            if (error < best) { best = error; selected = index; }
        }
        if (selected < 0) return false;
        unsigned offset = row*12+current[selected].slot; current.remove(selected);
        fit->underlay[offset+6] = fit->underlay[offset+12] = fit->underlay[offset]; return true;
    }
    NUFLIX_CODE bool deferUnderlay(unsigned row,Updates &current,Updates &next) {
        unsigned best=UINT32_MAX;int selected=-1;
        for(unsigned index=0;index<current.count;++index){
            const auto &update=current[index];if(update.type!=Underlay)continue;
            const unsigned previous=fit->underlay[row*12+update.slot]&15;
            unsigned error=update.early()?midDistance(row*2+1,update.slot,previous):0;
            if(row<99)error+=midDistance(row*2+2,update.slot,previous);
            if(error<best){best=error;selected=int(index);}
        }
        if(selected<0)return false;
        Update update=current[selected];current.remove(selected);
        const unsigned offset=row*12+update.slot;
        fit->underlay[offset+6]=fit->underlay[offset]&15;
        if(row<99){
            fit->underlay[offset+12]=fit->underlay[offset]&15;
            bool superseded=false;
            for(unsigned index=0;index<next.count;++index)superseded|=next[index].type==Underlay&&next[index].slot==update.slot&&next[index].early();
            if(!superseded){update.screen=update.lastScreen=int16_t(row*2+3);next.add(update);sortUpdates(next);}
        }
        ++deferredUnderlayUpdates;return next.valid;
    }
    NUFLIX_IMPL bool schedule(NativeDisplay &out, bool ntsc, const uint8_t *slowValues);
    NUFLIX_IMPL void pack(NativeDisplay &out, uint8_t *rendered = nullptr, uint32_t bands = 0x01ffffffu);
};
static_assert(sizeof(NativeDisplay) <= 23040, "Bounded live NUFLIX workspace");
}
