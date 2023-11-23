import subprocess
import os
from threading import Timer

bestParametersFile = ""

seeds = [1,2,5,10,25,50,100,200,500,1000]

base_pL = 50
base_sP = 80
base_sT = 3
base_rC = 1
base_cC = 50
base_pS = 5

def writeParamFile(pL, sP, sT, rC, cC, pS, name):
    f = open("parametersFile.txt", "w")
    s = ""
    s += "pathLength=" + str(pL) + "\n\n"
    s += "similarityPercentage=" + str(sP) + "\n"
    s += "similarityTolerance=" + str(sT) + "\n\n"
    s += "resourceCoverage=" + str(rC) + "\n"
    s += "cityCoverage=" + str(cC) + "\n\n"
    s += "pathStep=" + str(pS) + "\n\n"
    s += "fileName=" + name + "\n"
    
    f.write(s)
    f.close()
    
def getResult(file):
    f = open(file, "r")
    
    lines = f.readlines()
    score = 0
    
    for i in range(len(lines)):
        splits = (lines[i]).split('&')
        score += int(splits[1]) - int(splits[3])
        if i == (len(lines) - 1):
            if (int(splits[1]) - int(splits[3])) > 0:
                score += 50 * len(lines) #win reward
            
    return score/len(lines)

# path length
i = 1
results = []
for pathLength in range(50,151,5):
    for seed in seeds:
        
        #get these params in IA
        writeParamFile(pathLength, base_sP, base_sT, base_rC, base_cC, base_pS, "pL_Tests_" + str(i) + "_" + str(seed))
        print("results/pL_Tests_" + str(i) + "_" + str(seed) + ".txt")
        
        process = subprocess.Popen(['python3','./PlayMatch.py', str(seed)],#play the match
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE)
        
        timer = Timer(35, process.kill)
        try:
            timer.start()
            stdout, stderr = process.communicate()
        finally:
            timer.cancel()
        
        #stdout, stderr = process.communicate()
        
        #get the result and destroy the stat file
        results.append([pathLength,getResult("results/pL_Tests_" + str(i) + "_" + str(seed) + ".txt")])
        os.remove("results/pL_Tests_" + str(i) + "_" + str(seed) + ".txt")
        
    i += 1

#get the best parameter
bestPathLength = 15
bestPathLengthScore = -100000000000
for result in results:
    if result[1] > bestPathLengthScore:
        bestPathLengthScore = result[1]
        bestPathLength = result[0]
    
bestParametersFile += "best path length = " + str(bestPathLength)
    
# similarity of paths

# path linking detection

# enemy approach

file = open("results/bestParametersFile.txt", "w")
file.write(bestParametersFile)
file.close()