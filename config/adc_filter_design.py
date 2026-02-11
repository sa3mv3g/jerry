#!/usr/bin/env python3
"""
ADC Filter Coefficient Generator for Jerry Data Acquisition Firmware.

This script designs digital filters for ADC signal conditioning:
- 4th order Butterworth low-pass filter (500Hz cutoff)
- 10 IIR notch filters for 50Hz mains rejection and harmonics

The generated coefficients are formatted for CMSIS-DSP biquad cascade filters.
Uses Jinja2 templates for C/H file generation.

Usage:
    python adc_filter_design.py [--output-dir PATH] [--plot]

Author: Jerry Project
License: MIT
"""

import argparse
import os
from pathlib import Path
from typing import List, Tuple, Dict, Any

import numpy as np
from scipy import signal

try:
    from jinja2 import Environment, FileSystemLoader
except ImportError:
    print("Error: jinja2 is required. Install with: pip install jinja2")
    raise


# Filter design parameters
SAMPLE_RATE = 10000  # Hz
LPF_CUTOFF = 500  # Hz
LPF_ORDER = 4  # 4th order Butterworth
MAINS_FREQ = 50  # Hz
# Q factor for notch filters - lower values give wider notches but better stability
# Q=10 gives ~5Hz bandwidth at 50Hz for precise mains rejection
# Pole radius at 50Hz: r = 1 - π*5/10000 = 0.9984 (stable)
NOTCH_Q = 10  # Quality factor for notch filters
NUM_HARMONICS = 10  # Number of harmonics to reject (50Hz to 500Hz)

# Template and output paths (relative to this script's directory)
TEMPLATES_DIR = "templates"
DEFAULT_OUTPUT_DIR = "../application/dependencies/adc_filter"
HEADER_TEMPLATE = "adc_filter_coefficients.h.j2"
SOURCE_TEMPLATE = "adc_filter_coefficients.c.j2"
HEADER_OUTPUT = "inc/adc_filter_coefficients.h"
SOURCE_OUTPUT = "src/adc_filter_coefficients.c"


def design_butterworth_lpf(
    cutoff: float, fs: float, order: int
) -> np.ndarray:
    """
    Design a Butterworth low-pass filter and return second-order sections.

    Each section is normalized to have unity DC gain to prevent numerical
    instability when cascading multiple biquad stages.

    Args:
        cutoff: Cutoff frequency in Hz
        fs: Sampling frequency in Hz
        order: Filter order

    Returns:
        Second-order sections array (n_sections x 6), each with unity DC gain
    """
    nyquist = fs / 2.0
    normalized_cutoff = cutoff / nyquist

    # Design Butterworth filter and get SOS representation
    sos = signal.butter(order, normalized_cutoff, btype="low", output="sos")

    # Normalize each section to have unity DC gain
    # DC gain of a biquad section: (b0 + b1 + b2) / (1 + a1 + a2)
    # We scale b coefficients so DC gain = 1.0
    for i in range(len(sos)):
        b0, b1, b2, a0, a1, a2 = sos[i]
        dc_gain_num = b0 + b1 + b2
        dc_gain_den = a0 + a1 + a2
        dc_gain = dc_gain_num / dc_gain_den if dc_gain_den != 0 else 1.0

        if dc_gain != 0 and dc_gain != 1.0:
            # Normalize b coefficients to achieve unity DC gain
            sos[i, 0] = b0 / dc_gain
            sos[i, 1] = b1 / dc_gain
            sos[i, 2] = b2 / dc_gain

    return sos


def design_notch_filter(freq: float, fs: float, q: float) -> np.ndarray:
    """
    Design an IIR notch filter and return second-order sections.

    Uses the standard notch filter design formula for better numerical stability.
    The transfer function is:
        H(z) = (1 - 2*cos(w0)*z^-1 + z^-2) / (1 - 2*r*cos(w0)*z^-1 + r^2*z^-2)
    where w0 = 2*pi*freq/fs and r = 1 - pi*BW/fs (BW = freq/Q)

    Args:
        freq: Notch frequency in Hz
        fs: Sampling frequency in Hz
        q: Quality factor (higher = narrower notch)

    Returns:
        Second-order sections array (1 x 6)
    """
    # Calculate normalized angular frequency
    w0 = 2.0 * np.pi * freq / fs

    # Calculate bandwidth and pole radius
    bw = freq / q  # 3dB bandwidth
    r = 1.0 - (np.pi * bw / fs)

    # Clamp r to prevent instability (must be < 1)
    r = min(r, 0.999)

    # Calculate coefficients using standard notch formula
    cos_w0 = np.cos(w0)

    # Numerator: zeros on unit circle at w0
    b0 = 1.0
    b1 = -2.0 * cos_w0
    b2 = 1.0

    # Denominator: poles inside unit circle at radius r
    a0 = 1.0
    a1 = -2.0 * r * cos_w0
    a2 = r * r

    # Normalize to have unity gain at DC
    # DC gain = (b0 + b1 + b2) / (a0 + a1 + a2)
    dc_gain_num = b0 + b1 + b2
    dc_gain_den = a0 + a1 + a2
    dc_gain = dc_gain_num / dc_gain_den if dc_gain_den != 0 else 1.0

    if dc_gain != 0:
        b0 /= dc_gain
        b1 /= dc_gain
        b2 /= dc_gain

    # Return as SOS format
    sos = np.array([[b0, b1, b2, a0, a1, a2]])

    return sos


