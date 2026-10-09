#include "asr/SpeechCompaction.h"

#include <algorithm>
#include <cmath>

namespace vt {

std::vector<float> compactSpeech(const std::vector<float>& samples,
                                 std::vector<SpeechSpan> spans, int edgePad,
                                 int maxPause) {
    const int n = static_cast<int>(samples.size());
    for (SpeechSpan& s : spans) {
        s.start = std::clamp(s.start, 0, n);
        s.end = std::clamp(s.end, 0, n);
    }
    spans.erase(std::remove_if(spans.begin(), spans.end(),
                               [](const SpeechSpan& s) { return s.end <= s.start; }),
                spans.end());
    std::sort(spans.begin(), spans.end(),
              [](const SpeechSpan& a, const SpeechSpan& b) { return a.start < b.start; });

    std::vector<float> out;
    if (spans.empty())
        return out;
    out.reserve(samples.size());
    const auto append = [&](int from, int to) {
        out.insert(out.end(), samples.begin() + from, samples.begin() + to);
    };

    append(std::max(0, spans.front().start - edgePad), spans.front().end);
    int prevEnd = spans.front().end;
    for (std::size_t i = 1; i < spans.size(); ++i) {
        if (spans[i].end <= prevEnd)
            continue; // already covered by an overlapping span
        const int start = std::max(spans[i].start, prevEnd);
        if (start - prevEnd > maxPause) {
            // Keep both edges of the pause (the speaker's room tone around the
            // words) and drop the middle.
            const int head = maxPause / 2;
            append(prevEnd, prevEnd + head);
            append(start - (maxPause - head), start);
        } else {
            append(prevEnd, start);
        }
        append(start, spans[i].end);
        prevEnd = spans[i].end;
    }
    append(prevEnd, std::min(n, prevEnd + edgePad));
    return out;
}

float levelGain(const std::vector<float>& samples, int frame, float targetRms,
                float maxGain) {
    double loudest = 0.0; // mean square of the loudest frame
    for (std::size_t i = 0; i < samples.size(); i += frame) {
        const std::size_t end = std::min(samples.size(), i + frame);
        double sum = 0.0;
        for (std::size_t j = i; j < end; ++j)
            sum += static_cast<double>(samples[j]) * samples[j];
        loudest = std::max(loudest, sum / static_cast<double>(end - i));
    }
    if (loudest <= 0.0)
        return 1.0f;
    const double gain = targetRms / std::sqrt(loudest);
    return static_cast<float>(std::clamp(gain, 1.0, static_cast<double>(maxGain)));
}

} // namespace vt
