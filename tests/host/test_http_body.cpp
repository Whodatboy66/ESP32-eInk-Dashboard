// needs: miniz
// Host test for src/services/HttpBody.h: a calendar download's body with "Transfer-Encoding: chunked"
// and "Content-Encoding: gzip", as Google sends it (build 83: a 1,098-entry work calendar took 15 s
// uncompressed; Google packs iCal about 10 x). Run: tests/host/run.sh
#include <cstring>
#include <string>
#include <vector>

#include "src/services/HttpBody.h"  // first: miniz under the firmware's names (MinizNames.h)
#include "tests/host/check.h"

namespace {

using Bytes = std::vector<uint8_t>;

Bytes bytes(const std::string& s) { return Bytes(s.begin(), s.end()); }

// A work calendar's shape: every entry with a long attendee list (repeats a lot: packs well), over
// 32 KB so the decompressor's window wraps around several times.
std::string sampleCalendar(int events) {
  std::string s = "BEGIN:VCALENDAR\r\nVERSION:2.0\r\n";
  for (int i = 0; i < events; i++) {
    s += "BEGIN:VEVENT\r\nDTSTART:2026" + std::to_string(1000 + i % 300) + "T090000Z\r\nSUMMARY:Meeting " +
         std::to_string(i) + "\r\n";
    for (int a = 0; a < 6; a++)
      s += "ATTENDEE;CN=Person " + std::to_string(a) + ";ROLE=REQ-PARTICIPANT:mailto:person" +
           std::to_string((i * 7 + a) % 97) + "@example.com\r\n";
    s += "END:VEVENT\r\n";
  }
  return s + "END:VCALENDAR\r\n";
}

// gzip (RFC 1952) made with miniz's compressor: header (optionally with a name and an extra field),
// raw deflate, CRC-32 and the size.
Bytes gzip(const std::string& text, bool withName = false, bool withExtra = false) {
  Bytes out = {0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 2, 3};
  if (withExtra) {
    out[3] |= 4;
    out.insert(out.end(), {3, 0, 'a', 'b', 'c'});
  }
  if (withName) {
    out[3] |= 8;
    for (char c : std::string("basic.ics")) out.push_back((uint8_t)c);
    out.push_back(0);
  }
  size_t packedLen = 0;
  void* packed = tdefl_compress_mem_to_heap(text.data(), text.size(), &packedLen, TDEFL_DEFAULT_MAX_PROBES);
  out.insert(out.end(), (uint8_t*)packed, (uint8_t*)packed + packedLen);
  mz_free(packed);
  uint32_t crc = (uint32_t)mz_crc32(MZ_CRC32_INIT, (const uint8_t*)text.data(), text.size());
  uint32_t size = (uint32_t)text.size();
  for (int i = 0; i < 4; i++) out.push_back((uint8_t)(crc >> (8 * i)));
  for (int i = 0; i < 4; i++) out.push_back((uint8_t)(size >> (8 * i)));
  return out;
}

// Chunked framing like Google's: sizes in hex with leading zeros, the first chunks one byte each.
Bytes chunked(const Bytes& body) {
  Bytes out;
  size_t i = 0;
  auto chunk = [&](size_t n) {
    char head[16];
    std::snprintf(head, sizeof(head), "%08zx\r\n", n);
    out.insert(out.end(), head, head + std::strlen(head));
    out.insert(out.end(), body.begin() + i, body.begin() + i + n);
    out.push_back('\r');
    out.push_back('\n');
    i += n;
  };
  for (int k = 0; k < 3 && i < body.size(); k++) chunk(1);
  while (i < body.size()) chunk(body.size() - i < 8000 ? body.size() - i : 8000);
  const char* end = "0\r\n\r\n";
  out.insert(out.end(), end, end + 5);
  return out;
}

// Feeds `in` in pieces of `step` bytes (0: all at once); the output collected.
template <typename Stage>
bool feed(Stage& stage, const Bytes& in, size_t step, std::string& out) {
  for (size_t i = 0; i < in.size();) {
    size_t n = step ? (in.size() - i < step ? in.size() - i : step) : in.size();
    if (!stage.push(in.data() + i, n, [&](const uint8_t* p, size_t len) { out.append((const char*)p, len); }))
      return false;
    i += n;
  }
  return true;
}

void test_dechunk() {
  for (size_t step : {0, 1, 3}) {
    httpbody::Dechunker d;
    std::string out;
    CHECK(feed(d, bytes("00000001\r\nA\r\n5;name=x\r\nBCDEF\r\n0\r\n\r\n"), step, out));
    CHECK_STR(out.c_str(), "ABCDEF");
    CHECK(d.done());
  }
  httpbody::Dechunker trailer;  // a trailer header after the last chunk
  std::string out;
  CHECK(feed(trailer, bytes("3\r\nabc\r\n0\r\nX-Note: y\r\n\r\n"), 1, out) && trailer.done());
  CHECK_STR(out.c_str(), "abc");
  httpbody::Dechunker cut;  // the connection ended inside a chunk: not done
  out.clear();
  CHECK(feed(cut, bytes("8\r\nabc"), 0, out) && !cut.done());
  httpbody::Dechunker bad;
  out.clear();
  CHECK(!feed(bad, bytes("zz\r\nabc\r\n"), 0, out));
}

void test_gunzip_pieces() {
  const std::string text = sampleCalendar(400);
  CHECK(text.size() > 4 * 32768);
  const Bytes gz = gzip(text);
  CHECK(gz.size() * 5 < text.size());  // packs at least 5 x
  for (size_t step : {0, 1, 7, 1460, 4096}) {
    httpbody::Gunzip g;
    CHECK(g.begin());
    std::string out;
    CHECK(feed(g, gz, step, out));
    CHECK(g.done());
    CHECK(out == text);
    CHECK_EQ(g.packedBytes(), gz.size());
  }
}

void test_gunzip_header_fields() {
  const std::string text = "BEGIN:VCALENDAR\r\nEND:VCALENDAR\r\n";
  httpbody::Gunzip g;
  CHECK(g.begin());
  std::string out;
  CHECK(feed(g, gzip(text, true, true), 1, out) && g.done());
  CHECK(out == text);
}

void test_gunzip_damaged() {
  const std::string text = sampleCalendar(50);
  Bytes gz = gzip(text);
  {  // cut off: everything so far is fine, but it isn't done (the download counts as failed)
    httpbody::Gunzip g;
    CHECK(g.begin());
    std::string out;
    Bytes half(gz.begin(), gz.begin() + gz.size() / 2);
    CHECK(feed(g, half, 512, out) && !g.done());
  }
  {  // the chip's ROM decompressor (an older miniz) keeps up to 4 bytes after the data in its bit
     // buffer, so the 8-byte trailer can arrive incomplete: the data is whole when the decompressor
     // says so (device test of build 83: every calendar failed at its very end)
    for (size_t cut : {1, 4, 8}) {
      Bytes shortTrailer(gz.begin(), gz.end() - cut);
      httpbody::Gunzip g;
      CHECK(g.begin());
      std::string out;
      CHECK(feed(g, shortTrailer, 777, out));
      CHECK(g.done());
      CHECK(out == text);
    }
  }
  {  // a few packed bytes that unpack to more than the 32 KB window, and nothing after them: the
     // output the decompressor still holds must come out without more input
    const std::string same = std::string(300000, 'A') + "END";
    Bytes tight = gzip(same);
    tight.resize(tight.size() - 8);  // no trailer at all
    httpbody::Gunzip g;
    CHECK(g.begin());
    std::string out;
    CHECK(feed(g, tight, 0, out) && g.done());
    CHECK(out == same);
  }
  {  // not gzip at all
    httpbody::Gunzip g;
    CHECK(g.begin());
    std::string out;
    CHECK(!feed(g, bytes("BEGIN:VCALENDAR\r\n"), 0, out));
    CHECK_STR(g.failure(), "not gzip");
  }
  {  // the size in the trailer doesn't match
    Bytes wrong = gz;
    wrong[wrong.size() - 4] ^= 0x01;
    httpbody::Gunzip g;
    CHECK(g.begin());
    std::string out;
    CHECK(!feed(g, wrong, 0, out) || !g.done());
  }
}

void test_chunked_gzip() {
  // Both layers, as Google answers over HTTP/1.1: chunks first, then gzip.
  const std::string text = sampleCalendar(300);
  const Bytes wire = chunked(gzip(text));
  for (size_t step : {1, 256, 1024}) {
    httpbody::Dechunker d;
    httpbody::Gunzip g;
    CHECK(g.begin());
    std::string out;
    bool ok = true;
    for (size_t i = 0; i < wire.size() && ok; i += step) {
      size_t n = wire.size() - i < step ? wire.size() - i : step;
      ok = d.push(wire.data() + i, n, [&](const uint8_t* p, size_t len) {
        if (!g.push(p, len, [&](const uint8_t* q, size_t m) { out.append((const char*)q, m); })) ok = false;
      });
    }
    CHECK(ok && d.done() && g.done());
    CHECK(out == text);
  }
}

// The whole path a download takes on the device (Calendar.cpp): chunks off if chunked, then unpacked
// if packed, each byte exactly once (build 84 fed chunked data to the unpacker twice, once still
// framed: every packed calendar failed after a few KB, device test 2026-11-25).
void test_body_decoder() {
  const std::string text = sampleCalendar(300);
  const Bytes packed = gzip(text);
  const struct {
    bool chunked, gz;
    Bytes wire;
  } cases[] = {{true, true, chunked(packed)}, {false, true, packed}, {true, false, chunked(bytes(text))},
               {false, false, bytes(text)}};
  for (const auto& c : cases)
    for (size_t step : {1, 700, 1024}) {
      httpbody::Gunzip g;
      if (c.gz) CHECK(g.begin());
      httpbody::BodyDecoder body;
      body.begin(c.chunked, c.gz ? &g : nullptr);
      std::string out;
      bool ok = true;
      for (size_t i = 0; i < c.wire.size() && ok; i += step) {
        size_t n = c.wire.size() - i < step ? c.wire.size() - i : step;
        ok = body.push(c.wire.data() + i, n, [&](const uint8_t* p, size_t m) { out.append((const char*)p, m); });
      }
      CHECK(ok && body.whole());
      CHECK(out == text);
      CHECK(body.ended() == c.chunked);
    }
  // Damaged packed data inside chunks: the unpacker's reason, not a chunk error.
  Bytes bad = packed;
  for (size_t i = 200; i < 260; i++) bad[i] ^= 0x5A;
  httpbody::Gunzip g;
  CHECK(g.begin());
  httpbody::BodyDecoder body;
  body.begin(true, &g);
  std::string out;
  const Bytes wire = chunked(bad);
  bool ok = body.push(wire.data(), wire.size(), [&](const uint8_t* p, size_t m) { out.append((const char*)p, m); });
  CHECK(!ok && !body.whole());
  CHECK(body.failure() && std::strcmp(body.failure(), "chunk error"));
}

}  // namespace

int main() {
  test_dechunk();
  test_gunzip_pieces();
  test_gunzip_header_fields();
  test_gunzip_damaged();
  test_chunked_gzip();
  test_body_decoder();
  return finish("http body");
}
