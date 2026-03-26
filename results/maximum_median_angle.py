import os
import os.path
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import json
import statistics

plt.style.use("ieee.mplstyle")
   
def rad_to_degree(df_sender_log) -> None:
    # following computation is from pendulum_receiver1.ino 
    # ENCODER PARAMETERS
    encoderPPRhalf = 600 * 2
    encoderPPR = encoderPPRhalf * 2
    encoderOrigin = encoderPPRhalf
    angleSign = 1
    TWO_PI = 2 * np.pi
    RAD_PER_ESTEP = TWO_PI / encoderPPR
    ESTEP_PER_RAD = encoderPPR / TWO_PI

    angleRad = RAD_PER_ESTEP * (angleSign * ((df_sender_log["angleSensorValue"] - encoderOrigin - encoderPPRhalf) % encoderPPR) - encoderPPRhalf)
    df_sender_log["poleAngleDegrees"] = angleRad * 360 / TWO_PI
   
def absolute_error(file_path: str) -> list[float]:
    if os.path.isfile(file_path):
        file = open(file_path, "r")
        data = json.load(file)
        file.close()
          
        df_sender_log = pd.json_normalize(data["timePointLogs"])
        
        rad_to_degree(df_sender_log)
                    
        return abs(df_sender_log["poleAngleDegrees"]).tolist()
                
    else:
        raise FileNotFoundError
        
def values_one_config(runTime, file_path):
    directory_1 = runTime + '/run_' + runTime + '_1'
    directory_2 = runTime + '/run_' + runTime + '_2'
    directory_3 = runTime + '/run_' + runTime + '_3'
    directory_4 = runTime + '/run_' + runTime + '_4'
        
    return absolute_error(directory_1 + file_path) \
            + absolute_error(directory_2 + file_path) \
            + absolute_error(directory_3 + file_path) \
            + absolute_error(directory_4 + file_path)
    
     
angles_absolute  = []
labels = ['baseline', '1', '2', '3', '4', '5', '6', '7']
runTime = '15min'

angles_absolute.append(values_one_config(runTime, "/pendulumsender_et-tt90_base.json"))
angles_absolute.append(values_one_config(runTime, "/pendulumsender_et-tt90_0.json"))
angles_absolute.append(values_one_config(runTime, "/pendulumsender_et-tt90_1.json"))
angles_absolute.append(values_one_config(runTime, "/pendulumsender_et-tt90_2.json"))
angles_absolute.append(values_one_config(runTime, "/pendulumsender_et-tt90_3.json"))
angles_absolute.append(values_one_config(runTime, "/pendulumsender_et-tt90_4.json"))
angles_absolute.append(values_one_config(runTime, "/pendulumsender_et-tt90_5.json"))
#angles_absolute.append(values_one_config(runTime, "/pendulumsender_et-tt90_6.json"))
#angles_absolute.append(values_one_config(runTime, "/pendulumsender_et-tt90_7.json"))
#angles_absolute.append(values_one_config(runTime, "/pendulumsender_et-tt90_8.json"))
#angles_absolute.append(values_one_config(runTime, "/pendulumsender_et-tt90_9.json"))
#angles_absolute.append(values_one_config(runTime, "/pendulumsender_et-tt90_no_et.json"))

maxAngles = []
medianAngles = []
for i in range(len(angles_absolute)):
    maxAngles.append(max(angles_absolute[i]))
    medianAngles.append(statistics.median(angles_absolute[i]))

plt.plot([x+1 for x in range(len(maxAngles))], maxAngles, '+', label="Maximum")
plt.plot([x+1 for x in range(len(medianAngles))], medianAngles, 'x', label="Median")

#print(maxAngles)
#print(medianAngles)

plt.xticks([x+1 for x in range(len(maxAngles))], labels[:7])
plt.ylabel(r"Pole Angle Error $|\alpha|$ [°]")
plt.xlabel("$k$ in $(1,k)$-firm Deadline")
plt.legend()
plt.savefig("eval_angle_all_" + runTime + ".pdf", bbox_inches="tight", pad_inches=0.0)