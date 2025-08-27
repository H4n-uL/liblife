# LIFE Isn't FrAD Encoder

GNU LIFE Audio Processing System
Version 1.0.0

Copyright (C) 2024-2025 The LIFE Development Team
Copyright (C) 2024-2025 Free Software Foundation, Inc.

## LEGAL NOTICE

LIFE is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as 
published by the Free Software Foundation, either version 3 of 
the License, or (at your option) any later version.

LIFE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public 
License along with this program. If not, see 
<https://www.gnu.org/licenses/>.

Additional permissions under GNU AGPL version 3 section 7:
If you modify this Program, or any covered work, by linking or
combining it with proprietary audio processing libraries, the
resulting work is still covered by the GNU AGPL version 3.

## ABSTRACT

LIFE (LIFE Isn't FrAD Encoder) constitutes a comprehensive reference
implementation of the Fourier Analogue-in-Digital (FrAD) audio codec
specification, providing extensive support for multichannel audio
compression, decompression, and digital signal processing operations.
This implementation is written in strict conformance with ISO/IEC
9899:2018 (C18) programming language standard.

## TECHNICAL OVERVIEW

The LIFE software suite implements advanced digital signal processing
algorithms for audio compression utilizing discrete cosine transforms,
psychoacoustic modeling, and Reed-Solomon forward error correction.
The system architecture employs a modular design pattern facilitating
extensibility and maintainability while ensuring compliance with
POSIX.1-2017 specifications for maximum portability across Unix-like
operating systems.

### Principal Components

1. **Encoding Subsystem**: Implements forward discrete cosine
   transformation with configurable windowing functions and
   quantization strategies

2. **Decoding Subsystem**: Performs inverse transformation and
   reconstruction of pulse code modulated audio streams

3. **Error Correction Module**: Systematic Reed-Solomon codes over
   Galois Field GF(2^8) for robust error detection and correction

4. **Metadata Container**: Extensible metadata framework supporting
   JavaScript Object Notation (RFC 8259) and pseudo-Vorbis formats

5. **Audio Playback Interface**: Platform-agnostic audio output
   utilizing ALSA, PulseAudio, CoreAudio, and WASAPI backends

## SYSTEM REQUIREMENTS

### Minimum Hardware Specifications

- CPU Architecture: x86, x86_64, ARM, ARM64, or compatible
- System Memory: 512 MiB minimum, 2 GiB recommended
- Storage Space: 50 MiB for installation
- Audio Hardware: PCM-capable sound device (optional for playback)

### Software Dependencies

#### Required Components
- ISO/IEC 9899 compliant C compiler (GCC 4.7+, Clang 3.1+, or compatible)
- POSIX.1-2017 compliant operating system
- GNU Make 3.81 or compatible build system
- pkg-config 0.29 or later

#### Library Dependencies
- pocketfft 1.0+ - Fast Fourier Transform computations
- zlib 1.2.11+ - Compression library (RFC 1950, RFC 1951, RFC 1952)
- portaudio 19.7+ - Cross-platform audio I/O library

#### Optional Components
- Valgrind 3.15+ - Memory error detector and profiler
- GNU Debugger 8.0+ - Source-level debugging support
- Doxygen 1.8+ - Documentation generation system

## SUPPORTED AUDIO FORMATS

### Pulse Code Modulation Formats

The implementation provides comprehensive support for the following
PCM sample formats with full bidirectional conversion capabilities:

#### Floating-Point Representations (IEEE 754)
- `f16be`, `f16le` - 16-bit half-precision floating-point
- `f32be`, `f32le` - 32-bit single-precision floating-point
- `f64be`, `f64le` - 64-bit double-precision floating-point (default)

#### Signed Integer Representations (Two's Complement)
- `s8` - 8-bit signed integer
- `s16be`, `s16le` - 16-bit signed integer
- `s24be`, `s24le` - 24-bit signed integer (packed)
- `s32be`, `s32le` - 32-bit signed integer
- `s64be`, `s64le` - 64-bit signed integer

#### Unsigned Integer Representations
- `u8` - 8-bit unsigned integer
- `u16be`, `u16le` - 16-bit unsigned integer
- `u24be`, `u24le` - 24-bit unsigned integer (packed)
- `u32be`, `u32le` - 32-bit unsigned integer
- `u64be`, `u64le` - 64-bit unsigned integer

Note: Suffix notation indicates byte order where 'be' denotes
big-endian (network byte order) and 'le' denotes little-endian.

## INSTALLATION PROCEDURES

### Prerequisites Verification

Execute the following command to verify system readiness:

```bash
pkg-config --version
gcc --version || clang --version
make --version
```

### Source Code Acquisition

The canonical source repository is maintained at:
```bash
git clone https://github.com/H4n-uL/liblife.git
cd liblife
```

### Build Configuration

The build system utilizes GNU Make with the following targets:

```bash
make clean      # Remove all generated files
make all        # Build all components
make install    # Install to system directories
make uninstall  # Remove installed components
make check      # Execute test suite (if available)
```

### Standard Installation Procedure

```bash
./configure --prefix=/usr/local  # Configure installation paths
make                             # Compile source code
make check                       # Run test suite
sudo make install                # Install system-wide
sudo ldconfig                    # Update shared library cache
```

### Custom Installation Paths

To install in a non-standard location:

```bash
./configure --prefix=$HOME/local
make
make install
export PATH=$HOME/local/bin:$PATH
export LD_LIBRARY_PATH=$HOME/local/lib:$LD_LIBRARY_PATH
```

## OPERATIONAL PROCEDURES

### Command-Line Interface

The primary interface follows GNU command-line conventions:

```
life [GLOBAL-OPTIONS] COMMAND [COMMAND-OPTIONS] [ARGUMENTS]
```

### Encoding Operations

Transform PCM audio data to FrAD compressed format:

```bash
life encode input.pcm \
    --sample-rate 44100 \
    --channels 2 \
    --bits 16 \
    --profile 4 \
    --ecc 96 24 \
    --output output.frad
```

### Decoding Operations

Reconstruct PCM audio from FrAD compressed streams:

```bash
life decode input.frad \
    --format f32le \
    --enable-ecc \
    --output output.pcm
```

### Playback Operations

Direct audio playback with real-time decoding:

```bash
life play input.frad \
    --enable-ecc \
    --speed 1.0
```

### Error Correction Operations

Repair corrupted FrAD bitstreams:

```bash
life repair corrupted.frad \
    --ecc 120 30 \
    --output repaired.frad
```

### Metadata Operations

Manipulate embedded metadata chunks:

```bash
life meta add input.frad \
    --meta TITLE "Symphony No. 9" \
    --meta ARTIST "Beethoven" \
    --jsonmeta metadata.json
```

## DEVELOPMENT GUIDELINES

### Coding Standards

All contributions must adhere to the following standards:

1. ISO/IEC 9899:2018 compliance without compiler extensions
2. POSIX.1-2017 API usage for system interfaces
3. GNU Coding Standards for formatting and style
4. Comprehensive error handling with meaningful diagnostics
5. Memory safety verification using static and dynamic analysis

### Version Control Workflow

Development follows the Git-flow branching model:

```bash
git checkout -b feature/your-feature develop
# Implement changes
git commit -m "component: Detailed change description"
git push origin feature/your-feature
# Submit pull request for review
```

### Testing Requirements

All modifications must include:
- Unit tests for new functionality
- Integration tests for API changes
- Regression tests for bug fixes
- Performance benchmarks for optimizations

### Documentation Standards

Code documentation must include:
- Doxygen-formatted function headers
- Inline comments for complex algorithms
- README updates for user-facing changes
- Man page updates for command-line changes

## TECHNICAL SUPPORT

### Issue Reporting

Report bugs through the GitHub issue tracking system:
https://github.com/H4n-uL/liblife/issues

Include the following information:
1. LIFE version (`life --version`)
2. Operating system and version
3. Complete command-line invocation
4. Error messages and diagnostic output
5. Sample files reproducing the issue (if applicable)

### Community Resources

- Mailing List: life-users@lists.nongnu.org
- IRC Channel: #life on irc.libera.chat
- Forum: discourse.life-codec.org (nonexistent)
- Wiki: wiki.life-codec.org (also nonexistent)

## PERFORMANCE CHARACTERISTICS

### Computational Complexity

- Encoding: O(n log n) for DCT operations
- Decoding: O(n log n) for inverse DCT
- Error Correction: O(n²) for Reed-Solomon
- Metadata Parsing: O(n) for linear scanning

### Memory Requirements

- Encoding: 2 × frame_size × channels × sizeof(double)
- Decoding: 2 × frame_size × channels × sizeof(double)
- Playback: 8192 × channels × sizeof(float) ring buffer

### Benchmarks

Typical performance on reference hardware (Intel Core i7-9700K):
- Encoding: 150× realtime (Profile 4)
- Decoding: 200× realtime (Profile 4)
- Encoding: 25× realtime (Profile 0)
- Decoding: 30× realtime (Profile 0)

## ACKNOWLEDGMENTS

The LIFE development team acknowledges contributions from:

- Free Software Foundation for promoting software freedom
- PocketFFT developers for transform algorithms
- PortAudio contributors for audio abstraction layer
- The broader open-source community

Special recognition to early adopters who provided invaluable
feedback during the alpha testing phase.

## REFERENCES

1. ISO/IEC 9899:2018 - Programming Language C
2. IEEE Std 1003.1-2017 - POSIX Specifications  
3. RFC 8259 - The JavaScript Object Notation
4. RFC 4648 - Base16, Base32, and Base64 Data Encodings
5. ITU-R BS.1770-4 - Loudness Measurement Algorithms
6. GNU Coding Standards - Free Software Foundation

## COPYRIGHT AND LICENSING

Copyright (C) 2024-2025 The LIFE Development Team

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.

The complete license text is available in the COPYING file
distributed with this software and at:
https://www.gnu.org/licenses/agpl-3.0.html

## AUTHOR INFORMATION

Maintainer:
- Definitely Not HaמuL <jun061119@proton.me>

---
End of Document
Last Updated: 2025-01-01
Document Version: 1.0.0