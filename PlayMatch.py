import subprocess
import json
import os
import re
import sys
import random
import multiprocessing
import glob

seeds = [200]
bots = ["build-solution-x64\Debug\\1.exe", "build-solution-x64\Debug\\1.exe"]

def is_env_var_set(name):
    return name in os.environ and os.environ[name] != ''

cpu_cores = int(os.environ['LUX_CPU']) if 'LUX_CPU' in os.environ else 20
silent = is_env_var_set('LUX_SILENT')
verbose = is_env_var_set('LUX_VERBOSE')
no_timeout = is_env_var_set('LUX_NOTIMEOUT')

if len(sys.argv) > 1:
    seeds = list(range(int(sys.argv[1].replace("r", ""))))
    if 'r' in sys.argv[1]: seeds = [random.randint(0, 10000) for _ in seeds]
if len(sys.argv) > 3:
    bots = [sys.argv[2], sys.argv[3]]

def run_game(seed):
    [bot1, bot2] = bots
    result = subprocess.run([
        "lux-ai-2021",
        "--seed="+str(seed),
        "--loglevel=0",
        "--statefulReplay",
        f"--maxtime={1000000000 if no_timeout else 100000000}",
        bot1, bot2
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, shell=True)
    if result.stderr != '' and not silent:
        print('\033[31mErrors occured: ----------------\033[0m')
        print(result.stderr)
        print('\033[31m--------------------------------\033[0m')
    fixed_output = re.sub('(ranks|rank|agentID|name|replayFile|seed)', '"\g<1>"', result.stdout)
    fixed_output = re.sub("'", '"', fixed_output)
    try:
        game_results = json.loads(fixed_output)
    except:
        print('\033[31mCrash   \033[0m', fixed_output)
        return
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
    print('Config:', *map(lambda x: (x, eval(x)), ['no_timeout', 'cpu_cores', 'silent', 'verbose']))
    print(f'Running on {len(seeds)=}')
    
    if len(seeds) == 1:
        run_game(seeds[0])
    else:
        with multiprocessing.Pool(min(20, len(seeds))) as p:
            p.map(run_game, seeds)
        
    if len(seeds) == 1:
        list_of_files = glob.glob('errorlogs/*')
        latest_file = max(list_of_files, key=os.path.getctime)
        print('\033[95mBOT1 LOG -------------\033[0m')
        print(read_to_string(f'{latest_file}\\agent_0.log'))
        print('\033[95mBOT2 LOG -------------\033[0m')
        print(read_to_string(f'{latest_file}\\agent_1.log'))