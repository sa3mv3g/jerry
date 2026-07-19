import pandas as pd
import matplotlib.pyplot as plt

def main():
    file_path = 'temp/sinewave_2.csv'
    print(f"Reading {file_path}...")
    
    # Skip the first 7 lines (metadata and empty line)
    # The actual header "Time (s),PWM" is on line 8
    df = pd.read_csv(file_path, skiprows=7)
    
    # Clean the PWM column:
    # 1. Replace 'X' with NaN (Not a Number)
    # 2. Remove the '%' symbol
    # 3. Convert to numeric values
    print("Cleaning data...")
    df['PWM'] = df['PWM'].replace('X', pd.NA)
    df['PWM'] = df['PWM'].astype(str).str.replace(' %', '', regex=False)
    df['PWM'] = pd.to_numeric(df['PWM'], errors='coerce')
    
    # Drop NaN values for plotting
    df_clean = df.dropna(subset=['PWM'])
    
    print(f"Plotting {len(df_clean)} points out of {len(df)} total points...")
    
    # Create the plot
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
