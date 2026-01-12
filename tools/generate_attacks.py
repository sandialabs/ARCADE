'''
Prerequisites:
	- A csv or yaml of tag values and their upper and lower bounds
		csv: 
			name,lower,upper
			Tag1,0.0,100.0
			Tag2,-10.0,10.0
			Tag3,1.0,5.0
		yaml:
			- name: Tag1
  			lower: 0.0
			  upper: 100.0
			- name: Tag2
			  lower: -10.0
			  upper: 10.0
			- name: Tag3
			  lower: 1.0
			  upper: 5.0
	- A minimega topology script for the environment that you are testing
	- A minimega script for staging any relevant files for extraction
	- A minimega script for collecting all relevant files

Usage: generate_attacks.py [-h] --input_file INPUT_FILE --output_dir OUTPUT_DIR --minimega_filepath MINIMEGA_FILEPATH --staging_script STAGING_SCRIPT --collection_script COLLECTION_SCRIPT --attack_time ATTACK_TIME [--depth DEPTH] --sim_runtime SIM_RUNTIME

Generate attack JSON and YAML files with absolute paths and depth-based tag combinations.

options:
  -h, --help            show this help message and exit
  --input_file INPUT_FILE
                        Path to input YAML or CSV file with tags.
  --output_dir OUTPUT_DIR
                        Directory to output JSON and YAML files.
  --minimega_filepath MINIMEGA_FILEPATH
                        Path to minimega topology script.
  --staging_script STAGING_SCRIPT
                        Path to staging script.
  --collection_script COLLECTION_SCRIPT
                        Path to collection script.
  --attack_time ATTACK_TIME
                        Time value to inject into each attack JSON file.
  --depth DEPTH         Max depth of tag combinations to attack (default: 2).
  --sim_runtime SIM_RUNTIME
                        Duration of simulation.

'''

import os
import json
import yaml
import csv
import argparse
from itertools import combinations, product

# Reads in the tag file that is defined in the CLI parameters. Can be a yaml or a csv, format defined at top of file
def load_tags(file_path):
    if file_path.endswith((".yaml", ".yml")):
        with open(file_path, 'r') as f:
            return yaml.safe_load(f)
    elif file_path.endswith(".csv"):
        with open(file_path, newline='') as f:
            reader = csv.DictReader(f)
            return [{"name": row["name"], "lower": float(row["lower"]), "upper": float(row["upper"])} for row in reader]
    else:
        raise ValueError("Unsupported file format. Use .yaml, .yml, or .csv.")

# Generates the attack json for each tag_value in attack with the defined parameters and outputs it to a file
def create_attack_json(tag_values, attack_num, json_dir, attack_time):
    attack = {
        "Attacks": [
            {
                "Name": f"Attack_{attack_num}",
                "Type": "PNN",
                "Time": str(attack_time),
                "Set_All_Values": "False",
                **tag_values
            }
        ]
    }
    json_filename = f"attack_{attack_num}.json"
    json_path = os.path.join(json_dir, json_filename)
    with open(json_path, 'w') as f:
        json.dump(attack, f, indent=4)
    return os.path.abspath(json_path)

# Generates the attack yaml for each attack with the defined parameters and outputs it to a file
def create_yaml(json_path, attack_num, yaml_dir, minimega_path,
    endpoint_script_path, staging_script_path, collection_script_path,
    sim_runtime, wallclock_timeout, sim):
    data = {
        'new_sim': True,
        'exp_name': f'exp{attack_num}',
        'minimega_filepath': minimega_path,
        'max_simulation_runtime': sim_runtime,
        'wallclock_timeout': wallclock_timeout,
        'attack': {
            'attack_simulator_name': 'SIM',
            'input_files': {
                'file_1': {
                    'path': json_path,
                    'execute': False,
                    'vm': sim
                },
                'file_2': {
                    'path': endpoint_script_path,
                    'execute': False,
                    'vm': sim
                }
            }
        },
        'staging_script': staging_script_path,
        'collection_script': collection_script_path
    }
    yaml_path = os.path.join(yaml_dir, f"attack_{attack_num}.yaml")
    with open(yaml_path, 'w') as f:
        yaml.dump(data, f)
    return os.path.abspath(yaml_path)

# Takes in the tags and depth and generates all attacks
def generate_attacks(tags, depth, json_dir, yaml_dir,
                     minimega_path, staging_path, collection_path, attack_time,sim_runtime):
    attack_counter = 1

    for d in range(1, depth + 1):
        for combo in combinations(tags, d):
            value_options = [[t["lower"], t["upper"]] for t in combo]
            for values in product(*value_options):
                tag_values = {combo[i]["name"]: str(values[i]) for i in range(d)}
                json_path = create_attack_json(tag_values, attack_counter, json_dir, attack_time)
                create_yaml(json_path, attack_counter, yaml_dir,
                            minimega_path, staging_path, collection_path, sim_runtime, 'SIM')
                attack_counter += 1

    print(f"Generated {attack_counter - 1} attacks (depth 1 to {depth}) in '{os.path.abspath(json_dir)}' and YAMLs in '{os.path.abspath(yaml_dir)}'.")

def estimate_total_attacks(num_tags, max_depth):
    from math import comb
    total = 0
    for d in range(1, max_depth + 1):
        total += comb(num_tags, d) * (2 ** d)
    return total

def main(args):
    tags = load_tags(args.input_file)
    num_tags = len(tags)
    max_depth = args.depth

    # Estimate total number of attack scenarios
    total_attacks = estimate_total_attacks(num_tags, max_depth)

    # Threshold for interactive warning
    threshold = 100_000

    print(f"Estimated total number of attacks: {total_attacks} (depth={max_depth}, tags={num_tags})")

    if total_attacks > threshold:
        print("\nWARNING: This configuration may generate a very large number of files.")
        print(f"  - Estimated JSON/YAML files to be generated: {total_attacks}")
        print("  - This may take a long time or consume significant disk space.")

        proceed = input("Do you want to continue? (yes/no): ").strip().lower()
        if proceed != "yes":
            print("Aborting generation.")
            return
    
    json_dir = os.path.join(args.output_dir, "json")
    yaml_dir = os.path.join(args.output_dir, "yaml")
    os.makedirs(json_dir, exist_ok=True)
    os.makedirs(yaml_dir, exist_ok=True)

    generate_attacks(
        tags,
        args.depth,
        json_dir,
        yaml_dir,
        os.path.abspath(args.minimega_filepath),
        os.path.abspath(args.staging_script),
        os.path.abspath(args.collection_script),
        args.attack_time,
        args.sim_runtime
    )

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate attack JSON and YAML files with absolute paths and depth-based tag combinations.")
    parser.add_argument("--input_file", required=True, help="Path to input YAML or CSV file with tags.")
    parser.add_argument("--output_dir", required=True, help="Directory to output JSON and YAML files.")
    parser.add_argument("--minimega_filepath", required=True, help="Path to minimega topology script.")
    parser.add_argument("--staging_script", required=True, help="Path to staging script.")
    parser.add_argument("--collection_script", required=True, help="Path to collection script.")
    parser.add_argument("--attack_time", required=True, help="Time value to inject into each attack JSON file.")
    parser.add_argument("--depth", type=int, default=2, help="Max depth of tag combinations to attack (default: 2).")
    parser.add_argument("--sim_runtime", required=True, type=int, help="Duration of simulation.")
    args = parser.parse_args()

    main(args)
