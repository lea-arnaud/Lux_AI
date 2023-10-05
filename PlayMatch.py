import subprocess
import json
import os
import re
import sys
import random
import multiprocessing
import glob

seeds = [100]
bots = ["build-solution-x64\Debug\TeamName.exe", "build-solution-x64\Debug\TeamName.exe"]

cpu_cores = int(os.environ['LUX_CPU']) if 'LUX_CPU' in os.environ else 20
silent = 'LUX_SILENT' in os.environ and os.environ['LUX_SILENT'] != ''
verbose = 'LUX_VERBOSE' in os.environ and os.environ['LUX_VERBOSE'] != ''

if len(sys.argv) > 1:
    seeds = list(range(int(sys.argv[1].replace("r", ""))))
    if 'r' in sys.argv[1]: seeds = [random.randint(0, 10000) for _ in seeds]
if len(sys.argv) > 3:
    bots = [sys.argv[2], sys.argv[3]]

def run_game(seed):
    [bot1, bot2] = bots
    result = subprocess.run(["lux-ai-2021", "--seed="+str(seed), "--loglevel=0", bot1, bot2], stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, shell=True)
    if result.stderr != '' and not silent:
        print('\033[31mErrors occured: ----------------\033[0m')
        print(result.stderr)
        print('\033[31m--------------------------------\033[0m')
    fixed_output = re.sub('(ranks|rank|agentID|name|replayFile|seed)', '"\g<1>"', result.stdout)
    fixed_output = re.sub("'", '"', fixed_output)
    game_results = json.loads(fixed_output)
    game_results['ranks'].sort(key=lambda x: x['agentID'])
    [p0, p1] = game_results['ranks']
    print(f'Game seed={seed: 6} ', end='')
    if p0['rank'] == p1['rank']:
        print('\033[35mEquality\033[0m ', end='')
    elif p0['rank'] > p1['rank']:
        print('\033[35mWin     \033[0m ', end='')
    else:
        print('\033[35mDefeat  \033[0m ', end='')
    print('replay=' + game_results['replayFile'])
        
def read_to_string(file_path):
    with open(file_path, 'r') as file:
        return file.read()
       
if __name__ == '__main__':
    os.system('')
    print('Using bots\033[33m', *bots, '\033[0m')
    
    with multiprocessing.Pool(20) as p:
        p.map(run_game, seeds)
        
    if len(seeds) == 1:
        list_of_files = glob.glob('errorlogs/*')
        latest_file = max(list_of_files, key=os.path.getctime)
        print('\033[95mBOT1 LOG -------------\033[0m')
        print(read_to_string(f'{latest_file}\\agent_0.log'))
        print('\033[95mBOT2 LOG -------------\033[0m')
        print(read_to_string(f'{latest_file}\\agent_1.log'))