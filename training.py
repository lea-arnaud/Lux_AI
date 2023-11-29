import subprocess
import os
from threading import Timer

bestParametersFile = ""

seeds = [1,2,5,10,25,50,100,200,500,1000]

base_pL = 50
base_sP = 80
base_sT = 3
base_pR = 2
base_rT = 2
base_cT = 2
base_pS = 5

def writeParamFile(pL, sP, sT, pR, rT, cT, pS, name):
    f = open("parametersFile.txt", "w")
    s = ""
    s += "pathLength=" + str(pL) + "\n\n"
    s += "similarityPercentage=" + str(sP) + "\n"
    s += "similarityTolerance=" + str(sT) + "\n"
    s += "propagationRadius=" + str(pR) + "\n\n"
    s += "resourceTiles=" + str(rT) + "\n"
    s += "cityTiles=" + str(cT) + "\n\n"
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
for pathLength in range(20,81,5):
    for seed in seeds:
        
        #get these params in IA
        writeParamFile(pathLength, base_sP, base_sT, base_pR, base_rT, base_cT, base_pS, "pL_Tests_" + str(i) + "_" + str(seed))
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
    
bestParametersFile += "best path length = " + str(bestPathLength) + "\n\n"

file = open("results/bestParametersFile.txt", "w")
file.write(bestParametersFile)
file.close()

# similarity of paths

results = []
for similarityPercentage in range(15,66,5):
    for similarityTolerance in range(1,5):
        for propagationRadius in range(1,4):
            for seed in seeds:
                
                #get these params in IA
                writeParamFile(base_pL, similarityPercentage, similarityTolerance, propagationRadius, base_rT, base_cT, base_pS, "similar_Tests_" + str(similarityPercentage) + "_" + str(similarityTolerance) + "_" + str(propagationRadius) + "_" + str(seed))
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
                results.append([[similarityPercentage, similarityTolerance, propagationRadius],getResult("results/similar_Tests_" + str(similarityPercentage) + "_" + str(similarityTolerance) + "_" + str(propagationRadius) + "_" + str(seed) + ".txt")])
                os.remove("results/similar_Tests_" + str(similarityPercentage) + "_" + str(similarityTolerance) + "_" + str(propagationRadius) + "_" + str(seed) + ".txt")

#get the best parameter
bestSimilarity = []
bestSimilarityScore = -100000000000
for result in results:
    if result[1] > bestSimilarityScore:
        bestSimilarityScore = result[1]
        bestSimilarity = result[0]
    
bestParametersFile += "best similarityPercentage = " + str(bestSimilarity[0]) + "\n"
bestParametersFile += "best similarityTolerance = " + str(bestSimilarity[1]) + "\n"
bestParametersFile += "best propagationRadius = " + str(bestSimilarity[2]) + "\n"

file = open("results/bestParametersFile.txt", "w")
file.write(bestParametersFile)
file.close()

# path linking detection

results = []
for resourceTilesNeeded in range(1,6):
    for cityTilesNeeded in range(1,6):
        for seed in seeds:
            
            #get these params in IA
            writeParamFile(base_pL, base_sP, base_sT, base_pR, resourceTilesNeeded, cityTilesNeeded, base_pS, "linking_Tests_" + str(resourceTilesNeeded) + "_" + str(cityTilesNeeded) + "_" + str(seed))
            
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
            results.append([[similarityPercentage, similarityTolerance, propagationRadius],getResult("results/similar_Tests_" + str(resourceTilesNeeded) + "_" + str(cityTilesNeeded) + "_" + str(seed) + ".txt")])
            os.remove("results/similar_Tests_" + str(resourceTilesNeeded) + "_" + str(cityTilesNeeded) + "_" + str(seed) + ".txt")

#get the best parameter
bestLinking = []
bestLinkingScore = -100000000000
for result in results:
    if result[1] > bestLinkingScore:
        bestLinkingScore = result[1]
        bestLinking = result[0]
    
bestParametersFile += "best resourceTilesNeeded = " + str(bestLinking[0]) + "\n"
bestParametersFile += "best cityTilesNeeded = " + str(bestLinking[1]) + "\n"

file = open("results/bestParametersFile.txt", "w")
file.write(bestParametersFile)
file.close()

# enemy approach

i = 1
results = []
for pathStep in range(2,9):
    for seed in seeds:
        
        #get these params in IA
        writeParamFile(base_pL, base_sP, base_sT, base_pR, base_rT, base_cT, pathStep, "pS_Tests_" + str(i) + "_" + str(seed))
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
        results.append([pathLength,getResult("results/pS_Tests_" + str(i) + "_" + str(seed) + ".txt")])
        os.remove("results/pS_Tests_" + str(i) + "_" + str(seed) + ".txt")
        
    i += 1

#get the best parameter
bestPathStep = 2
bestPathStepScore = -100000000000
for result in results:
    if result[1] > bestPathStepScore:
        bestPathStepScore = result[1]
        bestPathStep = result[0]
    
bestParametersFile += "best path step = " + str(bestPathStep) + "\n\n"

file = open("results/bestParametersFile.txt", "w")
file.write(bestParametersFile)
file.close()