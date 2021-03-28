#!/usr/bin/env python
'''
Created on Aug 23, 2014

@author: tung
'''
import sys, os, time, multiprocessing, optparse 
import subprocess, logging, datetime

def cpu_count():
    ''' Returns the number of CPUs in the system
    '''
    num = 1
    if sys.platform == 'win32':
        try:
            num = int(os.environ['NUMBER_OF_PROCESSORS'])
        except (ValueError, KeyError):
            pass
    elif sys.platform == 'darwin':
        try:
            num = int(os.popen('sysctl -n hw.ncpu').read())
        except ValueError:
            pass
    else:
        try:
            num = os.sysconf('SC_NPROCESSORS_ONLN')
        except (ValueError, OSError, AttributeError):
            pass

    return num
    
def show_task_progress(frac, message):
    barLen = 75
    charsInGreen = int( frac * barLen );
    while len(message) < barLen:
        message += " "
    charsInGreenOrCyan  = len(message)
    if barLen < charsInGreenOrCyan:
        charsInGreenOrCyan = barLen
    formatted = "\33[2K\r\33[1;30;102m"
    formatted += message[0:charsInGreen] + "\33[1;30;106m"
    formatted += message[charsInGreen:charsInGreenOrCyan]
    formatted += "\33[0m" + message[charsInGreenOrCyan:len(message)]
    print(formatted,end="")

def show_task_complete(text):
    print("\33[2K\r" + text)

def exec_commands(cmds, name, num_cpus):
    ''' Exec commands in parallel in multiple process 
    (as much as we have CPU)
    '''
    if not cmds: return  # empty list

    def done(p):
        return p.poll() is not None
    def success(p):
        return p.returncode == 0
    def fail():
        sys.exit(1)
        
    # max_task = cpu_count()
    logger = logging.getLogger(name)
    logger.setLevel(logging.DEBUG)
    my_time = datetime.datetime.now()
    handler = logging.FileHandler(name + "." + str(my_time.year) + str(my_time.month) + str(my_time.day) + 
                                  str(my_time.hour) + str(my_time.minute) + str(my_time.second) + ".log")
    handler.setLevel(logging.DEBUG)
    formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    handler.setFormatter(formatter)
    logger.addHandler(handler)
    max_task = multiprocessing.cpu_count()
    logger.info("Available CPUs = " + str(max_task) + " / using " + str(num_cpus) + " CPUs")
    logger.info("Number of jobs = " + str(len(cmds)))
    processes = []
    cmdCount = len(cmds)
    cmdNumber = 0
    while True:
        while cmds and len(processes) < num_cpus:
            task = cmds.pop(0)
            #print subprocess.list2cmdline(task)
            task_id, cmd = task.split(" ", 1)
            logger.info("Executing job " + task_id + ": " + cmd.strip())
            #print cmd
            cmdNumber = cmdNumber + 1
            show_task_progress(cmdNumber/cmdCount, "Starting command " + str(cmdNumber) + " of " + str(cmdCount))
            task_output = open(task_id + ".out", "w")
            time_cmd = "time " + cmd
            processes.append([subprocess.Popen(time_cmd, stderr=subprocess.STDOUT, stdout=task_output, shell=True), task_id])

        for p in processes:
            if done(p[0]):
                if success(p[0]):
                    #print "Process with ID = ", p.pid, " has finished"
                    #print "number of processes before removal: ", len(processes)
                    logger.info("Job " + p[1] + " has finished")
                    processes.remove(p)
                    #print "number of processes after removal: ", len(processes)
                else:
                    logger.info("Job " + p[1] + " finished with ERROR CODE " + str(p[0].returncode))
                    processes.remove(p)

        if not processes and not cmds:
            show_task_complete("All " + str(cmdCount) + " tasks completed")
            break
        else:
            time.sleep(5)
        
if __name__ == '__main__':
    max_cores = multiprocessing.cpu_count()
    usage = "USAGE: %prog [options]"
    parser = optparse.OptionParser(usage=usage)
    parser.add_option('-f','--cmd', dest="cmd", help='File containing all commands')
    parser.add_option('-c','--cpu', dest="cpu", help='Number of CPU to use', default=max_cores)
    parser.add_option('-d','--dir', dest="directory", help='Output directory to use', default='.')
    (options, args) = parser.parse_args()
    if len(sys.argv) == 1:
        parser.print_help()
        exit(0)
    if options.cmd == "STDIN" or options.cmd == "":
        jobs = sys.stdin.readlines()
    else:
        jobs = open(options.cmd, "r").readlines()
    if (options.directory != "."):
        if not os.path.exists(options.directory):
            os.mkdir(options.directory)
        os.chdir(options.directory)
    exec_commands(jobs, options.cmd, int(options.cpu))
    
