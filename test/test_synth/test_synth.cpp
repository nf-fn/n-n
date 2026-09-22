// 波形合成のテスト
//
// 「ブザーに聞こえない」ための性質を固定する。耳で確かめるしかない部分も
// あるが、プチッと鳴る原因 (端が 0 でない、位相が飛ぶ) は数値で検出できる。

#include <unity.h>

#include <cmath>
#include <cstdlib>
#include <vector>

#include "Voice.h"
#include "VoiceSynth.h"

using pet::VoiceCue;
using pet::VoiceNote;
using pet::VoiceSynth;

namespace {

VoiceCue cueOf(std::vector<VoiceNote> notes) {
  VoiceCue cue;
  cue.count = static_cast<int>(notes.size());
  for (int i = 0; i < cue.count; ++i) {
    cue.notes[i] = notes[i];
  }
  return cue;
}

std::vector<int16_t> render(const VoiceCue &cue) {
  VoiceSynth synth;
  std::vector<int16_t> buf(VoiceSynth::kMaxSamples);
  const size_t n = synth.render(cue, buf.data(), buf.size());
  buf.resize(n);
  return buf;
}

}  // namespace

void setUp() {}
void tearDown() {}

// 長さが音符の合計と一致すること。
void test_length_matches_the_notes() {
  const auto pcm = render(cueOf({{800, 100}, {1200, 200}}));

  const size_t expected = 300u * VoiceSynth::kSampleRate / 1000u;
  TEST_ASSERT_EQUAL_UINT32(expected, pcm.size());
}

// 音の出だしと終わりが 0 付近であること。
// いきなり振幅が立つとプチッと鳴る。ブザー感の大きな原因。
void test_starts_and_ends_near_silence() {
  const auto pcm = render(cueOf({{900, 200}}));
  TEST_ASSERT_TRUE(pcm.size() > 100);

  TEST_ASSERT_TRUE_MESSAGE(std::abs(pcm.front()) < 1500,
                           "出だしが立ち上がっていない (プチッと鳴る)");
  TEST_ASSERT_TRUE_MESSAGE(std::abs(pcm.back()) < 1500,
                           "終わりが切れている (プチッと鳴る)");
}

// 途中で振幅が最大付近まで出ていること。立ち上げただけで鳴っていないと困る。
void test_reaches_a_useful_amplitude() {
  const auto pcm = render(cueOf({{900, 200}}));

  int16_t peak = 0;
  for (int16_t v : pcm) {
    const int16_t a = static_cast<int16_t>(std::abs(v));
    if (a > peak) peak = a;
  }
  TEST_ASSERT_TRUE_MESSAGE(peak > 8000, "音が小さすぎる");
  TEST_ASSERT_TRUE_MESSAGE(peak < 32000, "振幅が上限に張り付いている");
}

// 音符の継ぎ目で位相が飛ばないこと。
//
// 絶対値で測るのは誤り。高い音ほど 1 サンプルあたりの変化は大きくなるので、
// 正常な波形でも 1 万を超える。見るべきは「継ぎ目が他より悪いか」。
// 位相が飛べば振幅の 2 倍近い段差が出るので、全体の最大と比べれば分かる。
void test_note_joins_are_no_worse_than_the_rest() {
  const auto pcm = render(cueOf({{700, 120}, {1400, 120}, {900, 120}}));
  const size_t per = 120u * VoiceSynth::kSampleRate / 1000u;

  auto deltaAt = [&](size_t i) {
    return std::abs(pcm[i] - pcm[i - 1]);
  };

  int overall = 0;
  for (size_t i = 1; i < pcm.size(); ++i) {
    const int d = deltaAt(i);
    if (d > overall) overall = d;
  }

  int atJoins = 0;
  for (size_t join : {per, per * 2}) {
    for (size_t i = join - 1; i <= join + 1 && i < pcm.size(); ++i) {
      if (i == 0) continue;
      const int d = deltaAt(i);
      if (d > atJoins) atJoins = d;
    }
  }

  TEST_ASSERT_TRUE_MESSAGE(atJoins <= overall,
                           "音符の継ぎ目で波形が飛んでいる");
}

// 無音の音符が本当に無音であること。
void test_silence_note_is_silent() {
  const auto pcm = render(cueOf({{800, 100}, {0, 100}, {800, 100}}));

  const size_t per = 100u * VoiceSynth::kSampleRate / 1000u;
  for (size_t i = per + 10; i < per * 2 - 10; ++i) {
    TEST_ASSERT_EQUAL_INT16(0, pcm[i]);
  }
}

// 矩形波ではないこと。
// 矩形波は値が 2 つの極値に集中する。正弦系なら中間の値が多く出る。
void test_waveform_is_not_a_square() {
  const auto pcm = render(cueOf({{600, 300}}));

  int16_t peak = 0;
  for (int16_t v : pcm) {
    const int16_t a = static_cast<int16_t>(std::abs(v));
    if (a > peak) peak = a;
  }

  int middle = 0;
  for (int16_t v : pcm) {
    const int a = std::abs(v);
    if (a > peak / 5 && a < peak / 2) {
      ++middle;
    }
  }

  TEST_ASSERT_TRUE_MESSAGE(middle > static_cast<int>(pcm.size()) / 8,
                           "矩形波に近い (中間の値が少なすぎる)");
}

// 容量が足りなければ打ち切ること。あふれさせない。
void test_respects_capacity() {
  VoiceSynth synth;
  int16_t small[64];
  const size_t n =
      synth.render(cueOf({{800, 500}}), small, sizeof(small) / sizeof(small[0]));

  TEST_ASSERT_EQUAL_UINT32(64, n);
}

// 空の cue は何も書かない。
void test_empty_cue_writes_nothing() {
  VoiceSynth synth;
  int16_t buf[16] = {};
  TEST_ASSERT_EQUAL_UINT32(0, synth.render(VoiceCue{}, buf, 16));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_length_matches_the_notes);
  RUN_TEST(test_starts_and_ends_near_silence);
  RUN_TEST(test_reaches_a_useful_amplitude);
  RUN_TEST(test_note_joins_are_no_worse_than_the_rest);
  RUN_TEST(test_silence_note_is_silent);
  RUN_TEST(test_waveform_is_not_a_square);
  RUN_TEST(test_respects_capacity);
  RUN_TEST(test_empty_cue_writes_nothing);
  return UNITY_END();
}
