#pragma once
// The body of a download as it arrives (build 83): "Transfer-Encoding: chunked" removed, and
// "Content-Encoding: gzip" unpacked. Google sends iCal files about 10 x smaller packed; the device's
// TCP window (5,760 bytes in the ESP32 core) limits the speed, so fewer bytes are the only real
// gain: a 1,098-entry work calendar took 15 s unpacked. Plain C++, tested on the PC
// (tests/host/test_http_body.cpp); the decompressor is miniz 3.0.2's tinfl (third_party/miniz),
// the same code on the device (src/services/Inflate.c) and in the tests.
#include "MinizNames.h"  // own names: the ROM's older tinfl would win otherwise
#include "../../third_party/miniz/miniz.h"  // compiled by Inflate.c

#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace httpbody {

// Chunked transfer (RFC 9112 7.1): "<size in hex>[;extension]\r\n<data>\r\n" ... "0\r\n[trailers]\r\n".
class Dechunker {
 public:
  void reset() { *this = Dechunker(); }
  bool done() const { return state == State::Done; }

  // Bytes in, the body's bytes out through out(const uint8_t*, size_t). False after a framing error.
  template <typename Out>
  bool push(const uint8_t* p, size_t n, Out out) {
    while (n) {
      switch (state) {
        case State::Size: {
          const uint8_t c = *p;
          int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
          if (digit >= 0 && sizeDigits < 8) {
            remaining = remaining * 16 + static_cast<uint32_t>(digit);
            sizeDigits++;
          } else if (digit >= 0 && remaining == 0) {
            // more leading zeros: fine
          } else if ((c == ';' || c == ' ' || c == '\t') && sawDigit()) {
            state = State::Extension;
          } else if (c == '\r' && sawDigit()) {
            state = State::SizeLf;
          } else if (c == '\n' && sawDigit()) {
            endSizeLine();
          } else {
            return fail();
          }
          if (digit >= 0) anyDigit = true;
          p++, n--;
          break;
        }
        case State::Extension:
          if (*p == '\n') endSizeLine();
          p++, n--;
          break;
        case State::SizeLf:
          if (*p != '\n') return fail();
          endSizeLine();
          p++, n--;
          break;
        case State::Data: {
          const size_t take = n < remaining ? n : remaining;
          out(p, take);
          p += take, n -= take;
          remaining -= static_cast<uint32_t>(take);
          if (!remaining) state = State::DataCr;
          break;
        }
        case State::DataCr:
          if (*p == '\r') state = State::DataLf;
          else if (*p == '\n') state = State::Size;
          else return fail();
          p++, n--;
          break;
        case State::DataLf:
          if (*p != '\n') return fail();
          state = State::Size;
          p++, n--;
          break;
        case State::Trailer:  // header lines after the last chunk, until an empty one
          if (*p == '\n') {
            if (!trailerLen) state = State::Done;
            trailerLen = 0;
          } else if (*p != '\r') {
            trailerLen++;
          }
          p++, n--;
          break;
        case State::Done:
          return true;  // anything after the end is ignored
        case State::Failed:
          return false;
      }
    }
    return state != State::Failed;
  }

 private:
  enum class State : uint8_t { Size, Extension, SizeLf, Data, DataCr, DataLf, Trailer, Done, Failed };
  State state = State::Size;
  uint32_t remaining = 0;
  uint8_t sizeDigits = 0;
  bool anyDigit = false;
  size_t trailerLen = 0;

  bool sawDigit() const { return anyDigit; }
  bool fail() {
    state = State::Failed;
    return false;
  }
  void endSizeLine() {
    state = remaining ? State::Data : State::Trailer;
    sizeDigits = 0;
    anyDigit = false;
    trailerLen = 0;
  }
};

// gzip (RFC 1952): the header, the deflate data (tinfl with a 32 KB window: about 43 KB of heap
// between begin() and the destructor), the trailer's size checked when it arrives whole.
class Gunzip {
 public:
  Gunzip() = default;
  Gunzip(const Gunzip&) = delete;
  Gunzip& operator=(const Gunzip&) = delete;
  ~Gunzip() { end(); }

  static constexpr size_t HEAP_BYTES = TINFL_LZ_DICT_SIZE + sizeof(tinfl_decompressor);

  // False without the memory (then the download is asked for unpacked).
  bool begin() {
    end();
    window = static_cast<uint8_t*>(std::malloc(TINFL_LZ_DICT_SIZE));
    inflater = static_cast<tinfl_decompressor*>(std::malloc(sizeof(tinfl_decompressor)));
    if (!window || !inflater) {
      end();
      return false;
    }
    tinfl_init(inflater);
    state = State::Header;
    pending = false;
    why = nullptr;
    have = windowPos = 0;
    flags = 0;
    extraLeft = 0;
    unpacked = packed = 0;
    return true;
  }
  void end() {
    std::free(window);
    std::free(inflater);
    window = nullptr;
    inflater = nullptr;
  }
  // The data is whole once the decompressor has reached its end: the 8-byte trailer may arrive
  // short, because the chip's ROM tinfl (an older miniz) keeps up to 4 bytes after the data in its
  // bit buffer (newer versions give them back). Its size is checked when it arrives whole.
  bool done() const { return state == State::Trailer || state == State::Done; }
  uint32_t packedBytes() const { return packed; }
  uint32_t unpackedBytes() const { return unpacked; }
  const char* failure() const { return why; }  // after push() returned false: which check, for the log

