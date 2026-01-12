import pandas as pd
import json
import os

# Read the Excel file
file_path = "C:\\Users\\ashahn\\Documents\\GIT_SAND\\arcade\\tools\\ARCADE_UCA_Manual_Runs.xlsx"
df = pd.read_excel(file_path)

# Create a directory to store the JSON files
output_dir = 'json_outputs'
os.makedirs(output_dir, exist_ok=True)

# Initialize a counter for naming JSON files
file_counter = 1

# Iterate through each column starting from the 4th column
for col in df.columns[3:]:  # Start checking from the 4th column onward
    # Initialize a list to hold attack values for this column
    attacks = {}

    # Iterate through each row in the DataFrame
    for index, row in df.iterrows():
        row_number = index + 1  # Get the current row number (1-based index)
        lower_limit = row['Lower Limit']
        upper_limit = row['Upper Limit']

        # Check for 'U1-L' or 'U1-U' in the current column
        if row[col] == 'U1-L':
            attacks[f"Value{row_number}"] = str(lower_limit)  # Add lower limit
        elif row[col] == 'U1-U':
            attacks[f"Value{row_number}"] = str(upper_limit)  # Add upper limit

    # If there are any attacks recorded, write to a JSON file
    if attacks:
        json_data = {
            "Attacks": [
                {
                    "Name": "Attack_1",
                    "Type": "PNN",
                    "Time": "605",
                    "Set_All_Values": "False",
                    **attacks  # Unpack the attacks dictionary into the JSON structure
                }
            ]
        }
        # Write to a JSON file named according to the counter
        output_file_path = os.path.join(output_dir, f"attack_{file_counter}.json")
        with open(output_file_path, 'w') as json_file:
            json.dump(json_data, json_file, indent=4)
        
        # Increment the file counter for the next file
        file_counter += 1

print(f"JSON files have been written to the '{output_dir}' directory.")