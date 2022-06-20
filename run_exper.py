from subprocess import Popen, PIPE
import threading
import time
import random
import queue
import os
import sys
from pathlib import Path

arch_folder = "json_arch/"
benchmark_folder = "./applications/polybench/"

# target_arch = [(4,4),  (4,4, "leftmostmemory"), (4,4, "1reg"),(3,3), (5,5, "systolic")
all_arch = {'hycube_original.json': (4, 4)}
target_arch = {'hycube_original.json': (4, 4)}


ori_bench = ["2mm", "atax", "bicg", "cholesky", "doitgen", "gemm", "gemver", "gesummv", "mvt", "symm", "syr2k", "syrk", "trmm" ]
unroll_bench = ["2mm_unroll", "atax_unroll", "bicg_unroll", "cholesky_unroll", "doitgen_unroll", "gemm_unroll", "gemver_unroll", "gesummv_unroll", "mvt_unroll", "symm_unroll", "syr2k_unroll", "syrk_unroll", "trmm_unroll" ]
unroll_5_bench = ["2mm_unroll_5", "atax_unroll_5", "bicg_unroll_5", "cholesky_unroll_5", "doitgen_unroll_5", "gemm_unroll_5", "gemver_unroll_5", "gesummv_unroll_5", "mvt_unroll_5", "symm_unroll_5", "syr2k_unroll_5", "syrk_unroll_5", "trmm_unroll_5" ]

target_bench = ["2mm", "atax", "bicg", "cholesky", "doitgen"]
process_num =80 # number of cpu cores to be used

max_II = 36


#for GNN training
gnn_training_data_floder = "../lisa_gnn/data/morpher/morpher/"
gnn_training_data_target_arch = {  'hycube_original.json': (4, 4)}

gnn_training_start_index  = 0
training_num  = 10


class BasicTask(object):

    def __init__(self, name="", cmd="", args=[], output="none"):
        self._cmd = cmd
        self._args = args
        self._name = name
        self.output = output
        pass

    def run2(self):
        print
        'start task: %s' % self._name
        for i in range(10):
            n = random.randint(2, 5)
            print
            '%s is running..., sleep for %d sec ' % (self._name, n)
            time.sleep(n)
        print
        'End task: %s' % self._name

    def run(self):
        if self._cmd:
            print
            'Run command \'%s\' with arguments: %s' % (self._cmd, self._args)
            cmd_line = [self._cmd] + self._args
            print(cmd_line)
            if "none" in self.output:
                p = Popen(cmd_line)
            else:
                file =  open(self.output, "w")
                p = Popen(cmd_line, stdout= file, stderr=file)
            p.wait()
        elif self._args:
            p = Popen(self._args)
            p.wait()
        else:
            print
            "The command is NULL!"

    def setRunCommand(self, cmd):
        self._command = cmd

    def setArgument(self, args=[]):
        self._args = args

    def setName(self, name):
        self._name = name

    def getOutput(self):
        pass

    def getError(self):
        pass

    def getWarning(self):
        pass


class TaskManager(object):
    def __init__(self, n):
        self._pool = []
        self._bTerminated = False
        self._tasks = queue.Queue()
        self._tasks_lock = threading.Lock()
        self._sem = threading.Semaphore(0)
        self._num_task = n

        for i in range(n):
            name = 'Thread%30d' % i
            t = threading.Thread(target=self._run, name=name)
            self._pool.append(t)
            # t.start()

    def addTask(self, task):
        self._tasks_lock.acquire()
        self._tasks.put(task)
        self._tasks_lock.release()

    def _pick(self):
        print
        'Rest tasks: %d' % self._tasks.qsize()
        if self._bTerminated:
            return None

        self._tasks_lock.acquire()

        t = None
        if self._tasks.qsize() > 0:
            t = self._tasks.get()

        self._tasks_lock.release()

        return t

    def _run(self):
        # self._sem.acquire()
        while True:
            if self._bTerminated:
                break
            t = self._pick()
            if t:
                t.run()
            else:
                break

    def start(self):
        for t in self._pool:
            t.start()

        for t in self._pool:
            t.join()

    def stop(self):
        self._bTerminated = True
        self.start()

    def setNumThreads(self, n):
        self._num_task = n



tm = TaskManager(process_num)

