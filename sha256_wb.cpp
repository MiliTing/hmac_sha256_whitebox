#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>

#if !defined(__BYTE_ORDER__)
#error "Unable to determine target architecture endianess!"
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define SWAP_U32_BE(v) __builtin_bswap32((v))
#define SWAP_U64_BE(v) __builtin_bswap64((v))
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define SWAP_U32_BE(v) (v)
#define SWAP_U64_BE(v) (v)
#else
#error "Unsupported target architecture endianess!"
#endif

static const uint32_t SHA256_K[64] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
  0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
  0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
  0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
  0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
  0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
  0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
  0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
  0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

using StateType = std::array<uint32_t, 8>;
using BlockType = std::array<uint32_t, 16>;
using WType     = std::array<uint32_t, 64>;

namespace {

// BlockType constexpr helpers

template <std::size_t... I>
constexpr BlockType blocktype_xor(BlockType const A, uint32_t const B, std::index_sequence<I...>)
{
  return BlockType{A[I]^B ...};
}

constexpr BlockType blocktype_xor(BlockType const A, uint8_t const B)
{
  const uint32_t B32  = (uint32_t)B;
  const uint32_t B32x4 = B32|(B32<<8)|(B32<<16)|(B32<<24);
  return blocktype_xor(A, B32x4, std::make_index_sequence<16>());
}

constexpr uint32_t u8x4_to_u32(uint8_t const a, uint8_t const b, uint8_t const c, uint8_t const d)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return ((uint32_t)a)|(((uint32_t)b)<<8)|(((uint32_t)c)<<16)|(((uint32_t)d)<<24);
#else
  return ((uint32_t)d)|(((uint32_t)c)<<8)|(((uint32_t)b)<<16)|(((uint32_t)a)<<24);
#endif
}

template <std::size_t... I>
constexpr BlockType blocktype_from_u8(std::array<uint8_t, 64>  const Str, std::index_sequence<I...>)
{
  return BlockType{u8x4_to_u32(Str[I*4],Str[I*4+1],Str[I*4+2],Str[I*4+3])...};
}

constexpr BlockType blocktype_from_u8(std::array<uint8_t, 64>  const Str)
{
  return blocktype_from_u8(Str, std::make_index_sequence<16>());
}

// SHA256 routines
// Based on code from https://github.com/thomdixon/pysha2/blob/master/sha2/sha256.py

__attribute__((always_inline)) uint32_t rotr(uint32_t const v, int off)
{
  return (v >> off) | (v << (32-off));
}

__attribute__((always_inline)) void transform(StateType& S, const BlockType& data)
{
  WType W = {0};
#pragma unroll
  for (size_t i = 0; i < data.size(); ++i) {
    W[i] = SWAP_U32_BE(data[i]);
  }

#pragma unroll
  for (size_t i = 16; i < 64; ++i) {
      const uint32_t s0 = rotr(W[i-15], 7) ^ rotr(W[i-15], 18) ^ (W[i-15] >> 3);
      const uint32_t s1 = rotr(W[i-2], 17) ^ rotr(W[i-2], 19)  ^ (W[i-2] >> 10);
      W[i] = (W[i-16] + s0 + W[i-7] + s1);
  }

  StateType InS = S;
#pragma unroll
  for (size_t i = 0; i < 64; ++i) {
      uint32_t s0 = rotr(InS[0], 2) ^ rotr(InS[0], 13) ^ rotr(InS[0], 22);
      uint32_t maj = (InS[0] & InS[1]) ^ (InS[0] & InS[2]) ^ (InS[1] & InS[2]);
      uint32_t t2 = s0 + maj;
      uint32_t s1 = rotr(InS[4], 6) ^ rotr(InS[4], 11) ^ rotr(InS[4], 25);
      uint32_t ch = (InS[4] & InS[5]) ^ ((~InS[4]) & InS[6]);
      uint32_t t1 = InS[7] + s1 + ch + SHA256_K[i] + W[i];
      
      InS[7] = InS[6];
      InS[6] = InS[5];
      InS[5] = InS[4];
      InS[4] = (InS[3] + t1);
      InS[3] = InS[2];
      InS[2] = InS[1];
      InS[1] = InS[0];
      InS[0] = (t1 + t2);
  }

  for (size_t i = 0; i < std::tuple_size<StateType>(); ++i) {
    S[i] += InS[i];
  }
}

void __attribute__((always_inline)) sha256_wb(uint8_t* Hash, const uint8_t* Data, const uint64_t Len, const BlockType Key)
{
  StateType State = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  transform(State, Key);

  const uint64_t BlockCount = Len/sizeof(BlockType);
  BlockType const* Blocks = (BlockType const*) Data;
  for (uint64_t i = 0; i < BlockCount; ++i) {
    transform(State, Blocks[i]);
  }
  
  const uint64_t LenWB = Len+sizeof(BlockType);
  const uint64_t Rem = Len-BlockCount*sizeof(BlockType);

  union {
    BlockType B;
    struct {
      uint8_t Data[56];
      uint64_t Length;
    } S;
  } LastB;

  LastB.B = {0};
  memcpy(&LastB.S.Data[0], &Blocks[BlockCount], Rem);
  LastB.S.Data[Rem] = 0x80;
  if (Rem >= 56) {
    transform(State, LastB.B);
    LastB.B = {0};
  }
  LastB.S.Length = SWAP_U64_BE(LenWB << 3);
  transform(State, LastB.B);

  for (uint32_t& V: State) {
    V = SWAP_U32_BE(V);
  }
  memcpy(Hash, &State[0], sizeof(StateType));
}

} // namespace

void sha256_hmac_wb(uint8_t* Hash, const uint8_t* Data, const uint64_t Len)
{
  static constexpr BlockType Key = blocktype_from_u8({
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P'});
  static constexpr BlockType Key0 = blocktype_xor(Key, 0x36);
  static constexpr BlockType Key1 = blocktype_xor(Key, 0x5C);
  uint8_t Hash0[sizeof(StateType)];
  sha256_wb(Hash0, Data, Len, Key0);
  sha256_wb(Hash,  Hash0, sizeof(Hash0), Key1);
}

int main(int argc, char** argv)
{
  uint8_t Hash[sizeof(StateType)];
  sha256_hmac_wb(Hash, (const uint8_t*) argv[1], strlen(argv[1]));
  for (size_t i = 0; i < sizeof(StateType); ++i) {
    printf("%02x", Hash[i]);
  }
  printf("\n");
  return 0;
}
