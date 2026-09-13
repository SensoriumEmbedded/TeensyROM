#include "native-display.h"

namespace dosvm_nuflix {
bool DisplayConverter::schedule(NativeDisplay &out, bool ntsc, const uint8_t *slowValues) {
    out.codeBase=0;deferredUnderlayUpdates=0;
    fit = &out.fit; markBugUnused();
    FixedList<Update,16> rows[100];
    FixedList<Update,408> deferred;
    for (unsigned row = 0; row < 100; ++row) {
        for (unsigned vertical = row*2+1; vertical <= row*2+2 && vertical < 200; ++vertical) for (unsigned slot = 0; slot < 6; ++slot) {
            unsigned color = fit->underlay[vertical*6+slot]&15;
            if (color != (fit->underlay[(vertical-1)*6+slot]&15)) rows[row].add({int16_t(color),int16_t(vertical),int16_t(vertical),Underlay,uint8_t(slot)});
        }
        if (row < 99) for (unsigned slot = 0; slot < 4; ++slot) {
            int next = int8_t(fit->bug[(row+1)*4+slot]), color = next&15;
            if (color == (fit->bug[row*4+slot]&15)) continue;
            Update update{int16_t(color),int16_t(row*2+2),int16_t(row*2+2),Bug,uint8_t(slot)};
            if (next < 0) {
                int last = 100;
                for (unsigned future = row+2; future < 100; ++future) if ((fit->bug[future*4+slot]&15) != color) { last = future-1; break; }
                update.lastScreen = last*2; deferred.add(update);
            } else rows[row].add(update);
        }
        static constexpr int16_t bottom[]{0x98,0xa8,0xb8,0x88,0x98,0xa8,0xb8,0x88,0x98,0xa8,0xb8,0x88,0x98,0xa8,0xb8,0x88,0x98,0xa8,0xb8,0x98,0xa8,0xb8,0x88,0x98,0xa8,0xb8,0x88,0x98,0xa8,0xb8,0x88,0x98,0xa8,0xb8,0x88,0x18};
        int screen = row < 63 ? ((6-int(row&7))&7)*16+8 : row == 63 ? -253 : bottom[row-64];
        rows[row].add({int16_t(screen),int16_t(row*2+2),int16_t(row*2+2),Screen,255});
        rows[row].add({int16_t(row < 99 ? 0x38|((row*2+2)&7) : 0x10),int16_t(row*2+2),int16_t(row*2+2),Fli,255});
        rows[row].sort([](const Update &left,const Update &right){return left.first()<right.first() || (left.first()==right.first() && left.last()<right.last());});
        if (!rows[row].valid) return false;
    }
    for (unsigned slot = 0; slot < 8; ++slot) deferred.add({0xd4,0x7b,int16_t(0xa3-((slot^7)*2)),SpriteY,uint8_t(slot)});
    if (!deferred.valid) return false;
    deferred.sort([](const Update &left,const Update &right){return left.screen<right.screen || (left.screen==right.screen && (left.lastScreen<right.lastScreen || (left.lastScreen==right.lastScreen && left.address()<right.address())));});
    bool used[408]{};
    Updates current, next;
    for (unsigned index=0;index<rows[0].count;++index) current.add(rows[0][index]);
    for (unsigned index=0;index<rows[1].count;++index) next.add(rows[1][index]);
    uint8_t initial[2]{}; unsigned initialCount=0;
    for (unsigned index=0;index<current.count && initialCount<2;++index) {
        const auto &update=current[index]; bool found=false;
        for (unsigned previous=0;previous<initialCount;++previous) found |= initial[previous]==uint8_t(update.value) || (update.nybble() && (initial[previous]&15)==(update.value&15));
        if (!found) initial[initialCount++]=uint8_t(update.value);
    }
    out.initialX=initial[0]; out.initialY=initial[1]; out.bankOperand=0;
    for (unsigned slot=0;slot<4;++slot) fit->bug[slot]&=15;
    int regs[]{0x38,initial[0],initial[1]};
    unsigned length=0; bool valid=true;
    auto byte=[&](unsigned value){if(length<sizeof(out.code))out.code[length++]=uint8_t(value);else valid=false;};
    auto delay=[&](unsigned cycles){if(cycles==2)byte(0xea);else if(cycles==3){byte(0x24);byte(0);}else if(cycles==4){byte(0xea);byte(0xea);}else valid=false;};
    byte(0x8d);byte(0x11);byte(0xd0);
    if(ntsc)delay(3);
    int pad=0, previousX=initial[0]; bool endedLast=false;
    for(unsigned row=0;row<100;++row){
        Updates matching; unsigned indices[32];
        FixedList<unsigned,408> order;
        for(unsigned index=0;index<deferred.count;++index) if(!used[index] && deferred[index].screen<=int(row*2+2) && int(row*2+1)<=deferred[index].lastScreen) order.add(index);
        order.sort([&](unsigned left,unsigned right){return deferred[left].lastScreen<deferred[right].lastScreen;});
        for(unsigned index=0;index<order.count && index<32;++index){indices[index]=order[index];matching.add(deferred[order[index]]);}
        unsigned limit=matching.count<6?matching.count:6;
        Snippet snippet;
        bool scheduled=false;
        for(unsigned attempt=0;attempt<64;++attempt){
            snippet.generate(row,regs,limit,current,next,matching);
            if(!snippet.overflow){scheduled=true;break;}
            if(snippet.extras>0){limit=snippet.extras-1;continue;}
            if(!resolve(row,current,next)){
#if defined(MPE_DOS_NUFLIX_DOUBLE)
                if(deferUnderlay(row,current,next))continue;
#endif
                break;
            }
        }
        if(!scheduled || !current.valid || !next.valid)return false;
        for(unsigned index=0;index<snippet.included.count;++index)used[indices[snippet.included[index]]]=true;
        for(unsigned index=0;index<deferred.count;++index)if(deferred[index].lastScreen<int(row*2+3))used[index]=true;
        memcpy(regs,snippet.regs,sizeof(regs));
        if(row<99){
            memcpy(fit->bug+(row+1)*4,fit->bug+row*4,4);
            for(unsigned slot=0;slot<6;++slot)fit->underlay[row*12+6+slot]=fit->underlay[row*12+slot]&15;
            bool second=false;
            for(unsigned index=0;index<snippet.effective.count;++index){
                const auto &update=snippet.effective[index];
                if(update.type==Bug)fit->bug[(row+1)*4+update.slot]=update.value&15;
                if(update.type!=Underlay)continue;
                if(!second && update.screen>int(row*2+1)){
                    second=true;
                    for(unsigned slot=0;slot<6;++slot)fit->underlay[row*12+12+slot]=fit->underlay[row*12+6+slot]&15;
                }
                fit->underlay[(update.screen>int(row*2+1)?update.screen:row*2+1)*6+update.slot]=uint8_t(update.value);
            }
            if(!second)for(unsigned slot=0;slot<6;++slot)fit->underlay[row*12+12+slot]=fit->underlay[row*12+6+slot]&15;
        }
        pad+=endedLast?3:2;endedLast=snippet.code[snippet.code.count-1].operand==0xd02d;
        const bool trigger=(row&3)<3 || row==99;
        bool extended=trigger && snippet.code[snippet.code.count-1].op==Sta && snippet.code[snippet.code.count-2].operand==0xd02d;
        int xValue=previousX;
        if(extended)for(unsigned index=snippet.code.count-1;index-->0;)if(snippet.code[index].op==Ldx){xValue=snippet.code[index].operand;break;}
        if(ntsc)delay(pad);
        for(unsigned index=0;index<snippet.code.count;++index){
            const auto &instruction=snippet.code[index];
            if(ntsc && trigger && index+1==snippet.code.count){
                if(extended){const unsigned address=0xd011-(xValue&255);byte(0x9d);byte(address);byte(address>>8);continue;}
                delay(2);
            }
            if(instruction.op==Nop){delay(instruction.extra?3:2);continue;}
            if(instruction.op>=Sta){
                static constexpr uint8_t stores[]{0x8d,0x8e,0x8c};byte(stores[instruction.op-Sta]);byte(instruction.operand);byte(instruction.operand>>8);continue;
            }
            static constexpr uint8_t loads[]{0xa9,0xa2,0xa0},slowLoads[]{0xa5,0xa6,0xa4};
            if(instruction.operand==-253)out.bankOperand=uint16_t(0x1001+length);
            byte(instruction.extra?slowLoads[instruction.op-Lda]:loads[instruction.op-Lda]);
            if(instruction.extra){
                unsigned selected=32;
                for(unsigned candidate=0;candidate<32;++candidate)if(slowValues[candidate]==instruction.operand)selected=candidate;
                if(selected==32)return false;
                byte(selected+0xd1);
            }else byte(instruction.operand);
        }
        pad=trigger || endedLast?0:2;previousX=regs[1];
        current=next;next.clear();
        if(row+2<100)for(unsigned index=0;index<rows[row+2].count;++index)next.add(rows[row+2][index]);
    }
    byte(0x60);out.codeBytes=length;
    while(length<sizeof(out.code))byte(0);
    return valid && out.bankOperand>=0x1000 && out.bankOperand<0x2100;
}
void DisplayConverter::pack(NativeDisplay &out,uint8_t *rendered,uint32_t bands){
    fit=&out.fit;
    for(unsigned row=0;row<200;++row){
    if(!(bands&(1u<<(row/8))))continue;
    for(unsigned column=0;column<40;++column)out.bitmap[(row/8)*320+column*8+(row&7)]=0;
    memset(out.sprites+row*24,0,24);
    for(unsigned column=0;column<320;column+=2){
        unsigned attributes=fit->attributes[row/2*40+column/8];
        unsigned left=source.read(source.context,column,row),right=source.read(source.context,column+1,row);
        bool first=false,second=false; unsigned firstColor=0,secondColor=0;
        if(column<24){
            unsigned background,firstLayer,secondLayer;const auto palette=fit->bug+row/2*4;
            bugPair(left,right,palette,background,firstLayer,secondLayer);
            first=firstLayer==2;second=secondLayer==2;
            unsigned offset=row*24+18+column/8,bit=7-(column&7);
            if(firstLayer==1)out.sprites[offset]|=1u<<bit;
            if(secondLayer==1)out.sprites[offset]|=1u<<(bit-1);
            out.sprites[offset+3]|=background<<(bit-1);
            firstColor=first?15:firstLayer==1?(palette[0]&15):background?(palette[background]&15):15;
            secondColor=second?15:secondLayer==1?(palette[0]&15):background?(palette[background]&15):15;
        }else{
            int sprite=-1;
            if(column<312){
                unsigned colorRow=row<199 && (row&1) && column>=304?row+1:row;
                sprite=fit->underlay[colorRow*6+(column-24)/48]&15;
                if(column==304 && (row&1))sprite=-1;
            }
            unsigned pattern;midPair(left,right,attributes&15,attributes>>4,sprite,pattern);
            first=pattern<4?(pattern&2)!=0:pattern==6;second=pattern<4?(pattern&1)!=0:pattern==5;
            if(pattern>=4){unsigned position=(column-24)/2;out.sprites[row*24+position/8]|=0x80>>(position&7);}
            firstColor=first?attributes>>4:pattern>=4?unsigned(sprite):attributes&15;
            secondColor=second?attributes>>4:pattern>=4?unsigned(sprite):attributes&15;
        }
        unsigned offset=(row/8)*320+(column&~7u)+(row&7),bit=7-(column&7);
        out.bitmap[offset]|=(unsigned(first)<<bit)|(unsigned(second)<<(bit-1));
        if(rendered){rendered[row*320+column]=uint8_t(firstColor);rendered[row*320+column+1]=uint8_t(secondColor);}
    }
    }
}
}
