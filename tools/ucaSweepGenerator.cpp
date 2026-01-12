#include <iostream>
#include <fstream>

using namespace std;

#define TOTAL_UCAS 1

int main() {

	// TODO: be able to read in a CSV file of targets rather by hand

	vector<string> controlTargets = {"INT_RXPowSetpoint", 
		"CR_PosCmd", 
		"PZ_MainHeaterPowCmd",
		"PZ_BackupHeaterPowCmd",
		"PZ_CL1SprayValveCmd",
		"PZ_CL2SprayValveCmd",
		"AF_MakeupPumpCmd",
		"AF_MakeupValveCmd",
		"AF_LetdownValveCmd",
		"RC1_PumpSpeedCmd",
		"RC1_PumpOnOffCmd",
		"RC2_PumpSpeedCmd",
		"RC2_PumpOnOffCmd",
		"FW_Pump1SpeedCmd",
		"FW_Pump1OnOffCmd",
		"FW_Pump2SpeedCmd",
		"FW_Pump2OnOffCmd",
		"FW_Pump3SpeedCmd",
		"FW_Pump3OnOffCmd",
		"TB_SpeedCtrlValveCmd",
		"TB_OnOffCmd",
		"TB_IsoValveCmd",
		"CE_Pump1SpeedCmd",
		"CE_Pump1OnOffCmd",
		"CE_Pump2SpeedCmd",
		"CE_Pump2OnOffCmd",
		"CE_Pump3OnOffCmd",
		"CE_Pump3SpeedCmd",
		"CC_PumpSpeedCmd",
		"CC_PumpOnOffCmd",
		"SD_CtrlValveCmd",
		"SD_SafetyValveOnOffCmd",
		"CR_SCRAMCmd",
		"Boron_CMD",
		"PZ_Relief",
		"Turbine_RPM",
		"TB_BypassValveCmd"
	};

	// Open file for writing
	ofstream ucafile;
	ucafile.open("asherahUCASweep.csv");

	// file format: #, UCA#, L/H, target(s)

	int num = 1; 

	for(int i = 1; i <= TOTAL_UCAS; i++){
		for(string currTarget : controlTargets){
			ucafile << num << "," << i << ",L,X," << currTarget << '\n'; 
			num++;
			ucafile << num << "," << i << ",H,X," << currTarget << '\n'; 
			num++;

			int ourPos = find(controlTargets.begin(), controlTargets.end(), currTarget) - controlTargets.begin();

			// now go through all the proceeding targets since we are two deep
			// TODO make this expandable for >2 UCA combos
			for(int pos = ourPos + 1; pos < controlTargets.size(); pos++){
				string nextUCA = controlTargets.at(pos);

				ucafile << num << "," << i << ",L,L," << currTarget << "," << nextUCA << '\n';
				num++;
				ucafile << num << "," << i << ",L,H," << currTarget << "," << nextUCA << '\n';
				num++;
				ucafile << num << "," << i << ",H,H," << currTarget << "," << nextUCA << '\n';
				num++;
				ucafile << num << "," << i << ",H,L," << currTarget << "," << nextUCA << '\n';
				num++;
			}
		}
	}

	ucafile.close();

	return 1;

}
