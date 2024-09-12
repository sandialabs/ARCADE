import yaml
from Errors import *

class simulation:
    def __init__(self, path, uid):
        try:
            with open(path, 'r') as config_file:
                self.config = yaml.safe_load(config_file)
        except ConfigError as e:
            print("Config file not properly formatted")
            print(e)
            raise ConfigError
        self.exp_name = self.config["exp_name"]
        self.uid = uid
        self.minimega_filepath = self.config["minimega_filepath"]
        self.max_simulation_runtime = self.config["max_simulation_runtime"]
        self.attack = self.config["attack"]
        self.files = self.attack["input_files"]
        self.parsed = False
        self.temp_path = f'/tmp/{str(self.uid)}.py'