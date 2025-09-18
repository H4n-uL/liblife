# LIFE - LIFE Isn't FrAD Encoder

GNU LIFE Audio Processing System - A Reference Implementation of the
Fourier Analogue-in-Digital Audio Codec Specification

Released under the GNU Affero General Public License version 3 or later

Copyright (C) 2024-2025 The LIFE Development Consortium
Copyright (C) 2024-2025 Free Software Foundation, Inc.

## TABLE OF CONTENTS

1. [LEGAL NOTICE AND COPYRIGHT](#legal-notice-and-copyright)
2. [ABSTRACT AND EXECUTIVE SUMMARY](#abstract-and-executive-summary)
3. [TECHNICAL OVERVIEW](#technical-overview)
4. [SOFTWARE DEPENDENCIES](#software-dependencies)
5. [SUPPORTED FORMATS AND SPECIFICATIONS](#supported-formats-and-specifications)
6. [INSTALLATION PROCEDURES](#installation-procedures)
7. [OPERATIONAL PROCEDURES](#operational-procedures)
8. [ADVANCED USAGE SCENARIOS](#advanced-usage-scenarios)
9. [TECHNICAL SUPPORT AND RESOURCES](#technical-support-and-resources)
10. [THEORETICAL FOUNDATIONS](#theoretical-foundations)
11. [ACKNOWLEDGMENTS](#acknowledgments)
12. [REFERENCES AND BIBLIOGRAPHY](#references-and-bibliography)
13. [APPENDICES](#appendices)

## LEGAL NOTICE AND COPYRIGHT

This file is part of GNU LIFE.

GNU LIFE is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

GNU LIFE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public
License along with this program. If not, see
<https://www.gnu.org/licenses/>.

Additional permissions under GNU AGPL version 3 section 7:
If you modify this Program, or any covered work, by linking or
combining it with proprietary audio processing libraries (excluding
system libraries), the resulting work remains covered by the GNU AGPL
version 3, ensuring that all users who interact with the modified
program through a network receive the corresponding source code.

### Export Control Notice

This software contains cryptographic error correction algorithms that
may be subject to export restrictions in certain jurisdictions. Users
are responsible for compliance with all applicable export laws and
regulations.

## ABSTRACT AND EXECUTIVE SUMMARY

The GNU LIFE (LIFE Isn't FrAD Encoder) software suite constitutes a
comprehensive, standards-compliant reference implementation of the
Fourier Analogue-in-Digital (FrAD) audio codec specification, providing
extensive support for multichannel pulse-code modulated audio signal
compression, decompression, error correction, and digital signal processing
operations within a unified computational framework.

This implementation, written in strict conformance with the ISO/IEC
9899:2018 (C18) programming language standard without reliance on
compiler-specific extensions, demonstrates the practical viability of
spectral audio coding methodologies while maintaining full compliance
with the IEEE Std 1003.1-2017 (POSIX.1-2017) specification for maximum
portability across diverse computing platforms.

### Principal Objectives

1. **Standards Compliance**: Rigorous adherence to international
   standards for programming languages, system interfaces, and audio
   processing methodologies.

2. **Software Freedom**: Promotion of user freedom through copyleft
   licensing, ensuring that all derivatives remain free software.

3. **Technical Excellence**: Implementation of state-of-the-art digital
   signal processing algorithms with mathematical rigor and computational
   efficiency.

4. **Interoperability**: Seamless integration with existing audio
   processing workflows and multimedia frameworks.

5. **Accessibility**: Comprehensive documentation and user interfaces
   designed for both technical and non-technical users.

## TECHNICAL OVERVIEW

The GNU LIFE audio processing system implements a sophisticated pipeline
for digital audio signal manipulation, employing advanced mathematical
transformations and psychoacoustic modeling to achieve efficient
compression while maintaining perceptual transparency.

### Architectural Design Principles

The system architecture follows the Unix philosophy of modular design,
with each component performing a single, well-defined function. This
approach facilitates:

- **Separation of Concerns**: Distinct modules for encoding, decoding,
  error correction, and metadata processing
- **Composability**: Components can be combined in various configurations
  to achieve complex processing workflows
- **Testability**: Individual modules can be tested in isolation
- **Maintainability**: Localized changes with minimal system-wide impact

### Core Subsystems

#### 1. Signal Analysis and Transformation Engine

The encoding subsystem implements signal analysis utilizing:

- **Discrete Cosine Transform (DCT)**: Pure DCT transformation
  for frequency domain conversion
- **Configurable Frame Sizing**: User-defined frame sizes
  (default: 2048 samples)
- **Overlap Processing**: Optional overlapping frames with
  configurable overlap ratio

#### 2. Quantization and Entropy Coding Module

Implements perceptually-weighted quantization with:

- **Scale Factor Bands**: Frequency-dependent quantization resolution
- **Quantization**: Configurable quantization levels based on bit depth
- **Frame-based Storage**: Simple frame-by-frame organization

#### 3. Error Correction and Resilience Framework

Systematic Reed-Solomon forward error correction over Galois Field
GF(2^8) providing:

- **Configurable Redundancy**: Data/parity ratios from 223:32 to 1:254
- **Burst Error Handling**: Interleaving for improved burst tolerance
- **Syndrome Calculation**: Berlekamp-Massey algorithm implementation
- **Error Location**: Chien search with Forney algorithm for magnitude

#### 4. Metadata Container System

Extensible metadata framework supporting:

- **JSON Metadata**: RFC 8259 compliant structured metadata
- **Vorbis Comments**: Simplified key-value pair format
- **Binary Attachments**: Base64-encoded album artwork and lyrics
- **Charset Support**: Full UTF-8 Unicode support without restrictions

#### 5. Audio Output Abstraction Layer

Platform-agnostic audio playback utilizing:

- **ALSA**: Advanced Linux Sound Architecture (GNU/Linux)
- **PulseAudio**: Network-transparent sound server (GNU/Linux)
- **CoreAudio**: Core Audio framework (macOS/Darwin)
- **WASAPI**: Windows Audio Session API (Windows NT)

## SOFTWARE DEPENDENCIES

### Required System Components

The following software components must be present for successful
compilation and operation:

- **C Compiler Toolchain**:
  - GNU Compiler Collection (GCC) 4.7 or later
  - LLVM/Clang 3.1 or later
  - Intel C Compiler 13.0 or later
  - Any ISO/IEC 9899:2011 compliant compiler

- **Build System**:
  - GNU Make 3.81 or later
  - pkg-config 0.29 or later
  - GNU Autotools (for building from repository)
    - Autoconf 2.69 or later
    - Automake 1.14 or later
    - Libtool 2.4 or later

- **System Libraries**:
  - GNU C Library (glibc) 2.17 or later
  - POSIX Threads (pthreads) implementation
  - Mathematical library (libm)

### External Library Dependencies

- **pocketfft** (>= 1.0.0):
  Fast Fourier Transform implementation
  License: 3-clause BSD
  Purpose: Spectral transformations

- **zlib** (>= 1.2.11):
  Compression library
  License: zlib License
  Purpose: Metadata compression

- **libportaudio** (>= 19.7.0):
  Portable audio I/O library
  License: MIT
  Purpose: Cross-platform audio playback

### Optional Development Tools

- **Valgrind** (>= 3.15.0): Memory error detection
- **GNU Debugger** (>= 8.0): Source-level debugging
- **Doxygen** (>= 1.8.0): API documentation generation
- **GNU Texinfo** (>= 6.0): Info manual generation
- **Graphviz** (>= 2.40): Call graph visualization

## SUPPORTED FORMATS AND SPECIFICATIONS

### Audio Sample Format Support Matrix

The GNU LIFE system provides comprehensive support for various pulse-code
modulation sample formats with automatic format conversion capabilities:

#### Floating-Point Formats (IEEE 754-2019)

| Format  | Precision | Exponent | Mantissa | Range              | Epsilon    |
|---------|-----------|----------|----------|--------------------|------------|
| f16le   | Half      | 5 bits   | 10 bits  | ±65504             | 2^-10      |
| f16be   | Half      | 5 bits   | 10 bits  | ±65504             | 2^-10      |
| f32le   | Single    | 8 bits   | 23 bits  | ±3.4×10^38         | 2^-23      |
| f32be   | Single    | 8 bits   | 23 bits  | ±3.4×10^38         | 2^-23      |
| f64le   | Double    | 11 bits  | 52 bits  | ±1.7×10^308        | 2^-52      |
| f64be   | Double    | 11 bits  | 52 bits  | ±1.7×10^308        | 2^-52      |

#### Integer Formats (Two's Complement)

| Format  | Bits | Signed | Range                      | Quantization |
|---------|------|--------|----------------------------|--------------|
| u8      | 8    | No     | 0 to 255                   | 256 levels   |
| s8      | 8    | Yes    | -128 to 127                | 256 levels   |
| u16le/be| 16   | No     | 0 to 65535                 | 65536 levels |
| s16le/be| 16   | Yes    | -32768 to 32767            | 65536 levels |
| u24le/be| 24   | No     | 0 to 16777215              | 16.7M levels |
| s24le/be| 24   | Yes    | -8388608 to 8388607        | 16.7M levels |
| u32le/be| 32   | No     | 0 to 4294967295            | 4.3B levels  |
| s32le/be| 32   | Yes    | -2147483648 to 2147483647  | 4.3B levels  |

Note: Suffix notation indicates byte ordering where 'le' denotes
little-endian and 'be' denotes big-endian (network byte order).

### FrAD Codec Profiles

The implementation supports multiple compression profiles optimized
for different use cases:

#### Profile 0: DCT Archival Mode
- Classification: Lossless transform coding
- Bit depths: 12, 16, 24, 32, 48, 64 bits
- Sample rates: 1 Hz to 4.294967295 GHz
- Channels: 1 to 256
- Compression ratio: 1.5:1 to 3:1 (typical)

#### Profile 1: Psychoacoustic Lossy Compression
- Classification: Perceptual coding
- Bit depths: 8, 12, 16, 24, 32, 48, 64 bits
- Sample rates: 8000, 11025, 16000, 22050, 32000, 44100, 48000, 88200, 96000 Hz
- Channels: 1 to 8
- Quality levels: 0 (transparent) to 20 (low quality)

#### Profile 4: PCM Archival Mode (Default)
- Classification: Uncompressed storage
- Bit depths: 12, 16, 24, 32, 48, 64 bits
- Sample rates: 1 Hz to 4.294967295 GHz
- Channels: 1 to 256
- Compression ratio: 1:1 (plus header overhead)

## INSTALLATION PROCEDURES

### Installation

Clone the repository and build:

```bash
# Clone repository
git clone https://github.com/H4n-uL/liblife.git
cd liblife

# Build and install
make
sudo make install
```

That's it. The binary will be installed to `/usr/local/bin/life`.

## OPERATIONAL PROCEDURES

### Command-Line Interface Syntax

The GNU LIFE command-line interface follows the GNU standard for
command-line syntax as specified in the GNU Coding Standards:

```
life [GLOBAL-OPTIONS]... COMMAND [COMMAND-OPTIONS]... [--] [ARGUMENTS]...
```

Where:
- `GLOBAL-OPTIONS` affect overall program behavior
- `COMMAND` specifies the operation to perform
- `COMMAND-OPTIONS` modify command-specific behavior
- `--` explicitly terminates option processing
- `ARGUMENTS` are command-specific operands

### Encoding Operations

#### Basic PCM to FrAD Conversion

Transform uncompressed PCM audio to FrAD format:

```bash
# Minimal encoding with required parameters
life encode input.pcm --sample-rate 44100 --channels 2 --bits 16

# Explicit output specification
life encode input.pcm --sr 44100 --ch 2 --bits 16 --output output.frad

# High-quality archival encoding
life encode master.pcm \
    --sample-rate 96000 \
    --channels 2 \
    --bits 24 \
    --profile 0 \
    --ecc 192 32 \
    --output master_archive.frad

# Lossy encoding with metadata
life encode podcast.pcm \
    --sr 44100 \
    --ch 1 \
    --bits 16 \
    --profile 1 \
    --loss-level 8 \
    --meta TITLE "Episode 42" \
    --meta ARTIST "John Doe" \
    --meta DATE "2025-01-01" \
    --output podcast_ep42.frad
```

#### Batch Encoding Operations

Process multiple files efficiently:

```bash
# Encode all PCM files in directory
for file in *.pcm; do
    life encode "$file" \
        --sr 48000 --ch 2 --bits 24 \
        --output "${file%.pcm}.frad"
done

# Parallel encoding using GNU Parallel
parallel -j 4 life encode {} \
    --sr 44100 --ch 2 --bits 16 \
    --output {.}.frad ::: *.pcm

# Encode with consistent settings from configuration
cat > encode.conf << EOF
--sample-rate=48000
--channels=2
--bits=24
--profile=4
--ecc 96 24
EOF

life encode input.pcm @encode.conf --output output.frad
```

### Decoding Operations

#### FrAD to PCM Conversion

Reconstruct PCM audio from FrAD compressed files:

```bash
# Basic decoding to default format (f64be)
life decode input.frad --output output.pcm

# Decode to specific format with error correction
life decode damaged.frad \
    --format s16le \
    --enable-ecc \
    --output recovered.pcm

# Decode for pipe processing
life decode input.frad --format f32le | \
    sox -t raw -r 44100 -c 2 -e float -b 32 - output.wav

# Extract embedded metadata during decode
life decode album.frad \
    --output audio.pcm \
    --log 1  # Show metadata in log output
```

### Playback Operations

Direct audio playback with real-time processing:

```bash
# Simple playback
life play music.frad

# Playback with speed adjustment
life play audiobook.frad --speed 1.25

# Playback with pitch shifting
life play song.frad --keys -2  # Down 2 semitones

# Playback with error correction
life play corrupted.frad --enable-ecc

# Stream playback from stdin
cat stream.frad | life play -
```

### Error Correction and Repair

Recover corrupted FrAD files using Reed-Solomon FEC:

```bash
# Repair with default parameters
life repair damaged.frad --output repaired.frad

# Aggressive repair with maximum ECC
life repair corrupt.frad \
    --ecc 223 32 \
    --output fixed.frad

# Batch repair operation
for file in damaged/*.frad; do
    echo "Repairing $file..."
    life repair "$file" \
        --ecc 192 32 \
        --output "repaired/$(basename $file)"
done
```

### Metadata Management

Manipulate embedded metadata in FrAD files:

```bash
# Add metadata to existing file
life meta add song.frad \
    --meta TITLE "Symphony No. 9" \
    --meta COMPOSER "Beethoven" \
    --meta YEAR "1824"

# Import metadata from JSON
cat > metadata.json << EOF
[
    {"key": "TITLE", "type": "string", "value": "Test Track"},
    {"key": "ARTIST", "type": "string", "value": "Test Artist"},
    {"key": "COVER", "type": "base64", "value": "...base64 data..."}
]
EOF
life meta add audio.frad --jsonmeta metadata.json

# Remove specific metadata
life meta remove audio.frad --meta COMMENT

# Extract all metadata
life meta parse audio.frad --output metadata.json

# Replace all metadata
life meta overwrite audio.frad \
    --vorbis-meta new_tags.txt \
    --image cover.jpg
```

## ADVANCED USAGE SCENARIOS

### Pipeline Processing

GNU LIFE integrates seamlessly with Unix pipelines:

```bash
# Real-time encoding from microphone
arecord -f cd -t raw | \
    life encode - --sr 44100 --ch 2 --bits 16 > recording.frad

# Network streaming
nc -l 9999 | life decode - --ecc | aplay -f cd
```

### Scripting and Automation

```bash
#!/bin/bash
# Automated podcast processing script

set -euo pipefail

INPUT_DIR="raw_recordings"
OUTPUT_DIR="processed"
PROFILE="1"
QUALITY="5"

process_episode() {
    local input="$1"
    local basename=$(basename "$input" .wav)

    echo "Processing $basename..."

    # Convert to raw PCM
    sox "$input" -t raw -e float -b 32 - | \

    # Encode with metadata
    life encode - \
        --sr $(soxi -r "$input") \
        --ch $(soxi -c "$input") \
        --format f32le \
        --profile $PROFILE \
        --loss-level $QUALITY \
        --meta TITLE "$basename" \
        --meta DATE "$(date -I)" \
        --output "$OUTPUT_DIR/${basename}.frad"

    echo "Completed $basename"
}

# Process all episodes
export -f process_episode
find "$INPUT_DIR" -name "*.wav" -print0 | \
    xargs -0 -n 1 -P 4 bash -c 'process_episode "$@"' _
```

## TECHNICAL SUPPORT AND RESOURCES

### Bug Reporting Guidelines

Report bugs via the GitHub issue tracking system:
https://github.com/H4n-uL/liblife/issues

## THEORETICAL FOUNDATIONS

### Mathematical Background

The FrAD codec employs several mathematical transformations:

#### Discrete Cosine Transform (DCT)

The implementation uses standard DCT-II:

```
X[k] = Σ(n=0 to N-1) x[n] × cos[π/N × (n + 1/2) × k]
```

Where:
- X[k] = transform coefficient
- x[n] = input sample
- N = frame size

#### Reed-Solomon Error Correction

Based on polynomial arithmetic in Galois Field GF(2^8):

Generator polynomial:
```
g(x) = Π(i=0 to 2t-1) (x - α^i)
```

Where:
- α = primitive element of GF(2^8)
- t = error correction capability

### Psychoacoustic Principles

The perceptual model implements:

1. **Absolute Threshold of Hearing**
   ```
   T_q(f) = 3.64(f/1000)^-0.8 - 6.5e^(-0.6(f/1000-3.3)^2) + 10^-3(f/1000)^4
   ```

2. **Simultaneous Masking**
   - Spreading function based on critical bands
   - Tonality estimation using spectral flatness

3. **Temporal Masking**
   - Pre-masking: 5 ms
   - Post-masking: 100 ms exponential decay

## ACKNOWLEDGMENTS

The GNU LIFE development team expresses gratitude to:

### Institutional Support

- **Free Software Foundation**: For promoting software freedom and
  providing infrastructure for GNU projects
- **Software Freedom Conservancy**: For legal and administrative support
- **Archive.org**: For hosting historical releases and documentation

### Technical Contributors

- **PocketFFT Developers**: Martin Reinecke and contributors for the
  efficient FFT implementation
- **PortAudio Community**: Ross Bencina, Phil Burk, and contributors
  for cross-platform audio abstraction
- **zlib Authors**: Jean-loup Gailly and Mark Adler for compression
  algorithms

### Academic References

- **James W. Cooley and John W. Tukey**: For the Fast Fourier Transform
  algorithm (1965)
- **Irving S. Reed and Gustave Solomon**: For error correction codes
  (1960)
- **K. Brandenburg and G. Stoll**: For psychoacoustic modeling
  principles (1994)

### Community Contributors

Special recognition to early adopters and beta testers who provided
invaluable feedback during the development process. Their patience
with early versions and detailed bug reports significantly improved
the quality and stability of the final release.

## REFERENCES AND BIBLIOGRAPHY

### Standards and Specifications

1. **ISO/IEC 9899:2018**
   Information technology — Programming languages — C
   International Organization for Standardization, 2018

2. **IEEE Std 1003.1-2017**
   Standard for Information Technology—POSIX Base Specifications
   The Open Group Base Specifications Issue 7, 2018 Edition

3. **RFC 8259**
   The JavaScript Object Notation (JSON) Data Interchange Format
   T. Bray, Ed., December 2017

4. **RFC 4648**
   The Base16, Base32, and Base64 Data Encodings
   S. Josefsson, October 2006

5. **ITU-R BS.1770-4**
   Algorithms to measure audio programme loudness and true-peak level
   International Telecommunication Union, October 2015

### Books and Publications

6. **Oppenheim, A.V. and Schafer, R.W.**
   Discrete-Time Signal Processing, 3rd Edition
   Pearson, 2009

7. **Proakis, J.G. and Manolakis, D.G.**
   Digital Signal Processing: Principles, Algorithms, and Applications
   Pearson, 2006

8. **Sayood, K.**
   Introduction to Data Compression, 5th Edition
   Morgan Kaufmann, 2017

9. **Brandenburg, K.**
   "MP3 and AAC Explained"
   AES 17th International Conference, 1999

10. **GNU Project**
    GNU Coding Standards
    Free Software Foundation, 2020

### Online Resources

11. **GNU Operating System**
    https://www.gnu.org/

12. **Free Software Foundation**
    https://www.fsf.org/

13. **Open Source Initiative**
    https://opensource.org/

14. **Audio Engineering Society**
    https://www.aes.org/

## APPENDICES

### Appendix A: Glossary of Terms

**DCT**: Discrete Cosine Transform, a mathematical transformation used
to convert time-domain signals to frequency domain

**ECC**: Error Correcting Code, redundant data added to enable recovery
from transmission or storage errors

**FrAD**: Fourier Analogue-in-Digital, the proprietary audio codec
format implemented by this software

**DCT**: Discrete Cosine Transform, used for frequency domain conversion

**PCM**: Pulse Code Modulation, uncompressed digital audio representation

**Psychoacoustic**: Related to the perception of sound by the human
auditory system

## COPYRIGHT AND LICENSING

Copyright (C) 2024-2025 The LIFE Development Consortium
Copyright (C) 2024-2025 Free Software Foundation, Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.

### Trademark Notice

"GNU" is a registered trademark of the Free Software Foundation.
"LIFE" and "FrAD" are trademarks of the LIFE Development Consortium.
All other trademarks are the property of their respective owners.

### Patent Notice

The LIFE Development Consortium believes that software patents are
harmful to innovation and pledges not to pursue patent protection
for any algorithms implemented in this software. Users are encouraged
to verify that their use complies with all applicable patent laws
in their jurisdiction.

## DOCUMENT METADATA

Document: GNU LIFE Audio Processing System README
Last Modified: 2025-09-18
Authors: The LIFE Development Consortium
License: GNU Free Documentation License v1.3
Format: Markdown (CommonMark specification)

---
End of Document