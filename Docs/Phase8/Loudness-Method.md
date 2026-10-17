# Factory loudness review

NOT a human audition approval. `preset-loudness.csv` measures the copied raw float32 stereo 48 kHz files, including their existing 10 ms edge fades. Samples preserve factory OUTPUT. Peak is linear sample peak, RMS is whole-file RMS, crest is 20 log10(peak/RMS), overs counts individual channel samples with abs(value) >= 1. This is not LUFS or intersample true-peak analysis. No automatic retuning, limiting or loudness matching occurred.

Gain dB comes from Phase 6's regenerated pre-fade output/input RMS comparison. Low proxy is one-pole 350 Hz low-pass energy / full energy; bright proxy is the residual of a one-pole 3 kHz low-pass / full energy. These overlapping proxies are not perceptual loudness or formal spectral bands.

Review thresholds: RMS gain > +6 dB likely loud; < -6 dB likely quiet; low fraction > 0.85 bass-heavy; bright residual > 0.25 bright; sample overs potential clipping. Intentional bass presets can trigger bass flags. Thresholds only nominate human checks. Compare absolute levels and context across source types cautiously; sources differ.

Similarity compares all 435 pairs on one identical mono 48 kHz one-second composite of bass, vocal-like tones and periodic high-frequency transient bursts, aligned by the fixed 60-sample latency. Only analysis vectors are divided by their own RMS. Flag normalized difference < 0.10 (approximately correlation > 0.995). Different output gains alone are intentionally removed from this character analysis; the audition files retain those gains. One source and this metric can miss musical distinctions. Confirm candidates on real material and their intended source before suggesting changes.
