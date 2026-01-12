import pandas as pd
import matplotlib.pyplot as plt
import argparse
import os

def transform_csv(input_file, output_file):
    df = pd.read_csv(input_file)
    df['Time'] = df['Time'].astype(float)
    df = df.drop_duplicates(subset=['Time', 'Name'])
    df_pivot = df.pivot(index='Time', columns='Name', values='Value')
    df_pivot.reset_index(inplace=True)
    df_pivot.to_csv(output_file, index=False)

def filter_outliers(df, column):
    # Calculate the mean and standard deviation
    mean = df[column].mean()
    std_dev = df[column].std()

    # Define a threshold for outliers (e.g., 3 standard deviations from the mean)
    threshold = 2 * std_dev

    # Filter the DataFrame to ignore outliers
    return df[(df[column] >= mean - threshold) & (df[column] <= mean + threshold)]

def plot_csv_files(csv_files):
    dataframes = []
    for csv_file in csv_files:
        df = pd.read_csv(csv_file)
        df.set_index(df.columns[0], inplace=True)
        df.index = pd.to_numeric(df.index, errors='coerce')
        df = df.sort_index().interpolate(method='linear')

        # Debug: Print each dataframe to check for data
        print(f"DataFrame from {csv_file}:")
        print(df.head())

        dataframes.append(df)

    # Align all dataframes on index (Time)
    aligned_dfs = pd.concat(dataframes, axis=1, join='outer').interpolate(method='linear')

    # Debug: Print the aligned dataframe to check for missing values
    print("Aligned DataFrame:")
    print(aligned_dfs)

    output_directory = 'output_plots'
    if not os.path.exists(output_directory):
        os.makedirs(output_directory)

    for column in aligned_dfs.columns:
        plt.figure(figsize=(20, 10))
        for df, csv_file in zip(dataframes, csv_files):
            if column in df.columns:
                # Filter out outliers for the current column
                filtered_df = filter_outliers(df, column)
                filtered_df[column].plot(linewidth=3, label=f'{os.path.basename(csv_file)} - {column}')
        plt.xlabel('Timestep')
        plt.ylabel(column)
        plt.title(f'{column} Over Time')
        plt.grid(True)
        plt.legend()
        plt.savefig(os.path.join(output_directory, f'{column}_plot.png'))
        plt.show()
        plt.close()

def main():
    parser = argparse.ArgumentParser(description='Transform CSV files and generate plots.')
    parser.add_argument('input_files', nargs='+', help='Paths to the input CSV files')
    parser.add_argument('--formatted', action='store_true', help='Specify if the input CSV files are already formatted')
    parser.add_argument('--uid', type=str, required=True, help='UID of simulation to append to file names')
    args = parser.parse_args()

    output_files = []
    for input_file in args.input_files:
        base_name = os.path.basename(input_file)
        name, ext = os.path.splitext(base_name)
	uid = args.uid
        output_file = f'{uid}_formatted_{name}{ext}' if not args.formatted else input_file

        if not args.formatted:
            transform_csv(input_file, output_file)

        output_files.append(output_file)

    plot_csv_files(output_files)

if __name__ == "__main__":
    main()