def sos_to_cmsis_coefficients(sos: np.ndarray) -> List[float]:
    """
    Convert scipy SOS format to CMSIS-DSP biquad coefficient format.

    scipy SOS format: [b0, b1, b2, a0, a1, a2] where a0 is always 1.0
    CMSIS-DSP format: [b0, b1, b2, -a1, -a2] (note: a1 and a2 are negated)

    Args:
        sos: Second-order sections array from scipy

    Returns:
        List of coefficients in CMSIS-DSP format
    """
    coefficients = []

    for section in sos:
        b0, b1, b2, a0, a1, a2 = section

        # Normalize by a0 (should be 1.0, but just in case)
        if a0 != 1.0:
            b0 /= a0
            b1 /= a0
            b2 /= a0
            a1 /= a0
            a2 /= a0

        # CMSIS-DSP expects negated a1 and a2
        coefficients.extend([b0, b1, b2, -a1, -a2])

    return coefficients


def format_coefficient(coef: float) -> str:
    """Format a coefficient as a C float literal."""
    return f"{coef: .15e}f"


def design_complete_filter() -> Tuple[List[float], int, Dict[str, Any], List[Dict[str, Any]]]:
    """
    Design the complete filter chain (LPF + notch filters).

    Returns:
        Tuple of (coefficients list, number of stages, design info dict, coefficient sections)
    """
    all_sos = []
    coefficient_sections = []
    design_info = {
        "sample_rate": SAMPLE_RATE,
        "lpf_cutoff": LPF_CUTOFF,
        "lpf_order": LPF_ORDER,
        "notch_frequencies": [],
        "notch_q": NOTCH_Q,
    }

    # Design LPF
    lpf_sos = design_butterworth_lpf(LPF_CUTOFF, SAMPLE_RATE, LPF_ORDER)
    all_sos.append(lpf_sos)
    design_info["lpf_stages"] = len(lpf_sos)

    # Add LPF sections to coefficient_sections
    for i, section in enumerate(lpf_sos):
        b0, b1, b2, a0, a1, a2 = section
        if a0 != 1.0:
            b0 /= a0
            b1 /= a0
            b2 /= a0
            a1 /= a0
            a2 /= a0
        coeffs = [format_coefficient(c) for c in [b0, b1, b2, -a1, -a2]]
        coefficient_sections.append({
            "comment": f"LPF Stage {i + 1}",
            "coefficients": coeffs
        })

    # Design notch filters for each harmonic
    for harmonic in range(1, NUM_HARMONICS + 1):
        notch_freq = MAINS_FREQ * harmonic
        if notch_freq <= SAMPLE_RATE / 2:  # Must be below Nyquist
            notch_sos = design_notch_filter(notch_freq, SAMPLE_RATE, NOTCH_Q)
            all_sos.append(notch_sos)
            design_info["notch_frequencies"].append(notch_freq)

            # Add notch section to coefficient_sections
            section = notch_sos[0]
            b0, b1, b2, a0, a1, a2 = section
            if a0 != 1.0:
                b0 /= a0
                b1 /= a0
                b2 /= a0
                a1 /= a0
                a2 /= a0
            coeffs = [format_coefficient(c) for c in [b0, b1, b2, -a1, -a2]]
            coefficient_sections.append({
                "comment": f"Notch {notch_freq}Hz",
                "coefficients": coeffs
            })

    # Combine all SOS sections
    combined_sos = np.vstack(all_sos)
    num_stages = len(combined_sos)

    # Convert to CMSIS-DSP format
    coefficients = sos_to_cmsis_coefficients(combined_sos)

    design_info["total_stages"] = num_stages
    design_info["notch_stages"] = len(design_info["notch_frequencies"])

    return coefficients, num_stages, design_info, coefficient_sections


def render_template(
    env: Environment,
    template_name: str,
    output_path: Path,
    context: Dict[str, Any]
) -> None:
    """
    Render a Jinja2 template and write to file.

    Args:
        env: Jinja2 environment
        template_name: Name of the template file
        output_path: Path to write the output file
        context: Template context dictionary
    """
    template = env.get_template(template_name)
    content = template.render(**context)

    # Ensure output directory exists
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with open(output_path, "w", encoding="utf-8") as f:
        f.write(content)


