#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

class Stream {
public:
   virtual int available() = 0;
   virtual int read() = 0;
};

class MemoryStream : public Stream {
public:
   explicit MemoryStream(const std::string& bytes, size_t readable = SIZE_MAX)
      : data(bytes), limit(readable < bytes.size() ? readable : bytes.size()) {}
   int available() override { return position < limit; }
   int read() override { return position < limit ? (unsigned char)data[position++] : -1; }
private:
   std::string data;
   size_t position = 0, limit;
};

static unsigned stagingWrites, mainFlashMoves, okMessages, failedMessages, flashIdChecks;
static bool targetValid = true;
static std::vector<uint32_t> stagingAddresses;
static std::vector<std::string> messages;

static void SendMsgPrintfln(const char* format, ...) { messages.emplace_back(format ? format : ""); }
static void SendMsgOK() { ++okMessages; }
static void SendMsgFailed() { ++failedMessages; }
static int flash_write_block(uint32_t address, char*, uint32_t) {
   ++stagingWrites; stagingAddresses.push_back(address); return 0;
}
static int check_flash_id(uint32_t, uint32_t size, const char* id) {
   ++flashIdChecks;
   assert(size >= std::strlen(id));
   return targetValid;
}
static void flash_move(uint32_t, uint32_t, uint32_t) { ++mainFlashMoves; }
static void detachInterrupt(int) {}
static int digitalPinToInterrupt(int pin) { return pin; }

#define FXUTIL_HOST_TEST
#define Fab04_Features
#define FLASH_ID "host-test-firmware"
#define FLASH_BASE_ADDR UINT32_C(0x60000000)
#define IN_FLASH(address) (true)
#define Menu_Btn_In_PIN 1
#define PHI2_PIN 2
#define NVIC_DISABLE_IRQ(irq) ((void)(irq))
#define IRQ_ENET 3
#define IRQ_PIT 4
#define REBOOT do {} while (0)
#include "../Flash/FXUtil.cpp"

static std::string record(uint8_t type, uint16_t address, const std::vector<uint8_t>& data) {
   assert(data.size() <= 255);
   char field[16];
   std::snprintf(field,sizeof field,":%02X%04X%02X",unsigned(data.size()),unsigned(address),unsigned(type));
   std::string line(field);
   unsigned sum = unsigned(data.size()) + (address >> 8) + (address & 255) + type;
   for (uint8_t byte : data) {
      std::snprintf(field,sizeof field,"%02X",unsigned(byte)); line += field; sum += byte;
   }
   std::snprintf(field,sizeof field,"%02X",unsigned((-sum) & 255)); line += field;
   return line + "\n";
}

static std::string image(const std::vector<uint8_t>& data, bool eof = true) {
   std::string text = record(4,0,{0x60,0x00}) + record(0,0,data);
   if (eof) text += record(1,0,{});
   return text;
}

static void reset() {
   stagingWrites = mainFlashMoves = okMessages = failedMessages = flashIdChecks = 0;
   targetValid = true; stagingAddresses.clear(); messages.clear();
}

static bool saw(const char* fragment) {
   for (const std::string& message : messages) if (message.find(fragment) != std::string::npos) return true;
   return false;
}

static uint32_t crc32(const std::string& text) {
   uint32_t value=UINT32_MAX;
   for (unsigned char byte : text) value=firmware_crc32_byte(value,byte);
   return ~value;
}

static void run(const std::string& text, uint32_t size = 0x10000, size_t readable = SIZE_MAX,
                uint32_t expectedCRC = 0, bool verifyCRC = false) {
   MemoryStream input(text,readable), output("");
   update_firmware(&input,&output,0x70000000,size,expectedCRC,verifyCRC);
}