  // Packed bytes in, the unpacked ones out through out(const uint8_t*, size_t). False when the
  // data isn't gzip or is damaged.
  template <typename Out>
  bool push(const uint8_t* p, size_t n, Out out) {
    if (!inflater) return false;
    packed += static_cast<uint32_t>(n);
    while (n || pending) {  // pending: output the decompressor still holds (its window was full)
      switch (state) {
        case State::Header:  // ID1 ID2 CM FLG MTIME(4) XFL OS
          head[have++] = *p++, n--;
          if (have == 10) {
            if (head[0] != 0x1f || head[1] != 0x8b || head[2] != 8 || (head[3] & 0xE0)) return fail("not gzip");
            flags = head[3];
            have = 0;
            nextOptional();
          }
          break;
        case State::ExtraLen:
          head[have++] = *p++, n--;
          if (have == 2) {
            extraLeft = static_cast<uint16_t>(head[0] | (head[1] << 8));
            have = 0;
            state = State::Extra;
            if (!extraLeft) nextOptional();
          }
          break;
        case State::Extra:
          p++, n--;
          if (!--extraLeft) nextOptional();
          break;
        case State::Name:
        case State::Comment:
          if (*p++ == 0) nextOptional();
          n--;
          break;
        case State::HeaderCrc:
          p++, n--;
          if (++have == 2) {
            have = 0;
            nextOptional();
          }
          break;
        case State::Deflate: {
          size_t inSize = n, outSize = TINFL_LZ_DICT_SIZE - windowPos;
          const tinfl_status st = tinfl_decompress(inflater, p, &inSize, window, window + windowPos, &outSize,
                                                   TINFL_FLAG_HAS_MORE_INPUT);
          p += inSize, n -= inSize;
          if (outSize) {
            out(window + windowPos, outSize);
            unpacked += static_cast<uint32_t>(outSize);
            windowPos = (windowPos + outSize) & (TINFL_LZ_DICT_SIZE - 1);
          }
          pending = st == TINFL_STATUS_HAS_MORE_OUTPUT;
          if (st == TINFL_STATUS_DONE) {
            state = State::Trailer;
            have = 0;
          } else if (st < 0) {
            return fail("inflate error");
          } else if (st == TINFL_STATUS_NEEDS_MORE_INPUT && !inSize && !outSize) {
            return true;  // waits for the next bytes
          }
          break;
        }
        case State::Trailer:  // CRC-32, then the size (mod 2^32)
          head[have++] = *p++, n--;
          if (have == 8) {
            const uint32_t size = head[4] | (head[5] << 8) | (head[6] << 16) | (static_cast<uint32_t>(head[7]) << 24);
            if (size != unpacked) return fail("size mismatch");
            state = State::Done;
          }
          break;
        case State::Done:
          return true;
        case State::Failed:
          return false;
      }
    }
    return true;
  }

 private:
  enum class State : uint8_t { Header, ExtraLen, Extra, Name, Comment, HeaderCrc, Deflate, Trailer, Done, Failed };
  bool pending = false;
  const char* why = nullptr;
  uint8_t* window = nullptr;
  tinfl_decompressor* inflater = nullptr;
  State state = State::Header;
  uint8_t head[10] = {};
  size_t have = 0, windowPos = 0;
  uint8_t flags = 0;
  uint16_t extraLeft = 0;
  uint32_t unpacked = 0, packed = 0;

  bool fail(const char* reason) {
    state = State::Failed;
    why = reason;
    return false;
  }
  // The optional header parts in their order (FEXTRA, FNAME, FCOMMENT, FHCRC), then the data.
  void nextOptional() {
    if (flags & 0x04) {
      flags &= ~0x04;
      state = State::ExtraLen;
    } else if (flags & 0x08) {
      flags &= ~0x08;
      state = State::Name;
    } else if (flags & 0x10) {
      flags &= ~0x10;
      state = State::Comment;
    } else if (flags & 0x02) {
      flags &= ~0x02;
      state = State::HeaderCrc;
    } else {
      state = State::Deflate;
    }
  }
};

// The whole body path of a download (Calendar.cpp): the chunks taken off when chunked, then the
// data unpacked when packed (gz), each byte exactly once. Build 84 fed chunked data to the unpacker
// twice (once still framed) in the device code, where no test saw it: every packed download failed.
class BodyDecoder {
 public:
  void begin(bool isChunked, Gunzip* unpacker) {
    chunked = isChunked;
    gz = unpacker;
    dechunk.reset();
    failed = false;
    why = nullptr;
  }

  // Received bytes in, the body's (unpacked) bytes out through out(const uint8_t*, size_t). False
  // once the data is damaged (failure() says where).
  template <typename Out>
  bool push(const uint8_t* p, size_t n, Out out) {
    if (failed) return false;
    auto unpack = [&](const uint8_t* q, size_t m) {
      if (failed) return;
      if (!gz) {
        out(q, m);
      } else if (!gz->push(q, m, out)) {
        failed = true;
        why = gz->failure() ? gz->failure() : "packed data damaged";
      }
    };
    if (!chunked) {
      unpack(p, n);
    } else if (!dechunk.push(p, n, unpack) && !failed) {
      failed = true;
      why = "chunk error";
    }
    return !failed;
  }

  bool ended() const { return chunked && dechunk.done(); }  // the last chunk arrived: nothing more comes
  // Everything there: no damage, the last chunk (when chunked), the unpacker's end (when packed).
  bool whole() const { return !failed && (!chunked || dechunk.done()) && (!gz || gz->done()); }
  const char* failure() const { return why; }

 private:
  bool chunked = false, failed = false;
  Gunzip* gz = nullptr;
  Dechunker dechunk;
  const char* why = nullptr;
};

}  // namespace httpbody