def plot_frequency_response(coefficients: List[float], num_stages: int, design_info: Dict[str, Any]) -> None:
    """
    Plot the frequency response of the designed filter.

    Args:
        coefficients: List of filter coefficients
        num_stages: Number of biquad stages
        design_info: Dictionary with design parameters
    """
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("Warning: matplotlib not available, skipping plot")
        return

    # Convert CMSIS coefficients back to SOS for analysis
    sos = np.zeros((num_stages, 6))
    for i in range(num_stages):
        start = i * 5
        b0, b1, b2, neg_a1, neg_a2 = coefficients[start : start + 5]
        sos[i] = [b0, b1, b2, 1.0, -neg_a1, -neg_a2]

    # Compute frequency response
    w, h = signal.sosfreqz(sos, worN=2000, fs=SAMPLE_RATE)

    # Plot
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8))

    # Magnitude response
    ax1.plot(w, 20 * np.log10(np.abs(h) + 1e-10))
    ax1.set_xlabel("Frequency (Hz)")
    ax1.set_ylabel("Magnitude (dB)")
    ax1.set_title("ADC Filter Frequency Response")
    ax1.set_xlim(0, SAMPLE_RATE / 2)
    ax1.set_ylim(-80, 5)
    ax1.grid(True)
    ax1.axvline(LPF_CUTOFF, color="r", linestyle="--", label=f"LPF Cutoff ({LPF_CUTOFF}Hz)")

    # Mark notch frequencies
    for freq in design_info["notch_frequencies"]:
        ax1.axvline(freq, color="g", linestyle=":", alpha=0.5)

    ax1.legend()

    # Phase response
    ax2.plot(w, np.unwrap(np.angle(h)) * 180 / np.pi)
    ax2.set_xlabel("Frequency (Hz)")
    ax2.set_ylabel("Phase (degrees)")
    ax2.set_title("Phase Response")
    ax2.set_xlim(0, SAMPLE_RATE / 2)
    ax2.grid(True)

    plt.tight_layout()
    plt.savefig("adc_filter_response.png", dpi=150)
    print("Frequency response plot saved to: adc_filter_response.png")
    plt.show()


def main() -> None:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Generate ADC filter coefficients for CMSIS-DSP"
    )
    parser.add_argument(
        "--output-dir",
        type=str,
        default=DEFAULT_OUTPUT_DIR,
        help=f"Output directory for generated files (default: {DEFAULT_OUTPUT_DIR})",
    )
    parser.add_argument(
        "--plot",
        action="store_true",
        help="Plot frequency response (requires matplotlib)",
    )
    args = parser.parse_args()

    # Get script directory for relative paths
    script_dir = Path(__file__).parent.resolve()
    templates_dir = script_dir / TEMPLATES_DIR
    output_dir = (script_dir / args.output_dir).resolve()

    print("=" * 60)
    print("ADC Filter Coefficient Generator")
    print("=" * 60)

    # Verify templates directory exists
    if not templates_dir.exists():
        print(f"Error: Templates directory not found: {templates_dir}")
        return

    # Setup Jinja2 environment
    env = Environment(
        loader=FileSystemLoader(templates_dir),
        trim_blocks=True,
        lstrip_blocks=True,
    )

    # Design the filter
    print("\nDesigning filters...")
    coefficients, num_stages, design_info, coefficient_sections = design_complete_filter()

    print(f"  Sample Rate: {design_info['sample_rate']} Hz")
    print(f"  LPF: {design_info['lpf_order']}th order Butterworth, {design_info['lpf_cutoff']} Hz cutoff")
    print(f"  LPF Stages: {design_info['lpf_stages']}")
    print(f"  Notch Frequencies: {design_info['notch_frequencies']} Hz")
    print(f"  Notch Q Factor: {design_info['notch_q']}")
    print(f"  Total Biquad Stages: {num_stages}")
    print(f"  Total Coefficients: {len(coefficients)}")

    # Prepare template context
    context = {
        "design_info": design_info,
        "num_stages": num_stages,
        "coefficient_sections": coefficient_sections,
    }

    # Generate header file
    header_path = output_dir / HEADER_OUTPUT
    print(f"\nGenerating header file: {header_path}")
    render_template(env, HEADER_TEMPLATE, header_path, context)

    # Generate source file
    source_path = output_dir / SOURCE_OUTPUT
    print(f"Generating source file: {source_path}")
    render_template(env, SOURCE_TEMPLATE, source_path, context)

    print("\nGeneration complete!")

    # Plot if requested
    if args.plot:
        print("\nGenerating frequency response plot...")
        plot_frequency_response(coefficients, num_stages, design_info)


if __name__ == "__main__":
    main()