int main() {
   const std::vector<uint8_t> payload(32,0x5a);
   assert(crc32("123456789") == 0xcbf43926u);

   reset(); run(image(payload));
   assert(mainFlashMoves == 1 && stagingWrites == 1 && stagingAddresses[0] == 0x70000000);

   const std::string verified=image(payload);
   reset(); run(verified,0x10000,SIZE_MAX,crc32(verified),true);
   assert(mainFlashMoves == 1);

   reset(); run(verified,0x10000,SIZE_MAX,crc32(verified)^1u,true);
   assert(mainFlashMoves == 0 && saw("changed after confirmation"));

   reset(); run(image(std::vector<uint8_t>(255,0xa5)));
   assert(mainFlashMoves == 1 && stagingWrites == 1);

   reset(); run("");
   assert(mainFlashMoves == 0 && stagingWrites == 0 && saw("Unexpected end"));

   reset(); run("\r\n\n");
   assert(mainFlashMoves == 0 && stagingWrites == 0 && saw("Unexpected end"));

   reset(); run(image(payload,false));
   assert(mainFlashMoves == 0 && stagingWrites == 1 && saw("Unexpected end"));

   reset(); { std::string bad=image(payload); bad[bad.find("5A")]='G'; run(bad); }
   assert(mainFlashMoves == 0 && saw("Bad hex line"));

   for (const char* bad : {":0G00000000FF", ":010G000000FF", ":0100000G00FF",
                           ":010000000GFF", ":0100000000FG"}) {
      char bytes[256]; unsigned address=0,count=0,type=0;
      assert(parse_hex_line(bad,bytes,&address,&count,&type)==0);
   }

   reset(); run(std::string(600,'X') + "\n");
   assert(mainFlashMoves == 0 && stagingWrites == 0 && saw("too long"));

   reset(); { std::string truncated=image(payload); truncated.resize(20); run(truncated); }
   assert(mainFlashMoves == 0 && saw("Bad hex line"));

   reset(); run(record(4,0,{0x5f,0xff}) + record(0,0,payload) + record(1,0,{}));
   assert(mainFlashMoves == 0 && stagingWrites == 0 && saw("address out of range"));

   reset(); run(record(4,0,{0x60,0x00}) + record(0,0x1000,payload) + record(1,0,{}),0x1000);
   assert(mainFlashMoves == 0 && stagingWrites == 0 && saw("address out of range"));

   reset(); run(record(4,0,{0xff,0xff}) + record(0,0xfff0,payload) + record(1,0,{}),0x1000);
   assert(mainFlashMoves == 0 && stagingWrites == 0 && saw("address out of range"));

   reset(); run(record(1,0,{}));
   assert(mainFlashMoves == 0 && stagingWrites == 0 && saw("no firmware data"));

   reset(); run(record(4,0,{0x60,0x00}) + record(0,0,{0x5a}) + record(1,0,{}));
   assert(mainFlashMoves == 0 && flashIdChecks == 0 && failedMessages == 1);

   reset(); run(record(4,0,{0x60,0x00}) + record(0,1,payload) + record(1,0,{}));
   assert(mainFlashMoves == 0 && stagingWrites == 1 && flashIdChecks == 0 && saw("does not start"));

   reset(); run(image(payload) + "NOT ANOTHER IMAGE\n");
   assert(mainFlashMoves == 0 && saw("after hex EOF"));

   reset(); run(record(4,0,{0x60,0x00}) + record(0,0,{0}) +
      record(5,0,{0x60,0x00,0x00,0x20}) + record(0,0x20,payload) + record(1,0,{}));
   assert(mainFlashMoves == 1 && stagingWrites == 2 && stagingAddresses[0] == 0x70000000 &&
      stagingAddresses[1] == 0x70000020);

   reset(); run(record(1,0,{0}) + record(1,0,{}));
   assert(mainFlashMoves == 0 && stagingWrites == 0 && saw("Invalid hex code"));

   reset(); run(image(payload),0x10000,17);
   assert(mainFlashMoves == 0 && saw("Bad hex line"));

   std::puts("25 flash parser safety scenarios passed");
}
