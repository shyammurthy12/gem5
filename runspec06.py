import subprocess
import os
import multiprocessing.dummy

def GetTicks(statsfile):
    with open(statsfile, 'r') as f:
        for line in f:
            if 'sim_ticks' in line:
                return int(line.split()[1])
spec_home_directory = "/u/s/h/shyamm/spec2006/spec_cpu2006/benchspec/CPU2006/"
benchmark_directories = {
    'milc':'433.milc',
    'soplex':'450.soplex',
    'sphinx3':'482.sphinx3',
    'povray':'453.povray',
    'zeusmp':'434.zeusmp',
    'GemsFDTD':'459.GemsFDTD',
    'lbm':'470.lbm',
    'bwaves':'410.bwaves',
    'namd':'444.namd',
    'calculix':'454.calculix',
    'leslie3d':'437.leslie3d',
    'gromacs':'435.gromacs',
    'bzip2':'401.bzip2',
    'mcf':'429.mcf',
    'hmmer':'456.hmmer',
    'libquantum':'462.libquantum',
    'h264ref':'464.h264ref',
    'omnetpp':'471.omnetpp',
    'astar':'473.astar',
    'sjeng':'458.sjeng',
    'gobmk':'445.gobmk'}


def RunAndGetTicks(cliargs, benchmark, outdir, pipe_to_file=True):
    if pipe_to_file:
        proc = subprocess.Popen(cliargs,
                stdout=open(spec_home_directory + \
            benchmark_directories[benchmark]+ \
            '/run/run_base_ref_i386-m32-gcc42-nn.0000/'+ \
            outdir + '/stdout.txt', 'w+'),
                stderr=open(spec_home_directory + \
                        benchmark_directories[benchmark]+\
                        '/run/run_base_ref_i386-m32-gcc42-nn.0000/'\
                        + outdir + '/stderr.txt', 'w+'))
    else:
        proc = subprocess.Popen(cliargs)
    proc.wait()
    print("Benchmark {} finished\n".format(benchmark))
    return GetTicks('{}/stats.txt'.format(spec_home_directory + \
        benchmark_directories[benchmark]+\
                '/run/run_base_ref_i386-m32-gcc42-nn.0000/'+ outdir))

curWorkingDirectory = os.getcwd()
def RunBenchmark(benchmark_name):
    runtype = '_test'
    runtype = \
           '_VC_DSR_with_hashing_scheme_instruction_based_lifetime_1024_entry'
    checkpoint_dir = '_VC_DSR_O3CPU_squashed_load_analysis'
#    runtype = '_pointer_chased_loads_ref'
    target_path = spec_home_directory + \
            benchmark_directories[benchmark_name]+\
            '/run/run_base_ref_i386-m32-gcc42-nn.0000/'+\
            benchmark_name+runtype
    #print(target_path)
    if not(os.path.isdir(target_path)):
        os.mkdir(target_path)
    target_path = curWorkingDirectory + "/"+  benchmark_name+runtype
    #print(target_path)
    if not(os.path.isdir(target_path)):
        os.mkdir(target_path)
    cliargs = [
            'run_gem5_x86_spec06_benchmark.sh',
            '{}'.format(benchmark_name),
            '{}'.format(benchmark_name+runtype),
            '{}'.format(benchmark_name + checkpoint_dir)
            ]
#    proc = subprocess.Popen(cliargs)
#    proc.wait()
    return RunAndGetTicks(cliargs,benchmark_name,benchmark_name+runtype)

benchmark_names = ['milc','soplex','sphinx3','povray',
        'zeusmp','GemsFDTD','lbm','bwaves','namd','calculix',
        'leslie3d','gromacs','bzip2','mcf','hmmer','libquantum',
        'h264ref','omnetpp','astar','sjeng','gobmk']
#benchmark_names = ['astar']
p = multiprocessing.dummy.Pool(len(benchmark_names))
tick_counts = p.map(RunBenchmark,benchmark_names)
print(tick_counts)

