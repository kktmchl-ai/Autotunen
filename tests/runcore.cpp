// usage: runcore in.f32 out.f32 sr key scale retuneMs amount mix [channels]
#include "../Source/AutoTuneCore.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>
int main(int argc, char** argv)
{
    if (argc < 9) { fprintf(stderr, "args\n"); return 1; }
    double sr = atof(argv[3]); int key = atoi(argv[4]), scale = atoi(argv[5]);
    float ret = (float) atof(argv[6]), amt = (float) atof(argv[7]), mix = (float) atof(argv[8]);
    int ch = argc > 9 ? atoi(argv[9]) : 1;
    FILE* f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long bytes = ftell(f); fseek(f, 0, SEEK_SET);
    long frames = bytes / 4 / ch;
    std::vector<float> data((size_t)(frames * ch)); fread(data.data(), 4, data.size(), f); fclose(f);
    // de-interleave
    std::vector<std::vector<float>> c(ch, std::vector<float>((size_t) frames));
    for (long i = 0; i < frames; ++i) for (int k = 0; k < ch; ++k) c[k][(size_t)i] = data[(size_t)(i*ch+k)];
    autotune::Core core; core.prepare(sr, ch);
    core.setKey(key); core.setScale(scale); if (getenv("MASK")) core.setCustomMask((uint16_t) strtol(getenv("MASK"), nullptr, 16)); core.setRetuneMs(ret); core.setAmount(amt); core.setMix(mix);
    core.reset();
    auto t0 = std::chrono::steady_clock::now();
    const int B = 512;
    for (long pos = 0; pos < frames; pos += B)
    {
        int nblk = (int) std::min<long>(B, frames - pos);
        float* p[2] = { c[0].data() + pos, ch > 1 ? c[1].data() + pos : nullptr };
        core.process(p, nblk);
    }
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    fprintf(stderr, "latency=%d samples, cpu=%.2f%% of realtime\n", core.latencySamples(), 100.0 * secs / (frames / sr));
    FILE* o = fopen(argv[2], "wb");
    for (long i = 0; i < frames; ++i) for (int k = 0; k < ch; ++k) fwrite(&c[k][(size_t)i], 4, 1, o);
    fclose(o);
    printf("%d\n", core.latencySamples());
}
