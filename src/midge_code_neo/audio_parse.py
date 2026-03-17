import argparse
import pathlib
import wave

from midge_badge_framework.files import AudioMetaDataCSVfmt

PCM_SAMPLE_WIDTH_BYTES = 2


def main():
    parser = argparse.ArgumentParser(description="Parse raw signed 16-bit PCM data into a WAV file")
    parser.add_argument("input_file", type=str, help="Input raw PCM file to parse")
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        default=None,
        help="Output WAV file (default: <input>.wav)",
    )

    args = parser.parse_args()

    input_path = pathlib.Path(args.input_file)
    if not input_path.exists():
        msg = f"Input file does not exist: {input_path}"
        raise FileNotFoundError(msg)

    metadata_path = input_path.with_suffix(".m")
    if not metadata_path.exists():
        metadata_path_upper = input_path.with_suffix(".M")
        if metadata_path_upper.exists():
            metadata_path = metadata_path_upper
        else:
            msg = f"Metadata file does not exist: {metadata_path}"
            raise FileNotFoundError(msg)

    output_path = pathlib.Path(args.output) if args.output else input_path.with_suffix(".wav")

    with AudioMetaDataCSVfmt(str(metadata_path)) as metadata:
        sample_rate, channels = metadata.get_sample_info()

    if sample_rate <= 0:
        msg = f"Invalid sample rate in metadata: {sample_rate}"
        raise ValueError(msg)
    if channels <= 0:
        msg = f"Invalid channel count in metadata: {channels}"
        raise ValueError(msg)

    frame_size = PCM_SAMPLE_WIDTH_BYTES * channels

    with open(input_path, "rb") as raw_file:
        raw_audio = raw_file.read()

    if len(raw_audio) % frame_size != 0:
        msg = f"Raw PCM size ({len(raw_audio)} bytes) is not a multiple of frame size ({frame_size} bytes)"
        raise ValueError(msg)

    with wave.open(str(output_path), "wb") as wav_file:
        wav_file.setnchannels(channels)
        wav_file.setsampwidth(PCM_SAMPLE_WIDTH_BYTES)
        wav_file.setframerate(sample_rate)
        wav_file.writeframes(raw_audio)