# if not os.path.exists('lisa_training_log'):
#         os.makedirs('lisa_training_log')




#iterate arch file
# os.system('cp ./build/bin/cgrame ./build/bin/sa_more_time_cgrame')

if len(sys.argv) >1 and "t_lisa" in str(sys.argv[1]):
    os.system('cp ./build/src/cgra_xml_mapper ./build/src/cgra_xml_mapper_tlisa')
    for arch in target_arch.keys():
        arch_file = arch_folder + arch
        #iterate benchmark 
        for bench in target_bench:
            bench_file =  benchmark_folder + bench  + ".xml"
            print(arch_file, bench_file)
            # arg = [ "-m",  "0",  "-j",arch_file,  "-d", bench_file,">", "log/"+arch+"_"+bench+".txt"]
            arg = [ "-m",  "2",  "-j",arch_file,  "-d", bench_file, "--lisa_training"]
            ts = BasicTask(name="mapper", cmd="./build/src/cgra_xml_mapper_tlisa", args=arg)
            tm.addTask(ts)


elif len(sys.argv) >1 and "gnn_training_data" in str(sys.argv[1]):
    os.system('cp ./build/src/cgra_xml_mapper ./build/src/cgra_xml_mapper_gnndata')
    if not os.path.exists('lisa_training_log/'):
        os.makedirs('lisa_training_log/')

    for arch, size in gnn_training_data_target_arch.items():
        arch_file = arch_folder + arch
        arch_model_name = "morpher_"+str(size[0]) + "_" + str(size[1])
        #iterate benchmark 
        if not os.path.exists("../lisa_gnn/data/labels/"+arch_model_name):
            os.makedirs("../lisa_gnn/data/labels/"+arch_model_name)
            Path("../lisa_gnn/data/labels/"+arch_model_name+"/label_evaluate.txt").touch()


        for i in range (gnn_training_start_index, gnn_training_start_index + training_num):
            bench_file =  gnn_training_data_floder + str(i)  + ".xml"
            print(arch_file, "arch_model_name",bench_file)
            # arg = [ "-m",  "0",  "-j",arch_file,  "-d", bench_file,">", "log/"+arch+"_"+bench+".txt"]
            arg = [ "-m",  "2",  "-j",arch_file,  "-d", bench_file, "--lisa_training", "--dfg_id", str(i), "--arch_name", arch_model_name]
            ts = BasicTask(name="mapper", cmd="./build/src/cgra_xml_mapper_gnndata", args=arg)
            tm.addTask(ts)

elif len(sys.argv) >1 and "baseline" in str(sys.argv[1]):
    os.system('cp ./build/src/cgra_xml_mapper ./build/src/cgra_xml_mapper_baseline')
    for arch in target_arch.keys():
        arch_file = arch_folder + arch
        #iterate benchmark 
        for bench in target_bench:
            bench_file =  benchmark_folder + bench  + ".xml"
            print(arch_file, bench_file)
            # arg = [ "-m",  "0",  "-j",arch_file,  "-d", bench_file,">", "log/"+arch+"_"+bench+".txt"]
            arg = [ "-m",  "0",  "-j",arch_file,  "-d", bench_file]
            ts = BasicTask(name="mapper", cmd="./build/src/cgra_xml_mapper_baseline", args=arg)
            arg = [ "-m",  "1",  "-j",arch_file,  "-d", bench_file]
            ts = BasicTask(name="mapper", cmd="./build/src/cgra_xml_mapper_baseline", args=arg)
            tm.addTask(ts)
elif len(sys.argv) >1 and "lisa" in str(sys.argv[1]):
    os.system('cp ./build/src/cgra_xml_mapper ./build/src/cgra_xml_mapper_lisa')
    for arch, size in target_arch.items():
        arch_file = arch_folder + arch
        #iterate benchmark 
        for bench in target_bench:
            bench_file =  benchmark_folder + bench  + ".xml"
            print(arch_file, bench_file)
            # arg = [ "-m",  "0",  "-j",arch_file,  "-d", bench_file,">", "log/"+arch+"_"+bench+".txt"]
            arg = [ "-m",  "0",  "-j",arch_file,  "-d", bench_file]
            ts = BasicTask(name="mapper", cmd="./build/src/cgra_xml_mapper_lisa", args=arg)
            tm.addTask(ts)

        



tm.start()