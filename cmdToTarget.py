
# send cmds to a target machine:
#   docmd(machine, cmd)
#   return (1, output) when succeeds, (0, []) when fails

import os, time, popen2, tempfile;

secsForWait = 30;
def waitForJob(p, machine):
    count = 0;
    while (p.poll()):
        time.sleep(1);
        count += 1;
        if count == secsForWait:
            killJobs = "kill -9 `ps -ef | grep %s | egrep -v 'grep|python' "\
                       "| awk '{ print $2}'` 2>/dev/null" % machine;
            os.system(killJobs);
            return 0;
    return 1;

def docmd(rsh, machine, cmd):
    # if scheme tmpfile is not used, p will fail to return if the
    # output of cmd is large
    tmpFile = tempfile.mktemp();
    p = popen2.Popen3("%s %s '%s' > %s 2>/dev/null" % (rsh, machine, cmd, tmpFile));

    if not waitForJob(p, machine): return (0, []);

    lines = open(tmpFile).readlines();
    os.remove(tmpFile);
    return (1, lines);

# check if the machine can be talked to by rsh
def isMachineOK(rsh, machine):
    p = popen2.Popen3("%s %s 'date 2>/dev/null'" % (rsh, machine));

    if not waitForJob(p, machine): return False;
    
    lists = p.fromchild.readlines();
    
    if len(lists)==0: return False;
    return True;
    
