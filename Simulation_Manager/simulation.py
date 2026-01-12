import yaml
from Simulation_Manager import Errors

class simulation:
    def __init__(self, path, uid):
        try:
            with open(path, 'r') as config_file:
                self.config = yaml.safe_load(config_file)
        except Exception as e:
            print("Config file not properly formatted")
            raise ConfigError from e

        self.uid = uid
        self.new_sim = self.config.get("new_sim", False)
        self.exp_name = self.config["exp_name"]
        self.minimega_filepath = self.config["minimega_filepath"]
        self.wallclock_timeout = self.config["wallclock_timeout"]

        attack_config = self.config["attack"]
        self.attack_simulator_name = attack_config.get("attack_simulator_name", "Unknown")
        
        # Store input file details as a list of dicts
        self.input_files = []
        for key, val in attack_config.get("input_files", {}).items():
            self.input_files.append({
                "name": key,
                "path": val["path"],
                "execute": val["execute"],
                "vm": val["vm"]
            })

        self.staging_script = self.config.get("staging_script")
        self.collection_script = self.config.get("collection_script")

        self.parsed = False
        self.temp_path = f'/tmp/{str(self.uid)}.py'
        self.termination_script = self.config.get("termination_script", None)
