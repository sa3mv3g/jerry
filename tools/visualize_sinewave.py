import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

def main():
    file_path = 'temp/sinewave_2.csv'
    print(f"Reading {file_path}...")
    
    # First, read the sample rate from the file header
    with open(file_path, 'r') as f:
        for i in range(6):
            line = next(f)
            if i == 4:  # The 5th line (0-indexed 4) contains the sample rate
                # Example line: "#Sample rate: 1.5873e+06Hz"
                sample_rate_str = line.split()[2].replace('Hz', '')
                sample_rate = float(sample_rate_str)
    
    # Skip the first 7 lines (metadata and empty line)
    # The actual header "DateTime,PWM" is on line 8
    df = pd.read_csv(file_path, skiprows=7)
    
    # Clean the PWM column:
    # 1. Replace 'X' with NaN (Not a Number)
    # 2. Remove the '%' symbol
    # 3. Convert to numeric values
    print("Cleaning data...")
    df['PWM'] = df['PWM'].replace('X', pd.NA)
    df['PWM'] = df['PWM'].astype(str).str.replace(' %', '', regex=False)
    df['PWM'] = pd.to_numeric(df['PWM'], errors='coerce')
    
    # Add time in seconds based on sample rate
    df['Time (s)'] = df.index / sample_rate
    
    # Drop NaN values for plotting
    df_clean = df.dropna(subset=['PWM'])
    
    print(f"Plotting {len(df_clean)} points out of {len(df)} total points...")
    
    # Read the binary array data from temp/Untitled
    array_file = 'temp/sin.bin'
    if os.path.exists(array_file):
        print(f"Reading array data from {array_file}...")
        
        # Convert to numpy array of uint16 (little endian)
        array_16 = np.fromfile("temp/sin.bin", dtype=np.uint16)
        
        # Create the plot with two subplots
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))
        
        # Top plot: PWM vs Time
        ax1.plot(df_clean['Time (s)'], df_clean['PWM'], linestyle='-', marker='', color='b', linewidth=1.5)
        ax1.set_title('PWM vs Time (from CSV)')
        ax1.set_xlabel('Time (s)')
        ax1.set_ylabel('PWM (%)')
        ax1.grid(True, linestyle='--', alpha=0.7)
        
        # Bottom plot: Array data (channel 0) as percentage
        ax2.plot(array_16 , linestyle='-', marker='', color='r', linewidth=1.5)
        ax2.set_title('Array Data (Channel 0) vs Time (one carrier cycle)')
        ax2.set_xlabel('Time (s)')
        ax2.set_ylabel('PWM (%)')
        ax2.grid(True, linestyle='--', alpha=0.7)
        
        plt.tight_layout()
    else:
        # Fallback to single plot if array data is not available
        plt.figure(figsize=(12, 6))
        plt.plot(df_clean['Time (s)'], df_clean['PWM'], linestyle='-', marker='', color='b', linewidth=1.5)
        plt.title('PWM vs Time')
        plt.xlabel('Time (s)')
        plt.ylabel('PWM (%)')
        plt.grid(True, linestyle='--', alpha=0.7)
        plt.tight_layout()
    
    # Save the plot
    output_image = 'temp/sinewave_plot.png'
    plt.savefig(output_image)
    print(f"Plot saved to {output_image}")
    
    # Show the plot interactively
    plt.show()

if __name__ == "__main__":
    main()
